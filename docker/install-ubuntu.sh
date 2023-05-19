#!/usr/bin/bash

apt-get update

apt-get install -y clang wget perl make gcc

UNAME=$(uname -m)
if [ "${UNAME}" == "aarch64" ]; then
    ARCH="arm64"
    SHA256="a836972b8a7c34970fb9ecc44768ece172f184c5f7e2972c80033fcdcf8c1870"
else
    ARCH="amd64"
    SHA256="61699e22abb2a26304edfa1376f65ad24191f94a4ffed68a58d42b6fee01e124"
fi

FNAME="bazelisk-linux-${ARCH}"
wget "https://github.com/bazelbuild/bazelisk/releases/download/v1.17.0/${FNAME}"
FSUM=$(sha256sum ${FNAME})
if [ "${FSUM}" != "${SHA256}  ${FNAME}" ]; then
    echo "Invalid SHA256 sum: ${FSUM} vs. ${SHA256}"
    exit 1
fi
mv ${FNAME} /usr/bin/bazel
chmod a+x /usr/bin/bazel
