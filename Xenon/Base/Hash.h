// Copyright 2025 Xenon Emulator Project

#pragma once

#include <string_view>

namespace Base {

inline constexpr char jTolower(char const c) {
  return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

inline constexpr u32 joaatStringHash(const std::string_view string, bool const forceLowercase = true) {
  u32 hash = 0;

  const char *str = string.data();
  while (*str != '\0') {
    hash += forceLowercase ? jTolower(*str) : *str, ++str;
    hash += hash << 10;
    hash ^= hash >> 6;
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

inline constexpr u32 joaatDataHash(const char *data, size_t size, const u32 initValue) {
  u32 key = initValue;
  
  for (size_t i = 0; i != size; ++i) {
    key += data[i];
    key += (key << 10);
    key ^= (key >> 6);
  }

  key += (key << 3);
  key ^= (key >> 11);
  key += (key << 15);
  return key;
}

} // namespace Base

inline consteval u32 operator ""_j(const char *data, size_t size) {
  return Base::joaatDataHash(data, size, 0);
}