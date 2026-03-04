load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "crawler",
    srcs = ["crawler.cpp"],
    hdrs = [
        "crawler_listener.h",
        "domain_carousel.h",
        "parse_util.h",
        "bucket_manager.h",
        "network_util.h"
    ],
    deps = ["//lib:consts", "//lib:vector", "//lib:deque", "//lib:string", "//lib:utils"],
    visibility = ["//visibility:public"],
)
