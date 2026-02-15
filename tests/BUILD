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
    name = "algorithm_test",
    srcs = ["algorithm_test.cpp"],
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
