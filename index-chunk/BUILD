load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "chunk-manager",
    srcs = ["isr.cpp"],
    hdrs = [
        "chunk-manager.h",
        "isr.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//index:index_lib",
        "//lib:algorithm",
        "//lib:consts",
        "//lib:logger",
        "//lib:string",
        "//lib:utf8",
        "//lib:utils",
        "//lib:vector",
    ],
)
