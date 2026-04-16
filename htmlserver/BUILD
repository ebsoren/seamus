load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "htmlserver",
    srcs = [
        "main.cpp",
        "html_templates.h",  
    ],
    deps = [
        "//lib:rpc_listener", 
        "//lib:consts", 
        "//ranker:ranker", 
        "//lib:string",      
        "//lib:unordered_map", 
        "//lib:priority_queue", 
        "//lib:logger",
        "//query:expressions", 
        "//lib:algorithm", 
        "//index_chunk:chunk_manager",
        "//url_store:url_store"
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