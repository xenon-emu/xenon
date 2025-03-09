// Copyright 2025 Xenon Emulator Project

#pragma once
#include <type_traits>

namespace Base {

template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, size_t size) {
  static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
  auto mod{static_cast<T>(value % size)};
  value -= mod;
  return static_cast<T>(mod == T{0} ? value : value + size);
}

template <typename T>
[[nodiscard]] constexpr T AlignDown(T value, size_t size) {
  static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
  return static_cast<T>(value - value % size);
}

template <typename T>
  requires std::is_integral_v<T>
[[nodiscard]] constexpr bool IsAligned(T value, size_t alignment) {
  return (value & (alignment - 1)) == 0;
}

template <typename T>
  requires std::is_integral_v<T>
[[nodiscard]] constexpr bool Is16KBAligned(T value) {
  return (value & 0x3FFF) == 0;
}

template <typename T>
  requires std::is_integral_v<T>
[[nodiscard]] constexpr bool Is64KBAligned(T value) {
  return (value & 0xFFFF) == 0;
}

template <typename T>
  requires std::is_integral_v<T>
[[nodiscard]] constexpr bool Is2MBAligned(T value) {
  return (value & 0x1FFFFF) == 0;
}

} // namespace Base
