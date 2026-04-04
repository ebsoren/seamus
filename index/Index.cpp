#include "Index.h"
#include "lib/consts.h"
#include "lib/algorithm.h"
#include "lib/string.h"
#include "lib/utf8.h"
#include "lib/utils.h"
#include "lib/logger.h"
#include <sys/stat.h>


string IndexChunk::get_index_chunk_path() const {
    return string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(WORKER_NUMBER), "_", string(chunk), ".txt");
}


IndexChunk::IndexChunk(uint32_t worker_number) : curr_doc_(1), chunk(0), posts_count(0), WORKER_NUMBER(worker_number){
    // Important: Init curr_doc_ to 1 to allow for 00 to be used as new doc flag
    // Find the latest chunk ID for this thread
    while (file_exists(get_index_chunk_path())) chunk++;
}


void IndexChunk::persist() {
    mkdir(INDEX_OUTPUT_DIR, 0755); // no-op if already exists
    // Create a file (if it already exists, fail -- don't want to overwrite)
    string path = get_index_chunk_path();
    FILE* fd = fopen(path.data(), "wx");

    if (fd == nullptr) {
        logger::error("Worker %u: failed to open '%s' for writing (errno=%d: %s)", WORKER_NUMBER, path.data(), errno, strerror(errno));
        return;
    }

    // 4 bytes for each uint32_t ID, one byte for each char in string, one byte for each new line char
    uint64_t urls_bytes = urls.size() * 4;
    for (size_t i = 0; i < urls.size(); i++) urls_bytes += urls[i].size() + 1;

    // Write the size of the ID->URL mapping
    // <64b SIZE>\n
    fwrite(&urls_bytes, sizeof(urls_bytes), 1, fd);
    fwrite("\n", sizeof(char), 1, fd);

    // Write the ID->URL mapping
    // <32b ID> <varlen URL>\n
    for (uint32_t i = 0; i < urls.size(); i++) {
        uint32_t id = i + 1; // 1 indexed
        
        fwrite(&id, sizeof(id), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(urls[i].data(), sizeof(char), urls[i].size(), fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    vector<string> alphabetized_entries = sort_entries();
    const uint32_t N = alphabetized_entries.size();

    /**
     * FIRST PASS over postings
     * Calculate dictionary lookup table values (bytes from start of dict to first word of each letter)
     * Calculate posting list sizes/locations
     * Caclulate internal index sizes (part of posting list sizes) and locations
     *  */

    vector<uint64_t> posting_list_locations(N);
    uint64_t posting_list_size = 0;

    uint64_t dict_offsets[26];
    dict_offsets[0] = 0;
    uint64_t curr_offset = 0;
    char curr_char = 'a';

    vector<vector<uint64_t>> doc_offsets(N, vector<uint64_t>(curr_doc_ + 1, UINT32_MAX));
    vector<uint64_t> internal_index_sizes(N);

    for (uint32_t i = 0; i < N; i++) {
        // First word starting with this letter -- add its offset to the dict lookup table
        if (alphabetized_entries[i][0] > curr_char) {
            curr_char = alphabetized_entries[i][0];
            dict_offsets[curr_char - 'a'] = curr_offset;
        }

        // One byte per char, 6 bytes for posting list offset, 1 byte each for space and new line
        curr_offset += alphabetized_entries[i].size() + 6 + 2; 

        // Mark where the byte offset where current word's posting list begins
        posting_list_locations[i] = posting_list_size;

        // Header for each posting list: 64 bits each for # posts & # docs, plus 2 separating characters
        posting_list_size += 6 + 6 + 2;

        // Used to calculate the offset (instead of absolute value)
        uint32_t last_doc = 0;
        uint32_t last_loc = 0;

        // Used to fill the internal index at the top of a posting list, which maps doc ID to byte offset from start of index
        uint64_t internal_offset = 0;
        postings& entry = index[alphabetized_entries[i].str_view(0, alphabetized_entries[i].size())];
        uint32_t num_docs = entry.posts[0].doc == 0 ? 1 : 0;

        // 32 bit ID, 64 bit offset, 2 filler chars per entry
        const uint64_t INTERNAL_INDEX_ENTRY_SIZE = 5 + 6 + 2;

        for (post p : entry.posts) {
            // Utf8 encoding size of loc offset (no delimiters)
            uint64_t post_size = SizeOfUtf8(p.loc - last_loc);

            // Update offsets
            if (p.doc > last_doc) {
                // Only write a doc offset if it's a new document, in which case add 1 byte for leading flag
                post_size += 1 + SizeOfUtf8(p.doc - last_doc);
                last_doc = p.doc;
                last_loc = 0;

                doc_offsets[i][p.doc] = internal_offset;
                num_docs++;
            } else {
                last_loc = p.loc;
            }

            // Add to sizes
            posting_list_size += post_size;
            internal_offset += post_size;
        }

        // Calculate size of internal index itself
        internal_index_sizes[i] = num_docs * INTERNAL_INDEX_ENTRY_SIZE;

        // Extra 1 for newline at end of each word's posting list
        posting_list_size += internal_index_sizes[i] + 1;
    }

    // Write dictionary lookup table
    // <1B LETTER> <64b OFFSET>\n
    for (int i = 0; i < 26; i++) {
        char c = char(i + 'a');
        fwrite(&c, sizeof(char), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(dict_offsets + i, sizeof(uint64_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    // Write the dictionary itself
    // <varlen WORD> <64b OFFSET>\n
    for (uint32_t i = 0; i < N; i++) {
        fwrite(alphabetized_entries[i].data(), sizeof(char), alphabetized_entries[i].size(), fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(&posting_list_locations[i], sizeof(uint64_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    /**
     * SECOND PASS over postings
     * Write the posting lists themselves
     */

    // Loop through all words
    for (uint32_t i = 0; i < N; i++) {
        postings& entry = index[alphabetized_entries[i].str_view(0, alphabetized_entries[i].size())];
        uint64_t size = entry.posts.size(); // Needs to be an lvalue for fwrite

        // Write the number of occurrences and documents
        // <64b NUM POSTS> <64b NUM DOCS>\n
        fwrite(&size, sizeof(uint64_t), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(&entry.n_docs, sizeof(uint64_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);

        // Write the internal index
        // For each document that appears in this posting list: <32b DOC ID> <64b BYTE OFFSET FROM START OF INTERNAL INDEX>\n
        for (uint32_t j = 0; j < curr_doc_ + 1; j++) {
            // Skip docs that don't appear
            if (doc_offsets[i][j] == UINT32_MAX) continue;

            uint64_t total_offset = doc_offsets[i][j] + internal_index_sizes[i];
            fwrite(&j, sizeof(uint32_t), 1, fd);
            fwrite(" ", sizeof(char), 1, fd);
            fwrite(&total_offset, sizeof(uint64_t), 1, fd);
            fwrite("\n", sizeof(char), 1, fd);
        }

        // Write posts
        uint32_t last_doc = 0;
        uint32_t last_loc = 0;

        Utf8 doc_buff[MAX_UTF8_LEN + 1];
        Utf8 loc_buff[MAX_UTF8_LEN];
        doc_buff[0] = 0; // Set first bit of doc_buff to be the flag

        // For each word occurrence: (IF NEW DOC <0b0||varlen DOC OFFSET>)<varlen LOC ID/OFFSET>
        // Because we're doing UTF 8, no delimiters
        for (post p : entry.posts) {
            // Only write the doc offset if it's a new document, in which case write the offset after the flag
            if (p.doc > last_doc) {
                Utf8* doc_end = WriteUtf8(doc_buff + 1, p.doc - last_doc, doc_buff + MAX_UTF8_LEN + 1);
                fwrite(doc_buff, sizeof(Utf8), doc_end - doc_buff, fd);
            }

            // Write the loc offset
            Utf8* loc_end = WriteUtf8(loc_buff, p.loc - last_loc, loc_buff + MAX_UTF8_LEN);
            fwrite(loc_buff, sizeof(Utf8), loc_end - loc_buff, fd);

            // Update offsets
            if (p.doc > last_doc) {
                last_doc = p.doc;
                last_loc = 0;
            } else {
                last_loc = p.loc;
            }
        }

        fwrite("\n", sizeof(char), 1, fd);
    }
    
    fclose(fd);

    chunk++;
}

void IndexChunk::reset() {
    index = unordered_map<string, postings>();
    urls = vector<string>();
    posts_count = 0;
    curr_doc_ = 1; // Curr_doc must start at 1, 0 reserved for flag
}


void IndexChunk::flush() {
    persist();
    reset();
    logger::info("Worker %u: flush chunk: %u", WORKER_NUMBER, chunk-1);
}


vector<string> IndexChunk::sort_entries() {
    vector<string> res;
    res.reserve(index.size());

    for (auto it = index.begin(); it != index.end(); ++it) {
        res.push_back(string((*it).key.data(), (*it).key.size()));
    }

    radix_sort(res);
    return res;
}

bool IndexChunk::index_file(const string &path) {
    FILE* fd = fopen(path.data(), "r");
    if (fd == nullptr) {
        perror("Error opening file.\n");
        return false;
    }

    char buff[4096];
    char url[2048];
    
    while (true) {
        // Set of words already encountered in the document to track number of documents word appears in
        unordered_map<string, bool> word_set;

        // Check doc header (or EOF between documents)
        if (!fgets(buff, sizeof(buff), fd)) break;
        if (strcmp(buff, "<doc>\n")) {
            logger::error("Worker %u: expected <doc> header, got: %s in file: %s", WORKER_NUMBER, buff, path.data());
            fclose(fd);
            return false;
        }

        // Read in the URL (will end with \n\0)
        if (!fgets(url, sizeof(url), fd)) break;

        // Increment the doc count
        uint32_t doc = curr_doc_++;
        urls.push_back(string(url, strlen(url) - 1)); // -1 bc of \n at the end

        // Start a counter for word locations
        uint32_t loc = 0;

        // Parse title and body words
        while(fgets(buff, sizeof(buff), fd)) {
            if (!strcmp(buff, "</doc>\n")) {
                // Doc ended, go back to outer loop
                break;
            } else if (strcmp(buff, "</title>\n") && strcmp(buff, "<title>\n")) { // Don't push the title tags
                // -1 because all words have new line at the end from fgets
                size_t len = strlen(buff);
                if (len <= 1) continue; // skip empty lines
                string_view word_view = string_view(buff, len - 1);

                if (!word_set[word_view]) {
                    word_set[word_view] = true;
                    index[word_view].n_docs++;
                }

                index[word_view].posts.push_back({doc, ++loc});
                posts_count++;
            }
        }

        if (ferror(fd)) {
            logger::error("Worker %u: file read error on file: %s, with error: %s", WORKER_NUMBER, path.data(), strerror(errno));
            fclose(fd);
            return false;
        }
    }

    fclose(fd);
    logger::info("Worker %u: indexed file: %s", WORKER_NUMBER, path.data());

    if (posts_count > INDEX_POSTS_COUNT_FLUSH_THRESHOLD) {
        flush();
    }
    return true;
}
