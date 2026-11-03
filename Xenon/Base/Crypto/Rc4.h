/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <span>
#include <utility>

// RC4 stream cipher, used by the keyvault encryption.

namespace Base::Crypto {

  class Rc4 {
  public:
    explicit Rc4(std::span<const u8> key) {
      for (u32 i = 0; i < 256; ++i) { s[i] = static_cast<u8>(i); }
      u8 j = 0;
      for (u32 i = 0; i < 256; ++i) {
        j = static_cast<u8>(j + s[i] + key[i % key.size()]);
        std::swap(s[i], s[j]);
      }
    }

    // Encrypts or decrypts data in place.
    void Process(std::span<u8> data) {
      for (u8 &byte : data) {
        x = static_cast<u8>(x + 1);
        y = static_cast<u8>(y + s[x]);
        std::swap(s[x], s[y]);
        byte ^= s[static_cast<u8>(s[x] + s[y])];
      }
    }

  private:
    u8 s[256] = {};
    u8 x = 0;
    u8 y = 0;
  };

} // namespace Base::Crypto
