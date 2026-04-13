// parser.h
// Adapted from HtmlParser by Nicole Hamilton, nham@umich.edu

#pragma once

#include <cstring>

#include <fcntl.h>

#include <sys/stat.h>

#include "../lib/buffer.h"
#include "../lib/consts.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "../url_store/url_store.h"
#include "HtmlTags.h"
#include "lib/logger.h"
#include "url_buffers.h"
#include "word_array.h"

class HtmlParser {
public:
    HtmlParser()
        : parser_id_(-1)
        , out_fd_(-1)
        , url(""),
        title("") {}

    HtmlParser(size_t parser_id, LocalUrlBuffer *url_buff, UrlStore *url_store)
        : parser_id_(parser_id)
        , out_fd_(-1)
        , url(""),
        title("")
        , urlStore(url_store)
        , localBuffer(url_buff) {
        open_output_file();
        setup_link_callback();
    }

    ~HtmlParser() {
        if (out_fd_ >= 0) close(out_fd_);
    }

    HtmlParser &operator=(HtmlParser &&rhs) noexcept {
        if (out_fd_ >= 0) close(out_fd_);
        parser_id_ = rhs.parser_id_;
        out_fd_ = rhs.out_fd_;
        rhs.out_fd_ = -1;
        url = string("");
        title = string("");
        words.fd_ = out_fd_;
        localBuffer = rhs.localBuffer;
        urlStore = rhs.urlStore;
        setup_link_callback();
        return *this;
    }

    bool killed() const { return killed_; }

    // 1. Read into buffer
    // 2. Parse buffer
    // 3. Move anything un-parsed to the front of the buffer
    // General use - call in a loop until processing full page
    ssize_t parse_page(char *page, size_t size, uint16_t hops, uint16_t domain_hops, const char *link) {
        // Set member vars to reflect current page we're parsing
        hops_ = hops;
        domain_hops_ = domain_hops;
        url = string(link);

        killed_ = false;
        non_alnum_run_ = 0;
        in_a_ = false;
        in_title_ = false;
        done_with_title_ = false;
        in_comment_ = false;
        in_discard_ = false;
        base_found_ = false;
        discard_tag_len_ = 0;

        logger::debug("HtmlParser writing headers");
        // Write headers to links & doc for a new page
        write_headers();

        // Parse the page contents
        if (killed_) return 0;
        logger::debug("HtmlParser setting buffer");
        buf.set_buffer(page, size);


        logger::debug("HtmlParser parsing buffer");
        size_t consumed = parse_buf();

        logger::debug("HtmlParser finishing");
        finish();
        return consumed;
    }

    void finish() {
        // if (buf.size() > 0) {
        //     parse_buf();
        //     buf.clear();
        // }
        buf.clear();
        // Mark that this doc is done
        write_footers();

        // Flush any data remaining in buffer
        words.case_convert();
        words.flush();

        // Pass the links buffer to local URL buffer to disseminate
        /**
         * NOTE: This currently blocks. We discussed in 3/16 meeting whether we like that.
         * Esben is pro: thinks that avoiding copies > not blocking.
         * Aiden is anti: thinks that allowing max parallelism > avoiding copies.
         * To switch this to non-blocking, only two small changes are needed:
         *  a. Put this call in a thread
         *  b. Pass by copy, not by reference, to add_urls
         */
        localBuffer->add_urls(links);

        // Reset links word array
        links.reset();
    }

    void inline write_headers() {
        words.push_back("<doc>", 5);
        words.push_back(url.data(), url.size());
        write_link_headers();
    }

    // Only writes the link-side header. Used by the link flush callback
    // to start a fresh link batch without touching the words buffer, which
    // is still in the middle of the current document.
    void inline write_link_headers() {
        links.push_back("<doc>", 5);
        string hops_str(hops_);
        string dhops_str(domain_hops_);
        links.push_back(hops_str.data(), hops_str.size(), SPACE_DELIM);
        links.push_back(dhops_str.data(), dhops_str.size());
        string domain = extract_domain(string(url.data(), url.size()));
        links.push_back(domain.data(), domain.size());
    }

    void inline write_footers() {
        words.push_back("</doc>", 6);
        links.push_back("</doc>", 6);
    }

private:
    size_t parser_id_;
    int out_fd_;
    uint16_t hops_;
    uint16_t domain_hops_;
    string url;
    string title;
    buffer buf;
    char base[MAX_BASE_LEN] = {};
    size_t base_len = 0;
    word_array<MAX_WORD_MEMORY> words;
    link_array<MAX_LINK_MEMORY> links;
    UrlStore *urlStore;
    LocalUrlBuffer *localBuffer;

    void setup_link_callback() {
        links.set_callback([this](link_array<MAX_LINK_MEMORY> &arr) {
            logger::debug("link_array callback: flushing %zu bytes to add_urls", arr.size());
            links.push_docend();
            localBuffer->add_urls(arr);
            links.reset();
            // Only reset the link-side header -- the words buffer is still
            // mid-document and must not get a spurious <doc> marker.
            write_link_headers();
        });
    }

    inline void open_output_file() {
        mkdir(PARSER_OUTPUT_DIR, 0755);   // no-op if already exists
        char file_name[128];
        snprintf(file_name, sizeof(file_name), "%s/parser_%zu_out.txt", PARSER_OUTPUT_DIR, parser_id_);
        out_fd_ = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (out_fd_ < 0) {
            logger::error("open_output_file: open failed for '%s' (errno=%d)", file_name, errno);
        } else {
            logger::info("open_output_file: fd %d -> '%s'", out_fd_, file_name);
        }
        words.fd_ = out_fd_;
    }

    // Internal parse that operates on the current buffer contents
    size_t parse_buf() {
        const char *data = buf.data();         // Front of buffer
        const char *end = data + buf.size();   // End of buffer
        // Where parse_chunk should start (may be larger than data,
        // because the front of the buffer may have remnants from last chunk)
        const char *parse_start = data;

        // If continuing a comment from previous chunk, scan for -->
        if (in_comment_) {
            const char *p = data;
            while (p + 2 < end) {
                if (*p == '-' && *(p + 1) == '-' && *(p + 2) == '>') {
                    in_comment_ = false;

                    // Tell parse_chunk to ignore the comment
                    parse_start = p + 3;
                    break;
                }
                p++;
            }
            if (in_comment_) {
                // Entire chunk is still inside comment. Consume everything
                // except last 4 bytes, in case --> is cut off again.
                return (buf.size() >= 4) ? buf.size() - 4 : 0;
            }
        }

        // If continuing a discard section from previous chunk, scan for </tag>
        // Works pretty similarly to the in_comment_ logic.
        if (in_discard_) {
            const char *p = data;
            while (p < end) {
                if (*p == '<' && p + 1 < end && *(p + 1) == '/') {
                    const char *close_name = p + 2;
                    const char *close_p = close_name;
                    while (close_p < end && *close_p != '>' && !isspace(*close_p)) close_p++;
                    if (close_p < end && (size_t) (close_p - close_name) == discard_tag_len_
                        && !strncasecmp(close_name, discard_tag_, discard_tag_len_)) {
                        // Found matching close tag, skip past >
                        while (close_p < end && *close_p != '>') close_p++;
                        in_discard_ = false;

                        // Tell parse_chunk to ignore the discard area
                        parse_start = (close_p < end) ? close_p + 1 : end;
                        break;
                    }
                }
                p++;
            }
            // Entire chunk should be discarded. Discard everything
            // except last 12 bytes (max closing tag: </script> is 9 bytes).
            if (in_discard_) {
                return (buf.size() >= 12) ? buf.size() - 12 : 0;
            }
        }

        // Scan backwards until finding the first '>' to avoid splitting a tag, which would kinda screw us.
        // This also implicitly avoids splitting a word, which is nice.
        const char *safe_end = end;
        {
            const char *scan = end;
            while (scan > parse_start && *(scan - 1) != '>') scan--;
            if (scan > parse_start) safe_end = scan;
            // If no '>' found after parse_start, parse everything (plain text chunk)
        }


        logger::debug("Parser parsing HTML chunk");
        // Parse region after fixing any split-buffer issues
        parse_chunk(parse_start, safe_end - parse_start);

        // Return how many bytes from the front of the buffer were consumed
        return safe_end - data;

        /* Example to show why this stuff is necessary:

            buffer: [ello World! --> Hello World! </div> <d]

            1. parser sees we're in comment (from prev chunk), so it goes until finding
            '-->'. It sets parse_start to the ' ' after '-->'. That way the parse_chunk
            call ignores the residual comment.

            2. parse finds safe_end at the '>' in </div>. This 'reserves' the
            <d at the end of the buffer which will be saved and processed in the next parse.
            This way, we don't try to parse the <d which would be catastrophic as the
            next buffer would start with 'iv/>'.

            3. parse calls parse_chunk(parse_start, safe_end - parse_start), which
            ends up passing ' Hello World! </div> '. This is safe to parse.


        */
    }

    // Persistent state
    bool in_a_ = false;         // in <a>
    bool in_title_ = false;     // in <title>
    bool done_with_title_ = false;
    bool base_found_ = false;   // found base link

    // set to true if found MAX_CONSECUTIVE_NON_ALNUM non-alphanumeric characters in a row. Abort mission under the
    // assumption the page is not english or is at minimum low-quality junk
    bool killed_ = false;
    int non_alnum_run_ = 0;   // Flag to track how many non-alnums we have seen in a row

    // Needed for multi-chunk comments and discard sections
    bool in_comment_ = false;
    bool in_discard_ = false;

    // Store tag we are looking for from old buffer so new buffer has the right context
    // Looks problematic in the case of nesting, but this is how it's done normally.
    char discard_tag_[16] = {};
    size_t discard_tag_len_ = 0;

    // Characters we want to break words on
    static bool is_word_break_char(const char c) {
        return isspace(c) || c == ',' || c == '.' || c == ':' || c == ';' || c == '!' || c == '?' || c == '('
            || c == ')' || c == '"' || c == '[' || c == ']' || c == '{' || c == '}' || c == '-' || c == '+';
    }

    static bool comma_in_number(const char *p, const char *start, const char *end) {
        return (*p == ',' || *p == '.') && (p + 1 < end && *(p + 1) >= '0' && *(p + 1) <= '9')
            && (p > start && *(p - 1) >= '0' && *(p - 1) <= '9');
    }

    // Core parsing logic, man i'm glad David did most of this
    void parse_chunk(const char *buffer, size_t length) {
        if (!buffer || length == 0) {
            logger::warn("parse_chunk called with %s buffer, length=%zu", buffer ? "valid" : "null", length);
            return;
        }
        logger::debug("parse_chunk: parsing %zu bytes, words.size=%zu, links.size=%zu", length, words.size(),
                      links.size());

        const char *end = buffer + length;
        const char *p = buffer;
        const char *word_start = p;
        uint32_t num_words = 0;

        // Parse <length> bytes starting at <buffer>
        while (p < end) {
            if (is_word_break_char(*p)) {
                // Write back word if relevant
                if (p > word_start) {
                    size_t word_len = p - word_start;
                    if (in_a_) {
                        !comma_in_number(p, buffer, end) ? links.push_back(word_start, word_len, SPACE_DELIM)
                                                         : links.push_back(word_start, word_len, NULL_DELIM);

                        // Convert just the anchor text, not the URL (since URLs case sensitive)
                        links.case_convert(links.size() - (word_len + 1), links.size());
                    }

                    // If a comma in between two ints, treat the whole number as a single word
                    !comma_in_number(p, buffer, end) ? words.push_back(word_start, word_len)
                                                     : words.push_back(word_start, word_len, NULL_DELIM);
                    if (in_title_ && done_with_title_) {
                        title = string::join(" ", title, string(word_start, word_len));
                    }
                    num_words++;
                    word_start = ++p;
                } else {   // Not tracking a word, just continue
                    p++;
                    word_start++;
                }
            }

            // Handle potential start of tag
            else if (*p == '<') {
                const char *tag_start = ++p;
                bool is_closing = false;
                if (p < end && *p == '/') {   // Closing tag
                    is_closing = true;
                    tag_start = ++p;
                }

                // Continue until end of tag name
                while (p < end && !(isspace(*p) || *p == '/' || *p == '>')) p++;

                // If the tag starts with '!', see if it's a comment, otherwise use length until whitespace
                if (tag_start + 3 <= end && !strncmp(tag_start, "!--", 3)) p = tag_start + 3;
                size_t len = p - tag_start;

                // Look up tag name
                DesiredAction act = LookupPossibleTag(tag_start, tag_start + len);

                if (act != DesiredAction::OrdinaryText) {
                    // If the tag was immediately after a word, add it to our list
                    const char *word_end = is_closing ? tag_start - 2 : tag_start - 1;
                    if (word_end > word_start && word_end >= buffer) {
                        size_t word_len = word_end - word_start;
                        if (in_a_) {
                            links.push_back(word_start, word_len, SPACE_DELIM);
                            links.case_convert(links.size() - (word_len + 1), links.size());
                        }

                        words.push_back(word_start, word_len);
                        if (in_title_ && done_with_title_) {
                            title = string::join(" ", title, string(word_start, word_len));
                        }
                        num_words++;
                    }
                }

                switch (act) {
                case DesiredAction::OrdinaryText:
                    if (p < end && is_word_break_char(*p)) {
                        size_t word_len = p - word_start;

                        if (in_a_) {
                            links.push_back(word_start, word_len, SPACE_DELIM);

                            // Convert just the anchor text, not the URL (since URLs case sensitive)
                            links.case_convert(links.size() - (word_len + 1), links.size());
                        }

                        words.push_back(word_start, word_len);
                        if (in_title_ && done_with_title_) {
                            title = string::join(" ", title, string(word_start, word_len));
                        }
                        num_words++;
                        word_start = ++p;
                    } else if (p < end)
                        p++;
                    break;
                case DesiredAction::Title:
                    // Toggle title state accordingly
                    in_title_ = !is_closing;
                    if (in_title_) {
                        words.push_back("<title>", 7);
                    } else {
                        words.push_back("</title>", 8);
                        urlStore->updateTitleLen(string(url.data(), url.size()), num_words);
                        urlStore->updateTitle(string(url.data(), url.size()), title);
                        done_with_title_ = true;
                    }

                    while (p < end && *p != '>') p++;
                    if (p < end) word_start = ++p;
                    break;
                case DesiredAction::Comment:
                    // Scan for closing -->
                    while (p + 2 < end && !(*p == '-' && *(p + 1) == '-' && *(p + 2) == '>')) p++;
                    if (p + 2 >= end) {
                        // Comment spans into next chunk
                        in_comment_ = true;
                        return;
                    }
                    p += 3;
                    word_start = p;
                    break;
                case DesiredAction::Discard:
                    // Skip the tag and advance pointer
                    while (p < end && *p != '>') p++;
                    if (p < end) word_start = ++p;
                    break;
                case DesiredAction::DiscardSection:
                    // Advance until we hit a closing tag or the end, ignoring everything in between
                    // Loop guard checks whether it was an unmatched closing tag, in which case we just skip
                    while (!is_closing) {
                        while (p + 1 < end && !(*p == '<' && *(p + 1) == '/')) p++;
                        if (p + 1 >= end) {
                            // Discard section spans into next chunk
                            in_discard_ = true;
                            discard_tag_len_ = (len < sizeof(discard_tag_)) ? len : sizeof(discard_tag_) - 1;
                            memcpy(discard_tag_, tag_start, discard_tag_len_);
                            return;
                        }

                        // Once we've found closing tag, see if it matches
                        p += 2;
                        const char *close_start = p;
                        while (p < end && !(isspace(*p) || *p == '>')) p++;

                        // If closing tag matches the opening tag, we've exited the discard region
                        if (!strncasecmp(close_start, tag_start, len)) break;

                        // If closing tag doesn't match, keep looking for a closing tag
                    }

                    if (!in_discard_) {
                        while (p < end && *p != '>') p++;
                        if (p < end) word_start = ++p;
                    }
                    break;
                case DesiredAction::Anchor:
                    if (is_closing) {
                        in_a_ = false;
                        // Write just a newline delimiter to mark end of anchor text
                        links.push_back("", 0);
                        while (p < end && *p != '>') p++;
                    } else {
                        while (p < end && *p != '>') {
                            if (!strncasecmp(p, "href=\"", 6) && isspace(*(p - 1))) {
                                const char *a_start = (p += 6);
                                while (p < end && *p != '"') p++;

                                if (p != end && p > a_start) {
                                    if (*a_start == '/') {
                                        // Relative link -- prepend scheme+host only
                                        const char *s = url.data();
                                        const char *s_end = s + url.size();
                                        while (s < s_end && *s != ':') s++;
                                        if (s + 2 < s_end) s += 3;   // skip "://"
                                        while (s < s_end && *s != '/') s++;
                                        size_t host_len = s - url.data();

                                        size_t full_size = host_len + (p - a_start);
                                        char full_link[full_size];
                                        memcpy(full_link, url.data(), host_len);
                                        memcpy(full_link + host_len, a_start, p - a_start);
                                        links.push_back(full_link, full_size);
                                    } else {
                                        links.push_back(a_start, p - a_start);
                                    }
                                    in_a_ = true;
                                }
                            } else
                                p++;
                        }
                    }

                    if (p < end) word_start = ++p;
                    break;
                case DesiredAction::Base:
                    // If already found base link, discard this section
                    if (base_found_) {
                        while (p < end && *p != '>') p++;
                    } else {
                        while (p < end && *p != '>') {
                            // Search for href and capture the link
                            if (!strncasecmp(p, "href=\"", 6)) {
                                const char *base_start = (p += 6);
                                while (p < end && *p != '"') p++;

                                size_t len = p - base_start;
                                if (len < MAX_BASE_LEN) {
                                    memcpy(base, base_start, len);
                                    base[len] = '\0';
                                    base_len = len;
                                    base_found_ = true;
                                }
                            } else
                                p++;
                        }
                    }

                    if (p < end) word_start = ++p;
                    break;
                case DesiredAction::Embed:
                    while (p < end && *p != '>') {
                        // Search for src and capture link
                        if (!strncasecmp(p, "src=\"", 5)) {
                            const char *embed_start = (p += 5);
                            while (p < end && *p != '"') p++;

                            links.push_back(embed_start, p - embed_start);

                        } else
                            p++;
                    }

                    if (p < end) word_start = ++p;
                    break;
                }

            }

            // Handle HTML entities like &nbsp; &amp;, treat as word boundary, or discard
            else if (*p == '&') {
                // Check if it's an apostrophe, in which case we don't want to treat as space
                const char *entity_start = p + 1;
                const char *entity_end = entity_start;
                while (entity_end < end && *entity_end != ';' && *entity_end != '<' && !isspace(*entity_end))
                    entity_end++;
                size_t elen = entity_end - entity_start;
                bool is_apostrophe = (elen == 5 && !strncmp(entity_start, "rsquo", 5))
                                  || (elen == 4 && !strncmp(entity_start, "apos", 4));

                if (is_apostrophe) {
                    // Push back without adding a delimiter, so we get "cant", not "can t"
                    if (p > word_start) {
                        words.push_back(word_start, p - word_start, NULL_DELIM);
                        if (in_a_) {
                            links.push_back(word_start, p - word_start, NULL_DELIM);
                            links.case_convert(links.size() - ((p - word_start) + 1), links.size());
                        }
                    }
                } else {
                    // Treat as word boundary, i.e. space
                    if (p > word_start) {
                        size_t word_len = p - word_start;
                        if (in_a_) {
                            links.push_back(word_start, word_len, SPACE_DELIM);
                            links.case_convert(links.size() - (word_len + 1), links.size());
                        }
                        words.push_back(word_start, word_len);
                        if (in_title_ && done_with_title_) {
                            title = string::join(" ", title, string(word_start, word_len));
                        }
                        num_words++;
                    }
                }
                // Skip past the ';' or until space/tag
                p = (entity_end < end && *entity_end == ';') ? entity_end + 1 : entity_end;
                word_start = p;
            }

            // cover "\'"Special encoded apostrophe with UTF encoding 0xE28099
            else if (*p == '\''
                     || ((unsigned char) *p == 0xE2 && p + 2 < end && (unsigned char) *(p + 1) == 0x80
                         && (unsigned char) *(p + 2) == 0x99)) {
                if (p > word_start) {
                    words.push_back(word_start, p - word_start, NULL_DELIM);
                    if (in_a_) {
                        links.push_back(word_start, p - word_start, NULL_DELIM);
                        links.case_convert(links.size() - ((p - word_start) + 1), links.size());
                    }
                }
                int skip = (*p == '\'') ? 1 : 3;
                p += skip;
                word_start = p;
            }

            // UTF-8 non-breaking space 0xC2A0
            else if ((unsigned char) *p == 0xC2 && p + 1 < end && (unsigned char) *(p + 1) == 0xA0) {
                if (p > word_start) {
                    words.push_back(word_start, p - word_start);
                    if (in_a_) {
                        links.push_back(word_start, p - word_start, SPACE_DELIM);
                        links.case_convert(links.size() - ((p - word_start) + 1), links.size());
                    }
                    if (in_title_ && done_with_title_) {
                        title = string::join(" ", title, string(word_start, p - word_start));
                    }
                    num_words++;
                }
                p += 2;
                word_start = p;
            }

            // Non-alnum character: push partial word, skip char, track consecutive count
            else if (!isalnum((unsigned char) *p)) {
                non_alnum_run_++;
                if (non_alnum_run_ >= MAX_CONSECUTIVE_NON_ALNUM) {
                    logger::warn("parse_chunk: killed parser %zu — hit %d consecutive non-alnum chars", parser_id_,
                                 non_alnum_run_);
                    killed_ = true;
                    return;
                }
                if (p > word_start) {
                    words.push_back(word_start, p - word_start, NULL_DELIM);
                }
                word_start = ++p;
            }

            // Alphanumeric character: reset non-alnum counter, just increment p
            else {
                non_alnum_run_ = 0;
                p++;
            }
        }
    }
};
