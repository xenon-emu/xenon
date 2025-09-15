/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Error.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <cerrno>
#include <cstring>
#endif
#include <fmt/format.h>

namespace Base {

std::string NativeErrorToString(const s32 e) {
#ifdef _WIN32
  char *errString = nullptr;

  ul32 res = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, e, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                             reinterpret_cast<char*>(&errString), 1, nullptr);
  if (!res) {
    return fmt::format("Error code: {} (0x{:X})", e, e);
  }
  std::string ret{ errString };
  ::LocalFree(errString);
  return ret;
#else
  char errString[255] = {};
#if defined(__GLIBC__) && (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600)) ||   \
  defined(ANDROID)
  // Thread safe (GNU-specific)
  const char *str = ::strerror_r(e, errString, sizeof(errString));
  return str;
#else
  // Thread safe (XSI-compliant)
  s32 secondErr = ::strerror_r(e, errString, sizeof(errString));
  if (secondErr != 0) {
    return fmt::format("Error code: {} (0x{:X})", e, e);
  }
  return errString;
#endif // GLIBC etc.
#endif // _WIN32
}

std::string GetLastErrorMsg() {
#ifdef _WIN32
  return NativeErrorToString(::GetLastError());
#else
  return NativeErrorToString(errno);
#endif
}

} // namespace Base
