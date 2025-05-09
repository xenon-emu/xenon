// Copyright 2025 Xenon Emulator Project

#pragma once

#include <array>

namespace Base {

union alignas(16) VectorUnit {
  u64 qword;
  f64 fp;
  f32 db;
  std::array<u32, 2> dword;
  std::array<s32, 2> dsword;
  std::array<u16, 4> word;
  std::array<s16, 4> sword;
  std::array<u8, 16> bytes;
};

struct alignas(16) Vector128 {
  union {
    struct {
      f32 x;
      f32 y;
      f32 z;
      f32 w;
    };
    struct {
      s32 ix;
      s32 iy;
      s32 iz;
      s32 iw;
    };
    struct {
      u32 ux;
      u32 uy;
      u32 uz;
      u32 uw;
    };
    s128 vsword;
    u128 vuword;
    std::array<u64, 2> qword;
    std::array<s64, 2> qsword;
    std::array<f64, 2> dbl;
    std::array<f32, 4> flt;
    std::array<u32, 4> dword;
    std::array<s32, 4> dsword;
    std::array<u16, 8> word;
    std::array<s16, 8> sword;
    std::array<u8, 16> bytes;
    std::array<VectorUnit, 2> vu;
  };

  constexpr friend bool operator==(const Vector128 &a, const Vector128 &b) {
    return a.vuword == b.vuword;
  }

  constexpr friend bool operator!=(const Vector128 &a, const Vector128 &b) {
    return a.vuword != b.vuword;
  }

  constexpr friend Vector128 operator^(const Vector128 &a, const Vector128 &b) {
    Vector128 result = {};
    result.vuword = a.vuword ^ b.vuword;
    return result;
  }

  constexpr friend Vector128& operator^=(Vector128 &a, const Vector128 &b) {
    a.vuword ^= b.vuword;
    return a;
  }

  constexpr friend Vector128 operator&(const Vector128 &a, const Vector128 &b) {
    Vector128 result = {};
    result.vuword = a.vuword & b.vuword;
    return result;
  }

  constexpr friend Vector128& operator&=(Vector128 &a, const Vector128 &b) {
    a.vuword &= b.vuword;
    return a;
  }

  constexpr friend Vector128 operator|(const Vector128 &a, const Vector128 &b) {
    Vector128 result = {};
    result.vuword = a.vuword | b.vuword;
    return result;
  }

  constexpr friend Vector128& operator|=(Vector128 &a, const Vector128 &b) {
    a.vuword |= b.vuword;
    return a;
  }
  constexpr Vector128 operator~() const {
    Vector128 result = {};
    result.vuword = ~vuword;
    return result;
  }
};

constexpr inline Vector128 Vector128i(u32 src) {
  return Vector128{ .dword = {src, src, src, src} };
}

constexpr inline Vector128 Vector128i(u32 x, u32 y, u32 z, u32 w) {
  return Vector128{ .dword = {x, y, z, w} };
}

constexpr inline Vector128 Vector128q(u64 src) {
  return Vector128{ .qword = {src, src} };
}

constexpr inline Vector128 Vector128q(u64 x, u64 y) {
  return Vector128{ .qword = {x, y} };
}

constexpr inline Vector128 Vector128d(f64 src) {
  return Vector128{ .dbl = {src, src} };
}

constexpr inline Vector128 Vector128d(f64 x, f64 y) {
  return Vector128{ .dbl = {x, y} };
}

constexpr inline Vector128 Vector128f(f32 src) {
  return Vector128{ .flt = {src, src, src, src} };
}

constexpr inline Vector128 Vector128f(f32 x, f32 y, f32 z, f32 w) {
  return Vector128{ .flt = {x, y, z, w} };
}

constexpr inline Vector128 Vector128s(u16 src) {
  return Vector128{ .word = {src, src, src, src, src, src, src, src} };
}

constexpr inline Vector128 Vector128s(
  u16 x0, u16 x1, u16 y0, u16 y1,
  u16 z0, u16 z1, u16 w0, u16 w1
) {
  return Vector128{
    .word = {
      x1, x0,
      y1, y0,
      z1, z0,
      w1, w0
    }
  };
}

constexpr inline Vector128 Vector128b(u8 src) {
  return Vector128{ .bytes = {
    src, src, src, src,
    src, src, src, src,
    src, src, src, src,
    src, src, src, src
  }};
}

constexpr inline Vector128 Vector128b(
  u8 x0, u8 x1, u8 x2, u8 x3,
  u8 y0, u8 y1, u8 y2, u8 y3,
  u8 z0, u8 z1, u8 z2, u8 z3,
  u8 w0, u8 w1, u8 w2, u8 w3
) {
  return Vector128{ .bytes = {
    x3, x2, x1, x0,
    y3, y2, y1, y0,
    z3, z2, z1, z0,
    w3, w2, w1, w0
  }};
}

} // namespace Base