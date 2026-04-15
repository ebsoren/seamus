load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "htmlserver",
    srcs = ["main.cpp"],
    deps = ["//lib:rpc_listener", "//lib:consts", "//ranker:ranker", 
        "//lib:unordered_map", "//lib:priority_queue", "//lib:logger",
        "//query:expressions", "//lib:algorithm"],
    data = [
        "images/magnifying-glass.png",
        "images/tam-logo.png",
        "images/tank-favicon.png"
    ],
    visibility = ["//visibility:public"],
)
