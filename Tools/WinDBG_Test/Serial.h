// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Base/Types.h"

#ifdef _WIN32

#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace Serial {

#ifdef _WIN32
using Handle = void*;
#else
using Handle = s32;
#endif

extern Handle OpenPort(const char *portName);
extern void ClosePort(Handle handle);
extern bool Read(Handle handle, u8 *buffer, u64 bufSize, u32 &outBytesRead);
extern bool Write(Handle handle, const u8 *buffer, u64 bufSize, u32 &outBytesWritten);

template <typename T>
inline T Read(Handle handle) {
  T data{};
  u32 bytesRead{};
  Read(handle, reinterpret_cast<u8*>(&data), sizeof(T), bytesRead);
  return data;
}

template <u64 size>
inline u32 Read(Handle handle, u8(&data)[size]) {
  u32 bytesRead{};
  Read(handle, data, size, bytesRead);
  return bytesRead;
}

template <typename T>
inline u32 Write(Handle handle, const T data) {
  u32 bytesWritten{};
  Write(handle, reinterpret_cast<const u8*>(&data), sizeof(T), bytesWritten);
  return bytesWritten;
}

template <u64 size>
inline u32 Write(Handle handle, const u8(&data)[size]) {
  u32 bytesWritten{};
  Write(handle, data, size, bytesWritten);
  return bytesWritten;
}

} // namespace Serial