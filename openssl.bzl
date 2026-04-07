"""Module extension to locate system OpenSSL."""

def _openssl_repo_impl(repository_ctx):
    openssl_path = "/opt/homebrew/opt/openssl@3"

    if not repository_ctx.path(openssl_path).exists:
        openssl_path = "/usr/local/opt/openssl@3"

    if not repository_ctx.path(openssl_path).exists:
        openssl_path = "/usr"

    # Symlink individual header files to avoid Bazel glob issues with directory symlinks
    include_path = repository_ctx.path("{path}/include/openssl".format(path = openssl_path))
    for header in include_path.readdir():
        if str(header).endswith(".h"):
            repository_ctx.symlink(header, "include/openssl/" + header.basename)

    is_system = openssl_path == "/usr"

    repository_ctx.file("BUILD.bazel", content = """
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "ssl",
    hdrs = glob(["include/openssl/*.h"]),
    includes = ["include"],
    linkopts = {linkopts},
    visibility = ["//visibility:public"],
)
""".format(linkopts = '["-lssl", "-lcrypto"]' if is_system else '["-L{path}/lib", "-lssl", "-lcrypto"]'.format(path = openssl_path)))

_openssl_repo = repository_rule(implementation = _openssl_repo_impl)

def _openssl_ext_impl(module_ctx):
    _openssl_repo(name = "openssl")

openssl = module_extension(implementation = _openssl_ext_impl)
