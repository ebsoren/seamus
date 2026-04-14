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
// Call parse_query(const string& query) to get a QueryNode returned. The root will have an empty vector of words
// From here, there will be children in a vector of QueryNodes where each branch represents a different path
// Two different branches essentially are an OR from one another and the same path going all the way down is an AND
// Multiword phrases will have a flag and simply be contained in a string as well. 
// Negated entries will have a NOT flag as well. Most entries will just have 1 word.


// Custom Utilities

struct ParseError {
    const char* message;
};

template<typename T>
class UniquePtr {
    T* ptr;
public:
    UniquePtr() : ptr(nullptr) {}
    explicit UniquePtr(T* p) : ptr(p) {}
    ~UniquePtr() { delete ptr; }
    
    UniquePtr(UniquePtr&& other) : ptr(other.ptr) { other.ptr = nullptr; }
    UniquePtr& operator=(UniquePtr&& other) {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    T* get() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
};

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool string_view_equals(const string_view& a, const string_view& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// Core Structs
struct QueryNode {
    string_view val; // use helper functions to separate multi-word phrases by rarity
    vector<QueryNode*> children;
    bool is_negated = false; // flag for something hit by a NOT operator
    bool is_phrase = false; // flag for multi-word phrases
    int word_id = -1; // maps to the index in your boolean vector
};

struct QueryArena {
    static constexpr size_t MAX_NODES = 128; 
    QueryNode nodes[MAX_NODES];
    size_t used = 0;

    QueryNode* alloc() {
        if (used >= MAX_NODES) throw ParseError{"Arena out of memory"};
        QueryNode* n = &nodes[used++];
        n->val = string_view();
        n->children.clear();
        n->is_negated = false;
        n->is_phrase = false;
        return n;
    }
};

struct UniqueTerm {
    string_view val;
    bool is_phrase = false;
};

struct ParseResult {
    QueryNode* root = nullptr;
    UniquePtr<QueryArena> arena;
    vector<UniqueTerm> unique_terms;
};

// Syntax Tree & DNF logic

struct TermData {
    string_view val;
    bool is_negated = false;
    bool is_phrase = false; // Carries through AST
    int id = 0; 

    bool operator==(const TermData& o) const {
        return is_negated == o.is_negated && 
               is_phrase == o.is_phrase && 
               string_view_equals(val, o.val);
    }
};

enum ASTType { AST_TERM, AST_AND, AST_OR };

struct ASTNode {
    ASTType type;
    TermData term;
    vector<ASTNode> children;
};

static ASTNode negate_ast(ASTNode node) {
    if (node.type == AST_TERM) {
        node.term.is_negated = !node.term.is_negated;
        return node;
    }
    if (node.type == AST_AND) {
        node.type = AST_OR;
        for (size_t i = 0; i < node.children.size(); ++i) {
            node.children[i] = negate_ast(node.children[i]);
        }
        return node;
    }
    if (node.type == AST_OR) {
        node.type = AST_AND;
        for (size_t i = 0; i < node.children.size(); ++i) {
            node.children[i] = negate_ast(node.children[i]);
        }
        return node;
    }
    return node;
}

struct Path {
    vector<TermData> terms;
};

static vector<Path> get_dnf(const ASTNode& node) {
    if (node.type == AST_TERM) {
        Path p;
        p.terms.push_back(node.term);
        vector<Path> res;
        res.push_back(p);
        return res;
    }
    if (node.type == AST_OR) {
        vector<Path> res;
        for (size_t i = 0; i < node.children.size(); ++i) {
            vector<Path> sub = get_dnf(node.children[i]);
            for (size_t j = 0; j < sub.size(); ++j) {
                res.push_back(sub[j]);
            }
        }
        return res;
    }
    if (node.type == AST_AND) {
        vector<Path> res;
        Path empty_path;
        res.push_back(empty_path);

        for (size_t i = 0; i < node.children.size(); ++i) {
            vector<Path> sub = get_dnf(node.children[i]);
            vector<Path> next_res;
            
            for (size_t r = 0; r < res.size(); ++r) {
                for (size_t s = 0; s < sub.size(); ++s) {
                    Path combined = res[r];
                    for (size_t t = 0; t < sub[s].terms.size(); ++t) {
                        combined.terms.push_back(sub[s].terms[t]);
                    }
                    next_res.push_back(combined);
                }
            }
            res = next_res;
        }
        return res;
    }
    return vector<Path>();
}

// 3. Parser & Tokenizer

enum TokenType { TOK_WORD, TOK_PHRASE, TOK_LPAREN, TOK_RPAREN, TOK_PIPE, TOK_MINUS };

struct Token {
    TokenType type;
    string_view val; 
};

static vector<Token> tokenize(const string_view& str) {
    vector<Token> tokens;
    size_t i = 0;
    while (i < str.size()) {
        // 1. Skip leading spaces
        while (i < str.size() && is_space(str[i])) i++;
        if (i >= str.size()) break;

        // 2. Check for explicit single-character tokens
        if (str[i] == '(') { Token t; t.type = TOK_LPAREN; tokens.push_back(t); i++; }
        else if (str[i] == ')') { Token t; t.type = TOK_RPAREN; tokens.push_back(t); i++; }
        else if (str[i] == '|') { Token t; t.type = TOK_PIPE; tokens.push_back(t); i++; }
        
        // Because spaces are skipped above, this ONLY triggers if the token 
        // literally starts with a minus sign (e.g., "-dog" or " -(cat)")
        else if (str[i] == '-') { Token t; t.type = TOK_MINUS; tokens.push_back(t); i++; }
        
        else if (str[i] == '"') {
            i++; 
            size_t start = i;
            while (i < str.size() && str[i] != '"') i++;
            
            Token t; 
            t.type = TOK_PHRASE; 
            t.val = string_view(str.data() + start, i - start); 
            tokens.push_back(t);
            
            if (i < str.size()) i++; 
        }
        else {
            // 3. Scan a standard word
            size_t start = i;
            
            // FIX: Removed '&& str[i] != '-'' from this condition. 
            // Now, intra-word dashes like in "sci-fi" are treated as regular characters.
            while (i < str.size() && !is_space(str[i]) && str[i] != '(' && str[i] != ')' && str[i] != '|' && str[i] != '"') i++;
            
            Token t; 
            t.type = TOK_WORD;
            t.val = string_view(str.data() + start, i - start);
            tokens.push_back(t);
        }
    }
    return tokens;
}

static int g_term_id = 0;

static ASTNode parse_expr(const vector<Token>& tokens, size_t& pos);

static ASTNode parse_factor(const vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size()) throw ParseError{"Unexpected end of query"};
    
    if (tokens[pos].type == TOK_MINUS) {
        pos++;
        return negate_ast(parse_factor(tokens, pos));
    }
    if (tokens[pos].type == TOK_LPAREN) {
        pos++;
        ASTNode node = parse_expr(tokens, pos);
        if (pos >= tokens.size() || tokens[pos].type != TOK_RPAREN) {
            throw ParseError{"Missing closing parenthesis ')'"};
        }
        pos++;
        return node;
    }
    if (tokens[pos].type == TOK_WORD || tokens[pos].type == TOK_PHRASE) {
        ASTNode node;
        node.type = AST_TERM;
        node.term.id = g_term_id++;
        node.term.val = tokens[pos].val;
        // Check tokenizer type to set the boolean flag
        node.term.is_phrase = (tokens[pos].type == TOK_PHRASE);
        pos++;
        return node;
    }
    throw ParseError{"Unexpected token encountered"};
}

static ASTNode parse_term_seq(const vector<Token>& tokens, size_t& pos) {
    vector<ASTNode> factors;
    while (pos < tokens.size() && tokens[pos].type != TOK_PIPE && tokens[pos].type != TOK_RPAREN) {
        factors.push_back(parse_factor(tokens, pos));
    }
    if (factors.empty()) throw ParseError{"Empty term sequence"};
    if (factors.size() == 1) return factors[0];
    
    ASTNode node; 
    node.type = AST_AND; 
    node.children = factors;
    return node;
}

static ASTNode parse_expr(const vector<Token>& tokens, size_t& pos) {
    vector<ASTNode> seqs;
    seqs.push_back(parse_term_seq(tokens, pos));
    
    while (pos < tokens.size() && tokens[pos].type == TOK_PIPE) {
        pos++; 
        seqs.push_back(parse_term_seq(tokens, pos));
    }
    if (seqs.size() == 1) return seqs[0];

    ASTNode node; 
    node.type = AST_OR; 
    node.children = seqs;
    return node;
}

struct TermFreq {
    TermData term;
    int count;
};

static void assign_word_ids(QueryNode* node, vector<UniqueTerm>& dict) {
    if (!node) return;

    // Skip the root node (which has an empty string_view)
    if (node->val.size() > 0) {
        int id = -1;
        
        // Check if we already registered this base word/phrase
        for (size_t i = 0; i < dict.size(); ++i) {
            if (string_view_equals(dict[i].val, node->val) && dict[i].is_phrase == node->is_phrase) {
                id = (int)i;
                break;
            }
        }
        
        // If not found, add it to our dictionary and assign the new ID
        if (id == -1) {
            id = (int)dict.size();
            UniqueTerm t;
            t.val = node->val;
            t.is_phrase = node->is_phrase;
            dict.push_back(t);
        }
        
        node->word_id = id;
    }

    // Recurse down all branches
    for (size_t i = 0; i < node->children.size(); ++i) {
        assign_word_ids(node->children[i], dict);
    }
}


// PUBLIC INTERFACE CHARLIE!!
ParseResult parse_query_tree(const string_view& query_str) {
    g_term_id = 0;
    ParseResult res;
    res.arena = UniquePtr<QueryArena>(new QueryArena());

    vector<Token> tokens = tokenize(query_str);
    if (tokens.empty()) return res;

    size_t pos = 0;
    ASTNode ast = parse_expr(tokens, pos);
    if (pos < tokens.size()) throw ParseError{"Unexpected trailing tokens"};

    vector<Path> paths = get_dnf(ast);

    for (size_t p = 0; p < paths.size(); ++p) {
        bool has_pos = false;
        for (size_t t = 0; t < paths[p].terms.size(); ++t) { 
            if (!paths[p].terms[t].is_negated) { has_pos = true; break; } 
        }
        if (!has_pos) throw ParseError{"Query branch cannot contain ONLY negated terms"};
    }

    vector<TermFreq> freqs;
    for (size_t p = 0; p < paths.size(); ++p) {
        for (size_t t = 0; t < paths[p].terms.size(); ++t) {
            bool found = false;
            for (size_t f = 0; f < freqs.size(); ++f) {
                if (freqs[f].term == paths[p].terms[t]) {
                    freqs[f].count++;
                    found = true;
                    break;
                }
            }
            if (!found) {
                TermFreq tf;
                tf.term = paths[p].terms[t];
                tf.count = 1;
                freqs.push_back(tf);
            }
        }
    }

    for (size_t p = 0; p < paths.size(); ++p) {
        vector<TermData>& terms = paths[p].terms;
        for (size_t i = 0; i < terms.size(); ++i) {
            for (size_t j = 0; j < terms.size() - i - 1; ++j) {
                int freq_a = 0, freq_b = 0;
                for (size_t f = 0; f < freqs.size(); ++f) {
                    if (freqs[f].term == terms[j]) freq_a = freqs[f].count;
                    if (freqs[f].term == terms[j+1]) freq_b = freqs[f].count;
                }
                
                bool should_swap = false;
                if (freq_a < freq_b) should_swap = true;
                else if (freq_a == freq_b && terms[j].id > terms[j+1].id) should_swap = true;

                if (should_swap) {
                    TermData temp = terms[j];
                    terms[j] = terms[j+1];
                    terms[j+1] = temp;
                }
            }
        }
    }

    res.root = res.arena->alloc();
    for (size_t p = 0; p < paths.size(); ++p) {
        QueryNode* curr = res.root;
        for (size_t t = 0; t < paths[p].terms.size(); ++t) {
            const TermData& term = paths[p].terms[t];
            QueryNode* next = nullptr;
            for (size_t c = 0; c < curr->children.size(); ++c) {
                QueryNode* child = curr->children[c];
                
                // Compare the phrase flag alongside negation and value
                if (child->is_negated == term.is_negated && 
                    child->is_phrase == term.is_phrase &&
                    string_view_equals(child->val, term.val)) {
                    next = child; 
                    break;
                }
            }
            if (!next) {
                next = res.arena->alloc();
                next->val = term.val;
                next->is_negated = term.is_negated;
                next->is_phrase = term.is_phrase; // Assign phrase flag to tree node
                curr->children.push_back(next);
            }
            curr = next;
        }
    }
    assign_word_ids(res.root, res.unique_terms);

    return res;
}