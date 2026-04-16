#pragma once

#include <cstdint>
#include <cstring>

#include "lib/string.h"
#include "lib/vector.h"
#include "query/expressions.h"


// QueryISR: abstract base for the per-chunk execution tree. The tree
// mirrors the shape of the parsed query AST — leaves are TermISR /
// PhraseISR, interior nodes are AndOp / OrOp. Every node conceptually
// supports:
//   - probe(doc): is this specific doc in the node's match set?
//     Always well-defined, including for negated leaves and ops with
//     negated descendants.
//   - next_doc(): advance to the next doc in the match set. Only valid
//     if is_driveable() returns true; negated leaves are never driveable
//     because their match set is unbounded.
//
// Driveability recursion:
//   - TermISR / PhraseISR: driveable iff !is_negated
//   - AndOp: driveable iff at least one child is driveable (that child
//     is the leapfrog driver, others are probed via contains)
//   - OrOp: driveable iff every child is driveable (otherwise the union
//     would omit whole slabs of the non-driveable branch's match set)
//
// The recursive rule exactly reproduces the DNF "every path must
// contain at least one positive term" invariant, without requiring a
// DNF flattening pass. See the discussion in query/expressions.h.
//
// Phase 1 status: the base class and concrete types below are stubs.
// They track enough state to validate the AST -> tree conversion, the
// dedupe arena, and the driveability recursion, but they do not yet
// walk posting lists. chunk_manager::query builds the tree for
// validation then delegates execution to default_query over the flat
// positive term list. Real iteration lands in Phase 2+.
class QueryISR {
public:
    virtual ~QueryISR() = default;

    // Rough abstract estimate for doc count
    virtual uint32_t estimated_n_docs() const = 0;

    // Can next_doc() be called on this node? See comment above.
    virtual bool is_driveable() const = 0;
};


// Leaf: a single dictionary word. Carries its own is_negated flag.
// Negation is handled locally
class TermISR : public QueryISR {
public:
    TermISR(const string_view &w, bool is_negated)
        : word_(w.data(), w.size()), is_negated_(is_negated) {}

    const string &word() const { return word_; }
    bool is_negated() const { return is_negated_; }

    uint32_t estimated_n_docs() const override { return 0; }  
    bool is_driveable() const override { return !is_negated_; }

private:
    string word_;
    bool is_negated_;
};


// Phrase-based ISR. TODO
class PhraseISR : public QueryISR {
public:
    PhraseISR(const string_view &phrase, bool is_negated)
        : phrase_(phrase.data(), phrase.size()), is_negated_(is_negated) {}

    const string &phrase() const { return phrase_; }
    bool is_negated() const { return is_negated_; }

    uint32_t estimated_n_docs() const override { return 0; }  
    bool is_driveable() const override { return !is_negated_; }

private:
    string phrase_;
    bool is_negated_;
};


// AND over children
class AndOp : public QueryISR {
public:
    void add_child(QueryISR *c) { children_.push_back(c); }
    const vector<QueryISR *> &children() const { return children_; }

    // At least one child must be driveable — that child is the driver,
    // the rest are probed for containment.
    bool is_driveable() const override {
        for (size_t i = 0; i < children_.size(); ++i) {
            if (children_[i]->is_driveable()) return true;
        }
        return false;
    }

    // Rarest driveable child wins driver selection, so the min estimate
    // is what a parent AndOp cares about. Skip non-drivables (negations)
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

private:
    vector<QueryISR *> children_;
};


// OR over children
class OrOp : public QueryISR {
public:
    void add_child(QueryISR *c) { children_.push_back(c); }
    const vector<QueryISR *> &children() const { return children_; }

    // Every child must be driveable: if any child's match set is
    // unbounded (a negated leaf), the union is unbounded and we can't
    // iterate it from a finite starting point.
    // Ex: Can't drive on "x OR (NOT y)"
    bool is_driveable() const override {
        if (children_.size() == 0) return false;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (!children_[i]->is_driveable()) return false;
        }
        return true;
    }

    // Upper bound = sum of children.
    uint32_t estimated_n_docs() const override {
        uint64_t sum = 0;
        for (size_t i = 0; i < children_.size(); ++i) {
            sum += children_[i]->estimated_n_docs();
            if (sum >= UINT32_MAX) return UINT32_MAX;
        }
        return static_cast<uint32_t>(sum);
    }

private:
    vector<QueryISR *> children_;
};


// Owns every QueryISR built for one query on one chunk. 
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

    TermISR *dedup_term(const string_view &w, bool is_negated) {
        for (size_t i = 0; i < terms_.size(); ++i) {
            TermISR *t = terms_[i];
            if (t->is_negated() == is_negated && t->word().size() == w.size()
                && memcmp(t->word().data(), w.data(), w.size()) == 0) {
                return t;
            }
        }
        TermISR *t = new TermISR(w, is_negated);
        terms_.push_back(t);
        return t;
    }

    PhraseISR *dedup_phrase(const string_view &p, bool is_negated) {
        for (size_t i = 0; i < phrases_.size(); ++i) {
            PhraseISR *ph = phrases_[i];
            if (ph->is_negated() == is_negated && ph->phrase().size() == p.size()
                && memcmp(ph->phrase().data(), p.data(), p.size()) == 0) {
                return ph;
            }
        }
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


// Recursively convert a parsed AST into a QueryISR tree stored in `arena`.
inline QueryISR *build_isr_tree(const ASTNode &ast, ISRArena &arena) {
    if (ast.type == AST_TERM) {
        if (ast.term.is_phrase) {
            return arena.dedup_phrase(ast.term.val, ast.term.is_negated);
        }
        return arena.dedup_term(ast.term.val, ast.term.is_negated);
    }
    if (ast.type == AST_AND) {
        AndOp *op = arena.make_and();
        for (size_t i = 0; i < ast.children.size(); ++i) {
            op->add_child(build_isr_tree(ast.children[i], arena));
        }
        return op;
    }
    if (ast.type == AST_OR) {
        OrOp *op = arena.make_or();
        for (size_t i = 0; i < ast.children.size(); ++i) {
            op->add_child(build_isr_tree(ast.children[i], arena));
        }
        return op;
    }
    return nullptr;
}
