load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "htmlserver",
    srcs = ["main.cpp"],
    deps = ["//lib:rpc_listener", "//lib:consts"],
    visibility = ["//visibility:public"],
)
