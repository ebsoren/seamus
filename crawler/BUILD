load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "crawler",
    srcs = ["crawler.cpp"],
    hdrs = [
        "crawler_listener.h",
        "domain_carousel.h",
        "parse_util.h",
    ],
    deps = ["//lib:consts", "//lib:vector", "//lib:deque"],
    visibility = ["//visibility:public"],
)
