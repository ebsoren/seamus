load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "crawler_lib",
    hdrs = [
        "crawler_listener.h",
        "domain_carousel.h",
        "parser.h",
        "bucket_manager.h",
        "network_util.h",
        "crawler_worker.h",
    ],
    deps = ["//lib:consts", "//lib:vector", "//lib:deque", "//lib:string", "//lib:utils", "//lib:rpc_listener", "//lib:rpc_crawler"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "crawler",
    srcs = ["main.cpp"],
    deps = [":crawler_lib"],
    visibility = ["//visibility:public"],
)
