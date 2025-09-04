// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Core/XeMain.h"
#include "Base/Param.h"
#include "Base/Exit.h"
#include "Base/Hangup.h"
#include "Base/Thread.h"

PARAM(help, "Prints this message", false);

#define AUTO_FLIP 1
s32 main(s32 argc, char *argv[]) {
  MicroProfileOnThreadCreate("Main");
  // Init params
  Base::Param::Init(argc, argv);
  // Handle help param
  if (PARAM_help.Present()) {
    ::Base::Param::Help();
    return 0;
  }
  // Enable profiling
  MicroProfileSetEnableAllGroups(true);
  MicroProfileSetForceMetaCounters(true);
#if AUTO_FLIP
  MicroProfileStartAutoFlip(30);
#endif
  // Set thread name
  Base::SetCurrentThreadName("[Xe] Main");
  // Define profiler
  {
    // Create all handles
    XeMain::Create();
    // Setup hangup
    if (Base::InstallHangup() != 0) {
      LOG_CRITICAL(System, "Failed to install signal handler. Clean shutdown is not possible through console");
    }
    // Start execution of the emulator
    XeMain::StartCPU();
  }
  // Inf wait until told otherwise
  while (XeRunning) {
#if MICROPROFILE_ENABLED == 1 && AUTO_FLIP == 0
    MicroProfileFlip(nullptr);
#endif
#ifndef NO_GFX
    if (XeMain::renderer.get())
      XeMain::renderer->HandleEvents();
#endif
    std::this_thread::sleep_for(100ms);
  }
  if (Base::RemoveHangup() != 0) {
    LOG_WARNING(System, "Failed to remove signal handler. Ign");
  }
  // Shutdown
  XeMain::Shutdown();
  return 0;
}
