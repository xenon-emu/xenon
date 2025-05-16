// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Assert.h"
#include "Arch.h"

#include "Logging/Backend.h"
#include "Core/Xe_Main.h"

#ifdef  _MSC_VER
#define Crash() __debugbreak()
#else
#if defined(ARCH_X86_64)
#define Crash() __asm__ __volatile__("int $3")
#elif defined(ARCH_X86)
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

void throw_fail_impl() {
  ::fflush(stdout);
  Crash();
}

void assert_fail_impl() {
  if (Config::debug.softHaltOnAssertions) {
    if (Xe_Main.get() && Xe_Main->getCPU())
      Xe_Main->getCPU()->Halt();
    fmt::print("Assertion Failed! Soft halting emulator...\n");
  }
}

[[noreturn]] void unreachable_impl() {
  ::fflush(stdout);
  Crash();
  throw std::runtime_error("Unreachable code");
}

void assert_fail_debug_msg(const std::string& msg) {
  fmt::print("Assertion Failed! {}\n", msg.data());
  assert_fail_impl();
}

void throw_fail_debug_msg(const std::string& msg) {
  fmt::print("Assertion Failed! {}\n", msg.data());
  throw_fail_impl();
}
