load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "crawler_lib",
    hdrs = [
        "crawler_listener.h",
        "domain_carousel.h",
        "parser.h",
        "bucket_manager.h",
        "network_util.h",
        "crawler_worker.h",
    ],
    copts = ["-I/opt/homebrew/opt/openssl@3/include"],
    linkopts = [                                                        # todo(hershey): add proper linkopts when we build for linux
        "-L/opt/homebrew/opt/openssl@3/lib",
        "-lssl",
        "-lcrypto",
    ],
    deps = ["//lib:consts", "//lib:vector", "//lib:deque", "//lib:string", "//lib:utils", "//lib:rpc_listener", "//lib:rpc_crawler", "//lib:logger", "//parser:parser"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "crawler",
    srcs = ["main.cpp"],
    deps = [":crawler_lib", "//url_store"],
    visibility = ["//visibility:public"],
)

