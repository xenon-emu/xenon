/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Hangup.h"
#include "Exit.h"

namespace Base {

s32 globalShutdownHandler();

#ifdef _WIN32
BOOL WINAPI consoleControlHandler(ul32 ctrlType);
#elif defined(__linux__) || defined(__APPLE__)
extern "C" void hangup(s32);
#endif

const s32 InstallHangup() {
#ifdef _WIN32
  if (!SetConsoleCtrlHandler(consoleControlHandler, TRUE))
    return -1;
#elif defined(__linux__) || defined(__APPLE__)
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);

  // Block in this thread, and new threads inherit the mask
  if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
    return -1;
  }

  signalThreadRunning = true;
  signalThread = std::thread([] {
    sigset_t waitset;
    sigemptyset(&waitset);
    sigaddset(&waitset, SIGHUP);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);

    int sig = 0;
    while (true) {
      if (sigwait(&waitset, &sig) != 0) {
        continue;
      }

      if (!signalThreadRunning) {
        break;
      }

      if (!gShutdownRequested.exchange(true)) {
        (void)globalShutdownHandler();
      } else {
        gForceExitRequested = true;
        const s32 exitCode = Base::fexit(-1);
        if (exitCode != 0) // This will never happen, it's to silence the compiler warnings
          break;
      }
    }
  });
#endif
  return 0;
}

const s32 RemoveHangup() {
#ifdef _WIN32
  if (!SetConsoleCtrlHandler(consoleControlHandler, FALSE))
    return -1;
#elif defined(__linux__) || defined(__APPLE__)
  signalThreadRunning = false;

  if (signalThread.joinable()) {
    if (std::this_thread::get_id() == signalThread.get_id()) {
      signalThread.detach();
    } else {
      pthread_kill(signalThread.native_handle(), SIGTERM);
      signalThread.join();
    }
  }
#endif
  return 0;
}

// Clean shutdown when we are sent by the OS to shutdown
s32 globalShutdownHandler() {
  // If we have been told we cannot safely terminate, just force exit without cleanup
  // The OS will need to handle it, but better than deadlocking the process
  if (XePaused) {
    return Base::exit(-1); // Avoid cleanup, as we cannot ensure it'll be done properly
  }

  // If we tried to exit gracefully the first time and failed, use fexit to forcefully send a SIGTERM
  if (gForceExitRequested) {
    return Base::fexit(-1);
  }

  // Cleanly shutdown without the exit syscall
  XeRunning.store(false, std::memory_order_release);

  // Since we wait to ensure shutdown is finished instead of a broad timer,
  // We need to ensure we call it here, because a timer deadlocks the main thread.
  XeMain::Shutdown();

  // Give everything a while to shut down. If it still hasn't shutdown, then something hung
  int attempts = 0;
  while (!gShutdownFinished) {
    if (gForceExitRequested || attempts >= 400) {
      // If it more than 400ms, just kill it forcefully, or if we get another interrupt.
      return Base::fexit(-1);
    }
    ++attempts;
    std::this_thread::sleep_for(1ms);
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
#endif

} // namespace Base