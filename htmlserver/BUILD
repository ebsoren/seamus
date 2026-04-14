load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "htmlserver",
    srcs = ["main.cpp"],
    deps = ["//lib:rpc_listener", "//lib:consts"],
    data = [
        "images/magnifying-glass.png",
        "images/tam-logo.png",
        "images/tank-favicon.png"
    ],
    visibility = ["//visibility:public"],
)
