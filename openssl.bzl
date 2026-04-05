"""Module extension to locate system OpenSSL (Homebrew)."""

def _openssl_repo_impl(repository_ctx):
    openssl_path = "/opt/homebrew/opt/openssl@3"
    
    if not repository_ctx.path(openssl_path).exists:
        openssl_path = "/usr/local/opt/openssl@3"

    repository_ctx.symlink("{path}/include".format(path = openssl_path), "include")

    # Add the load statement at the top of the generated BUILD.bazel
    repository_ctx.file("BUILD.bazel", content = """
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "ssl",
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    linkopts = [
        "-L{path}/lib",
        "-lssl",
        "-lcrypto",
    ],
    visibility = ["//visibility:public"],
)
""".format(path = openssl_path))

_openssl_repo = repository_rule(implementation = _openssl_repo_impl)

def _openssl_ext_impl(module_ctx):
    _openssl_repo(name = "openssl")

openssl = module_extension(implementation = _openssl_ext_impl)