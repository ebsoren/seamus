load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "vector",
    hdrs = ["vector.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "deque",
    hdrs = ["deque.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "algorithm",
    hdrs = ["algorithm.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "priority_queue",
    hdrs = ["priority_queue.h"],
    deps = [":vector"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "thread_pool",
    hdrs = ["thread_pool.h"],
    deps = [":vector"],
    visibility = ["//visibility:public"],
)
