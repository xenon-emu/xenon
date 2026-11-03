/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Sha1.h"

#include <algorithm>
#include <bit>
#include <cstring>

namespace Base::Crypto {

  void Sha1::Reset() {
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    state[4] = 0xC3D2E1F0;
    length = 0;
    bufferUsed = 0;
  }

  void Sha1::Transform(const u8 *block) {
    u32 w[80];
    for (u32 i = 0; i < 16; ++i) {
      w[i] = (static_cast<u32>(block[i * 4]) << 24) | (static_cast<u32>(block[i * 4 + 1]) << 16)
             | (static_cast<u32>(block[i * 4 + 2]) << 8) | block[i * 4 + 3];
    }
    for (u32 i = 16; i < 80; ++i) { w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1); }

    u32 a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (u32 i = 0; i < 80; ++i) {
      u32 f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const u32 t = std::rotl(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = std::rotl(b, 30);
      b = a;
      a = t;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
  }

  void Sha1::Update(std::span<const u8> data) {
    length += data.size();
    size_t pos = 0;
    if (bufferUsed) {
      const size_t take = std::min(BlockSize - bufferUsed, data.size());
      std::memcpy(buffer + bufferUsed, data.data(), take);
      bufferUsed += take;
      pos = take;
      if (bufferUsed < BlockSize) { return; }
      Transform(buffer);
      bufferUsed = 0;
    }
    for (; pos + BlockSize <= data.size(); pos += BlockSize) { Transform(data.data() + pos); }
    if (pos < data.size()) {
      bufferUsed = data.size() - pos;
      std::memcpy(buffer, data.data() + pos, bufferUsed);
    }
  }

  Sha1::Digest Sha1::Final() {
    const u64 bits = length * 8;
    static constexpr u8 pad[BlockSize] = {0x80};
    Update({pad, 1 + ((BlockSize * 2 - 9 - bufferUsed) % BlockSize)});
    u8 lengthBytes[8];
    for (u32 i = 0; i < 8; ++i) { lengthBytes[i] = static_cast<u8>(bits >> (56 - i * 8)); }
    Update(lengthBytes);

    Digest digest;
    for (u32 i = 0; i < 5; ++i) {
      digest[i * 4] = static_cast<u8>(state[i] >> 24);
      digest[i * 4 + 1] = static_cast<u8>(state[i] >> 16);
      digest[i * 4 + 2] = static_cast<u8>(state[i] >> 8);
      digest[i * 4 + 3] = static_cast<u8>(state[i]);
    }
    Reset();
    return digest;
  }

  Sha1::Digest HmacSha1(std::span<const u8> key, std::initializer_list<std::span<const u8>> parts) {
    u8 block[Sha1::BlockSize] = {};
    if (key.size() > Sha1::BlockSize) {
      const Sha1::Digest hashed = Sha1::Hash(key);
      std::memcpy(block, hashed.data(), hashed.size());
    } else {
      std::memcpy(block, key.data(), key.size());
    }

    u8 pad[Sha1::BlockSize];
    for (size_t i = 0; i < Sha1::BlockSize; ++i) { pad[i] = block[i] ^ 0x36; }
    Sha1 inner;
    inner.Update(pad);
    for (const auto &part : parts) { inner.Update(part); }
    const Sha1::Digest innerDigest = inner.Final();

    for (size_t i = 0; i < Sha1::BlockSize; ++i) { pad[i] = block[i] ^ 0x5C; }
    Sha1 outer;
    outer.Update(pad);
    outer.Update(innerDigest);
    return outer.Final();
  }

} // namespace Base::Crypto
