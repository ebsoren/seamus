"""Module extension to locate system OpenSSL (Homebrew)."""

def _openssl_repo_impl(repository_ctx):
    openssl_path = "/opt/homebrew/opt/openssl@3"
    repository_ctx.execute([
        "bash", "-c",
        "cp -R {path}/include . && find include -name BUILD -delete -o -name BUILD.bazel -delete".format(path = openssl_path),
    ])
    repository_ctx.file("BUILD.bazel", content = """
cc_library(
    name = "ssl",
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    linkopts = [
        "-L/opt/homebrew/opt/openssl@3/lib",
        "-lssl",
        "-lcrypto",
    ],
    visibility = ["//visibility:public"],
)
""")

_openssl_repo = repository_rule(implementation = _openssl_repo_impl)

def _openssl_ext_impl(module_ctx):
    _openssl_repo(name = "openssl")

openssl = module_extension(implementation = _openssl_ext_impl)
