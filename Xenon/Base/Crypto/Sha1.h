/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <array>
#include <initializer_list>
#include <span>

// SHA-1 and HMAC-SHA1.

namespace Base::Crypto {

  class Sha1 {
  public:
    static constexpr size_t DigestSize = 20;
    static constexpr size_t BlockSize = 64;
    using Digest = std::array<u8, DigestSize>;

    Sha1() { Reset(); }

    void Reset();
    void Update(std::span<const u8> data);
    Digest Final();

    static Digest Hash(std::span<const u8> data) {
      Sha1 sha;
      sha.Update(data);
      return sha.Final();
    }

  private:
    void Transform(const u8 *block);

    u32 state[5] = {};
    u64 length = 0; // bytes hashed so far
    u8 buffer[BlockSize] = {};
    size_t bufferUsed = 0;
  };

  // HMAC-SHA1 over the concatenation of parts, which saves callers joining the pieces of a message first.
  Sha1::Digest HmacSha1(std::span<const u8> key, std::initializer_list<std::span<const u8>> parts);

} // namespace Base::Crypto
