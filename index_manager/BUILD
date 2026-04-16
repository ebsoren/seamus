load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "index_manager",
    hdrs = ["index_manager.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//index_chunk:chunk_manager",
        "//lib:atomic_vector",
        "//lib:consts",
        "//lib:logger",
        "//lib:chunk_manager_query",
        "//lib:string",
        "//lib:utils",
        "//lib:vector",
        "//query:expressions",
    ],
)

cc_binary(
    name = "index_manager_cli",
    srcs = ["main.cpp"],
    deps = [
        ":index_manager",
        "//lib:consts",
        "//lib:chunk_manager_query",
        "//lib:string",
    ],
)
