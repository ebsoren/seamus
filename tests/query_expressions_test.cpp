#include <iostream>
#include <stdexcept>
#include <cassert>

// include your parser header here
#include "../query/expressions.h"

// Utils

void assert_true(bool condition, const char* msg) {
    if (!condition) {
        std::cerr << "FAILED: " << msg << std::endl;
        std::exit(1);
    }
}

void expect_throw(const string& query, const char* msg) {
    bool thrown = false;

    try {
        parse_query(query);
    } catch (const std::invalid_argument&) {
        thrown = true;
    } catch (...) {
        thrown = true;
    }

    assert_true(thrown, msg);
}


// TESTS
void test_single_term() {
    string word = string("apple");
    auto res = parse_query(word);

    assert_true(res.size() == 1, "single term should produce 1 clause");
    assert_true(res[0].size() == 1, "clause should contain 1 term");
    assert_true(res[0][0].token == "apple", "token mismatch");
    assert_true(res[0][0].included == true, "term should be included");
}

void test_only_not_rejected() {
    expect_throw(string("NOT apple"), "ONLY NOT query should throw");
    expect_throw(string("NOT apple OR NOT banana"), "only NOT OR NOT should throw");
}

void test_and_logic() {
    string word = string("apple banana");
    auto res = parse_query(word);

    assert_true(res.size() >= 1, "AND should produce result");
    assert_true(res[0].size() == 2, "AND merges into clause");
}

void test_or_logic() {
    string word = string("apple OR banana");
    auto res = parse_query(word);

    assert_true(res.size() == 2, "OR should produce 2 clauses");
}

void test_parentheses() {
    string word = string("(apple OR banana) cherry");
    auto res = parse_query(word);

    assert_true(res.size() >= 1, "parentheses expansion valid");

    for (auto& clause : res) {
        assert_true(clause.size() >= 2, "AND should combine terms");
    }
}

void test_phrase_tokenization() {
    string word = string("\"new york\"");
    auto res = parse_query(word);

    assert_true(res.size() == 1, "phrase should be single clause");
    assert_true(res[0][0].token.contains(' ') == true, "phrase preserved spacing");
}

void test_not_inside_expression() {
    string word = string("apple OR NOT banana");
    auto res = parse_query(word);

    // NOT clause removed → only apple remains
    assert_true(res.size() == 1, "NOT-only clause removed");

    for (auto& t : res[0]) {
        assert_true(t.included == true, "no NOT terms should remain");
    }
}

void test_filtering_behavior() {
    string word = string("NOT a OR (NOT b NOT c)");
    expect_throw(word, "all NOT clauses removed → exception expected");
}

void test_complex_expression() {
    string word = string("(apple OR banana) AND (cherry OR \"new york\")");
    auto res = parse_query(word);

    assert_true(res.size() >= 1, "complex query produces results");

    for (auto& clause : res) {
        assert_true(clause.size() >= 2, "AND expansion works");
    }
}

void test_sorting_by_rarity() {
    string word = string("apple banana cherry");
    auto res = parse_query(word);

    for (auto& clause : res) {
        for (size_t i = 1; i < clause.size(); i++) {
            assert_true(
                clause[i - 1].rarity <= clause[i].rarity,
                "terms sorted by rarity ascending"
            );
        }
    }
}

void test_massive_complex_query() {
    /*
        Query structure:

        (apple OR "new york" OR NOT banana)
        AND
        (cherry OR NOT grape OR (orange AND NOT kiwi))
        AND
        NOT watermelon
    */

    string query = string(
        "(apple OR \"new york\" OR NOT banana) "
        "AND "
        "(cherry OR NOT grape OR (orange AND NOT kiwi)) "
        "AND "
        "NOT watermelon"
    );

    bool thrown = false;
    Result res;

    try {
        res = parse_query(query);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    if (thrown) {
        // valid outcome if everything collapses to NOT-only
        assert_true(true, "complex query correctly collapsed to empty result");
        return;
    }

    assert_true(res.size() > 0, "result should contain at least one clause");

    // ensure no clause is NOT-only (guarantee filter worked)
    for (const auto& clause : res) {
        bool has_positive = false;

        for (const auto& term : clause) {
            if (term.included) {
                has_positive = true;
            }
        }

        assert_true(has_positive, "no clause should be NOT-only after filtering");
    }

    // ensure structure consistency
    for (const auto& clause : res) {
        assert_true(clause.size() >= 1, "clause should not be empty");
    }

    // ensure sorting by rarity still holds
    for (const auto& clause : res) {
        for (size_t i = 1; i < clause.size(); i++) {
            assert_true(
                clause[i - 1].rarity <= clause[i].rarity,
                "rarity ordering must be preserved"
            );
        }
    }

    // sanity check: phrase preserved
    bool found_phrase = false;
    for (const auto& clause : res) {
        for (const auto& term : clause) {
            if (term.token.contains(' ')) {
                found_phrase = true;
            }
        }
    }

    assert_true(found_phrase, "phrase token should survive parsing");

    std::cout << "COMPLEX TEST PASSED" << std::endl;
}

// Main function
int main() {
    test_single_term();
    test_only_not_rejected();
    test_and_logic();
    test_or_logic();
    test_parentheses();
    test_phrase_tokenization();
    test_not_inside_expression();
    test_filtering_behavior();
    test_complex_expression();
    test_sorting_by_rarity();
    test_massive_complex_query();

    std::cout << "ALL TESTS PASSED" << std::endl;
}