load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "consts",
    hdrs = ["consts.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":string",
    ],
)

cc_library(
    name = "Frontier",
    hdrs = ["Frontier.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":string", ":vector", ":priority_queue", ":unordered_map", ":consts"
    ],
)

cc_library(
    name = "vector",
    hdrs = ["vector.h"],
    visibility = ["//visibility:public"],
    deps = [":utils"],
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
    deps = [
        ":utils",
        ":vector",
    ],
)

cc_library(
    name = "priority_queue",
    hdrs = ["priority_queue.h"],
    visibility = ["//visibility:public"],
    deps = [":vector"],
)

cc_library(
    name = "thread_pool",
    hdrs = ["thread_pool.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":deque",
        ":vector",
    ],
)

cc_library(
    name = "unordered_map",
    hdrs = ["unordered_map.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":string",
        ":vector",
        ":utils",
    ],
)

cc_library(
    name = "utf8",
    hdrs = ["utf8.h"],
    visibility = ["//visibility:public"],
    deps = [],
)

cc_library(
    name = "utils",
    hdrs = ["utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":consts",
        ":string",
    ],
)

cc_library(
    name = "string",
    hdrs = ["string.h"],
    visibility = ["//visibility:public"],
    deps = [],
)

cc_library(
    name = "rpc_common",
    hdrs = ["rpc_common.h"],
    visibility = ["//visibility:public"],
    deps = [":string"],
)

cc_library(
    name = "rpc_urlstore",
    hdrs = ["rpc_urlstore.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rpc_common",
        ":string",
        ":vector",
        ":utils"
    ],
)

cc_library(
    name = "rpc_listener",
    hdrs = ["rpc_listener.h"],
    visibility = ["//visibility:public"],
    deps = [":thread_pool"],
)

cc_library(
    name = "rpc_crawler",
    hdrs = ["rpc_crawler.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rpc_common",
        ":string",
        ":vector",
    ],
)

cc_library(
    name = "logger",
    hdrs = ["logger.h"],
    visibility = ["//visibility:public"],
    deps = [":consts"],
)
