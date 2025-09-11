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
#if MICROPROFILE_ENABLED
  // Enable profiling
  MicroProfileSetEnableAllGroups(true);
  MicroProfileSetForceMetaCounters(true);
#if AUTO_FLIP
  MicroProfileStartAutoFlip(30);
#endif // AUTO_FLIP
#endif // MICROPROFILE_ENABLED
  // Set thread name
  Base::SetCurrentThreadName("[Xe] Main");
  // Setup hangup
  if (Base::InstallHangup() != 0) {
    printf("Failed to install signal handler. Clean shutdown is not possible through console\n");
  }
  // Create all handles
  XeMain::Create();
  // Give errors a second to catch up, incase of the async backend
  std::this_thread::sleep_for(200ms);
  // Start execution of the emulator
  XeMain::StartCPU();
  // Inf wait until told otherwise
  while (XeRunning) {
#if MICROPROFILE_ENABLED && !AUTO_FLIP
    MicroProfileFlip(nullptr);
#endif // MICROPROFILE_ENABLED && !AUTO_FLIP
#ifndef NO_GFX
    if (XeMain::renderer.get())
      XeMain::renderer->HandleEvents();
#else
    std::this_thread::sleep_for(100ms);
#endif // !NO_GFX
  }
  // Shutdown
  XeMain::Shutdown();
  // Remove hangup
  if (Base::RemoveHangup() != 0) {
    printf("Failed to remove signal handler. (this is more of a warning, than an issue)\n");
  }
  return 0;
}
