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
    name = "query_helpers",
    hdrs = ["query_helpers.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":expressions",
    ],
)