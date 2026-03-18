load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "parser",
    hdrs = [
        "HtmlTags.h",
        "parser.h",
        "word_array.h"
    ],
    deps = [
        "//lib:buffer",
        "//lib:string",
        "//url_store:url_store",
        "//lib:utils",
    ],
    visibility = ["//visibility:public"]
)
