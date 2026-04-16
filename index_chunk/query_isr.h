#pragma once

#include <climits>
#include <cstdint>
#include <cstring>

#include "isr.h"
#include "lib/string.h"
#include "lib/vector.h"
#include "query/expressions.h"


// QueryISR: abstract base for the per-chunk execution tree. The tree
// mirrors the shape of the parsed query AST — leaves are TermISR /
// PhraseISR, interior nodes are AndOp / OrOp.
//
// Every node supports two operations:
//   - next_doc(): advance to the next doc in the match set and return
//     its id (0 = exhausted). Only valid if is_driveable() is true.
//   - probe(doc): is this specific doc in the match set? Always valid
//     on any node, including negated leaves. May advance internal
//     cursors; callers must probe in monotonically increasing order.
//
// Driveability recursion:
//   - TermISR / PhraseISR: driveable iff !is_negated
//   - AndOp: driveable iff at least one child is driveable
//   - OrOp: driveable iff every child is driveable
class QueryISR {
public:
    virtual ~QueryISR() = default;

    virtual uint32_t estimated_n_docs() const = 0;
    virtual bool is_driveable() const = 0;

    virtual uint32_t next_doc() = 0;
    virtual bool probe(uint32_t doc) = 0;
};


// Leaf: a single dictionary word backed by a real IndexStreamReader.
// Each tree position gets its own TermISR and its own ISR cursor —
// no sharing, because different tree branches advance independently.
class TermISR : public QueryISR {
public:
    TermISR(const string_view &w, bool is_negated, LoadedIndex *li)
        : word_(w.data(), w.size())
        , is_negated_(is_negated)
        , isr_(string(w.data(), w.size()), li) {}

    const string &word() const { return word_; }
    bool is_negated() const { return is_negated_; }

    uint32_t estimated_n_docs() const override { return isr_.n_docs; }
    bool is_driveable() const override { return !is_negated_; }

    uint32_t next_doc() override {
        post p = isr_.advance_to_next_doc();
        return p.doc;
    }

    bool probe(uint32_t doc) override {
        post p = isr_.advance_to(doc);
        bool present = (p.doc == doc);
        return is_negated_ ? !present : present;
    }

private:
    string word_;
    bool is_negated_;
    IndexStreamReader isr_;
};


// Phrase leaf: holds per-word IndexStreamReaders internally and
// checks position adjacency. A phrase "w0 w1 w2" matches a doc iff
// there exists a position P where w0 is at P, w1 at P+1, w2 at P+2.
// Anchors on stream 0 (first word) and walks positions to verify.
class PhraseISR : public QueryISR {
public:
    PhraseISR(const string_view &phrase, bool is_negated, LoadedIndex *li)
        : is_negated_(is_negated) {
        // Split phrase on spaces into individual word streams.
        size_t i = 0;
        while (i < phrase.size()) {
            while (i < phrase.size() && phrase[i] == ' ') i++;
            if (i >= phrase.size()) break;
            size_t start = i;
            while (i < phrase.size() && phrase[i] != ' ') i++;
            string w(phrase.data() + start, i - start);
            streams_.push_back(IndexStreamReader(w, li));
            words_.push_back(string(w.data(), w.size()));
        }
    }

    bool is_negated() const { return is_negated_; }

    const vector<string> &constituent_words() const { return words_; }

    uint32_t estimated_n_docs() const override {
        if (streams_.size() == 0) return 0;
        uint32_t min_docs = streams_[0].n_docs;
        for (size_t i = 1; i < streams_.size(); ++i) {
            if (streams_[i].n_docs < min_docs) min_docs = streams_[i].n_docs;
        }
        return min_docs;
    }

    bool is_driveable() const override { return !is_negated_; }

    uint32_t next_doc() override {
        if (streams_.size() == 0) return 0;

        post ground = streams_[0].advance_to_next_doc();
        while (ground.doc != 0) {
            if (streams_.size() == 1) return ground.doc;
            if (check_adjacency(ground)) return ground.doc;
            // Try next position of stream 0 — might be same doc or next.
            ground = streams_[0].advance();
        }
        return 0;
    }

    bool probe(uint32_t doc) override {
        if (streams_.size() == 0) return is_negated_;

        post ground = streams_[0].advance_to(doc);
        if (ground.doc != doc) return is_negated_;
        if (streams_.size() == 1) return !is_negated_;

        while (ground.doc == doc) {
            if (check_adjacency(ground)) return !is_negated_;
            ground = streams_[0].advance();
            if (ground.doc == 0) break;
        }
        return is_negated_;
    }

private:
    // Given stream 0 is at `ground`, check that streams 1..n-1 each
    // have a post at (ground.doc, ground.loc + i). Same algorithm as
    // chunk_manager::phrase_query's validate_pos.
    bool check_adjacency(post ground) {
        for (size_t i = 1; i < streams_.size(); ++i) {
            uint32_t want_loc = ground.loc + static_cast<uint32_t>(i);
            post p = streams_[i].advance_to(ground.doc);
            if (p.doc != ground.doc) return false;
            while (p.doc == ground.doc && p.loc < want_loc) {
                p = streams_[i].advance();
                if (p.doc == 0) return false;
            }
            if (p.doc != ground.doc || p.loc != want_loc) return false;
        }
        return true;
    }

    bool is_negated_;
    vector<IndexStreamReader> streams_;
    vector<string> words_;
};


// AND over children. Picks the rarest driveable child as driver
// during add_child, then leapfrogs: driver advances, others are probed.
class AndOp : public QueryISR {
public:
    void add_child(QueryISR *c) {
        children_.push_back(c);
        uint32_t s = c->estimated_n_docs();
        if (c->is_driveable() && (!driver_ || s < driver_score_)) {
            driver_ = c;
            driver_score_ = s;
        }
    }
    const vector<QueryISR *> &children() const { return children_; }

    bool is_driveable() const override {
        for (size_t i = 0; i < children_.size(); ++i) {
            if (children_[i]->is_driveable()) return true;
        }
        return false;
    }

    uint32_t estimated_n_docs() const override {
        uint32_t best = 0;
        bool any = false;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (!children_[i]->is_driveable()) continue;
            uint32_t n = children_[i]->estimated_n_docs();
            if (!any || n < best) {
                best = n;
                any = true;
            }
        }
        return any ? best : 0;
    }

    uint32_t next_doc() override {
        if (!driver_) return 0;
        while (true) {
            uint32_t candidate = driver_->next_doc();
            if (candidate == 0) return 0;
            bool all_match = true;
            for (size_t i = 0; i < children_.size(); ++i) {
                if (children_[i] == driver_) continue;
                if (!children_[i]->probe(candidate)) {
                    all_match = false;
                    break;
                }
            }
            if (all_match) return candidate;
        }
    }

    bool probe(uint32_t doc) override {
        for (size_t i = 0; i < children_.size(); ++i) {
            if (!children_[i]->probe(doc)) return false;
        }
        return true;
    }

private:
    vector<QueryISR *> children_;
    QueryISR *driver_ = nullptr;
    uint32_t driver_score_ = UINT32_MAX;
};


// OR over children. Tracks each child's current doc; next_doc returns
// the global minimum and advances only the children that were at it.
class OrOp : public QueryISR {
public:
    void add_child(QueryISR *c) {
        children_.push_back(c);
        // Prime the child's first doc so current_docs_ is ready.
        current_docs_.push_back(c->next_doc());
    }
    const vector<QueryISR *> &children() const { return children_; }

    bool is_driveable() const override {
        if (children_.size() == 0) return false;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (!children_[i]->is_driveable()) return false;
        }
        return true;
    }

    uint32_t estimated_n_docs() const override {
        uint64_t sum = 0;
        for (size_t i = 0; i < children_.size(); ++i) {
            sum += children_[i]->estimated_n_docs();
            if (sum >= UINT32_MAX) return UINT32_MAX;
        }
        return static_cast<uint32_t>(sum);
    }

    uint32_t next_doc() override {
        // Lazy advance: if a previous next_doc() returned a doc, advance
        // the children that contributed to it now (deferred so callers
        // can collect positions from those children first).
        if (pending_advance_ != 0) {
            for (size_t i = 0; i < current_docs_.size(); ++i) {
                if (current_docs_[i] == pending_advance_) {
                    current_docs_[i] = children_[i]->next_doc();
                }
            }
            pending_advance_ = 0;
        }

        // Find the minimum current doc across all children.
        uint32_t min_doc = 0;
        for (size_t i = 0; i < current_docs_.size(); ++i) {
            uint32_t d = current_docs_[i];
            if (d == 0) continue;
            if (min_doc == 0 || d < min_doc) min_doc = d;
        }
        if (min_doc == 0) return 0;

        pending_advance_ = min_doc;
        return min_doc;
    }

    bool probe(uint32_t doc) override {
        for (size_t i = 0; i < children_.size(); ++i) {
            if (children_[i]->probe(doc)) return true;
        }
        return false;
    }

private:
    vector<QueryISR *> children_;
    vector<uint32_t> current_docs_;
    uint32_t pending_advance_ = 0;
};


// Owns every QueryISR built for one query on one chunk. The arena
// lives on the stack of chunk_manager::query; when it destructs,
// every node is freed.
//
// No dedup: each tree position gets its own TermISR with its own
// IndexStreamReader cursor. Shared cursors would conflict when
// different branches of the tree (e.g. both sides of an OR) try
// to walk the same posting list independently.
class ISRArena {
public:
    ISRArena() = default;
    ~ISRArena() {
        for (size_t i = 0; i < terms_.size(); ++i) delete terms_[i];
        for (size_t i = 0; i < phrases_.size(); ++i) delete phrases_[i];
        for (size_t i = 0; i < ops_.size(); ++i) delete ops_[i];
    }

    ISRArena(const ISRArena &) = delete;
    ISRArena &operator=(const ISRArena &) = delete;

    TermISR *make_term(const string_view &w, bool is_negated, LoadedIndex *li) {
        TermISR *t = new TermISR(w, is_negated, li);
        terms_.push_back(t);
        return t;
    }

    PhraseISR *make_phrase(const string_view &p, bool is_negated, LoadedIndex *li) {
        PhraseISR *ph = new PhraseISR(p, is_negated, li);
        phrases_.push_back(ph);
        return ph;
    }

    AndOp *make_and() {
        AndOp *op = new AndOp();
        ops_.push_back(op);
        return op;
    }

    OrOp *make_or() {
        OrOp *op = new OrOp();
        ops_.push_back(op);
        return op;
    }

    const vector<TermISR *> &terms() const { return terms_; }
    const vector<PhraseISR *> &phrases() const { return phrases_; }

private:
    vector<TermISR *> terms_;
    vector<PhraseISR *> phrases_;
    vector<QueryISR *> ops_;
};


// Recursively convert a parsed AST into a QueryISR tree. Each leaf
// gets its own IndexStreamReader bound to `li`. The arena owns
// everything; the returned pointer is the root.
inline QueryISR *build_isr_tree(const ASTNode &ast, ISRArena &arena, LoadedIndex *li) {
    if (ast.type == AST_TERM) {
        if (ast.term.is_phrase) {
            return arena.make_phrase(ast.term.val, ast.term.is_negated, li);
        }
        return arena.make_term(ast.term.val, ast.term.is_negated, li);
    }
    if (ast.type == AST_AND) {
        AndOp *op = arena.make_and();
        for (size_t i = 0; i < ast.children.size(); ++i) {
            op->add_child(build_isr_tree(ast.children[i], arena, li));
        }
        return op;
    }
    if (ast.type == AST_OR) {
        OrOp *op = arena.make_or();
        for (size_t i = 0; i < ast.children.size(); ++i) {
            op->add_child(build_isr_tree(ast.children[i], arena, li));
        }
        return op;
    }
    return nullptr;
}
