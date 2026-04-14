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

// 3 thread pools needed:
// spawn ISRs per unique term in the query
// use big thread pool to craw local index chunks
//     use fix sized inner thread pool to query each index chunk for multiple words
// use another thread pool to fan out rpc query indices to other indexServers (17 other machines to fanout too)

// retrieve top 10 best page results from other machines
// top 10 best pages from this machine
// total of 180 pages to rank, then return top 10 amongst those to the client

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