// Copyright 2025 Xenon Emulator Project

#include "Assert.h"
#include "Arch.h"

#include "Logging/Backend.h"
#include "Core/Xe_Main.h"

#ifdef  _MSC_VER
#define Crash() __debugbreak()
#else
#if defined(ARCH_X86_64)
#define Crash() __asm__ __volatile__("int $3")
#elif defined(ARCH_ARM64)
#define Crash() __asm__ __volatile__("brk 0")
#elif defined(ARCH_PPC)
#include <signal.h>
#ifdef SIGTRAP
#define Crash() raise(SIGTRAP)
#else
#define Crash() raise(SIGABRT)
#endif
#else
#define Crash() __builtin_trap()
#endif
#endif // _MSVC_VER

#ifdef _DEBUG
void assert_fail_impl() {
  Base::Log::Stop();
  std::fflush(stdout);
  Crash();
}
#else
void assert_fail_impl() {
  Xe_Main->getCPU()->Halt();
  LOG_CRITICAL(Debug, "Assertion Failed! Soft halting emulator...");
}
#endif

[[noreturn]] void unreachable_impl() {
  Base::Log::Stop();
  std::fflush(stdout);
  Crash();
  throw std::runtime_error("Unreachable code");
}

void assert_fail_debug_msg(const std::string& msg) {
  LOG_CRITICAL(Debug, "Assertion Failed!\n{}", msg.data());
  assert_fail_impl();
}
