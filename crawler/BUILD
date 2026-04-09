load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "domain_carousel",
    hdrs = ["domain_carousel.h"],
    deps = ["//lib:consts", "//lib:deque", "//lib:string", "//lib:rpc_crawler", "//lib:logger"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "network_util",
    hdrs = ["network_util.h"],
    deps = ["//lib:consts", "//lib:string", "//lib:utils", "@openssl//:ssl"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "crawler_metrics",
    hdrs = ["crawler_metrics.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "crawler_lib",
    hdrs = [
        "crawler_listener.h",
        "bucket_manager.h",
        "network_util.h",
        "crawler_worker.h",
        "crawler_instrumentation.h",
    ],
    deps = [":domain_carousel", ":crawler_metrics", ":network_util", "//lib:consts", "//lib:vector", "//lib:deque", "//lib:string", "//lib:utils", "//lib:rpc_listener", "//lib:rpc_crawler", "//lib:logger", "//parser:parser", "//parser:robots_manager"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "crawler",
    srcs = ["main.cpp"],
    deps = [":crawler_lib", "//url_store"],
    visibility = ["//visibility:public"],
)

