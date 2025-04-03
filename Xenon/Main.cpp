// Copyright 2025 Xenon Emulator Project

#include "Core/Xe_Main.h"
#include "Base/Exit.h"

// Clean shutdown when we are sent by the OS to shutdown
s32 globalShutdownHandler() {
  // If we have been told we cannot safely terminate, just force exit without cleanup
  // The OS will need to handle it, but better than deadlocking the process
  if (!Base::gSafeTerm) {
    s32 exitCode = Base::exit(-1);
    return exitCode; // Avoid cleanup, as we cannot ensure it'll be done properly
  }
  XeRunning = false;
  Xe_Main.reset();
  return 0;
}
#ifdef _WIN32
BOOL WINAPI consoleControlHandler(ul32 ctrlType) {
  switch (ctrlType) {
  case CTRL_C_EVENT:
  case CTRL_CLOSE_EVENT:
    globalShutdownHandler();
    return globalShutdownHandler() == 0 ? TRUE : FALSE; // Signal handled, prevent default behavior if told so
  }
  return FALSE; // Default handling
}
s32 installHangup() {
  return SetConsoleCtrlHandler(consoleControlHandler, TRUE) ? 0 : -1;
}
s32 removeHangup() {
  return SetConsoleCtrlHandler(consoleControlHandler, FALSE) ? 0 : -1;
}
#elif defined(__linux__)
#include <signal.h>
volatile sig_atomic_t hupflag = 0;
extern "C" void hangup(s32) {
  hupflag = 1;
  // Should this be handled here, if we deadlock our main thread?
  (void)globalShutdownHandler();
}
s32 installHangup() {
  struct sigaction act {};
  act.sa_handler = hangup;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  
  if (sigaction(SIGHUP, &act, nullptr) < 0) {
    return -1;
  }
  return 0;
}
s32 removeHangup() {
  return 0;
}
#else
// I have zero clue if macOS handles it in the same way as Linux does
// Better to not handle it, until macOS is fully supported by Xenon
s32 installHangup() {
  return 0;
}
s32 removeHangup() {
  return 0;
}
#endif

s32 main(s32 argc, char *argv[]) {
  Xe_Main = std::make_unique<STRIP_UNIQUE(Xe_Main)>();
  if (installHangup() != 0) {
    LOG_CRITICAL(System, "Failed to install signal handler. Clean shutdown is not possible through console");
  }
  LOG_INFO(System, "Starting Xenon.");
  Xe_Main->start();
  while (XeRunning) {
#ifdef __linux__
    if (hupflag) {
      globalShutdownHandler();
      return 0;
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Xe_Main.reset();
  return 0;
}