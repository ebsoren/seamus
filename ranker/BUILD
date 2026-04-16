load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "ranker",
    srcs = ["Ranker.cpp"], # Assuming you have a .cpp file; if header-only, remove this
    hdrs = ["Ranker.h"],
    deps = [
        ":ranker_consts",
        "//lib:vector",
        "//lib:string",
        "//lib:unordered_map",
        "//lib:priority_queue",
        "//lib:utils",
        "//lib:consts",
        "//lib:logger",
        "//lib:rpc_query_handler",
        "//query:expressions",
        "//index_chunk:chunk_manager",
        "//url_store:url_store",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ranker_consts",
    hdrs = ["ranker_consts.h"],
    visibility = ["//visibility:public"],
)