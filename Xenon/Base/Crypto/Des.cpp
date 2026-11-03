/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Des.h"

#include <bit>
#include <cstring>

namespace Base::Crypto {

  // FIPS 46-3 tables, bit numbers 1-based from the most significant bit.
  constexpr u8 kIP[64] = {58, 50, 42, 34, 26, 18, 10, 2,  60, 52, 44, 36, 28, 20, 12, 4,  62, 54, 46, 38, 30, 22,
                          14, 6,  64, 56, 48, 40, 32, 24, 16, 8,  57, 49, 41, 33, 25, 17, 9,  1,  59, 51, 43, 35,
                          27, 19, 11, 3,  61, 53, 45, 37, 29, 21, 13, 5,  63, 55, 47, 39, 31, 23, 15, 7};
  constexpr u8 kFP[64] = {40, 8,  48, 16, 56, 24, 64, 32, 39, 7,  47, 15, 55, 23, 63, 31, 38, 6,  46, 14, 54, 22,
                          62, 30, 37, 5,  45, 13, 53, 21, 61, 29, 36, 4,  44, 12, 52, 20, 60, 28, 35, 3,  43, 11,
                          51, 19, 59, 27, 34, 2,  42, 10, 50, 18, 58, 26, 33, 1,  41, 9,  49, 17, 57, 25};
  constexpr u8 kE[48] = {32, 1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,  8,  9,  10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
                         16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1};
  constexpr u8 kP[32] = {16, 7, 20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,
                         2,  8, 24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25};
  constexpr u8 kPC1[56] = {57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42, 34, 26, 18, 10, 2,  59, 51, 43,
                           35, 27, 19, 11, 3,  60, 52, 44, 36, 63, 55, 47, 39, 31, 23, 15, 7,  62, 54,
                           46, 38, 30, 22, 14, 6,  61, 53, 45, 37, 29, 21, 13, 5,  28, 20, 12, 4};
  constexpr u8 kPC2[48] = {14, 17, 11, 24, 1,  5,  3,  28, 15, 6,  21, 10, 23, 19, 12, 4,
                           26, 8,  16, 7,  27, 20, 13, 2,  41, 52, 31, 37, 47, 55, 30, 40,
                           51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32};
  constexpr u8 kShifts[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};
  // clang-format off
  constexpr u8 kS[8][64] = {
    {14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
      0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
      4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
     15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13},
    {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
      3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
      0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
     13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9},
    {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
     13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
     13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
      1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12},
    { 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
     13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
     10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
      3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14},
    { 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
     14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
      4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
     11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3},
    {12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
     10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
      9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
      4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13},
    { 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
     13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
      1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
      6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12},
    {13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
      1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
      7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
      2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11}};
  // clang-format on

  // Picks bits table (1-based from the MSB of an inBits-wide value) into a count-bit result.
  static u64 Permute(u64 in, const u8 *table, u32 count, u32 inBits) {
    u64 out = 0;
    for (u32 i = 0; i < count; ++i) { out = (out << 1) | ((in >> (inBits - table[i])) & 1); }
    return out;
  }

  static u64 Load64(const u8 *p) {
    u64 v = 0;
    for (u32 i = 0; i < 8; ++i) { v = (v << 8) | p[i]; }
    return v;
  }

  static void Store64(u64 v, u8 *p) {
    for (u32 i = 0; i < 8; ++i) { p[i] = static_cast<u8>(v >> (56 - i * 8)); }
  }

  static u32 Feistel(u32 r, u64 subkey) {
    const u64 x = Permute(r, kE, 48, 32) ^ subkey;
    u32 s = 0;
    for (u32 i = 0; i < 8; ++i) {
      const u32 six = static_cast<u32>(x >> (42 - i * 6)) & 0x3F;
      const u32 row = ((six >> 4) & 2) | (six & 1);
      const u32 col = (six >> 1) & 0xF;
      s = (s << 4) | kS[i][row * 16 + col];
    }
    return static_cast<u32>(Permute(s, kP, 32, 32));
  }

  Des::Des(const u8 *key8) {
    const u64 cd = Permute(Load64(key8), kPC1, 56, 64);
    u32 c = static_cast<u32>(cd >> 28) & 0xFFFFFFF;
    u32 d = static_cast<u32>(cd) & 0xFFFFFFF;
    for (u32 round = 0; round < 16; ++round) {
      for (u32 s = 0; s < kShifts[round]; ++s) {
        c = ((c << 1) | (c >> 27)) & 0xFFFFFFF;
        d = ((d << 1) | (d >> 27)) & 0xFFFFFFF;
      }
      subkeys[round] = Permute((static_cast<u64>(c) << 28) | d, kPC2, 48, 56);
    }
  }

  void Des::Crypt(const u8 *in, u8 *out, bool decrypt) const {
    const u64 block = Permute(Load64(in), kIP, 64, 64);
    u32 l = static_cast<u32>(block >> 32);
    u32 r = static_cast<u32>(block);
    for (u32 round = 0; round < 16; ++round) {
      const u32 next = l ^ Feistel(r, subkeys[decrypt ? 15 - round : round]);
      l = r;
      r = next;
    }
    Store64(Permute((static_cast<u64>(r) << 32) | l, kFP, 64, 64), out);
  }

  void Des3::EncryptBlock(const u8 *in, u8 *out) const {
    u8 tmp[8];
    k1.EncryptBlock(in, tmp);
    k2.DecryptBlock(tmp, tmp);
    k1.EncryptBlock(tmp, out);
  }

  void Des3::DecryptBlock(const u8 *in, u8 *out) const {
    u8 tmp[8];
    k1.DecryptBlock(in, tmp);
    k2.EncryptBlock(tmp, tmp);
    k1.DecryptBlock(tmp, out);
  }

  void Des3::CbcEncrypt(std::span<u8> data, u8 *iv) const {
    for (size_t pos = 0; pos + 8 <= data.size(); pos += 8) {
      u8 *block = data.data() + pos;
      for (u32 i = 0; i < 8; ++i) { block[i] ^= iv[i]; }
      EncryptBlock(block, block);
      std::memcpy(iv, block, 8);
    }
  }

  void Des3::CbcDecrypt(std::span<u8> data, u8 *iv) const {
    for (size_t pos = 0; pos + 8 <= data.size(); pos += 8) {
      u8 *block = data.data() + pos;
      u8 cipher[8];
      std::memcpy(cipher, block, 8);
      DecryptBlock(block, block);
      for (u32 i = 0; i < 8; ++i) { block[i] ^= iv[i]; }
      std::memcpy(iv, cipher, 8);
    }
  }

  void DesParity(std::span<u8> key) {
    for (u8 &b : key) {
      // Odd parity over the 7 key bits plus the parity bit.
      b = static_cast<u8>((b & 0xFE) | ((std::popcount(static_cast<u32>(b & 0xFE)) & 1) ^ 1));
    }
  }

} // namespace Base::Crypto
