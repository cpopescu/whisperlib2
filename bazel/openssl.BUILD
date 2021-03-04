## Per: https://github.com/envoyproxy/envoy-openssl/blob/main/openssl.BUILD

licenses(["notice"])  # Apache 2

package(
    default_visibility = ["//visibility:public"],
)

load("@rules_cc//cc:defs.bzl", "cc_library")

# We need to label this for configure_make.
filegroup(
    name = "all",
    srcs = glob(["**"]),
)

openssl_headers = [
    "install/include/openssl/pkcs7.h",
    "install/include/openssl/cterr.h",
    "install/include/openssl/tls1.h",
    "install/include/openssl/pkcs12.h",
    "install/include/openssl/rand_drbg.h",
    "install/include/openssl/rc2.h",
    "install/include/openssl/blowfish.h",
    "install/include/openssl/seed.h",
    "install/include/openssl/opensslconf.h",
    "install/include/openssl/buffererr.h",
    "install/include/openssl/tserr.h",
    "install/include/openssl/x509err.h",
    "install/include/openssl/asyncerr.h",
    "install/include/openssl/buffer.h",
    "install/include/openssl/ecerr.h",
    "install/include/openssl/sha.h",
    "install/include/openssl/engine.h",
    "install/include/openssl/bioerr.h",
    "install/include/openssl/kdf.h",
    "install/include/openssl/conf_api.h",
    "install/include/openssl/sslerr.h",
    "install/include/openssl/err.h",
    "install/include/openssl/x509_vfy.h",
    "install/include/openssl/md4.h",
    "install/include/openssl/ts.h",
    "install/include/openssl/pkcs7err.h",
    "install/include/openssl/modes.h",
    "install/include/openssl/md2.h",
    "install/include/openssl/pemerr.h",
    "install/include/openssl/ecdh.h",
    "install/include/openssl/dh.h",
    "install/include/openssl/rand.h",
    "install/include/openssl/rc5.h",
    "install/include/openssl/srp.h",
    "install/include/openssl/aes.h",
    "install/include/openssl/cmserr.h",
    "install/include/openssl/mdc2.h",
    "install/include/openssl/asn1_mac.h",
    "install/include/openssl/lhash.h",
    "install/include/openssl/randerr.h",
    "install/include/openssl/dherr.h",
    "install/include/openssl/x509v3.h",
    "install/include/openssl/camellia.h",
    "install/include/openssl/crypto.h",
    "install/include/openssl/obj_mac.h",
    "install/include/openssl/kdferr.h",
    "install/include/openssl/hmac.h",
    "install/include/openssl/ocsp.h",
    "install/include/openssl/asn1err.h",
    "install/include/openssl/idea.h",
    "install/include/openssl/bio.h",
    "install/include/openssl/ssl.h",
    "install/include/openssl/objectserr.h",
    "install/include/openssl/ossl_typ.h",
    "install/include/openssl/cmac.h",
    "install/include/openssl/pem.h",
    "install/include/openssl/ecdsa.h",
    "install/include/openssl/comp.h",
    "install/include/openssl/asn1.h",
    "install/include/openssl/ebcdic.h",
    "install/include/openssl/symhacks.h",
    "install/include/openssl/opensslv.h",
    "install/include/openssl/cast.h",
    "install/include/openssl/bnerr.h",
    "install/include/openssl/pkcs12err.h",
    "install/include/openssl/ssl2.h",
    "install/include/openssl/x509.h",
    "install/include/openssl/e_os2.h",
    "install/include/openssl/comperr.h",
    "install/include/openssl/ui.h",
    "install/include/openssl/ocsperr.h",
    "install/include/openssl/srtp.h",
    "install/include/openssl/md5.h",
    "install/include/openssl/ssl3.h",
    "install/include/openssl/safestack.h",
    "install/include/openssl/stack.h",
    "install/include/openssl/evp.h",
    "install/include/openssl/store.h",
    "install/include/openssl/bn.h",
    "install/include/openssl/pem2.h",
    "install/include/openssl/txt_db.h",
    "install/include/openssl/cms.h",
    "install/include/openssl/conf.h",
    "install/include/openssl/whrlpool.h",
    "install/include/openssl/rsa.h",
    "install/include/openssl/des.h",
    "install/include/openssl/dsa.h",
    "install/include/openssl/evperr.h",
    "install/include/openssl/conferr.h",
    "install/include/openssl/ripemd.h",
    "install/include/openssl/ec.h",
    "install/include/openssl/dsaerr.h",
    "install/include/openssl/storeerr.h",
    "install/include/openssl/x509v3err.h",
    "install/include/openssl/objects.h",
    "install/include/openssl/engineerr.h",
    "install/include/openssl/rc4.h",
    "install/include/openssl/cryptoerr.h",
    "install/include/openssl/ct.h",
    "install/include/openssl/async.h",
    "install/include/openssl/dtls1.h",
    "install/include/openssl/asn1t.h",
    "install/include/openssl/uierr.h",
    "install/include/openssl/rsaerr.h",
]

genrule(
    name = "build",
    srcs = glob(["**"], exclude=["install/**"]),
    outs = [
        "install/lib/libssl.so.1.1",
        "install/lib/libcrypto.so",
        "install/lib/engines-1.1/afalg.so",
        "install/lib/engines-1.1/padlock.so",
        "install/lib/engines-1.1/capi.so",
        "install/lib/libssl.so",
        "install/lib/pkgconfig/openssl.pc",
        "install/lib/pkgconfig/libcrypto.pc",
        "install/lib/pkgconfig/libssl.pc",
        "install/lib/libcrypto.so.1.1",
        "install/lib/libssl.a",
        "install/lib/libcrypto.a",
    ] + openssl_headers,
    cmd = """
    ROOT=$$(dirname $(execpath NOTES.ANDROID))
    INSTALL_ROOT=$$(dirname $$(dirname $$(dirname $(location install/lib/libssl.a))))
    pushd $$ROOT
    ./config --prefix=$$(pwd)/install no-tests > /dev/null
    make -j$$(nproc) > /dev/null
    make install > /dev/null

    # Move compiled libraries to the expected destinations.
    popd
    mkdir -p $$INSTALL_ROOT/install
    cp -rf $$ROOT/install $$INSTALL_ROOT/
    """,
    message = "Installing OpenSSL libraries and headers",
)

cc_library(
    name = "openssl",
    srcs = [
        "install/lib/libssl.a",
        "install/lib/libcrypto.a",
    ] + openssl_headers,
    hdrs = glob(["install/include/openssl/*.h"]),
    includes = ["install/include"],
    visibility = ["//visibility:public"],
)
