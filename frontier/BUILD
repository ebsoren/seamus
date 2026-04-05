load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "Frontier",
    srcs = ["Frontier.cpp"],
    hdrs = ["Frontier.h"],
    visibility = ["//visibility:public"],   # <-- add this
    deps = [
        "//lib:vector",
        "//lib:string",
        "//lib:unordered_map",
        "//lib:priority_queue",
        "//lib:utils",
        "//lib:consts",
    ],
)
