load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "htmlserver",
    srcs = [
        "main.cpp",
        "html_templates.h",  
    ],
    deps = [
        "//lib:rpc_listener",
        "//lib:algorithm",
        "//ranker:ranker",
        "//lib:unordered_map",
        "//lib:priority_queue",
        "//lib:logger",
        "//query:expressions",
        "//query:query_handler"
    ],
    data = [
        "images/magnifying-glass.png",
        "images/tam-logo.png",
        "images/tank-favicon.png",
        "images/seamusfinalphoto.png",
        "images/sirtophamhatt.png",
    ],
    visibility = ["//visibility:public"],
)