/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PrecisionTimer.h"
#include "Base/Logging/Log.h"

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#else
#include <time.h>
#include <errno.h>
#endif

#include <chrono>

namespace Base {

// 
// Windows
// 
#ifdef _WIN32

// Cached QPC frequency (ticks per second). Set once at init.
static double s_qpcNsPerTick = 0.0;
static LONGLONG s_qpcFrequency = 0;
static bool s_initialized = false;

// High-resolution waitable timer support ( Should work after Windows 10 1803+)
static bool s_highResTimerAvailable = false;

void InitPrecisionTimerWin32() {
  if (s_initialized) return;

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  s_qpcFrequency = freq.QuadPart;
  s_qpcNsPerTick = 1'000'000'000.0 / static_cast<double>(freq.QuadPart);

  // Probe for CREATE_WAITABLE_TIMER_HIGH_RESOLUTION support
  // The flag value is 0x00000002.
  constexpr DWORD HIGH_RES_FLAG = 0x00000002;
  HANDLE testTimer = CreateWaitableTimerExW(nullptr, nullptr, HIGH_RES_FLAG, TIMER_ALL_ACCESS);
  if (testTimer) {
    s_highResTimerAvailable = true;
    CloseHandle(testTimer);
  } else {
    s_highResTimerAvailable = false;
    // Fall back to timeBeginPeriod for better Sleep resolution.
    timeBeginPeriod(1);
  }

  s_initialized = true;
}

u64 GetMonotonicNanos() {
  if (!s_initialized) InitPrecisionTimerWin32();

  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return static_cast<u64>(static_cast<double>(counter.QuadPart) * s_qpcNsPerTick);
}

void PrecisionSleepNs(u64 ns) {
  if (!s_initialized) InitPrecisionTimerWin32();
  if (ns == 0) return;

  // WaitableTimers can't reliably handle sub-50µs sleeps.
  // Caller should spin for anything shorter.
  constexpr u64 MIN_SLEEP_NS = 50'000ULL;
  if (ns < MIN_SLEEP_NS) return;

  constexpr DWORD HIGH_RES_FLAG = 0x00000002;

  // Cache the timer handle per-thread to avoid creating/destroying
  // a kernel object on every call (~400K calls/sec in the timer loop).
  thread_local HANDLE cachedTimer = []() -> HANDLE {
    DWORD flags = CREATE_WAITABLE_TIMER_MANUAL_RESET;
    if (s_highResTimerAvailable) {
      flags |= HIGH_RES_FLAG;
    }
    return CreateWaitableTimerExW(nullptr, nullptr, flags, TIMER_ALL_ACCESS);
  }();

  if (!cachedTimer) return;

  // Convert ns to 100-ns units (negative = relative).
  LARGE_INTEGER interval;
  interval.QuadPart = -static_cast<LONGLONG>(ns / 100ULL);
  if (interval.QuadPart == 0) interval.QuadPart = -1; // Minimum 100ns

  SetWaitableTimer(cachedTimer, &interval, 0, nullptr, nullptr, FALSE);
  WaitForSingleObject(cachedTimer, INFINITE);
}

void PrecisionSleepUntil(u64 deadlineNs) {
  u64 now = GetMonotonicNanos();
  if (now >= deadlineNs) return;
  PrecisionSleepNs(deadlineNs - now);
}

//
// Linux / POSIX Implementation
//

// NOTE: The following code is based on something i found online and google searches/ChatGPT.
// @Vali, Please test and verify.

#else

u64 GetMonotonicNanos() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<u64>(ts.tv_sec) * 1'000'000'000ULL + static_cast<u64>(ts.tv_nsec);
}

void PrecisionSleepNs(u64 ns) {
  if (ns == 0) return;

  struct timespec req;
  req.tv_sec = static_cast<time_t>(ns / 1'000'000'000ULL);
  req.tv_nsec = static_cast<long>(ns % 1'000'000'000ULL);

  // Use clock_nanosleep for monotonic-clock-relative sleep.
  // EINTR is retried automatically with remaining time.
  while (clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &req) == EINTR) {}
}

void PrecisionSleepUntil(u64 deadlineNs) {
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(deadlineNs / 1'000'000'000ULL);
  ts.tv_nsec = static_cast<long>(deadlineNs % 1'000'000'000ULL);

  // TIMER_ABSTIME makes this an absolute deadline sleep — the most accurate
  // option for periodic timers since it doesn't accumulate call overhead.
  while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr) == EINTR) {}
}

#endif

} // namespace Base
