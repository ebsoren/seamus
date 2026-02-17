#include "Index.h"

#include "lib/algorithm.h"
#include "lib/utf8.h"
#include "lib/utils.h"

void init_index() {
    // Find the latest chunk ID
    if (chunk == 0) {
        while (file_exists("index_chunk_" + string::to_string(WORKER_NUMBER) + "_" + string::to_string(chunk) + ".txt")) chunk++;
    }

    IndexChunk index;
}

void IndexChunk::persist() {
    // Create a file (if it already exists, fail -- don't want to overwrite)
    FILE* fd = fopen("index_chunk_" + string::to_string(WORKER_NUMBER) + "_" + string::to_string(chunk) + ".txt", "wx");

    if (fd == nullptr) perror("Error opening index chunk file for writing.");

    // 4 bytes for each uint32_t ID, one byte for each char in string, one byte for each new line char
    uint64_t urls_bytes = urls.size() * 4;
    for (string s : urls) urls_bytes += s.size() + 1;

    // Write the size of the ID->URL mapping
    fwrite(&urls_bytes, sizeof(urls_bytes), 1, fd);
    fwrite("\n", sizeof(char), 1, fd);

    // Write the ID->URL mapping
    for (uint32_t i = 0; i < urls.size(); i++) {
        fwrite(&i, sizeof(i), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(&urls[i], sizeof(char), urls[i].size(), fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    vector<string> alphabetized_entries = sort_entries();
    const int N = alphabetized_entries.size();

    /**
     * FIRST PASS over postings
     * Calculate dictionary lookup table values (bytes from start of dict to first word of each letter)
     * Calculate posting list sizes/locations
     *  */ 
    uint64_t posting_list_locations[N];
    uint64_t posting_list_size = 0;

    uint64_t dict_offsets[26];
    dict_offsets[0] = 0;
    uint64_t curr_offset = 0;
    char curr_char = 'a';

    for (int i = 0; i < N; i++) {
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
        vector<uint64_t> doc_offsets(curr_doc_ + 1, 0);
        uint64_t internal_offset = 0;
        uint32_t num_docs = index[alphabetized_entries[i]].posts[0].doc == 0 ? 1 : 0;

        // 32 bit ID, 64 bit offset, 2 filler chars per entry
        const uint64_t INTERNAL_INDEX_ENTRY_SIZE = 5 + 6 + 2;

        for (post p : index[alphabetized_entries[i]].posts) {
            // Utf8 encoding size of doc & loc offset, plus two separating characters
            posting_list_size += SizeOfUtf8(p.doc - last_doc) + SizeOfUtf8(p.loc - last_loc) + 2;
            internal_offset += SizeOfUtf8(p.doc - last_doc) + SizeOfUtf8(p.loc - last_loc) + 2;

            // Update offsets
            if (p.doc > last_doc) {
                last_doc = p.doc;
                last_loc = 0;

                doc_offsets[p.doc] = internal_offset;
                num_docs++;
            } else {
                last_loc = p.loc;
            }
        }

        // Calculate size of internal index itself
        uint64_t internal_index_size = num_docs * INTERNAL_INDEX_ENTRY_SIZE;
        posting_list_size += internal_index_size;
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
        fwrite(&alphabetized_entries[i], sizeof(char), alphabetized_entries[i].size(), fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(&posting_list_locations[i], sizeof(uint64_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    // TODO: Write the posting lists (SECOND PASS)
    // DON'T FORGET to add internal_index_size to all elements in internal index
    fclose(fd);
}

vector<string> IndexChunk::sort_entries() {
    vector<string> res(index.size());

    // TODO: Have to copy all the keys into res

    radix_sort(res);
    return res;
}