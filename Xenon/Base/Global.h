/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <cstdlib>

#ifdef TOOL
#define SIMPLE_FMT
#endif

#ifndef SIMPLE_FMT
#define FMT(...) fmt::format(__VA_ARGS__)
#define MK_FMT_ARGS(...) fmt::make_format_args(__VA_ARGS__)
#define VFMT(...) fmt::vformat(__VA_ARGS__)
#else
#define FMT(...) std::format(__VA_ARGS__)
#define MK_FMT_ARGS(...) std::make_format_args(__VA_ARGS__)
#define VFMT(...) std::vformat(__VA_ARGS__)
#endif

#include "Logging/Log.h"
#include "Types.h"

#ifndef TOOL
#include "microprofile.h"
#include "microprofile_html.h"
#endif

// Global running state
inline volatile bool XeRunning{ true };
inline std::atomic<bool> XeShutdownSignaled{ false };
// Global paused state
inline std::atomic<bool> XePaused{ false };

namespace Xe::XCPU { class XenonCPU; }

// Handles system pause
namespace Base {

inline void SystemPause() {
  XePaused = true;
#ifndef TOOL
  Base::Log::NoFmtMessage(Base::Log::Class::Log, Base::Log::Level::Critical, "Press Enter to continue...");
  std::this_thread::sleep_for(10ms); // Wait for log to catchup
#else
  printf("Press Enter to continue...");
#endif
  s32 c = getchar();
  if (c == EOF && errno == EINTR) {
    // Interrupted by signal � let main loop handle shutdown
    return;
  }
}

} // namespace Base

// Handles various CPU routines, done in Xe_Main.cpp
namespace XeMain {

extern void Shutdown();

extern void StartCPU();

extern void ShutdownCPU();

extern void Reboot(u32 type);

extern Xe::XCPU::XenonCPU *GetCPU();

} // namespace XeMain

// Global shutdown handler
extern s32 globalShutdownHandler();