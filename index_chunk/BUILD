load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "chunk_manager",
    srcs = ["isr.cpp"],
    hdrs = [
        "chunk_manager.h",
        "isr.h",
        "query_isr.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//index:index_lib",
        "//lib:algorithm",
        "//lib:atomic_vector",
        "//lib:consts",
        "//lib:logger",
        "//lib:chunk_manager_query",
        "//lib:string",
        "//lib:utf8",
        "//lib:utils",
        "//lib:vector",
        "//query:expressions",
        "//lib:rpc_query_handler",
    ],
)
