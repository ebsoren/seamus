#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/utils.h"
#include "../lib/consts.h"

#include "query_handler.h"
#include "../ranker/Ranker.h"
#include "../index/Index.h"
#include "../index-stream-reader/isr.h"

int main(int argc, char* argv[]) {

    // TODO: determine query language syntax/how to parse queries before feeding into QueryCompiler
    // parse query here
    auto query_words = vector<string>{"example", "query", "words"};

    // TODO(charlie): input constructor args for ISR
    IndexServer index_server;
    IndexStreamReader isr;
    QueryHandler qh(&index_server, &isr);
    vector<RankedPage> results = qh.get_results(query_words);

    Ranker r();

    // rank pages, and return top k results somewhere


    return 0;
}