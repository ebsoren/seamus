#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/utils.h"
#include "../lib/consts.h"
#include "../url_store/url_store.h"

#include "query_handler.h"
#include "../ranker/Ranker.h"
#include "../index/Index.h"
#include "../index-stream-reader/isr.h"

/*
Query Language Syntax:
    - needs to support AND, OR, NOT, and parentheses for precedence and ordering
    - queries without explicit operators will be treated as AND queries (e.g. "hello world" is treated as "hello AND world")
    - for simplicity, we will assume that all queries are well-formed and do not contain syntax errors (e.g. unbalanced parentheses, invalid operator usage, etc.)
        - can add error handling if needed
    - if words in quotes, there must be an exact match
*/
int main(int argc, char* argv[]) {

    // TODO: recover index from disk and initialize ISR with it
    IndexChunk index; // todo: how do we interact with this index? how do we recover from disk
    IndexStreamReader isr;
    UrlStore url_store(nullptr, 0);


    IndexServer index_server(&url_store);
    QueryHandler qh(&isr);
    vector<RankedPage> results = qh.get_results(...);

    Ranker r();

    // rank pages, and return top k results somewhere


    return 0;
}