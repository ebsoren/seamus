#pragma once

#include "../lib/string.h"
#include "../lib/vector.h"
#include "../lib/algorithm.h"
#include <cassert>
#include <cstring>

// LOGIC:
// "..." → PHRASE
// - → NOT
// | → OR
// space → AND


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

enum PhraseFlag {
    INDIV,
    PHRASE
};

enum IncludedFlag {
    NOT_INCLUDED,
    INCLUDED
};

double getRarity(string_view token, PhraseFlag flag) {
    if(flag == PHRASE) return 0.0;
    return 1.0;
}

struct Term {
    string_view token;
    IncludedFlag included;
    double rarity;
    PhraseFlag flag;

    Term(string_view t, IncludedFlag i, PhraseFlag f = INDIV)
        : token(t), included(i), rarity(getRarity(t, f)), flag(f) {}
};

inline bool termComp(const Term& a, const Term& b) {
    return a.rarity < b.rarity;
}

using Clause = vector<Term>;
using Result = vector<Clause>;

struct Token {
    string_view text;
    PhraseFlag flag;
};

class QueryParser {
private:
    vector<Token> tokens;
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

    void tokenize() {
        size_t n = input.size();

        for (size_t j = 0; j < n;) {
            if (isspace(input[j])) { j++; continue; }

            if (input[j] == '(' || input[j] == ')' ||
                input[j] == '|' || input[j] == '-') {
                tokens.push_back({string_view(input.data() + j, 1), INDIV});
                j++;
                continue;
            }

            if (input[j] == '"') {
                size_t k = j + 1;
                while (k < n && input[k] != '"') k++;

                tokens.push_back({
                    string_view(input.data() + j + 1, k - j - 1),
                    PHRASE
                });

                j = k + 1;
                continue;
            }

            size_t k = j;
            while (k < n &&
                   !isspace(input[k]) &&
                   input[k] != '(' &&
                   input[k] != ')' &&
                   input[k] != '|' &&
                   input[k] != '-') {
                k++;
            }

            tokens.push_back({
                string_view(input.data() + j, k - j),
                INDIV
            });

            j = k;
        }
    }

    Result parseExpression() {
        Result left = parseTerm();

        while (i < tokens.size() && tokens[i].text == "|") {
            i++;
            Result right = parseTerm();

            left.reserve_exact(left.size() + right.size());
            for (auto& c : right) {
                left.emplace_back(std::move(c));
            }
        }

        return left;
    }

    Result parseTerm() {
        Result result;
        result.emplace_back();

        while (i < tokens.size() &&
               !(tokens[i].text == ")") &&
               !(tokens[i].text == "|")) {

            Result next = parseFactor();
            result = combineAND(result, next);
        }

        return result;
    }

    Result parseFactor() {
        bool negate = false;

        if (tokens[i].text == "-") {
            negate = true;
            i++;
        }

        Result res;

        if (tokens[i].text == "(") {
            i++;
            Result inner = parseExpression();
            assert(tokens[i].text == ")");
            i++;

            if (negate) {
                Clause merged;

                for (const auto& c : inner) {
                    for (const auto& t : c) {
                        IncludedFlag flipped =
                            (t.included == INCLUDED) ? NOT_INCLUDED : INCLUDED;

                        merged.emplace_back(t.token, flipped, t.flag);
                    }
                }

                res.emplace_back(std::move(merged));
            } else {
                return inner;
            }
        }
        else {
            Clause clause;

            IncludedFlag inc = negate ? NOT_INCLUDED : INCLUDED;

            clause.emplace_back(
                tokens[i].text,
                inc,
                tokens[i].flag
            );

            i++;
            res.emplace_back(std::move(clause));
        }

        return res;
    }

    static Result combineAND(const Result& a, const Result& b) {
        Result res;
        res.reserve_exact(a.size() * b.size());

        for (const auto& ca : a) {
            for (const auto& cb : b) {

                Clause merged;
                merged.reserve_exact(ca.size() + cb.size());

                for (const auto& t : ca) merged.emplace_back(t);
                for (const auto& t : cb) merged.emplace_back(t);

                res.emplace_back(std::move(merged));
            }
        }

        return res;
    }
};


// 🔥 REMOVE DUPES + CONTRADICTIONS
inline void dedupe_and_clean(Result& res) {
    Result cleaned;

    for (auto& clause : res) {

        Clause new_clause;
        bool contradiction = false;

        for (const auto& t : clause) {

            bool found = false;

            for (auto& existing : new_clause) {
                if (existing.token == t.token &&
                    existing.flag == t.flag) {

                    if (existing.included != t.included) {
                        contradiction = true;
                        break;
                    }

                    found = true;
                    break;
                }
            }

            if (contradiction) break;

            if (!found) {
                new_clause.emplace_back(t);
            }
        }

        if (!contradiction && !new_clause.empty()) {
            cleaned.emplace_back(std::move(new_clause));
        }
    }

    res = std::move(cleaned);
}


// remove clauses with only NOTs
inline void filter_query(Result& res) {
    Result filtered;

    for (const auto& clause : res) {
        bool has_positive = false;

        for (const auto& term : clause) {
            if (term.included == INCLUDED) {
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


// USER INTERFACE
inline Result parse_query(const string& query) {
    QueryParser p(query);
    Result res = p.parse();

    dedupe_and_clean(res);   // 🔥 new
    filter_query(res);

    if (res.empty()) {
        throw std::invalid_argument("Query cannot contain only NOT terms");
    }

    for (Clause &c : res) {
        if (!c.empty()) {
            quickSort(c, 0, c.size() - 1, termComp);
        }
    }

    return res;
}