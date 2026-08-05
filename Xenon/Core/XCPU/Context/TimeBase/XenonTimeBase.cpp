/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "XenonTimeBase.h"

#include "Base/Assert.h"
#include "Base/Logging/Log.h"
#include "Base/Thread.h"
#include "Core/XCPU/PPU/PowerPC.h"

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace Xe::XCPU {

  // Constructor
  TimeBase::TimeBase() {
    hostEpoch = Clock::now();
    tbEpoch = 0;

    // Initialise all DEC write-times to now so lazy reads return sane values
    // before any guest write occurs.
    const auto now = Clock::now();
    for (auto& d : decState) { d.writeTime = now; }
    for (auto& h : hdecState) { h.writeTime = now; }
  }

  // Destructor
  TimeBase::~TimeBase() {
    // Signal the dec thread to stop and wake it up.
    decThreadActive.store(false, std::memory_order_release);
    decCV.notify_all();
    if (decThread.joinable()) decThread.join();
  }

  // Init
  void TimeBase::Init(std::array<sPPEState*, 3> inPpeStates) {
    // Check for null pointers in the array.
    if (inPpeStates[0] == nullptr || inPpeStates[1] == nullptr || inPpeStates[2] == nullptr) {
      LOG_CRITICAL(Xenon, "TimeBase: Init called with null PPE state pointer");
      UNREACHABLE_MSG("TimeBase: Init called with null PPE state pointer");
    }

    // Assign pointers.
    ppeStates = inPpeStates;

    // Start the decrementer watcher thread.
    decThreadActive.store(true, std::memory_order_release);
    decThread = std::thread(&TimeBase::DecThread, this);
  }

  // Enable/disable
  void TimeBase::SetEnabled(bool enabled) {
    const bool wasEnabled = tbEnabled.exchange(enabled, std::memory_order_acq_rel);
    if (wasEnabled == enabled) return;

    if (enabled) {
      // Re-anchor: TB was frozen while disabled. The guest TB value should pick up from where it stopped, so reset the
      // host epoch to now while keeping tbEpoch unchanged.
      hostEpoch = Clock::now();

      // Recompute all DEC deadlines from the current frozen DEC values.
      std::lock_guard<std::mutex> decLock(decMutex);
      for (u8 i = 0; i < MAX_THREADS; ++i) {
        auto& d = decState[i];
        if (!d.pending) {
          // Lazy-read the current DEC value, then re-arm. DEC froze at valueAtWrite at the moment we disabled TB.
          // We don't subtract elapsed time because TB wasn't running.
          d.deadline = ComputeDecDeadline(d.valueAtWrite);
          d.writeTime = Clock::now();
        }
      }
      for (u8 i = 0; i < 3; ++i) {
        auto& h = hdecState[i];
        if (!h.pending) {
          h.deadline = ComputeDecDeadline(h.valueAtWrite);
          h.writeTime = Clock::now();
        }
      }
      decCV.notify_all();
    } else {
      // Freeze: snapshot the current TB so ReadTB() returns it unchanged while the TB is off.
      tbEpoch = ReadTB();
      hostEpoch = Clock::now();

      // Cancel all pending DEC deadlines while frozen.
      std::lock_guard<std::mutex> decLock(decMutex);
      for (auto& d : decState) d.deadline = NO_DEADLINE;
      for (auto& h : hdecState) h.deadline = NO_DEADLINE;
      decCV.notify_all();
    }

    LOG_DEBUG(Xenon, "TimeBase: TB {}", enabled ? "enabled" : "disabled");
  }

  // TB read
  u64 TimeBase::ReadTB() const {
    if (!tbEnabled.load(std::memory_order_relaxed)) {
      // TB is frozen, return the last snapshotted value.
      std::lock_guard<std::mutex> lock(epochMutex);
      return tbEpoch;
    }

    std::lock_guard<std::mutex> lock(epochMutex);
    const auto now = Clock::now();
    const u64 elapsedNs
      = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - hostEpoch).count());
    return tbEpoch + NsToTicks(elapsedNs);
  }

  // TB write (hypervisor mtspr TBL / TBU)

  void TimeBase::WriteTBL(u32 value) {
    std::lock_guard<std::mutex> lock(epochMutex);
    // Preserve upper 32 bits, replace lower 32.
    tbEpoch = (tbEpoch & 0xFFFFFFFF00000000ULL) | static_cast<u64>(value);
    hostEpoch = Clock::now();
    LOG_TRACE(Xenon, "TimeBase: TBL written 0x{:X}, new TB=0x{:X}", value, tbEpoch);
  }

  void TimeBase::WriteTBU(u32 value) {
    std::lock_guard<std::mutex> lock(epochMutex);
    // Preserve lower 32 bits, replace upper 32.
    tbEpoch = (tbEpoch & 0x00000000FFFFFFFFULL) | (static_cast<u64>(value) << 32);
    hostEpoch = Clock::now();
    LOG_TRACE(Xenon, "TimeBase: TBU written 0x{:X}, new TB=0x{:X}", value, tbEpoch);
  }

  // DEC read / write

  s32 TimeBase::ReadDEC(u8 threadId) const {
    if (threadId >= MAX_THREADS) return 0x7FFFFFFF;

    if (!tbEnabled.load(std::memory_order_relaxed)) {
      std::lock_guard<std::mutex> lock(decMutex);
      return decState[threadId].valueAtWrite;
    }

    std::lock_guard<std::mutex> lock(decMutex);
    const auto& d = decState[threadId];
    const auto now = Clock::now();

    // Elapsed TB ticks since DEC was written.
    const u64 elapsedNs
      = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - d.writeTime).count());
    const u64 ticksElapsed = NsToTicks(elapsedNs);

    // DEC counts down; subtract elapsed ticks.
    // Cast through u32 to replicate the 32-bit wrap-around behaviour.
    const u32 rawDec = static_cast<u32>(d.valueAtWrite) - static_cast<u32>(ticksElapsed);
    return static_cast<s32>(rawDec);
  }

  void TimeBase::WriteDEC(u8 threadId, s32 value) {
    if (threadId >= MAX_THREADS) return;

    // Re-arming the decrementer squashes any not-yet-delivered DEC interrupt.
    const u8 ppuIdx = threadId / 2;
    const u8 threadIdx = threadId % 2;
    if (ppuIdx < 3 && ppeStates[ppuIdx]) {
      sPPUThread& thread = ppeStates[ppuIdx]->ppuThread[static_cast<ePPUThreadID>(threadIdx)];
      thread.decExpired.store(false, std::memory_order_release);
      thread.ClearExc(ppuDecrementerEx);
    }

    ArmDecDeadline(threadId, value);

    LOG_TRACE(Xenon, "TimeBase: Thread {} DEC written 0x{:X}", threadId, static_cast<u32>(value));
  }

  // HDEC read / write

  s32 TimeBase::ReadHDEC(u8 ppuId) const {
    if (ppuId >= 3) return 0x7FFFFFFF;

    if (!tbEnabled.load(std::memory_order_relaxed)) {
      std::lock_guard<std::mutex> lock(decMutex);
      return hdecState[ppuId].valueAtWrite;
    }

    std::lock_guard<std::mutex> lock(decMutex);
    const auto& h = hdecState[ppuId];
    const auto now = Clock::now();
    const u64 elapsedNs
      = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - h.writeTime).count());
    const u64 ticksElapsed = NsToTicks(elapsedNs);
    const u32 rawDec = static_cast<u32>(h.valueAtWrite) - static_cast<u32>(ticksElapsed);
    return static_cast<s32>(rawDec);
  }

  void TimeBase::WriteHDEC(u8 ppuId, s32 value) {
    if (ppuId >= 3) return;

    // Re-arming squashes stale expiry signals.
    if (ppeStates[ppuId]) {
      ppeStates[ppuId]->ppuThread[ePPUThread_Zero].hdecExpired.store(false, std::memory_order_release);
      ppeStates[ppuId]->ppuThread[ePPUThread_One].hdecExpired.store(false, std::memory_order_release);
    }

    ArmHDecDeadline(ppuId, value);

    LOG_TRACE(Xenon, "TimeBase: PPU {} HDEC written 0x{:X}", ppuId, static_cast<u32>(value));
  }

  // Internal helpers

  void TimeBase::ReanchorLocked() {
    // Called while epochMutex is held.
    const auto now = Clock::now();
    const u64 elapsedNs
      = static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - hostEpoch).count());
    tbEpoch += NsToTicks(elapsedNs);
    hostEpoch = now;
  }

  std::chrono::steady_clock::time_point TimeBase::ComputeDecDeadline(s32 decValue) const {
    // If TB is disabled there is no running clock.
    if (!tbEnabled.load(std::memory_order_relaxed)) return NO_DEADLINE;

    // The decrementer counts down from decValue.  It fires when the MSB transitions from 0->1 (when the 32-bit unsigned
    // value crosses 0x80000000 after starting below it). If the value is already negative (MSB set) the interrupt fires
    // on the next write to MSR[EE]=1, we treat it as "fire immediately".
    const u32 uval = static_cast<u32>(decValue);
    if (uval >= 0x80000000U) {
      // Already expired, fire as soon as possible.
      return Clock::now();
    }

    // Ticks until wrap: we need to count down by (uval + 1) ticks.
    // (On the last tick the counter goes from 0 to 0xFFFFFFFF.)
    const u64 ticksRemaining = static_cast<u64>(uval) + 1ULL;
    const u64 nsRemaining = TicksToNs(ticksRemaining);

    return Clock::now() + std::chrono::nanoseconds(static_cast<s64>(nsRemaining));
  }

  void TimeBase::ArmDecDeadline(u8 threadId, s32 decValue) {
    const auto deadline = ComputeDecDeadline(decValue);

    {
      std::lock_guard<std::mutex> lock(decMutex);
      auto& d = decState[threadId];
      d.valueAtWrite = decValue;
      d.writeTime = Clock::now();
      d.deadline = deadline;
      d.pending = false;
    }
    decCV.notify_one();
  }

  void TimeBase::ArmHDecDeadline(u8 ppuId, s32 hdecValue) {
    const auto deadline = ComputeDecDeadline(hdecValue);

    {
      std::lock_guard<std::mutex> lock(decMutex);
      auto& h = hdecState[ppuId];
      h.valueAtWrite = hdecValue;
      h.writeTime = Clock::now();
      h.deadline = deadline;
      h.pending = false;
    }
    decCV.notify_one();
  }

  // Interrupt firing helpers

  void TimeBase::FireDecInterrupt(u8 threadId) {
    if (threadId >= MAX_THREADS) return;

    // Map PIR (0-5) to the owning sPPEState and thread slot:
    const u8 ppuIdx = threadId / 2;
    const u8 threadIdx = threadId % 2;

    if (ppuIdx >= 3 || !ppeStates[ppuIdx]) return;

    // Set the decrementer signal.
    sPPUThread& thread = ppeStates[ppuIdx]->ppuThread[static_cast<ePPUThreadID>(threadIdx)];
    thread.decExpired.store(true, std::memory_order_release);

    LOG_TRACE(Xenon, "TimeBase: DEC expired, signal set on thread {}", threadId);
  }

  void TimeBase::FireHDecInterrupt(u8 ppuId) {
    if (ppuId >= 3 || !ppeStates[ppuId]) return;

    // HDEC is a per-PPE resource but can be taken by either of its two
    // hardware threads depending on which one is running in hypervisor
    // context. Set the flag on both threads.
    ppeStates[ppuId]->ppuThread[ePPUThread_Zero].hdecExpired.store(true, std::memory_order_release);
    ppeStates[ppuId]->ppuThread[ePPUThread_One].hdecExpired.store(true, std::memory_order_release);

    LOG_TRACE(Xenon, "TimeBase: HDEC expired, signal set on PPU {}", ppuId);
  }

  // Decrementer watcher thread
  void TimeBase::DecThread() {
    Base::SetCurrentThreadName("[Xe] TimeBase/DEC Thread");

    while (decThreadActive.load(std::memory_order_acquire)) {

      std::unique_lock<std::mutex> lock(decMutex);

      // Find the nearest deadline across all threads and HDECs.

      auto nearest = NO_DEADLINE;

      for (const auto& d : decState) {
        if (!d.pending && d.deadline < nearest) nearest = d.deadline;
      }
      for (const auto& h : hdecState) {
        if (!h.pending && h.deadline < nearest) nearest = h.deadline;
      }

      // Sleep until the nearest deadline or until we are signalled.

      if (nearest == NO_DEADLINE) {
        // No active deadlines — wait indefinitely for a new DEC write
        // or a TB enable event.
        decCV.wait(lock, [this] {
          if (!decThreadActive.load(std::memory_order_acquire)) return true;
          for (const auto& d : decState)
            if (!d.pending && d.deadline != NO_DEADLINE) return true;
          for (const auto& h : hdecState)
            if (!h.pending && h.deadline != NO_DEADLINE) return true;
          return false;
        });
      } else {
        // Sleep until the nearest deadline (or until woken early by a new write that might have an even sooner
        // deadline).
        decCV.wait_until(lock, nearest);
      }

      if (!decThreadActive.load(std::memory_order_acquire)) break;

      // Check which deadlines have expired and fire their interrupts.
      const auto now = Clock::now();

      for (u8 i = 0; i < MAX_THREADS; ++i) {
        auto& d = decState[i];
        if (!d.pending && d.deadline != NO_DEADLINE && now >= d.deadline) {
          d.pending = true;
          d.deadline = NO_DEADLINE;
          lock.unlock();
          FireDecInterrupt(i);
          lock.lock();
        }
      }

      for (u8 i = 0; i < 3; ++i) {
        auto& h = hdecState[i];
        if (!h.pending && h.deadline != NO_DEADLINE && now >= h.deadline) {
          h.pending = true;
          h.deadline = NO_DEADLINE;
          lock.unlock();
          FireHDecInterrupt(i);
          lock.lock();
        }
      }
    }

    LOG_DEBUG(Xenon, "TimeBase: DEC thread exiting.");
  }

} // namespace Xe::XCPU
