load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "parser",
    hdrs = [
        "HtmlTags.h",
        "english_dict.h",
        "parser.h",
        "url_buffers.h",
        "word_array.h"
    ],
    deps = [
        "//lib:buffer",
        "//lib:consts",
        "//lib:string",
        "//lib:unordered_map",
        "//url_store:url_store",
        "//lib:utils",
    ],
    visibility = ["//visibility:public"]
)

cc_library(
    name = "robots_manager",
    srcs = ["RobotsManager.cpp"],
    hdrs = ["RobotsManager.h"],
    deps = [
        ":parser",
        "//lib:consts",
        "//lib:string",
        "//lib:unordered_map",
        "//crawler:network_util",
    ],
    visibility = ["//visibility:public"]
)
