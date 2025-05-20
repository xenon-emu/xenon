// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <atomic>
#include <bit>
#include <type_traits>
#ifndef TOOL
#include <fmt/format.h>
#else
#include <format>
#endif

#include "Arch.h"

#ifndef TOOL
#define FMT(...) fmt::format(__VA_ARGS__)
#else
#define FMT(...) std::format(__VA_ARGS__)
#endif

// Vali0004: Helper macro to make me sane when doing RAII
#define STRIP_UNIQUE(x) std::remove_pointer_t<decltype(x.get())>
#define STRIP_UNIQUE_ARR(x) std::remove_pointer_t<decltype(x.get())>[]

// Compile time macros to get endianess
#ifdef _MSC_VER
#define __LITTLE_ENDIAN__ 1
#else
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__ 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define __BIG_ENDIAN__ 1
#endif
#endif

// Signed
using s8 = signed char;
using s16 = signed short;
using s32 = signed int;
using sl32 = signed long;
using s64 = signed long long;

// Unsigned
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using ul32 = unsigned long;
using u64 = unsigned long long;

using uptr = uintptr_t;

// Floating point
using f32 = float;
using f64 = double;

// UDLs for memory size values
[[nodiscard]] constexpr u64 operator""_KB(const u64 x) {
  return 1024ULL * x;
}
[[nodiscard]] constexpr u64 operator""_MB(const u64 x) {
  return 1024_KB * x;
}
[[nodiscard]] constexpr u64 operator""_GB(const u64 x) {
  return 1024_MB * x;
}

// Byteswap to BE
template <class T>
  requires std::is_integral_v<T>
[[nodiscard]] constexpr T byteswap_be(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return std::byteswap<T>(value);
  }

  return value;
}

// Byteswap to LE
template <class T>
  requires std::is_integral_v<T>
[[nodiscard]] constexpr T byteswap_le(T value) noexcept {
  if constexpr (std::endian::native == std::endian::big) {
    return std::byteswap<T>(value);
  }

  return value;
}

// Min/max value of a typetemplate <typename T>
template <typename T>
[[nodiscard]] consteval T min_t() noexcept {
  if constexpr (std::is_unsigned_v<T>) {
    return 0;
  } else if constexpr (std::is_same_v<T, s64>) {
    return (-0x7FFFFFFFFFFFFFFF - 1);
  } else if constexpr (std::is_same_v<T, s32> || std::is_same_v<T, sl32>) {
    return (-0x7FFFFFFF - 1);
  } else if constexpr (std::is_same_v<T, s16>) {
    return (-0x7FFF - 1);
  } else if constexpr (std::is_same_v<T, s8>) {
    return (-0x7F - 1);
  }
}

template <typename T>
class min {
public:
  static constexpr T value = min_t<T>();
};

template <typename T>
constexpr T min_v = min<T>::value;
template <typename T>
[[nodiscard]] consteval T max_t() noexcept {
  if constexpr (std::is_same_v<T, u64>) {
    return 0xFFFFFFFFFFFFFFFFu;
  } else if constexpr (std::is_same_v<T, s64>) {
    return 0x7FFFFFFFFFFFFFFF;
  } else if constexpr (std::is_same_v<T, u32> || std::is_same_v<T, ul32>) {
    return 0xFFFFFFFF;
  } else if constexpr (std::is_same_v<T, s32> || std::is_same_v<T, sl32>) {
    return 0x7FFFFFFF;
  } else if constexpr (std::is_same_v<T, u16>) {
    return 0xFFFF;
  } else if constexpr (std::is_same_v<T, s16>) {
    return 0x7FFF;
  } else if constexpr (std::is_same_v<T, u8>) {
    return 0xFF;
  } else if constexpr (std::is_same_v<T, s8>) {
    return 0x7F;
  }
}

template <typename T>
class max {
public:
  static constexpr T value = max_t<T>();
};

template <typename T>
constexpr T max_v = max<T>::value;

extern void throw_fail_debug_msg(const std::string& msg);

// Array accessors
template <typename cT, typename T>
  requires requires (cT&& x) { std::size(x); std::data(x); } || requires (cT && x) {  std::size(x); x.front(); }
[[nodiscard]] constexpr auto& c_at(cT&& c, T&& idx) {
  // Associative container
  size_t cSize = c.size();
  if (cSize <= idx) [[unlikely]] {
#ifndef TOOL
    throw_fail_debug_msg(fmt::format("Range check failed! (index: {}{})", idx, cSize != max_v<size_t> ? fmt::format(", size: {}", cSize) : ""));
#else
    throw_fail_debug_msg(std::format("Range check failed! (index: {}{})", idx, cSize != max_v<size_t> ? std::format(", size: {}", cSize) : ""));
#endif
  }
  auto it = std::begin(std::forward<cT>(c));
  std::advance(it, idx);
  return *it;
}

template <typename cT, typename T>
  requires requires(cT&& x, T&& y) { x.count(y); x.find(y); }
[[nodiscard]] static constexpr auto& c_at(cT&& c, T&& idx) {
  // Associative container
  const auto found = c.find(std::forward<T>(idx));
  size_t cSize = max_v<size_t>;
  if constexpr ((requires() { c.size(); }))
    cSize = c.size();
  if (found == c.end()) [[unlikely]] {
    throw_fail_debug_msg(FMT("Range check failed! (index: {}{})", idx, cSize != max_v<size_t> ? FMT(", size: {}", cSize) : ""));
  }
  return found->second;
}

#if defined(_MSC_VER) || defined(__MINGW32__)
#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include <xmmintrin.h>

#if defined(__clang__) && (__clang_major__ >= 20)
static inline constexpr u8 _addcarry_u64(u8, u64, u64, u64*);
static inline constexpr u8 _subborrow_u64(u8, u64, u64, u64*);

extern "C" {
  u64 __shiftleft128(u64, u64, u8);
  u64 __shiftright128(u64, u64, u8);
  u64 _umul128(u64, u64, u64*);
}
#else
extern "C" {
  u8 _addcarry_u64(u8, u64, u64, u64*);
  u8 _subborrow_u64(u8, u64, u64, u64*);
  u64 __shiftleft128(u64, u64, u8);
  u64 __shiftright128(u64, u64, u8);
  u64 _umul128(u64, u64, u64*);
}
#endif // ifdef __clang__ && __clang_major__ >= 20

// Unsigned 128-bit integer implementation.
class alignas(16) u128 {
public:
  u128() noexcept = default;

  template <typename T, std::enable_if_t<std::is_unsigned_v<T>, u64> = 0>
  constexpr u128(T arg) noexcept :
    lo(arg), hi(0)
  {}

  template <typename T, std::enable_if_t<std::is_signed_v<T>, s64> = 0>
  constexpr u128(T arg) noexcept :
    lo(s64{ arg }), hi(s64{ arg } >> 63)
  {}

  constexpr explicit operator bool() const noexcept {
    return !!(lo | hi);
  }
  constexpr explicit operator u64() const noexcept {
    return lo;
  }
  constexpr explicit operator s64() const noexcept {
    return lo;
  }

  constexpr friend bool operator==(const u128 &l, const u128 &r) {
    return l.lo == r.lo && l.hi == r.hi;
  }

  constexpr friend bool operator!=(const u128 &l, const u128 &r) {
    return l.lo != r.lo && l.hi != r.hi;
  }

  constexpr friend u128 operator+(const u128 &l, const u128 &r) {
    u128 value = l;
    value += r;
    return value;
  }

  constexpr friend u128 operator-(const u128 &l, const u128 &r) {
    u128 value = l;
    value -= r;
    return value;
  }
  constexpr friend u128 operator*(const u128 &l, const u128 &r) {
    u128 value = l;
    value *= r;
    return value;
  }

  constexpr u128 operator+() const {
    return *this;
  }
  constexpr u128 operator-() const {
    u128 value{};
    value -= *this;
    return value;
  }
  constexpr u128& operator++() {
    *this += 1;
    return *this;
  }
  constexpr u128 operator++(int) {
    u128 value = *this;
    *this += 1;
    return value;
  }
  constexpr u128& operator--() {
    *this -= 1;
    return *this;
  }
  constexpr u128 operator--(int) {
    u128 value = *this;
    *this -= 1;
    return value;
  }

  constexpr u128 operator<<(u128 shift_value) const {
    u128 value = *this;
    value <<= shift_value;
    return value;
  }
  constexpr u128 operator>>(u128 shift_value) const {
    u128 value = *this;
    value >>= shift_value;
    return value;
  }
  constexpr u128 operator~() const {
    u128 value{};
    value.lo = ~lo;
    value.hi = ~hi;
    return value;
  }
  constexpr friend u128 operator&(const u128 &l, const u128 &r) {
    u128 value{};
    value.lo = l.lo & r.lo;
    value.hi = l.hi & r.hi;
    return value;
  }
  constexpr friend u128 operator|(const u128 &l, const u128 &r) {
    u128 value{};
    value.lo = l.lo | r.lo;
    value.hi = l.hi | r.hi;
    return value;
  }
  constexpr friend u128 operator^(const u128 &l, const u128 &r) {
    u128 value{};
    value.lo = l.lo ^ r.lo;
    value.hi = l.hi ^ r.hi;
    return value;
  }

  constexpr u128& operator+=(const u128 &r) {
    if (std::is_constant_evaluated()) {
      lo += r.lo;
      hi += r.hi + (lo < r.lo);
    }
    else {
      _addcarry_u64(_addcarry_u64(0, r.lo, lo, &lo), r.hi, hi, &hi);
    }

    return *this;
  }
  constexpr u128& operator-=(const u128 &r) {
    if (std::is_constant_evaluated()) {
      hi -= r.hi + (lo < r.lo);
      lo -= r.lo;
    }
    else {
      _subborrow_u64(_subborrow_u64(0, lo, r.lo, &lo), hi, r.hi, &hi);
    }

    return *this;
  }
  constexpr u128& operator*=(const u128 &r) {
    const u64 _hi = r.hi * lo + r.lo * hi;
    if (std::is_constant_evaluated()) {
      hi = (lo >> 32) * (r.lo >> 32) + (((lo >> 32) * (r.lo & 0xffffffff)) >> 32) + (((r.lo >> 32) * (lo & 0xffffffff)) >> 32);
      lo = lo * r.lo;
    }
    else {
      lo = _umul128(lo, r.lo, &hi);
    }

    hi += _hi;
    return *this;
  }

  constexpr u128& operator<<=(const u128 &r) {
    if (std::is_constant_evaluated()) {
      if (r.hi == 0 && r.lo < 64) {
        hi = (hi << r.lo) | (lo >> (64 - r.lo));
        lo = (lo << r.lo);
        return *this;
      }
      else if (r.hi == 0 && r.lo < 128) {
        hi = (lo << (r.lo - 64));
        lo = 0;
        return *this;
      }
    }
    const u64 v0 = lo << (r.lo & 63);
    const u64 v1 = __shiftleft128(lo, hi, static_cast<u8>(r.lo));

    lo = (r.lo & 64) ? 0 : v0;
    hi = (r.lo & 64) ? v0 : v1;
    return *this;
  }
  constexpr u128& operator>>=(const u128 &r) {
    if (std::is_constant_evaluated()) {
      if (r.hi == 0 && r.lo < 64) {
        lo = (lo >> r.lo) | (hi << (64 - r.lo));
        hi = (hi >> r.lo);
        return *this;
      }
      else if (r.hi == 0 && r.lo < 128) {
        lo = (hi >> (r.lo - 64));
        hi = 0;
        return *this;
      }
    }
    const u64 v0 = hi >> (r.lo & 63);
    const u64 v1 = __shiftright128(lo, hi, static_cast<u8>(r.lo));

    lo = (r.lo & 64) ? v0 : v1;
    hi = (r.lo & 64) ? 0 : v0;
    return *this;
  }
  constexpr u128& operator&=(const u128 &r) {
    lo &= r.lo;
    hi &= r.hi;
    return *this;
  }
  constexpr u128& operator|=(const u128 &r) {
    lo |= r.lo;
    hi |= r.hi;
    return *this;
  }
  constexpr u128& operator^=(const u128 &r) {
    lo ^= r.lo;
    hi ^= r.hi;
    return *this;
  }

  u64 lo, hi;
};

// Signed 128-bit integer implementation
class s128 : public u128 {
public:
  using u128::u128;

  constexpr s128 operator>>(u128 shift_value) const {
    s128 value = *this;
    value >>= shift_value;
    return value;
  }
  constexpr s128& operator>>=(const u128 &r) {
    if (std::is_constant_evaluated()) {
      if (r.hi == 0 && r.lo < 64) {
        lo = (lo >> r.lo) | (hi << (64 - r.lo));
        hi = (static_cast<s64>(hi) >> r.lo);
        return *this;
      }
      else if (r.hi == 0 && r.lo < 128) {
        const s64 _lo = static_cast<s64>(hi) >> (r.lo - 64);
        lo = _lo;
        hi = _lo >> 63;
        return *this;
      }
    }

    const u64 v0 = static_cast<s64>(hi) >> (r.lo & 63);
    const u64 v1 = __shiftright128(lo, hi, static_cast<u8>(r.lo));
    lo = (r.lo & 64) ? v0 : v1;
    hi = (r.lo & 64) ? static_cast<s64>(hi) >> 63 : v0;
    return *this;
  }
};
#endif // ifdef ARCH_X86 || ARCH_X86_64
#else
using u128 = __uint128_t;
using s128 = __int128_t;
#endif // ifdef _MSC_VER || __MINGW32__
