/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Crypto/Rc4.h"
#include "Base/Crypto/Sha1.h"
#include "Base/Types.h"

#include <algorithm>
#include <array>
#include <span>

// What the 1BL and the bootloaders after it do to decrypt the stage that follows. A stage carries a 0x10 byte nonce
// after its header, and the RC4 key is the first 16 bytes of HMAC-SHA1(secret, nonce).

namespace Xe::BootCrypto {

  // The 1BL ROM's key for CB_A and the CF patches. It is public, and the same on every console.
  inline constexpr std::array<u8, 16> k1blKey = {0xDD, 0x88, 0xAD, 0x0C, 0x9E, 0xD6, 0x69, 0xE7,
                                                 0xB5, 0x67, 0x94, 0xFB, 0x68, 0x56, 0x3E, 0xFA};

  // What the earliest S2 developer bootloaders are decrypted with instead.
  inline constexpr std::array<u8, 16> kZeroKey{};

  // The RC4 key for a stage: HMAC-SHA1(secret, nonce), of which the first 16 bytes count.
  inline Base::Crypto::Sha1::Digest DeriveKey(std::span<const u8> secret, std::span<const u8> nonce) {
    return Base::Crypto::HmacSha1(secret, {nonce});
  }

  // Decrypts a stage in place the way the 1BL does: the key derived from the nonce at 'nonceOffset' replaces the nonce,
  // and everything after it is RC4 decrypted with that key.
  inline void DecryptBootloader(std::span<const u8> secret, std::span<u8> stage, u32 nonceOffset) {
    constexpr u32 kNonceSize = 0x10;
    if (stage.size() < nonceOffset + kNonceSize) { return; }
    const Base::Crypto::Sha1::Digest key = DeriveKey(secret, {stage.data() + nonceOffset, kNonceSize});
    std::copy_n(key.begin(), kNonceSize, stage.begin() + nonceOffset);
    Base::Crypto::Rc4 rc4(std::span<const u8>(stage.data() + nonceOffset, kNonceSize));
    rc4.Process(stage.subspan(nonceOffset + kNonceSize));
  }

} // namespace Xe::BootCrypto
