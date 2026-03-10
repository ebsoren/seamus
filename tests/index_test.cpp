#include <cassert>

#include "../index/Index.h"

void test_add_page_simple() {
    IndexChunk idx;

    idx.add_page(string("index_test_simple.in"));
}

int main() {
    printf("\n=== RUNNING INDEX TESTS ===\n\n");

    test_add_page_simple();

    printf("\n=== ALL INDEX TESTS FINISHED ===\n\n");
    return 0;
}