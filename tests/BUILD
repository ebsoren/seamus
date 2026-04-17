load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "vector_test",
    srcs = ["vector_test.cpp"],
    deps = ["//lib:vector"],
)

cc_test(
    name = "deque_test",
    srcs = ["deque_test.cpp"],
    deps = ["//lib:deque"],
)

cc_test(
    name = "frontier_test",
    srcs = ["frontier_test.cpp"],
    deps = [
        "//lib:Frontier",
        "//lib:priority_queue",
        "//lib:string",
        "//lib:unordered_map",
        "//lib:utils",
        "//lib:vector",
    ],
)

cc_test(
    name = "binary_search_test",
    srcs = ["binary_search_test.cpp"],
    deps = ["//lib:algorithm"],
)

cc_test(
    name = "priority_queue_test",
    srcs = ["priority_queue_test.cpp"],
    deps = ["//lib:priority_queue"],
)

cc_test(
    name = "thread_pool_benchmark",
    srcs = ["thread_pool_benchmark.cpp"],
    deps = ["//lib:thread_pool"],
)

cc_test(
    name = "joinable_thread_pool_test",
    srcs = ["joinable_thread_pool_test.cpp"],
    deps = ["//lib:joinable_thread_pool"],
)

cc_test(
    name = "deque_benchmark",
    srcs = ["deque_benchmark.cpp"],
    deps = ["//lib:deque"],
)

cc_test(
    name = "unordered_map_test",
    srcs = ["unordered_map_test.cpp"],
    deps = [
        "//lib:unordered_map",
        "//lib:utils",
        "//lib:vector",
    ],
)

cc_test(
    name = "unordered_map_benchmark",
    srcs = ["unordered_map_benchmark.cpp"],
    deps = [
        "//lib:unordered_map",
        "//lib:utils",
        "//lib:vector",
    ],
)

cc_test(
    name = "string_test",
    srcs = ["string_test.cpp"],
    copts = ["-fsanitize=address"],
    linkopts = ["-fsanitize=address"],
    deps = ["//lib:string"],
)

cc_test(
    name = "string_view_test",
    srcs = ["string_view_test.cpp"],
    copts = ["-fsanitize=address"],
    linkopts = ["-fsanitize=address"],
    deps = ["//lib:string"],
)

cc_test(
    name = "utils_test",
    srcs = ["utils_test.cpp"],
    deps = [
        "//lib:consts",
        "//lib:utils",
    ],
)

cc_test(
    name = "string_benchmark",
    srcs = ["string_benchmark.cpp"],
    deps = ["//lib:string"],
)

cc_test(
    name = "rpc_test",
    srcs = ["rpc_test.cpp"],
    deps = [
        "//lib:rpc_common",
        "//lib:rpc_crawler",
        "//lib:rpc_listener",
        "//lib:rpc_urlstore",
        "//lib:vector",
    ],
)

cc_test(
    name = "crawler_test",
    srcs = ["crawler_test.cpp"],
    deps = [
        "//crawler:crawler_lib",
        "//parser:parser"
    ],
)

cc_test(
    name = "url_store_test",
    srcs = ["url_store_test.cpp"],
    deps = [
        "//lib:consts",
        "//lib:utils",
        "//url_store:url_store",
        "//crawler:domain_carousel",
    ],
)

cc_test(
    name = "index_test",
    srcs = ["index_test.cpp"],
    data = ["index_test_simple.in"],
    deps = [
        "//index:index_lib"
    ]
)

cc_test(
    name = "instrumentation_test",
    srcs = ["instrumentation_test.cpp"],
    deps = [
        "//crawler:crawler_lib",
    ],
)

cc_test(
    name = "https_benchmark",
    srcs = ["https_benchmark.cpp"],
    deps = [
        "//crawler:crawler_lib",
        "//lib:utils",
        "//lib:string",
    ],
)

cc_test(
    name = "query_expressions_test",
    srcs = ["query_expressions_test.cpp"],
    deps = [
        "//query:expressions",
        "//query:query_helpers",
        "//lib:algorithm",
        "//lib:string",
        "//lib:vector",
    ],
)

cc_test (
    name = "index_util_test",
    srcs = ["index_util_test.cpp"],
    tags = ["no-sandbox", "local", "exclusive"],
    deps = [
        "//index_chunk:chunk_manager",
    ],
)

cc_test (
    name = "query_isr_test",
    srcs = ["query_isr_test.cpp"],
    tags = ["no-sandbox", "local", "exclusive"],
    deps = [
        "//index_chunk:chunk_manager",
        "//index:index_lib",
        "//lib:consts",
        "//lib:string",
        "//lib:vector",
        "//query:expressions",
    ],
)

cc_test (
    name = "chunk_manager_ranking_test",
    srcs = ["chunk_manager_ranking_test.cpp"],
    tags = ["no-sandbox", "local", "exclusive"],
    deps = [
        "//index_chunk:chunk_manager",
        "//index:index_lib",
        "//lib:atomic_vector",
        "//lib:chunk_manager_query",
        "//lib:consts",
        "//lib:rpc_query_handler",
        "//lib:string",
        "//lib:vector",
        "//query:expressions",
        "//ranker:ranker",
    ],
)

cc_test (
    name = "index_manager_test",
    srcs = ["index_manager_test.cpp"],
    tags = ["no-sandbox", "local", "exclusive"],
    deps = [
        "//index_chunk:chunk_manager",
        "//index_manager:index_manager",
        "//index:index_lib",
        "//lib:consts",
        "//lib:chunk_manager_query",
        "//lib:string",
        "//lib:vector",
    ],
)
