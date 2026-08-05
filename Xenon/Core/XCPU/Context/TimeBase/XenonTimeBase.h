/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

// Forward declaration
struct sPPEState;

namespace Xe::XCPU {

  // Xenon timebase emulation. The timebase is a 64-bit counter that increments at a fixed frequency
  // (50 MHz on the Xbox 360). It is used for timing and scheduling in the PowerPC architecture,
  // including the decrementer (DEC) and hypervisor decrementer (HDEC) interrupts.
  class TimeBase {
  public:
    // Xbox 360 timebase frequency: 50 MHz
    static constexpr u64 XB_FREQ = 50'000'000ULL;

    // Sentinel meaning "no active deadline"
    static constexpr auto NO_DEADLINE = std::chrono::steady_clock::time_point::max();

    // Maximum number of hardware threads (3 PPEs × 2 threads)
    static constexpr u8 MAX_THREADS = 6;

    // Constructor
    TimeBase();
    // Destructor
    ~TimeBase();

    // Must be called once all PPU objects are fully constructed, before any guest code runs.
    // @param ppeStates is an array of pointers to the 3 PPE states.
    void Init(std::array<sPPEState*, 3> ppeStates);

    // Timebase enable / disable  (mirrors HID6[tb_enable])
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return tbEnabled.load(std::memory_order_relaxed); }

    // TB read
    // Returns the current guest TB value derived from the host clock.
    u64 ReadTB() const;

    // TB write  (mtspr TBL/TBU – hypervisor only)
    // Re-anchors the epoch to the current host time with the supplied value.
    void WriteTBL(u32 value);
    void WriteTBU(u32 value);

    // DEC read
    // @param threadId is the thread ID in PIR value (0-5).
    s32 ReadDEC(u8 threadId) const;

    // DEC write  (mtspr DEC)
    // Records the new DEC value and schedules the decrementer interrupt.
    // @param threadId is the thread ID in PIR value (0-5).
    void WriteDEC(u8 threadId, s32 value);

    // HDEC read/write (hypervisor decrementer, per-PPE not per-thread)
    // @param ppuId is 0-2.
    s32 ReadHDEC(u8 ppuId) const;
    void WriteHDEC(u8 ppuId, s32 value);

  private:
    // Convert a host duration (in nanoseconds) to guest TB ticks.
    static u64 NsToTicks(u64 ns) {
      // ticks = ns * XB_FREQ / 1e9
      // We approximate with: ticks = (ns * 50) / 1000  (= ns/20)
      return ns / 20ULL; // 1 tick = 20 ns at 50 MHz
    }

    // Convert guest TB ticks to nanoseconds.
    static u64 TicksToNs(u64 ticks) {
      return ticks * 20ULL; // 1 tick = 20 ns at 50 MHz
    }

    // Re-anchor epoch to now. Caller must hold epochMutex.
    void ReanchorLocked();

    // Compute absolute host deadline for a DEC value written now.
    // Returns NO_DEADLINE if TB is disabled or value is already negative.
    std::chrono::steady_clock::time_point ComputeDecDeadline(s32 decValue) const;

    // Arm or re-arm a DEC deadline for the given thread.
    // Caller must NOT hold decMutex.
    void ArmDecDeadline(u8 threadId, s32 decValue);

    // Arm or re-arm an HDEC deadline for the given PPU.
    void ArmHDecDeadline(u8 ppuId, s32 hdecValue);

    // Entry point for the decrementer watcher thread.
    void DecThread();

    // Fire the decrementer interrupt on the given hardware thread.
    void FireDecInterrupt(u8 threadId);

    // Fire the hypervisor decrementer interrupt on the given PPU.
    void FireHDecInterrupt(u8 ppuId);

    // Epoch state.
    mutable std::mutex epochMutex;

    // Host time at which tbEpoch was recorded.
    std::chrono::steady_clock::time_point hostEpoch;

    // Guest TB value at hostEpoch.
    u64 tbEpoch = 0;

    // True if the Time Base is enabled.
    std::atomic<bool> tbEnabled{true};

    // Dec State Mutex and Condition Variable for the decrementer watcher thread.
    mutable std::mutex decMutex;
    std::condition_variable decCV;

    // Per-thread DEC state.
    struct DecState {
      // Host-clock absolute time when this decrementer reaches zero.
      std::chrono::steady_clock::time_point deadline{NO_DEADLINE};
      // The DEC value at the moment it was written (for lazy reads).
      s32 valueAtWrite = 0x7FFFFFFF;
      // Host time at which the above value was written.
      std::chrono::steady_clock::time_point writeTime{};
      // True while a pending interrupt has been raised but not yet acknowledged (prevents re-firing).
      bool pending = false;
    };

    // Member DEC state for each of the 6 hardware threads.
    std::array<DecState, MAX_THREADS> decState{};

    // Per-PPU HDEC state.
    struct HDecState {
      std::chrono::steady_clock::time_point deadline{NO_DEADLINE};
      s32 valueAtWrite = 0x7FFFFFFF;
      std::chrono::steady_clock::time_point writeTime{};
      bool pending = false;
    };

    // Member HDEC state for each of the 3 PPEs.
    std::array<HDecState, 3> hdecState{};

    // Decrementer watcher thread.
    std::thread decThread;
    // True while the decrementer watcher thread is running.
    std::atomic<bool> decThreadActive{false};

    // External references to the PPE states (set by Init, never changed afterwards).
    std::array<sPPEState*, 3> ppeStates{nullptr, nullptr, nullptr};
  };

} // namespace Xe::XCPU
