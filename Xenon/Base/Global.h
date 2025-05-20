// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <atomic>

#include "Types.h"

#include "microprofile.h"
#include "microprofile_html.h"

// Global running state
inline std::atomic<bool> XeRunning{ true };
// Global paused state
inline std::atomic<bool> XePaused{ false };

class Xenon;

inline void SystemPause() {
  XePaused = true;
  printf("Press Enter to continue...");
  (void)::getchar();
}

// Handles various CPU routines, done in Xe_Main.cpp
namespace XeMain {

extern void StartCPU();

extern void ShutdownCPU();

extern void Reboot(u32 type);

extern Xenon *GetCPU();

} // namespace XeMain

// Global shutdown handler
extern s32 globalShutdownHandler();