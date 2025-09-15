/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Hangup.h"
#include "Exit.h"

namespace Base {

s32 globalShutdownHandler();

#ifdef _WIN32
BOOL WINAPI consoleControlHandler(ul32 ctrlType);
#elif defined(__linux__)
extern "C" void hangup(s32);
#endif

const s32 InstallHangup() {
#ifdef _WIN32
  if (!SetConsoleCtrlHandler(consoleControlHandler, TRUE))
    return -1;
#elif defined(__linux__)
  struct sigaction act {};
  act.sa_handler = hangup;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if (sigaction(SIGHUP, &act, nullptr) < 0) {
    return -1;
  }
  if (sigaction(SIGINT, &act, nullptr) < 0) {
    return -1;
  }
  if (sigaction(SIGTERM, &act, nullptr) < 0) {
    return -1;
  }
  if (sigaction(SIGHUP, &act, nullptr) < 0) {
    return -1;
  }
#else
  // I have zero clue if macOS handles it in the same way as Linux does
  // Better to not handle it, until macOS is fully supported by Xenon
#endif
  return 0;
}

const s32 RemoveHangup() {
#ifdef _WIN32
  if (!SetConsoleCtrlHandler(consoleControlHandler, FALSE))
    return -1;
#endif
  return 0;
}

// Clean shutdown when we are sent by the OS to shutdown
s32 globalShutdownHandler() {
  // If we have been told we cannot safely terminate, just force exit without cleanup
  // The OS will need to handle it, but better than deadlocking the process
  if (XePaused) {
    const s32 exitCode = Base::exit(-1);
    return exitCode; // Avoid cleanup, as we cannot ensure it'll be done properly
  }
  // If we tried to exit gracefully the first time and failed,
  // use fexit to forcefully send a SIGTERM
  if (!hupflag) {
    printf("\nAttempting to clean shutdown...\n");
    hupflag = 1;
  } else {
    printf("\nUnable to clean shutdown!\n");
    printf("Press Crtl+C again to forcefully exit...\n");
    const s32 exitCode = Base::fexit(-1);
    return exitCode;
  }
  // Cleanly shutdown without the exit syscall
  XeRunning = false;
  // Give everything a while to shut down. If it still hasn't shutdown, then something hung
  std::this_thread::sleep_for(15s);
  if (XeShutdownSignaled) {
    printf("This was called because after 15s, and a shutdown call, it still hasn't shutdown.\n");
    printf("Something likely hung. If you have issues, please make a GitHub Issue report with this message in it\n");
    // We should force exit. We only should call shutdown once, and if we don't, then it hung
    return Base::exit(-1);
  } else {
    XeMain::Shutdown();
  }
  return 0;
}

#ifdef _WIN32
BOOL WINAPI consoleControlHandler(ul32 ctrlType) {
  switch (ctrlType) {
  case CTRL_C_EVENT:
  case CTRL_CLOSE_EVENT:
    return globalShutdownHandler() == 0 ? TRUE : FALSE; // Signal handled, prevent default behavior if told so
  }
  return FALSE; // Default handling
}
#elif defined(__linux__)
extern "C" void hangup(s32) {
  // Should this be handled here, if we deadlock our main thread?
  (void)globalShutdownHandler();
}
#endif

} // namespace Base