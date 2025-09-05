// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <unistd.h>
#endif

namespace Base {

inline std::atomic<bool> gSafeTerm{ true };

// Platform independent exit (If we need to force exit, Windows handles its exit syscall differently)
[[nodiscard]] inline const s32 exit(const s32 code) {
#ifdef _WIN32
  ::ExitProcess(code);
#else
  ::exit(code);
#endif
  return code;
}

// Platform independent force exit (If we need to force exit, Windows handles its exit syscall differently)
[[nodiscard]] inline const s32 fexit(const s32 code) {
#ifdef _WIN32
  const HANDLE process = ::GetCurrentProcess();
  if (!process) {
    return -1;
  }
  if (!::TerminateProcess(process, code)) {
    return -1;
  }
#else
  ::_exit(code);
#endif
  return code;
}

} // namespace Base
