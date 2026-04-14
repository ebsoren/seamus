#pragma once

#include "../lib/string.h"
#include "../lib/vector.h"
#include "../lib/algorithm.h"
#include "../query/expressions.h"
#include <cassert>
#include <cstring>

// THE NEXT TWO FUNCTIONS SUPPORT GETTING A VECTOR OF ALL WORDS / MULTIWORD PHRASES TO SEARCH FOR IN THE INDEX
// USE THE SECOND FUNCTION AS A PUBLIC INTERFACE, THE ONE BELOW IS JUST A HELPER
// Recursive helper to traverse the tree and collect unique terms
static void collect_unique_words(QueryNode* node, vector<string_view>& out) {
    if (!node) return;

    //  root node  has an empty val, so we skip empty string_views
    if (node->val.size() > 0) {
        bool found = false;
        for (size_t i = 0; i < out.size(); ++i) {
            if (string_view_equals(out[i], node->val)) {
                found = true;
                break;
            }
        }
        // only push if we haven't seen it yet
        if (!found) {
            out.push_back(node->val);
        }
    }

    // recurse down the branches
    for (size_t i = 0; i < node->children.size(); ++i) {
        collect_unique_words(node->children[i], out);
    }
}

// PUBLIC INTERFACE FUNCTION TO GET THE UNIQUE WORDS TO SEARCH FOR!!
static vector<string_view> get_unique_words(QueryNode* root) {
    vector<string_view> unique_words;
    unique_words.reserve(16); // lil memory optimization for u guys
    
    collect_unique_words(root, unique_words);
    return unique_words;
}

// helper function to get rarity of a word from the index
double get_word_rarity(string_view word) {
    return 1.0; // TODO: Call some type of index function here to get rarity
}

struct ScoredTerm {
    string_view word;
    float rarity_score;
};
static bool compare_rarity_desc(const ScoredTerm& a, const ScoredTerm& b) {
    return a.rarity_score < b.rarity_score;
}

// function that takes the string of a multi-word phrase and gives an output vector in order of their rarity
vector<string_view> tokenize_and_sort_by_rarity(const string_view& text) {
    vector<ScoredTerm> scored_terms;
    
    size_t i = 0;
    while (i < text.size()) {

        while (i < text.size() && is_space(text[i])) i++;
        if (i >= text.size()) break;
        
        size_t start = i;
        while (i < text.size() && !is_space(text[i])) i++;
        
        string_view current_word(text.data() + start, i - start);
        
        ScoredTerm st;
        st.word = current_word;
        st.rarity_score = get_word_rarity(current_word);
        scored_terms.push_back(st);
    }
    
    if (scored_terms.size() > 1) {
        quickSort(scored_terms, 0, scored_terms.size() - 1, compare_rarity_desc);
    }

    vector<string_view> sorted_words;
    for (size_t j = 0; j < scored_terms.size(); ++j) {
        sorted_words.push_back(scored_terms[j].word);
    }
    
    return sorted_words;
}

// function that given the vector of words that we've found, decides whether there is a match in the tree
// doc_terms_found is a boolean array where index == word_id. The index should match the original vector of unique words 
// passed from the expression parser. It is set to true if the document contains that specific query term.
bool evaluate_query(QueryNode* node, const vector<bool>& doc_terms_found) {
    if (!node) return false;

    // root node check (root gets word_id == -1)
    if (node->word_id < 0) {
        if (node->children.empty()) return false; 
        
        for (size_t i = 0; i < node->children.size(); ++i) {
            if (evaluate_query(node->children[i], doc_terms_found)) {
                return true;
            }
        }
        return false;
    }

    // array lookup
    bool found = doc_terms_found[node->word_id];
    
    // logic for negation
    bool is_satisfied = node->is_negated ? !found : found;

    // kill branch if not satisfied
    if (!is_satisfied) return false;

    // If satisfied and it's a leaf, the whole path is complete and return good ole true!
    if (node->children.empty()) return true;

    // If satisfied but has children, we must find at least one valid path forward and continue on!
    for (size_t i = 0; i < node->children.size(); ++i) {
        if (evaluate_query(node->children[i], doc_terms_found)) {
            return true;
        }
    }

    return false;
}