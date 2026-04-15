load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "index_isr_validation",
    srcs = ["main.cpp"],
    deps = [
        "//index:index_lib",
        "//index_chunk:chunk_manager",
        "//lib:consts",
        "//lib:logger",
        "//lib:string",
        "//lib:utf8",
        "//lib:vector",
    ],
)
