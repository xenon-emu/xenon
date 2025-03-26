// Copyright 2025 Xenon Emulator Project

#include "Core/Xe_Main.h"

// Clean shutdown when we are sent by the OS to shutdown
#ifdef _WIN32
BOOL WINAPI consoleControlHandler(ul32 ctrlType) {
  switch (ctrlType) {
  case CTRL_C_EVENT:
  case CTRL_CLOSE_EVENT:
    std::cout << "\nReceived shutdown signal. Cleaning up...\n";
    XeRunning = false;
    Xe_Main.reset();
    return TRUE; // Signal handled, prevent default behavior
  }
  return FALSE; // Default handling
}
int installHangup() {
  return SetConsoleCtrlHandler(consoleControlHandler, TRUE) ? 0 : -1;
}
int removeHangup() {
  return SetConsoleCtrlHandler(consoleControlHandler, FALSE) ? 0 : -1;
}
#else
#include <signal.h>
volatile sig_atomic_t hupflag = 0;
extern "C" void hangup(int) {
  hupflag = 1;
}
int installHangup() {
  struct sigaction act {};
  act.sa_handler = hangup;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  
  if (sigaction(SIGHUP, &act, nullptr) < 0) {
    return -1;
  }
  return 0;
}
int removeHangup() {
  return 0;
}
#endif

int main(int argc, char *argv[]) {
  Xe_Main = std::make_unique<STRIP_UNIQUE(Xe_Main)>();
  if (installHangup() != 0) {
    LOG_CRITICAL(System, "Failed to install signal handler. Clean shutdown is not possible through console");
  }
  LOG_INFO(System, "Starting Xenon.");
  Xe_Main->start();
  while (XeRunning) {
#ifdef __linux__
    if (hupflag) {
      XeRunning = false;
      break;
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Xe_Main.reset();
  return 0;
}
