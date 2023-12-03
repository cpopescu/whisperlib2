workspace(name = "net_whisper_lib")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_cc",
    urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.6/rules_cc-0.0.6.tar.gz"],
    sha256 = "3d9e271e2876ba42e114c9b9bc51454e379cbf0ec9ef9d40e2ae4cec61a31b40",
    strip_prefix = "rules_cc-0.0.6",
)
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")
rules_cc_dependencies()

## Rules for macos build:
http_archive(
    name = "build_bazel_apple_support",
    sha256 = "cf4d63f39c7ba9059f70e995bf5fe1019267d3f77379c2028561a5d7645ef67c",
    url = "https://github.com/bazelbuild/apple_support/releases/download/1.11.1/apple_support.1.11.1.tar.gz",
)

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)

apple_support_dependencies()

## Google Abseil library
http_archive(
    name = "com_google_absl",
    sha256 = "5366d7e7fa7ba0d915014d387b66d0d002c03236448e1ba9ef98122c13b35c36",
    strip_prefix = "abseil-cpp-20230125.3",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20230125.3.tar.gz"],
)

## Google protobuf
http_archive(
    name = "com_google_protobuf",
    sha256 = "f6251f2d00aad41b34c1dfa3d752713cb1bb1b7020108168a4deaa206ba8ed42",
    strip_prefix = "protobuf-3.21.8",
    urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v21.8/protobuf-cpp-3.21.8.tar.gz"],
)

## Proto rules
http_archive(
    name = "rules_proto",
    sha256 = "a4382f78723af788f0bc19fd4c8411f44ffe0a72723670a34692ffad56ada3ac",
    strip_prefix = "rules_proto-f7a30f6f80006b591fa7c437fe5a951eb10bcbcf",
    urls = ["https://github.com/bazelbuild/rules_proto/archive/f7a30f6f80006b591fa7c437fe5a951eb10bcbcf.zip"],
)


load("@rules_proto//proto:repositories.bzl",
     "rules_proto_dependencies",
     "rules_proto_toolchains")

rules_proto_dependencies()
rules_proto_toolchains()

## Google test & benchmark
http_archive(
    name = "com_google_googletest",
    sha256 = "81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2",
    strip_prefix = "googletest-release-1.12.1",
    urls = ["https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz"],
)

http_archive(
    name = "com_github_google_benchmark",
    sha256 = "3aff99169fa8bdee356eaa1f691e835a6e57b1efeadb8a0f9f228531158246ac",
    strip_prefix = "benchmark-1.7.0",
    urls = ["https://github.com/google/benchmark/archive/refs/tags/v1.7.0.tar.gz"],
)

## Foreign rules
http_archive(
    name = "rules_foreign_cc",
    sha256 = "2a4d07cd64b0719b39a7c12218a3e507672b82a97b98c6a89d38565894cf7c51",
    strip_prefix = "rules_foreign_cc-0.9.0",
    url = "https://github.com/bazelbuild/rules_foreign_cc/archive/refs/tags/0.9.0.tar.gz",
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies(register_built_tools=False)

## ICU
http_archive(
    name = "icu",
    strip_prefix = "icu-release-64-2",
    sha256 = "10cd92f1585c537d937ecbb587f6c3b36a5275c87feabe05d777a828677ec32f",
    urls = [
        "https://github.com/unicode-org/icu/archive/release-64-2.zip",
    ],
    build_file = "@net_whisper_lib//third_party/icu:icu.BUILD",
    patches = ["@net_whisper_lib//third_party/icu:icu-64-2.patch"],
    patch_args = ["-p1"],
)

## Open SSL
http_archive(
    name = "openssl",
    build_file = "@net_whisper_lib//third_party/openssl:openssl.BUILD",
    strip_prefix = "openssl-openssl-3.1.0",
    sha256 = "a86ead5c3e259bf17514decb30b290e0bf3a6366831686cffbacf750df441527",
    urls = ["https://github.com/openssl/openssl/archive/refs/tags/openssl-3.1.0.tar.gz"],
)
