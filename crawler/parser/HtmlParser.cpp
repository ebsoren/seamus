// HtmlParser.cpp
// Nicole Hamlton, nham@umich.edu

// If you don't define the HtmlParser class methods inline in
// HtmlParser.h, you may do it here.

#include <cstring>
#include "HtmlParser.h"

HtmlParser::HtmlParser(const char *buffer, size_t length) {
    const char* end = buffer + length;
    const char* p = buffer;
    const char* word_start = p;

    bool in_a = false, in_title = false, base_found = false;
    while (p < end) {
        if (isspace(*p)) {
            if (p > word_start) {
                std::string word(word_start, p - word_start);
                if (in_a) links.back().anchorText.push_back(word);

                if (in_title) titleWords.push_back(word);
                else words.push_back(word);
                word_start = ++p;
            } else {
                p++;
                word_start++;
            }
        }

        // Handle potential start of tag
        else if (*p == '<') {
            const char* tag_start = ++p;
            bool is_closing = false;
            if (*p == '/') {
                // Closing tag
                is_closing = true;
                tag_start = ++p;
            }

            // Continue until end of tag name
            while (p < end && !(isspace(*p) || *p == '/' || *p == '>')) p++;

            // If the tag starts with '!', see if it's a comment -- otherwise use length until whitespace
            if (!strncmp(tag_start, "!--", 3)) p = tag_start + 3;
            size_t len = p - tag_start;

            // Lookup tag name
            DesiredAction act = LookupPossibleTag(tag_start, tag_start + len);

            if (act != DesiredAction::OrdinaryText) {
                // If the tag was immediately after a word, add it to our list
                if ((!is_closing && word_start < tag_start - 1) || word_start < tag_start - 2) {
                    size_t word_len = is_closing ? (tag_start - 2) - word_start : (tag_start - 1) - word_start;
                    std::string word(word_start, word_len);
                    if (in_a) links.back().anchorText.push_back(word);

                    if (in_title) titleWords.push_back(word);
                    else words.push_back(word);
                }
            }

            switch (act) {
                case DesiredAction::OrdinaryText:
                    // Treat as normal text -- just advance pointer
                    if (isspace(*p)) {
                        std::string word(word_start, p - word_start);
                        if (in_a) links.back().anchorText.push_back(word);

                        if (in_title) titleWords.push_back(word);
                        else words.push_back(word);
                        word_start = ++p;
                    } else p++;
                    break;
                case DesiredAction::Title:
                    // Toggle title state accordingly
                    in_title = !is_closing;
                    while (p < end && *p != '>') p++;
                    word_start = ++p;
                    break;
                case DesiredAction::Comment:
                    // Advance until we hit a closing tag or the end, ignoring everything in between
                    while (true) {
                        while (p < end && !(*p == '-' && *(p + 1) == '-' && *(p + 2) == '>')) p++;

                        // Found closing tag, advance pointer past it
                        // (In theory this could advance the pointer past the end, but if so we'll exit the outer while, so nbd)
                        p += 3;
                        word_start = p;
                        break;
                    }
                    break;
                case DesiredAction::Discard:
                    // Skip the tag and advance pointer
                    while (p < end && *p != '>') p++;
                    word_start = ++p;
                    break;
                case DesiredAction::DiscardSection:
                    // Advance until we hit a closing tag or the end, ignoring everything in between
                    // Loop guard checks whether it was an unmatched closing tag, in which case we just skip
                    while (!is_closing) {
                        while (p < end && !(*p == '<' && *(p + 1) == '/')) p++;
                        if (p == end) break;

                        // Once we've found closing tag, see if it matches
                        p += 2;
                        const char* close_start = p;
                        while (p < end && !(isspace(*p) || *p == '>')) p++;

                        // If closing tag matches the opening tag, we've exited the discard region
                        if (!strncasecmp(close_start, tag_start, len)) break;

                        // If closing tag doesn't match, keep looking for a closing tag
                    }

                    word_start = ++p;
                    break;
                case DesiredAction::Anchor:
                    if (is_closing) {
                        in_a = false;
                        while (p < end && *p != '>') p++;
                    } else {
                        while (p < end && *p != '>') {
                            if (!strncasecmp(p, "href=\"", 6) && isspace(*(p - 1))) {
                                const char* a_start = (p += 6);
                                while (p < end && *p != '"') p++;

                                if (p != end && p > a_start) {
                                    links.push_back(Link(std::string(a_start, p - a_start)));
                                    in_a = true;
                                }
                            } else p++;
                        }
                    }

                    word_start = ++p;
                    break;
                case DesiredAction::Base:
                    // If already found base link, discard this section
                    if (base_found) {
                        while (p < end && *p != '>') p++;
                    } else {
                        while (p < end && *p != '>') {
                            // Search for href and capture the link
                            if (!strncasecmp(p, "href=\"", 6)) {
                                const char* base_start = (p += 6);
                                while (p < end && *p != '"') p++;

                                base = std::string(base_start, p - base_start);
                                base_found = true;
                            } else p++;
                        }
                    }

                    word_start = ++p;
                    break;
                case DesiredAction::Embed:
                    while (p < end && *p != '>') {
                        // Search for src and capture link
                        if (!strncasecmp(p, "src=\"", 5)) {
                            const char* embed_start = (p += 5);
                            while (p < end && *p != '"') p++;

                            links.push_back(std::string(embed_start, p - embed_start));
                        } else p++;
                    }

                    word_start = ++p;
                    break;
            }

        }

        // If normal character, just increment p
        else p++;
    }
}
