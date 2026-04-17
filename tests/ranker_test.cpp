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
    
    // --- SETUP QUERY FIRST SO WE KNOW THE INDICES ---
    // 0: how, 1: much, 2: wood, 3: can, 4: a, 5: woodchuck, 6: chuck
    vector<QueryInfo> woodchuckQuery;
    woodchuckQuery.push_back(QueryInfo{string("how"), 500000, false});
    woodchuckQuery.push_back(QueryInfo{string("much"), 100000, false});
    woodchuckQuery.push_back(QueryInfo{string("wood"), 50000, false});
    woodchuckQuery.push_back(QueryInfo{string("can"), 450000, false});
    woodchuckQuery.push_back(QueryInfo{string("a"), 1000000, false}); // highly common
    woodchuckQuery.push_back(QueryInfo{string("woodchuck"), 500, false}); // rare
    woodchuckQuery.push_back(QueryInfo{string("chuck"), 10000, false});

    // ==========================================
    // SETUP PAGE 1 (Low relevance)
    // ==========================================
    RankedPage page1;
    page1.url = string("https://forest-encyclopedia.com/woodchucks");
    page1.title = string("Comprehensive Woodchuck Study");
    page1.doc_len = 1000;
    page1.word_positions.resize(7);

    page1.seed_list_dist = 50;
    page1.domains_from_seed = 1; 
    page1.times_seen = 100;

    // NEW FAKE DATA: Anchor Text (Low volume, low diversity)
    page1.unique_phrases_matched_anchor = 1;
    page1.total_link_frequency_anchor = 5;

    // NEW FAKE DATA: Boolean Vectors (Index matches the query vector)
    // Old test said: anchor(1), title(0), url(2)
    page1.anchor_words_found = {false, false, false, false, false, true, false}; 
    page1.title_words_found  = {false, false, false, false, false, false, false}; 
    page1.url_words_found    = {false, false, true, false, false, true, false}; 

    // Positions for "How much wood can a woodchuck chuck" - Scattered
    page1.word_positions[0] = {10, 500, 950};
    page1.word_positions[1] = {45, 480, 800};
    page1.word_positions[2] = {100, 300, 600, 900};
    page1.word_positions[3] = {5, 250, 550, 850};
    page1.word_positions[4] = {20, 220, 420, 620, 820};
    page1.word_positions[5] = {150, 450, 750};
    page1.word_positions[6] = {200, 400, 700, 990};


    // ==========================================
    // SETUP PAGE 2 (High relevance)
    // ==========================================
    RankedPage page2;
    page2.url = string("https://rhymes.org/woodchuck");
    page2.title = string("Woodchuck Tongue Twister");
    page2.doc_len = 100;
    page2.word_positions.resize(7);

    page2.seed_list_dist = 3;
    page2.domains_from_seed = 10; 
    page2.times_seen = 100;

    // NEW FAKE DATA: Anchor Text (High volume, high diversity)
    page2.unique_phrases_matched_anchor = 15; 
    page2.total_link_frequency_anchor = 350; 

    // NEW FAKE DATA: Boolean Vectors 
    // Old test said: anchor(1), title(4), url(3)
    // We'll fake hits on rare words to pump the IDF scores!
    page2.anchor_words_found = {false, false, true, false, false, true, false};
    page2.title_words_found  = {false, false, true, false, true, true, true}; 
    page2.url_words_found    = {false, false, true, false, false, true, true}; 

    // Sequence 1: "How much wood can a woodchuck..." at start
    page2.word_positions[0] = {1};  // How
    page2.word_positions[1] = {2};  // much
    page2.word_positions[2] = {3, 50};  // wood
    page2.word_positions[3] = {4, 51};  // can
    page2.word_positions[4] = {5, 52};  // a
    page2.word_positions[5] = {6, 53};  // woodchuck
    page2.word_positions[6] = {54}; // chuck (second sequence only)


    // ==========================================
    // EXECUTE RANKER
    // ==========================================
    UrlStore * url_store = new UrlStore(nullptr, 0);
    Ranker r = Ranker(url_store, 10, 0.7, true);

    double d1 = r.testScore(page1, woodchuckQuery);
    double d2 = r.testScore(page2, woodchuckQuery);

    printf("\n===== SCORE OUTPUTS =====\n\n");
    printf("Page 1 score: %.6f\n", d1);
    printf("Page 2 score: %.6f\n", d2);

    return 0;
}