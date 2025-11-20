#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

#include "ccapi_cpp/ccapi_util_private.h"

namespace ccapi {
struct Secp256k1SignatureHex {
  std::string r;
  std::string s;
  int recoveryId;
};

namespace detail {
template <class T, void (*Deleter)(T*)>
using OpenSslPtr = std::unique_ptr<T, decltype(Deleter)>;

using BignumPtr = OpenSslPtr<BIGNUM, BN_free>;
using EcKeyPtr = OpenSslPtr<EC_KEY, EC_KEY_free>;
using EcPointPtr = OpenSslPtr<EC_POINT, EC_POINT_free>;
using EcdsaSigPtr = OpenSslPtr<ECDSA_SIG, ECDSA_SIG_free>;
using BnCtxPtr = OpenSslPtr<BN_CTX, BN_CTX_free>;

struct OpenSslCharDeleter {
  void operator()(char* ptr) const {
    if (ptr) {
      OPENSSL_free(ptr);
    }
  }
};

inline std::string normalizeHex(const std::string& hex) {
  auto lowered = hex;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
  return "0x" + UtilString::leftPadTo(lowered, 64, '0');
}
}  // namespace detail

inline Secp256k1SignatureHex signDigestSecp256k1(const std::vector<uint8_t>& digest, const std::string& privateKeyHex) {
  if (digest.size() != 32) {
    throw std::runtime_error("secp256k1 digest must be 32 bytes");
  }
  std::string privateKeyBytes = UtilAlgorithm::hexToString(privateKeyHex);
  if (privateKeyBytes.size() != 32) {
    throw std::runtime_error("secp256k1 private key must be 32 bytes");
  }

  detail::BnCtxPtr ctx(BN_CTX_new(), BN_CTX_free);
  if (!ctx) {
    throw std::runtime_error("BN_CTX_new failed");
  }

  detail::BignumPtr priv(BN_bin2bn(reinterpret_cast<const unsigned char*>(privateKeyBytes.data()), 32, nullptr), BN_free);
  if (!priv) {
    throw std::runtime_error("BN_bin2bn failed for private key");
  }

  detail::EcKeyPtr key(EC_KEY_new_by_curve_name(NID_secp256k1), EC_KEY_free);
  if (!key) {
    throw std::runtime_error("EC_KEY_new_by_curve_name failed");
  }
  if (EC_KEY_set_private_key(key.get(), priv.get()) != 1) {
    throw std::runtime_error("EC_KEY_set_private_key failed");
  }

  const EC_GROUP* group = EC_KEY_get0_group(key.get());
  detail::EcPointPtr pub(EC_POINT_new(group), EC_POINT_free);
  if (!pub) {
    throw std::runtime_error("EC_POINT_new failed");
  }
  if (EC_POINT_mul(group, pub.get(), priv.get(), nullptr, nullptr, ctx.get()) != 1) {
    throw std::runtime_error("EC_POINT_mul failed for public key");
  }
  if (EC_KEY_set_public_key(key.get(), pub.get()) != 1) {
    throw std::runtime_error("EC_KEY_set_public_key failed");
  }

  detail::EcdsaSigPtr sig(ECDSA_do_sign(digest.data(), static_cast<int>(digest.size()), key.get()), ECDSA_SIG_free);
  if (!sig) {
    throw std::runtime_error("ECDSA_do_sign failed");
  }

  const BIGNUM* r_sig;
  const BIGNUM* s_sig;
  ECDSA_SIG_get0(sig.get(), &r_sig, &s_sig);
  detail::BignumPtr r(BN_dup(r_sig), BN_free);
  detail::BignumPtr s(BN_dup(s_sig), BN_free);
  if (!r || !s) {
    throw std::runtime_error("BN_dup failed for signature components");
  }

  detail::BignumPtr order(BN_new(), BN_free);
  if (!order || EC_GROUP_get_order(group, order.get(), ctx.get()) != 1) {
    throw std::runtime_error("EC_GROUP_get_order failed");
  }
  detail::BignumPtr z(BN_bin2bn(digest.data(), static_cast<int>(digest.size()), nullptr), BN_free);
  if (!z) {
    throw std::runtime_error("BN_bin2bn failed for digest");
  }

  int recoveryId = -1;
  for (int i = 0; i < 4 && recoveryId == -1; ++i) {
    detail::EcPointPtr R(EC_POINT_new(group), EC_POINT_free);
    detail::BignumPtr x(BN_dup(r.get()), BN_free);
    if (!R || !x) {
      throw std::runtime_error("Failed to allocate recovery helpers");
    }
    if (i >= 2) {
      if (BN_add(x.get(), x.get(), order.get()) != 1) {
        throw std::runtime_error("BN_add failed while computing recovery id");
      }
    }
    if (!EC_POINT_set_compressed_coordinates_GFp(group, R.get(), x.get(), i % 2, ctx.get())) {
      continue;
    }

    detail::BignumPtr rInv(BN_mod_inverse(nullptr, r.get(), order.get(), ctx.get()), BN_free);
    if (!rInv) {
      throw std::runtime_error("BN_mod_inverse failed");
    }
    detail::BignumPtr u1(BN_new(), BN_free);
    detail::BignumPtr u2(BN_new(), BN_free);
    if (!u1 || !u2) {
      throw std::runtime_error("BN_new failed while computing recovery id");
    }
    if (BN_mod_mul(u1.get(), z.get(), rInv.get(), order.get(), ctx.get()) != 1 ||
        BN_mod_mul(u2.get(), s.get(), rInv.get(), order.get(), ctx.get()) != 1) {
      throw std::runtime_error("BN_mod_mul failed while computing recovery id");
    }
    detail::BignumPtr negU1(BN_new(), BN_free);
    if (!negU1 || BN_mod_sub(negU1.get(), order.get(), u1.get(), order.get(), ctx.get()) != 1) {
      throw std::runtime_error("BN_mod_sub failed while computing recovery id");
    }

    detail::EcPointPtr candidate(EC_POINT_new(group), EC_POINT_free);
    if (!candidate) {
      throw std::runtime_error("EC_POINT_new failed for recovery candidate");
    }
    if (EC_POINT_mul(group, candidate.get(), negU1.get(), R.get(), u2.get(), ctx.get()) != 1) {
      continue;
    }
    if (EC_POINT_cmp(group, candidate.get(), pub.get(), ctx.get()) == 0) {
      recoveryId = i;
      break;
    }
  }

  if (recoveryId == -1) {
    throw std::runtime_error("Could not recover ECDSA public key");
  }

  std::unique_ptr<char, detail::OpenSslCharDeleter> r_hex(BN_bn2hex(r.get()));
  std::unique_ptr<char, detail::OpenSslCharDeleter> s_hex(BN_bn2hex(s.get()));
  if (!r_hex || !s_hex) {
    throw std::runtime_error("BN_bn2hex failed");
  }

  Secp256k1SignatureHex signature;
  signature.r = detail::normalizeHex(r_hex.get());
  signature.s = detail::normalizeHex(s_hex.get());
  signature.recoveryId = recoveryId + 27;
  return signature;
}
}  // namespace ccapi

