load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "index_lib",
    srcs = ["Index.cpp"],
    hdrs = ["Index.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//lib:algorithm",
        "//lib:consts",
        "//lib:deque",
        "//lib:logger",
        "//lib:string",
        "//lib:unordered_map",
        "//lib:utf8",
        "//lib:utils",
        "//lib:vector",
    ],
)

cc_binary(
    name = "index",
    srcs = ["main.cpp"],
    deps = [":index_lib"],
)
