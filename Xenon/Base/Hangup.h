/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#if defined(__linux__) || defined(__APPLE__)
  #include <signal.h>
  #include <pthread.h>
#elif defined(_WIN32)
  #include <Windows.h>
#endif

namespace Base {

inline std::thread signalThread{};
inline std::atomic<bool> signalThreadRunning = false;

[[nodiscard]] extern const s32 InstallHangup();
[[nodiscard]] extern const s32 RemoveHangup();

} // namespace Base
