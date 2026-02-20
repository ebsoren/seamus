load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "url_store",
    srcs = ["url_store.cpp"],
    hdrs = ["url_store.h"],
    deps = ["//lib:vector", "//lib:unordered_map", "//lib:rpc_listener", "//lib:rpc_urlstore"],
    visibility = ["//visibility:public"],
)
