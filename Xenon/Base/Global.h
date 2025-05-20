// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <atomic>

#include "Logging/Log.h"
#include "Types.h"

#include "microprofile.h"
#include "microprofile_html.h"

// Global running state
inline volatile bool XeRunning{ true };
inline std::atomic<bool> XeShutdonSignaled{ false };
// Global paused state
inline std::atomic<bool> XePaused{ false };

class Xenon;

// Handles system pause
namespace Base {

inline void SystemPause() {
  XePaused = true;
#ifndef TOOL
  Base::Log::NoFmtMessage(Base::Log::Class::Log, Base::Log::Level::Critical, "Press Enter to continue...");
#else
  printf("Press Enter to continue...");
#endif
  (void)::getchar();
}

} // namespace Base

// Handles various CPU routines, done in Xe_Main.cpp
namespace XeMain {

extern void Shutdown();

extern void StartCPU();

extern void ShutdownCPU();

extern void Reboot(u32 type);

extern Xenon *GetCPU();

} // namespace XeMain

// Global shutdown handler
extern s32 globalShutdownHandler();