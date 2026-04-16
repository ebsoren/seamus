load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "ranker",
    hdrs = ["Ranker.h"],
    deps = [
        ":ranker_consts",
        "//lib:chunk_manager_query",
        "//lib:consts",
        "//lib:logger",
        "//lib:priority_queue",
        "//lib:rpc_query_handler",
        "//query:expressions",
        "//url_store:url_store",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ranker_consts",
    hdrs = ["ranker_consts.h"],
    visibility = ["//visibility:public"],
)