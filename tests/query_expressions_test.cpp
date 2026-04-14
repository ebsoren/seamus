#include <iostream>
#include <stdexcept>
#include <cassert>
#include <functional>

#include "../query/expressions.h"
#include "../query/query_helpers.h"
 
// Test Harness

#include <iostream>

static int g_passed = 0, g_failed = 0;

#define ASSERT(cond, msg)                                                        \
    do {                                                                         \
        if (cond) { ++g_passed; } else {                                         \
            ++g_failed;                                                          \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__                \
                      << "  " << (msg) << "\n";                                  \
        }                                                                        \
    } while (0)

#define ASSERT_THROWS(expr, msg)                                                 \
    do {                                                                         \
        bool caught_ = false;                                                    \
        try { expr; } catch (const ParseError&) { caught_ = true; }              \
        ASSERT(caught_, msg);                                                    \
    } while (0)

static void print_node(QueryNode* node, const std::string& prefix, bool is_last) {
    const std::string branch  = is_last ? "└── " : "├── ";
    const std::string indent  = is_last ? "    " : "│   ";
    std::cout << prefix << branch;

    if (node->is_negated) std::cout << "[NOT] ";

    // Visualize the phrase flag
    if (node->is_phrase) {
        std::cout << "<phrase: " << std::string(node->val.data(), node->val.size()) << ">\n";
    } else {
        std::cout << '"' << std::string(node->val.data(), node->val.size()) << "\"\n";
    }

    for (size_t i = 0; i < node->children.size(); ++i) {
        bool last = (i == node->children.size() - 1);
        print_node(node->children[i], prefix + indent, last);
    }
}

static void print_tree(const char* query_str, QueryNode* root) {
    std::cout << "\n  Query : \"" << query_str << "\"\n";
    std::cout << "  Tree  :\n\n    <root>\n";
    for (size_t i = 0; i < root->children.size(); ++i) {
        bool last = (i == root->children.size() - 1);
        print_node(root->children[i], "    ", last);
    }
    std::cout << '\n';
}

// Helper to simulate an engine matching document words against the unique_terms dictionary
static bool simulate_match(const ParseResult& res, const vector<string_view>& doc_words) {
    vector<bool> doc_terms_found;
    
    // Initialize the boolean mask to false for all unique dictionary terms
    for (size_t i = 0; i < res.unique_terms.size(); ++i) {
        doc_terms_found.push_back(false);
    }

    // Populate the mask: if the document has the word, set its ID to true
    for (size_t i = 0; i < res.unique_terms.size(); ++i) {
        for (size_t j = 0; j < doc_words.size(); ++j) {
            // For testing, we are ignoring the is_phrase flag distinction, 
            // but in a real engine you would verify positional phrase matches here too.
            if (string_view_equals(res.unique_terms[i].val, doc_words[j])) {
                doc_terms_found[i] = true;
                break;
            }
        }
    }

    return evaluate_query(res.root, doc_terms_found);
}

static string_view make_sv(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    return string_view(str, len);
}

// Quick string_view vector builder for tests
static vector<string_view> make_doc(const char* w1, const char* w2 = nullptr, const char* w3 = nullptr, const char* w4 = nullptr) {
    vector<string_view> doc;
    if (w1) doc.push_back(make_sv(w1));
    if (w2) doc.push_back(make_sv(w2));
    if (w3) doc.push_back(make_sv(w3));
    if (w4) doc.push_back(make_sv(w4));
    return doc;
}

// Tests

void run_all_tests() {

    // Basic AND Logic
    {
        ParseResult r = parse_query_tree(make_sv("apple banana cherry"));
        ASSERT(r.root != nullptr, "Basic: Root is valid");
        ASSERT(r.root->children.size() == 1, "Basic: One continuous path for ANDs");
    }

    // Exact Phrases
    {
        ParseResult r = parse_query_tree(make_sv("\"new york\" pizza"));
        ASSERT(r.root->children.size() == 1, "Phrase: Single path");
        
        // Traverse to check phrase flag
        QueryNode* first_term = r.root->children[0]; 
        bool has_phrase = false;
        if (first_term->is_phrase || (!first_term->children.empty() && first_term->children[0]->is_phrase)) {
            has_phrase = true;
        }
        ASSERT(has_phrase, "Phrase: Flag is successfully preserved in AST");
    }

    // Hyphens vs Minus
    {
        ParseResult r = parse_query_tree(make_sv("spider-man -venom"));
        ASSERT(r.root->children.size() == 1, "Hyphen: Single path");
        
        QueryNode* curr = r.root->children[0];
        bool found_spider_man = false;
        bool found_neg_venom = false;
        
        while (curr) {
            if (string_view_equals(curr->val, make_sv("spider-man")) && !curr->is_negated) found_spider_man = true;
            if (string_view_equals(curr->val, make_sv("venom")) && curr->is_negated) found_neg_venom = true;
            curr = curr->children.empty() ? nullptr : curr->children[0];
        }
        ASSERT(found_spider_man, "Hyphen: Intra-word dash kept intact");
        ASSERT(found_neg_venom, "Hyphen: Leading dash treated as NOT");
    }

    // OR Logic & Branching
    {
        ParseResult r = parse_query_tree(make_sv("red | blue | green"));
        ASSERT(r.root->children.size() == 3, "OR: 3 distinct branches at root");
    }

    // Cartesian Product
    {
        ParseResult r = parse_query_tree(make_sv("(cat | dog) (food | toys)"));
        // Due to tree deduplication, the root should have 2 children (cat, dog)
        // and each of those should have 2 children (food, toys)
        ASSERT(r.root->children.size() == 2, "DNF: 2 main prefixes at root");
        ASSERT(r.root->children[0]->children.size() == 2, "DNF: Branch 1 splits into 2");
        ASSERT(r.root->children[1]->children.size() == 2, "DNF: Branch 2 splits into 2");
    }

    // Tree Prefix Merging
    {
        ParseResult r = parse_query_tree(make_sv("database (sql | nosql)"));
        // "database sql" OR "database nosql" -> Should merge 'database' at root
        ASSERT(r.root->children.size() == 1, "Merge: Root collapses to 1 child ('database')");
        ASSERT(r.root->children[0]->children.size() == 2, "Merge: Splits after the shared prefix");
    }

    // Negation & De Morgan's Law
    {
        ParseResult r = parse_query_tree(make_sv("cars -(ford | chevy)"));
        // Translates to: cars AND -ford AND -chevy
        ASSERT(r.root->children.size() == 1, "De Morgan: Becomes a single sequential AND path");
        
        int neg_count = 0;
        QueryNode* curr = r.root->children[0];
        while (curr) {
            if (curr->is_negated) neg_count++;
            curr = curr->children.empty() ? nullptr : curr->children[0];
        }
        ASSERT(neg_count == 2, "De Morgan: Negation distributed to 2 terms");
    }

    // Error Handling & Throws
    {
        // Unclosed parentheses
        ASSERT_THROWS(parse_query_tree(make_sv("(apple banana")), "Error: Unclosed parenthesis");
        ASSERT_THROWS(parse_query_tree(make_sv("apple banana)")), "Error: Unmatched closing parenthesis");

        // Trailing/Hanging operators
        ASSERT_THROWS(parse_query_tree(make_sv("apple |")), "Error: Trailing pipe");
        ASSERT_THROWS(parse_query_tree(make_sv("apple -")), "Error: Trailing minus");
        ASSERT_THROWS(parse_query_tree(make_sv("apple -| banana")), "Error: Minus before pipe");

        // Empty sequences
        ASSERT_THROWS(parse_query_tree(make_sv("apple () banana")), "Error: Empty parentheses");
        ASSERT_THROWS(parse_query_tree(make_sv("apple || banana")), "Error: Empty OR sequence");

        // Only Negative branches
        ASSERT_THROWS(parse_query_tree(make_sv("-apple")), "Error: Root cannot be only negative");
        ASSERT_THROWS(parse_query_tree(make_sv("-(apple | banana)")), "Error: Group cannot be only negative");
        ASSERT_THROWS(parse_query_tree(make_sv("-apple -banana")), "Error: Sequence cannot be only negative");
        ASSERT_THROWS(parse_query_tree(make_sv("good | -bad")), "Error: OR branch cannot be only negative");
    }

    // Document Evaluation & Boolean Matching
    std::cout << "\n--- Testing Document Evaluation ---\n";
    {
        // Test A: Basic AND Sequence
        ParseResult r_and = parse_query_tree(make_sv("apple banana"));
        ASSERT(simulate_match(r_and, make_doc("apple", "banana")) == true,  "Eval: AND - Full match");
        ASSERT(simulate_match(r_and, make_doc("apple", "orange")) == false, "Eval: AND - Missing term");
        ASSERT(simulate_match(r_and, make_doc("orange", "grape")) == false, "Eval: AND - Complete miss");

        // Test B: Basic OR Sequence
        ParseResult r_or = parse_query_tree(make_sv("cat | dog"));
        ASSERT(simulate_match(r_or, make_doc("cat", "bird")) == true,       "Eval: OR - Left match");
        ASSERT(simulate_match(r_or, make_doc("fish", "dog")) == true,       "Eval: OR - Right match");
        ASSERT(simulate_match(r_or, make_doc("cat", "dog")) == true,        "Eval: OR - Both match");
        ASSERT(simulate_match(r_or, make_doc("fish", "bird")) == false,     "Eval: OR - No match");

        // Test C: Negation & Exclusion
        ParseResult r_not = parse_query_tree(make_sv("movie -scary"));
        ASSERT(simulate_match(r_not, make_doc("movie", "funny")) == true,   "Eval: NOT - Found pos, missed neg");
        ASSERT(simulate_match(r_not, make_doc("movie", "scary")) == false,  "Eval: NOT - Found pos, found neg (Fail)");
        ASSERT(simulate_match(r_not, make_doc("book", "funny")) == false,   "Eval: NOT - Missed pos");

        // Test D: Complex DNF Expansion
        ParseResult r_dnf = parse_query_tree(make_sv("(car | truck) (red | blue) -broken"));
        
        // Valid paths: 
        // 1. car red -broken
        // 2. car blue -broken
        // 3. truck red -broken
        // 4. truck blue -broken
        ASSERT(simulate_match(r_dnf, make_doc("car", "red", "fast")) == true,        "Eval: DNF - Valid Path 1");
        ASSERT(simulate_match(r_dnf, make_doc("truck", "blue", "slow")) == true,     "Eval: DNF - Valid Path 4");
        
        // Failing paths
        ASSERT(simulate_match(r_dnf, make_doc("car", "green")) == false,             "Eval: DNF - Missing second OR group");
        ASSERT(simulate_match(r_dnf, make_doc("car", "red", "broken")) == false,     "Eval: DNF - Hits negative term");
        ASSERT(simulate_match(r_dnf, make_doc("van", "red")) == false,               "Eval: DNF - Missing first OR group");

        // Test E: De Morgan's Negation Distribution
        ParseResult r_demorgan = parse_query_tree(make_sv("recipe -(nuts | dairy)"));
        // Matches ONLY IF "recipe" is found AND "nuts" is NOT found AND "dairy" is NOT found
        ASSERT(simulate_match(r_demorgan, make_doc("recipe", "chicken")) == true,         "Eval: DeMorgan - Valid");
        ASSERT(simulate_match(r_demorgan, make_doc("recipe", "nuts", "chicken")) == false,"Eval: DeMorgan - Hits first negated OR");
        ASSERT(simulate_match(r_demorgan, make_doc("recipe", "dairy")) == false,          "Eval: DeMorgan - Hits second negated OR");
    }
    
    ParseResult c = parse_query_tree(make_sv("(\"star wars\" | \"star trek\") -(sequels | \"phantom menace\") sci-fi"));
    print_tree("(\"star wars\" | \"star trek\") -(sequels | \"phantom menace\") sci-fi", c.root);

    ParseResult c3 = parse_query_tree(make_sv("database (sql server | sql lite)"));
    print_tree("database (sql server | sql lite)", c3.root);

    ParseResult c2 = parse_query_tree(make_sv("\"machine learning\" (python | c++) -(beginner | \"crash course\")"));
    print_tree("\"machine learning\" (python | c++) -(beginner | \"crash course\")", c2.root);

    ParseResult c1 = parse_query_tree(make_sv("(cat | dog) (food | toys) cheap"));
    print_tree("(cat | dog) (food | toys) cheap", c1.root);

    std::cout << "\n════════════════════════════\n";
    std::cout << "  Passed : " << g_passed << "\n";
    std::cout << "  Failed : " << g_failed << "\n";
    std::cout << "════════════════════════════\n\n";
}

int main() {
    run_all_tests();
    return g_failed == 0 ? 0 : 1;
}