/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#ifdef _WIN32
#include <Windows.h>
#endif

// For _mm_pause intrinsic (x86/x64)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

namespace Base {

// Cross-platform high-resolution monotonic clock and precision sleep utilities.
// Designed for sub-microsecond timer loops that need both accuracy and low CPU usage.

// Returns current time in nanoseconds from a monotonic clock source.
// Windows: QueryPerformanceCounter
// Linux/POSIX: clock_gettime(CLOCK_MONOTONIC)
u64 GetMonotonicNanos();

// Sleeps for approximately `ns` nanoseconds using the most accurate
// OS-assisted sleep available. Does NOT busy-spin.
// Windows: WaitableTimer (high-resolution if available)
// Linux/POSIX: clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)
void PrecisionSleepNs(u64 ns);

// Sleeps until an absolute deadline (in nanoseconds from the monotonic clock).
// More accurate than relative sleep for periodic timers since it avoids
// accumulating overhead from the function-call/setup latency.
// Falls back to relative sleep on platforms without absolute-time sleep support.
void PrecisionSleepUntil(u64 deadlineNs);

// CPU-friendly spin hint that reduces power and contention.
// x86: _mm_pause (PAUSE instruction)
// ARM: yield
// Fallback: no-op
inline void SpinHint() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
  __yield();
#else
  // No-op fallback
#endif
}

#ifdef _WIN32
// One-time initialization for Windows high-resolution timer support.
// Called automatically on first use. Safe to call multiple times.
void InitPrecisionTimerWin32();
#endif

} // namespace Base
