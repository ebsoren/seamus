#pragma once

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
        : word_(w.data(), w.size()), is_negated_(is_negated),
          isr_(string(w.data(), w.size()), li) {}

    const string &word() const { return word_; }
    bool is_negated() const { return is_negated_; }

    uint32_t estimated_n_docs() const override { return isr_.n_docs; }
    bool is_driveable() const override { return !is_negated_; }

    // Advance to the next doc containing this word. On a fresh ISR
    // (doc_offset_==0), advance_to_next_doc calls advance_to(1) which
    // reads the first post. Returns 0 when exhausted.
    uint32_t next_doc() override {
        post p = isr_.advance_to_next_doc();
        return p.doc;
    }

    // Seek to `doc` and check presence. For negated terms, invert:
    // the word being absent means the negation is satisfied.
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


// Phrase leaf — Phase 4 will hold per-word ISRs and check adjacency.
// Stubbed for now: next_doc returns 0, probe returns false.
class PhraseISR : public QueryISR {
public:
    PhraseISR(const string_view &phrase, bool is_negated)
        : phrase_(phrase.data(), phrase.size()), is_negated_(is_negated) {}

    const string &phrase() const { return phrase_; }
    bool is_negated() const { return is_negated_; }

    uint32_t estimated_n_docs() const override { return 0; }
    bool is_driveable() const override { return !is_negated_; }

    uint32_t next_doc() override { return 0; }
    bool probe(uint32_t) override { return false; }

private:
    string phrase_;
    bool is_negated_;
};


// AND over children. Step 3 implements real leapfrog execution.
class AndOp : public QueryISR {
public:
    void add_child(QueryISR *c) { children_.push_back(c); }
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

    uint32_t next_doc() override { return 0; }
    bool probe(uint32_t) override { return false; }

private:
    vector<QueryISR *> children_;
};


// OR over children. Step 4 implements real union execution.
class OrOp : public QueryISR {
public:
    void add_child(QueryISR *c) { children_.push_back(c); }
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

    uint32_t next_doc() override { return 0; }
    bool probe(uint32_t) override { return false; }

private:
    vector<QueryISR *> children_;
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

    PhraseISR *make_phrase(const string_view &p, bool is_negated) {
        PhraseISR *ph = new PhraseISR(p, is_negated);
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
            return arena.make_phrase(ast.term.val, ast.term.is_negated);
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
