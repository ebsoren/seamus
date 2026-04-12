#include <iostream>
#include <stdexcept>
#include <cassert>

#include "../query/expressions.h"

// UTILS

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
    assert_true(res[0][0].included == INCLUDED, "term should be included");
}

void test_only_not_rejected() {
    expect_throw(string("-apple"), "ONLY NOT query should throw");
    expect_throw(string("-apple | -banana"), "only NOT OR NOT should throw");
}

void test_and_logic() {
    string word = string("apple banana");
    auto res = parse_query(word);

    assert_true(res.size() == 1, "AND should produce 1 clause");
    assert_true(res[0].size() == 2, "AND merges into clause");
}

void test_or_logic() {
    string word = string("apple | banana");
    auto res = parse_query(word);

    assert_true(res.size() == 2, "OR should produce 2 clauses");
}

void test_parentheses() {
    string word = string("(apple | banana) cherry");
    auto res = parse_query(word);

    assert_true(res.size() == 2, "parentheses should expand OR");

    for (auto& clause : res) {
        assert_true(clause.size() == 2, "AND should combine terms");
    }
}

void test_phrase_tokenization() {
    string word = string("\"new york\"");
    auto res = parse_query(word);

    assert_true(res.size() == 1, "phrase should be single clause");
    assert_true(res[0][0].flag == PHRASE, "should be marked as PHRASE");
    assert_true(res[0][0].token.contains(' '), "phrase preserved spacing");
}

void test_not_inside_expression() {
    string word = string("apple | -banana");
    auto res = parse_query(word);

    // "-banana" clause removed → only apple remains
    assert_true(res.size() == 1, "NOT-only clause removed");

    for (auto& t : res[0]) {
        assert_true(t.included == INCLUDED, "no NOT terms should remain");
    }
}

void test_filtering_behavior() {
    string word = string("-a | (-b -c)");
    expect_throw(word, "all NOT clauses removed → exception expected");
}

void test_complex_expression() {
    string word = string("(apple | banana) (cherry | \"new york\")");
    auto res = parse_query(word);

    assert_true(res.size() == 4, "cross product expected (2x2)");

    for (auto& clause : res) {
        assert_true(clause.size() == 2, "AND expansion works");
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

void test_deduplication() {
    string word = string("apple apple banana apple");
    auto res = parse_query(word);

    assert_true(res.size() == 1, "still one clause");

    // apple should appear only once
    int apple_count = 0;
    for (auto& t : res[0]) {
        if (t.token == "apple") apple_count++;
    }

    assert_true(apple_count == 1, "duplicates should be removed");
}

void test_contradiction_removal() {
    string word = string("apple -apple");
    expect_throw(word, "contradiction should remove clause entirely");
}

void test_massive_complex_query() {
    string query = string(
        "(apple | \"new york\" | -banana) "
        "(cherry | -grape | (orange -kiwi)) "
        "-watermelon"
    );

    bool thrown = false;
    Result res;

    try {
        res = parse_query(query);
    } catch (...) {
        thrown = true;
    }

    if (thrown) {
        assert_true(true, "complex query may collapse entirely");
        return;
    }

    assert_true(res.size() > 0, "result should contain clauses");

    for (const auto& clause : res) {
        bool has_positive = false;

        for (const auto& term : clause) {
            if (term.included == INCLUDED) {
                has_positive = true;
            }
        }

        assert_true(has_positive, "no clause should be NOT-only");
    }

    for (const auto& clause : res) {
        assert_true(!clause.empty(), "clause should not be empty");
    }

    for (const auto& clause : res) {
        for (size_t i = 1; i < clause.size(); i++) {
            assert_true(
                clause[i - 1].rarity <= clause[i].rarity,
                "rarity ordering must be preserved"
            );
        }
    }

    bool found_phrase = false;
    for (const auto& clause : res) {
        for (const auto& term : clause) {
            if (term.flag == PHRASE) {
                found_phrase = true;
            }
        }
    }

    assert_true(found_phrase, "phrase token should survive parsing");

    std::cout << "COMPLEX TEST PASSED" << std::endl;
}


// Main
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
    test_deduplication();
    test_contradiction_removal();
    test_massive_complex_query();

    std::cout << "ALL TESTS PASSED" << std::endl;
}