workspace(name = "net_whisper_lib")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_cc",
    sha256 = "9a446e9dd9c1bb180c86977a8dc1e9e659550ae732ae58bd2e8fd51e15b2c91d",
    strip_prefix = "rules_cc-262ebec3c2296296526740db4aefce68c80de7fa",
    urls = [
        "https://github.com/bazelbuild/rules_cc/archive/262ebec3c2296296526740db4aefce68c80de7fa.zip",
    ],
)
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")
rules_cc_dependencies()

## Google Abseil library

http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-master",
    urls = ["https://github.com/abseil/abseil-cpp/archive/master.zip"],
)

## Google test
http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-master",
    urls = ["https://github.com/google/googletest/archive/master.zip"],
)
http_archive(
    name = "com_github_google_benchmark",
    strip_prefix = "benchmark-master",
    urls = ["https://github.com/google/benchmark/archive/master.zip"],
)
## Proto rules
http_archive(
    name = "rules_proto",
    sha256 = "602e7161d9195e50246177e7c55b2f39950a9cf7366f74ed5f22fd45750cd208",
    strip_prefix = "rules_proto-97d8af4dc474595af3900dd85cb3a29ad28cc313",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl",
     "rules_proto_dependencies",
     "rules_proto_toolchains")

rules_proto_dependencies()
rules_proto_toolchains()

## Foreign rules
http_archive(
    name = "rules_foreign_cc",
    strip_prefix = "rules_foreign_cc-ed3db61a55c13da311d875460938c42ee8bbc2a5",
    urls = [
        "https://github.com/bazelbuild/rules_foreign_cc/archive/ed3db61a55c13da311d875460938c42ee8bbc2a5.tar.gz",
    ],
    sha256 = "219bc7280bbb9305938d76067c816954ad2cc0629063412e8b765e9bc6972304",
)

load("@rules_foreign_cc//:workspace_definitions.bzl",
     "rules_foreign_cc_dependencies")
rules_foreign_cc_dependencies()

## ICU
http_archive(
    name = "icu",
    build_file = "@net_whisper_lib//bazel:icu.BUILD",
    strip_prefix = "icu",
    sha256 = "53e37466b3d6d6d01ead029e3567d873a43a5d1c668ed2278e253b683136d948",
    urls = ["https://github.com/unicode-org/icu/releases/download/release-65-1/icu4c-65_1-src.tgz"],
    patches = ["@net_whisper_lib//bazel:icu4c-64_2.patch"],
)

## Open SSL
http_archive(
    name = "openssl",
    build_file = "@net_whisper_lib//bazel:openssl.BUILD",
    strip_prefix = "openssl-OpenSSL_1_1_1g",
    sha256 = "281e4f13142b53657bd154481e18195b2d477572fdffa8ed1065f73ef5a19777",
    urls = ["https://github.com/openssl/openssl/archive/OpenSSL_1_1_1g.tar.gz"],
)

## Google glog and gflags
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
)

http_archive(
    name = "com_github_google_glog",
    sha256 = "62efeb57ff70db9ea2129a16d0f908941e355d09d6d83c9f7b18557c0a7ab59e",
    strip_prefix = "glog-d516278b1cd33cd148e8989aec488b6049a4ca0b",
    urls = ["https://github.com/google/glog/archive/d516278b1cd33cd148e8989aec488b6049a4ca0b.zip"],
)
