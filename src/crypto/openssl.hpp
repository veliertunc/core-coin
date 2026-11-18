#pragma once
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <memory>

namespace corecoin::__detail {
// RAII owners for OpenSSL objects used by the crypto component.
template<class T, auto Free> using OpenSslHandle = std::unique_ptr<T, decltype(Free)>;
using PKey = OpenSslHandle<EVP_PKEY, EVP_PKEY_free>;
using PKeyContext = OpenSslHandle<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;
using Bio = OpenSslHandle<BIO, BIO_free>;
using DigestContext = OpenSslHandle<EVP_MD_CTX, EVP_MD_CTX_free>;
using BigNum = OpenSslHandle<BIGNUM, BN_free>;
using EcdsaSig = OpenSslHandle<ECDSA_SIG, ECDSA_SIG_free>;
using ParamBuilder = OpenSslHandle<OSSL_PARAM_BLD, OSSL_PARAM_BLD_free>;
using Params = OpenSslHandle<OSSL_PARAM, OSSL_PARAM_free>;
}
