#pragma once

#include "index-chunk/isr.h"
#include "index/Index.h"
#include "lib/algorithm.h"
#include "lib/string.h"
#include "lib/vector.h"


class chunk_manager {
public:
    explicit chunk_manager(const string& path) : li(path) {}

    chunk_manager(const chunk_manager&)            = delete;
    chunk_manager& operator=(const chunk_manager&) = delete;

    // Return all doc ids in this chunk where every query word appears.
    // Uses a leapfrog join driven by the rarest word.
    vector<uint32_t> get_docIDs(const vector<string>& words) {
        vector<uint32_t> doc_ids;
        if (words.size() == 0) return doc_ids;

        // One ISR per query word against this chunk.
        vector<IndexStreamReader> isrs;
        isrs.reserve(words.size());
        for (size_t i = 0; i < words.size(); ++i) {
            isrs.push_back(IndexStreamReader(words[i], &li));
        }

        // If any word has no posts in this chunk, intersection is empty.
        for (size_t i = 0; i < isrs.size(); ++i) {
            if (isrs[i].n_posts == 0) return doc_ids;
        }

        // Sort ascending by n_docs so isrs[0] is the rarest word (driver).
        // Bounds total leapfrog iterations by min(n_docs) across the query.
        sort(isrs, compare_by_n_docs);

        // Seed leapfrog with driver's first post. {0, 0} means exhausted.
        post p = isrs[0].advance();
        if (p.doc == 0) return doc_ids;
        uint32_t target = p.doc;

        while (true) {
            bool all_match = true;

            // Try to land every non-driver on target.
            for (size_t i = 1; i < isrs.size(); ++i) {
                post q = isrs[i].advance_to(target);
                if (q.doc == 0) return doc_ids;     // exhausted → done
                if (q.doc > target) {
                    target = q.doc;                  // overshoot → new target
                    all_match = false;
                    break;
                }
            }

            if (!all_match) {
                // A non-driver overshot. Drive the driver up to match.
                post q = isrs[0].advance_to(target);
                if (q.doc == 0) return doc_ids;
                if (q.doc > target) target = q.doc;  // driver overshot too — re-leapfrog
                continue;
            }

            // All ISRs agree — emit and advance driver past this doc.
            doc_ids.push_back(target);
            p = isrs[0].advance_to(target + 1);
            if (p.doc == 0) return doc_ids;
            target = p.doc;
        }
    }


private:
    static bool compare_by_n_docs(const IndexStreamReader& a, const IndexStreamReader& b) {
        return a.n_docs < b.n_docs;
    }

    LoadedIndex li;
};
