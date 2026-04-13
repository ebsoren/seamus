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
    vector<LoadedIndex> indexChunks;
    recover_index_chunks(indexChunks);
    UrlStore url_store(nullptr, 0);

    Ranker r;
    IndexServer index_server(&url_store, indexChunks);
    QueryHandler qh(&r, &index_server);
    qh.start();

    return 0;
}