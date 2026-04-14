load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "isr",
    srcs = ["isr.cpp"],
    hdrs = ["isr.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//index:index_lib",
        "//lib:consts",
        "//lib:logger",
        "//lib:string",
        "//lib:utf8",
    ],
)
