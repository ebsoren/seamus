load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "expressions",
    hdrs = ["expressions.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//lib:algorithm",
        "//lib:string",
        "//lib:vector",
    ],
)

cc_library(
    name = "query_handler",
    hdrs = ["query_handler.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//lib:vector",
        "//lib:string",
        "//lib:unordered_map",
        "//lib:utils",
        "//lib:consts",
        "//lib:rpc_query_handler",
        "//lib:thread_pool",
        "//lib:algorithm",
        ":index_server",
        ":query_helpers",
    ],
)

cc_library(
    name = "index_server",
    hdrs = ["index_server.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//lib:rpc_listener",
        "//lib:rpc_query_handler",
        "//lib:consts",
        "//lib:thread_pool",
        "//ranker:ranker",
        "//index_manager:index_manager",
    ],
)

cc_library(
    name = "query_helpers",
    hdrs = ["query_helpers.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":expressions",
    ],
)