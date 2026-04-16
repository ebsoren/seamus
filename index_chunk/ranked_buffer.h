#pragma once

#include "../lib/atomic_vector.h"
#include "../lib/chunk_manager_query.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/vector.h"
#include "../ranker/Ranker.h"
#include "../url_store/url_store.h"


// ranked buffer impl
class RankedBuffer {
public:
    static constexpr size_t CAPACITY = 10;

    RankedBuffer(Ranker *ranker, UrlStore *url_store)
        : ranker_(ranker), url_store_(url_store) {
        pages_.reserve(CAPACITY);
    }

    // Score `doc`, build a LeanPage, and place it in the buffer in
    // descending-by-score order. When full, evicts the worst page
    // (lowest score, at the end of the vector) — but only if `doc`
    // beats it. Returns true if the page was inserted, false if it
    // was rejected (buffer full and new score no better than worst).
    // INVARIANT: MUST HAVE URLSTORE SET
    bool insert(DocInfo doc) {
        double score = ranker_->getScore(doc);

        if (pages_.size() == CAPACITY && score <= pages_[CAPACITY - 1].score) {
            return false;
        }

        // Pull the title out of the url store. If the url isn't
        // registered, fall back to an empty title rather than
        // dropping the doc entirely.
        UrlData *data = url_store_->getUrl(doc.url);
        string title = (data != nullptr)
            ? string(data->title.data(), data->title.size())
            : string("");

        LeanPage page;
        page.url = move(doc.url);
        page.title = move(title);
        page.score = score;

        if (pages_.size() == CAPACITY) pages_.pop_back();

        // Append and bubble up until the new page sits in sorted position.
        pages_.push_back(move(page));
        for (size_t i = pages_.size() - 1; i > 0; --i) {
            if (pages_[i - 1].score >= pages_[i].score) break;
            LeanPage tmp = move(pages_[i - 1]);
            pages_[i - 1] = move(pages_[i]);
            pages_[i] = move(tmp);
        }
        return true;
    }

    void set_url_store(UrlStore* us) { url_store_ = us; }

    // Move the sorted pages out of the buffer. Leaves the buffer empty.
    vector<LeanPage> take() { return move(pages_); }

    // Read-only traversal of the buffer in descending-score order.
    using const_iterator = const LeanPage *;
    const_iterator begin() const { return pages_.begin(); }
    const_iterator end() const { return pages_.end(); }
    size_t size() const { return pages_.size(); }

private:
    Ranker *ranker_;
    UrlStore *url_store_;
    vector<LeanPage> pages_;
};


// utils

// Drain every DocInfo out of `data` and insert it into `buffer`.
// `buffer` keeps only the top RankedBuffer::CAPACITY by score.
inline void drain_into(atomic_vector<DocInfo> *data, RankedBuffer *buffer) {
    vector<DocInfo> docs = data->take();
    for (size_t i = 0; i < docs.size(); ++i) {
        buffer->insert(move(docs[i]));
    }
}


// Simple k-way merge of already-sorted RankedBuffers. Each buffer holds
// pages in descending-score order, so we keep one cursor per buffer and
// at each step pick the cursor whose current head has the highest score,
// move that page into the result, and advance. Stops when the result
// hits `cap` pages or every cursor has exhausted its buffer. Drains the
// buffers via take() — they are empty on return.
inline vector<LeanPage> merge_top_pages(const vector<RankedBuffer *> &buffers, size_t cap) {
    size_t k = buffers.size();

    vector<vector<LeanPage>> sources;
    sources.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        sources.push_back(buffers[i]->take());
    }

    vector<size_t> cursor;
    cursor.reserve(k);
    for (size_t i = 0; i < k; ++i) cursor.push_back(0);

    vector<LeanPage> result;
    result.reserve(cap);

    while (result.size() < cap) {
        size_t best = k;  // sentinel: no source has anything left
        for (size_t i = 0; i < k; ++i) {
            if (cursor[i] >= sources[i].size()) continue;
            if (best == k || sources[i][cursor[i]].score > sources[best][cursor[best]].score) {
                best = i;
            }
        }
        if (best == k) break;
        result.push_back(move(sources[best][cursor[best]]));
        ++cursor[best];
    }

    return result;
}

