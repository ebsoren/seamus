#pragma once

#include "../lib/string.h"
#include "../lib/vector.h"
#include "../lib/algorithm.h"
#include <cassert>
#include <cstring>

// LOGIC FOR EXPRESSIONS IS AS FOLLOWS:
// 1) Words surrounded by "" are a phrase and must be found in that specific order
// 2) NOT means the word is NOT included in the query. There cannot ONLY be NOTs in a query
// 3) OR means either the lhs or rhs must be satisfied for there to be a match
// 4) Parenthesis are used to simplify the flow of logic
// 5) If there is a space between 2 words it is assumed there is an AND between them for logics sake

// HOW TO USE: 
// Call parse_query(const string& query) to get a vector<Clause> returned. This is a 2D vector 
// where each vector is a valid combination of Terms. A Term struct will tell you the token of the word
// and whether it should be included or NOT in the document we are searching

// TIME OPTIMIZATIONS:
// Each vector<Term> should be organized from least common Term to most common so search is able to 
// be done more efficiently. This should be kept in mind by the user


// TODO: SHOULD CALL INDEX TO GET RARITY HERE!
double getRarity(string_view token) {
    if(!token.contains(' ')) {
        return 1.0; // placeholder for index call
    }
    return 0.0; // we assign the lowest probability value to multi-word phrases
}

struct Term {
    string_view token;
    bool included;
    double rarity; 

    Term(string_view token_init, bool included_init) : 
        token(token_init), included(included_init), rarity(getRarity(token_init)) { }
};

inline bool termComp(const Term& a, const Term& b) {
    return a.rarity < b.rarity;
}

using Clause = vector<Term>;
using Result = vector<Clause>;

class QueryParser {
private:
    vector<string_view> tokens;
    size_t i = 0;
    string_view input;

public:
    explicit QueryParser(const string& s)
        : input(s.data(), s.size()) {
        tokenize();
    }

    Result parse() {
        return parseExpression();
    }

private:

    // Tokenize the input
    void tokenize() {
        size_t n = input.size();

        for (size_t j = 0; j < n;) {
            if (isspace(input[j])) { j++; continue; }

            if (input[j] == '(' || input[j] == ')') {
                tokens.emplace_back(input.data() + j, 1);
                j++;
                continue;
            }

            if (input[j] == '"') {
                size_t k = j + 1;
                while (k < n && input[k] != '"') k++;
                tokens.emplace_back(input.data() + j + 1, k - j - 1);
                j = k + 1;
                continue;
            }

            size_t k = j;
            while (k < n &&
                   !isspace(input[k]) &&
                   input[k] != '(' &&
                   input[k] != ')') {
                k++;
            }

            tokens.emplace_back(input.data() + j, k - j);
            j = k;
        }
    }

    // Parse the input according to the logic

    Result parseExpression() {
        Result left = parseTerm();

        while (i < tokens.size() && tokens[i] == "OR") {
            i++;
            Result right = parseTerm();

            left.reserve_exact(left.size() + right.size());

            for (size_t k = 0; k < right.size(); ++k) {
                left.emplace_back(std::move(right[k]));
            }
        }

        return left;
    }

    Result parseTerm() {
        Result result;
        result.emplace_back(); // start with one empty clause

        while (i < tokens.size() &&
               !(tokens[i] == ")") &&
               !(tokens[i] == "OR")) {

            Result next = parseFactor();
            result = combineAND(result, next);
        }

        return result;
    }

    Result parseFactor() {
        bool negate = false;

        if (tokens[i] == "NOT") {
            negate = true;
            i++;
        }

        Result res;

        if (tokens[i] == "(") {
            i++;
            Result inner = parseExpression();
            assert(tokens[i] == ")");
            i++;

            if (negate) {
                Clause merged;

                // precompute total size
                size_t total = 0;
                for (const auto& c : inner) total += c.size();

                merged.reserve_exact(total);

                size_t offset = 0;

                for (const auto& c : inner) {
                    memcpy(
                        merged.data() + offset,
                        c.data(),
                        c.size() * sizeof(Term)
                    );

                    // flip flags in-place
                    for (size_t j = 0; j < c.size(); ++j) {
                        merged.data()[offset + j].included =
                            !merged.data()[offset + j].included;
                    }

                    offset += c.size();
                }

                merged.unsafe_set_size(total);
                res.emplace_back(std::move(merged));
            } else {
                return inner;
            }
        }
        else {
            Clause clause;
            clause.reserve_exact(1);

            clause.emplace_back(Term(
                tokens[i],
                !negate
            ));

            i++;
            res.emplace_back(std::move(clause));
        }

        return res;
    }

    // AND logic

    static Result combineAND(const Result& a, const Result& b) {
        Result res;

        size_t total = a.size() * b.size();
        res.reserve_exact(total);

        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < b.size(); ++j) {

                const Clause& ca = a[i];
                const Clause& cb = b[j];

                size_t new_size = ca.size() + cb.size();

                Clause merged;
                merged.reserve_exact(new_size);

                // memcpy both blocks
                for (const auto& t : ca) {
                    merged.emplace_back(t);
                }

                for (const auto& t : cb) {
                    merged.emplace_back(t);
                }

                merged.unsafe_set_size(new_size);

                res.emplace_back(std::move(merged));
            }
        }

        return res;
    }
};


// doesn't allow clauses with only NOTs
inline void filter_query(Result& res) {
    Result filtered;

    for (const auto& clause : res) {
        bool has_positive = false;

        for (const auto& term : clause) {
            if (term.included) {
                has_positive = true;
                break;
            }
        }

        if (has_positive) {
            filtered.push_back(clause);
        }
    }

    res = std::move(filtered);
}

// USER INTERFACE RIGHT HERE!
inline Result parse_query(const string& query) {
    QueryParser p(query);
    Result res = p.parse();

    // remove clauses that are ONLY NOT terms using a filter function
    filter_query(res);

    // if nothing remains → error
    if (res.empty()) {
        throw std::invalid_argument("Query cannot contain only NOT terms");
    }

    // sort each clause by rarity
    for (Clause &c : res) {
        if (!c.empty()) {
            quickSort(c, 0, c.size() - 1, termComp);
        }
    }

    return res;
}