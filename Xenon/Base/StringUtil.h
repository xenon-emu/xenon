/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#else
#include <filesystem>
#endif

#include "Vector128.h"

namespace Base {

[[nodiscard]] std::string UTF16ToUTF8(const std::wstring_view &input);
[[nodiscard]] std::wstring UTF8ToUTF16W(const std::string_view &str);
[[nodiscard]] std::string ToLower(const std::string &str);

// Template for converting a string to T.

template <typename T>
inline T getFromString(const char* inString, bool forceHexOutput = false) {
  // <T> isn't implmented yet, oh well..
  throw;
}

template <>
inline bool getFromString<bool>(const char* inString, bool forceHexOutput) {
  return std::strcmp(inString, "true") == 0 || inString[0] == '1';
}

template <>
inline int32_t getFromString<int32_t>(const char* inString, bool forceHexOutput) {
  if (forceHexOutput || std::strchr(inString, 'h') != nullptr) {
    return std::strtol(inString, nullptr, 16);
  }
  else {
    return std::strtol(inString, nullptr, 0);
  }
}

template <>
inline uint32_t getFromString<uint32_t>(const char* inString, bool forceHexOutput) {
  if (forceHexOutput || std::strchr(inString, 'h') != nullptr) {
    return std::strtoul(inString, nullptr, 16);
  }
  else {
    return std::strtoul(inString, nullptr, 0);
  }
}

template <>
inline int64_t getFromString<int64_t>(const char* inString, bool forceHexOutput) {
  if (forceHexOutput || std::strchr(inString, 'h') != nullptr) {
    return std::strtoll(inString, nullptr, 16);
  }
  else {
    return std::strtoll(inString, nullptr, 0);
  }
}

template <>
inline uint64_t getFromString<uint64_t>(const char* inString, bool forceHexOutput) {
  if (forceHexOutput || std::strchr(inString, 'h') != nullptr) {
    return std::strtoull(inString, nullptr, 16);
  }
  else {
    return std::strtoull(inString, nullptr, 0);
  }
}

template <>
inline float getFromString<float>(const char* inString, bool forceHexOutput) {
  if (forceHexOutput || std::strstr(inString, "0x") == inString ||
    std::strchr(inString, 'h') != nullptr) {
    union {
      uint32_t ui;
      float flt;
    } v;
    v.ui = getFromString<uint32_t>(inString, forceHexOutput);
    return v.flt;
  }
  return std::strtof(inString, nullptr);
}

template <>
inline double getFromString<double>(const char* inString, bool forceHexOutput) {
  if (forceHexOutput || std::strstr(inString, "0x") == inString ||
    std::strchr(inString, 'h') != nullptr) {
    union {
      uint64_t ui;
      double dbl;
    } v;
    v.ui = getFromString<uint64_t>(inString, forceHexOutput);
    return v.dbl;
  }
  return std::strtod(inString, nullptr);
}

template <>
inline Vector128 getFromString<Vector128>(const char* inString, bool forceHexOutput) {
  Vector128 v;
  char* p = const_cast<char*>(inString);
  bool hexOutput = forceHexOutput;
  if (*p == '[') {
    hexOutput = true;
    ++p;
  }
  else if (*p == '(') {
    hexOutput = false;
    ++p;
  }
  else {
    // Assume hex?
    hexOutput = true;
    ++p;
  }
  if (hexOutput) {
    v.dsword[0] = std::strtoul(p, &p, 16);
    while (*p == ' ' || *p == ',') ++p;
    v.dsword[1] = std::strtoul(p, &p, 16);
    while (*p == ' ' || *p == ',') ++p;
    v.dsword[2] = std::strtoul(p, &p, 16);
    while (*p == ' ' || *p == ',') ++p;
    v.dsword[3] = std::strtoul(p, &p, 16);
  }
  else {
    v.flt[0] = std::strtof(p, &p);
    while (*p == ' ' || *p == ',') ++p;
    v.flt[1] = std::strtof(p, &p);
    while (*p == ' ' || *p == ',') ++p;
    v.flt[2] = std::strtof(p, &p);
    while (*p == ' ' || *p == ',') ++p;
    v.flt[3] = std::strtof(p, &p);
  }
  return v;
}


} // namespace Base
