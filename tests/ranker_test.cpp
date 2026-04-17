#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <stdio.h>
#include "../ranker/Ranker.h"
#include "../lib/string.h"
#include "../lib/vector.h"

int main() {
    setbuf(stdout, NULL);
    RankedPage page1;
    page1.url = string("https://forest-encyclopedia.com/woodchucks");
    page1.title = string("Comprehensive Woodchuck Study");
    page1.doc_len = 1000;
    page1.word_positions.resize(7);

    page1.seed_list_dist = 50;
    page1.domains_from_seed = 1; 
    page1.num_unique_words_found_anchor = 1; 
    page1.num_unique_words_found_title = 0;
    page1.num_unique_words_found_url = 2;
    page1.times_seen = 100;


    // "How" - Scattered
    page1.word_positions[0] = {10, 500, 950};
    // "much"
    page1.word_positions[1] = {45, 480, 800};
    // "wood"
    page1.word_positions[2] = {100, 300, 600, 900};
    // "can"
    page1.word_positions[3] = {5, 250, 550, 850};
    // "a"
    page1.word_positions[4] = {20, 220, 420, 620, 820};
    // "woodchuck"
    page1.word_positions[5] = {150, 450, 750};
    // "chuck"
    page1.word_positions[6] = {200, 400, 700, 990};


    RankedPage page2;
    page2.url = string("https://rhymes.org/woodchuck");
    page2.title = string("Woodchuck Tongue Twister");
    page2.doc_len = 100;
    page2.word_positions.resize(7);

    page2.seed_list_dist = 3;
    page2.domains_from_seed = 10; 
    page2.num_unique_words_found_anchor = 1; 
    page2.num_unique_words_found_title = 4;
    page2.num_unique_words_found_url = 3;
    page2.times_seen = 100;

    // Sequence 1: "How much wood can a woodchuck..." at start
    // Indices:      0    1    2    3   4   5
    page2.word_positions[0] = {1};  // How
    page2.word_positions[1] = {2};  // much
    page2.word_positions[2] = {3, 50};  // wood
    page2.word_positions[3] = {4, 51};  // can
    page2.word_positions[4] = {5, 52};  // a
    page2.word_positions[5] = {6, 53};  // woodchuck
    page2.word_positions[6] = {54}; // chuck (second sequence only)

    // Note: Page 2 has the exact phrase "wood can a woodchuck chuck" 
    // starting at position 50.
    vector<QueryInfo> woodchuckQuery;
    // "a" is extremely common
    woodchuckQuery.push_back(QueryInfo{string("how"), 500000, false});
    woodchuckQuery.push_back(QueryInfo{string("much"), 100000, false});
    woodchuckQuery.push_back(QueryInfo{string("wood"), 50000, false});
    woodchuckQuery.push_back(QueryInfo{string("can"), 450000, false});
    woodchuckQuery.push_back(QueryInfo{string("a"), 1000000, false});
    woodchuckQuery.push_back(QueryInfo{string("woodchuck"), 500, false});
    woodchuckQuery.push_back(QueryInfo{string("chuck"), 10000, false});
    

    UrlStore * url_store = new UrlStore(nullptr, 0);
    Ranker r = Ranker(url_store, 10, 0.7, true);

    double d1 = r.testScore(page1, woodchuckQuery);
    double d2 = r.testScore(page2, woodchuckQuery);

    printf("\n===== SCORE OUTPUTS =====\n\n");
    printf("Page 1 score: %.6f\n", d1);
    printf("Page 2 score: %.6f\n", d2);

    return 0;
}