load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "mutex_test",
    srcs = ["mutex_test.cpp"],
    deps = ["//lib:mutex"],
)

cc_test(
    name = "raii_test",
    srcs = ["raii_test.cpp"],
    deps = ["//lib:mutex", "//lib:lock_guard"],
)

cc_test(
    name = "condition_variable_test",
    srcs = ["condition_variable_test.cpp"],
    deps = ["//lib:mutex", "//lib:condition_variable"],
)

cc_test(
    name = "lock_guard_test",
    srcs = ["lock_guard_test.cpp"],
    deps = ["//lib:mutex", "//lib:lock_guard"],
)

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
        "//lib:vector",
        "//lib:string",
        "//lib:unordered_map",
        "//lib:priority_queue",
        "//lib:utils",
        "//frontier:Frontier",
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
    name = "deque_benchmark",
    srcs = ["deque_benchmark.cpp"],
    deps = ["//lib:deque"],
)

cc_test(
    name = "unordered_map_test",
    srcs = ["unordered_map_test.cpp"],
    deps = ["//lib:unordered_map"],
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
    deps = ["//lib:utils"],
)

cc_test(
    name = "string_benchmark",
    srcs = ["string_benchmark.cpp"],
    deps = ["//lib:string"],
)

cc_test(
    name = "rpc_test",
    srcs = ["rpc_test.cpp"],
    deps = ["//lib:rpc_common", "//lib:rpc_listener", "//lib:rpc_urlstore", "//lib:vector"],
)
