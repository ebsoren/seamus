load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "chunk_manager",
    srcs = ["isr.cpp"],
    hdrs = [
        "chunk_manager.h",
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
