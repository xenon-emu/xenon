/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <span>

// DES and two-key triple DES (EDE, K1 K2 K1). Parity bits of the keys are ignored, as the algorithm itself does.

namespace Base::Crypto {

  class Des {
  public:
    static constexpr size_t BlockSize = 8;

    explicit Des(const u8 *key8);

    void EncryptBlock(const u8 *in, u8 *out) const { Crypt(in, out, false); }
    void DecryptBlock(const u8 *in, u8 *out) const { Crypt(in, out, true); }

  private:
    void Crypt(const u8 *in, u8 *out, bool decrypt) const;

    u64 subkeys[16] = {}; // 48-bit round keys
  };

  class Des3 {
  public:
    // 16-byte key: K1 = key[0..8), K2 = key[8..16).
    explicit Des3(const u8 *key16)
        : k1(key16)
        , k2(key16 + 8) {}

    void EncryptBlock(const u8 *in, u8 *out) const;
    void DecryptBlock(const u8 *in, u8 *out) const;

    // CBC over whole blocks of data, in place. iv ends up as the last ciphertext block.
    void CbcEncrypt(std::span<u8> data, u8 *iv) const;
    void CbcDecrypt(std::span<u8> data, u8 *iv) const;

  private:
    Des k1;
    Des k2;
  };

  // Sets the odd parity bit of every key byte.
  void DesParity(std::span<u8> key);

} // namespace Base::Crypto
