// Copyright 2025 Xenon Emulator Project

#pragma once

#ifdef __linux__
#include <unistd.h>
#elif defined(_WIN32)
#include <processthreadsapi.h>
#endif

#include "Types.h"

namespace Base {

// Platform independent exit (If we need to force exit, Windows handles its exit syscall differently)
[[nodiscard]] inline s32 exit(s32 code) {
#ifdef _WIN32
  ::ExitProcess(code);
#else
  ::exit(code);
#endif
  return code;
}

// Platform independent force exit (If we need to force exit, Windows handles its exit syscall differently)
[[nodiscard]] inline s32 fexit(s32 code) {
#ifdef _WIN32
  HANDLE process = ::GetCurrentProcess();
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
