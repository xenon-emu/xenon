/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <array>

#include "Base/Types.h"

// Forward declarations
struct sPPEState;

namespace Xe::XCPU {

// 
// Xenon TimeBase
//

//  * The Time Base (TB) register is a free-running 64-bit counter driven at ~50 MHz on real hardware. Rather than 
//    incrementing it on every emulator tick, we record a single (hostEpoch, tbEpoch) pair and derive the current TB 
//    value on demand using the host high-resolution clock. This eliminates the timer thread and removes all the TB 
//    bookkeeping overhead.

//  * Each hardware thread has its own Decrementer register. When software writes DEC we compute the host-clock 
//    deadline at which the decrementer will underflow (reach 0x80000000 wrap) and store it. A single dedicated 
//    thread sleeps until the nearest deadline, then raises the interrupt by writing ppuDecrementerEx / 
//    ppuHypervisorDecrementerEx directly into sPPUThread::exceptReg.
//
//  * HID6[tb_enable] works as intended, while disabled both TB advancement and decrementer countdowns are frozen.
class XenonTimeBase {
public:
    // Xbox 360 timebase frequency: ~50 MHz
    static constexpr u64 XB_FREQ = 50'000'000ULL;

    // Expression meaning "no active deadline"
    static constexpr auto NO_DEADLINE = std::chrono::steady_clock::time_point::max();

    // Maximum number of hardware threads (3 PPEs × 2 threads)
    static constexpr u8 MAX_THREADS = 6;

    XenonTimeBase();
    ~XenonTimeBase();

    // Must be called once all PPU objects are fully constructed, before any guest code runs.
    void Init(std::array<sPPEState*, 3> ppeStates);

    // Timebase enable/disable  (on writes to HID6[tb_enable])
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return tbEnabled.load(std::memory_order_relaxed); }

    // TB read, returns the current guest TB value derived from the host clock.
    u64  ReadTB() const;

    // TB write, re-anchors the epoch to the current host time with the supplied value.
    void WriteTBL(u32 value);
    void WriteTBU(u32 value);


    // DEC read, threadId is the PIR value (0-5).
    s32  ReadDEC(u8 threadId) const;


    // DEC write, records the new DEC value and schedules the decrementer interrupt.
    // threadId is the PIR value (0-5).
    void WriteDEC(u8 threadId, s32 value);

    // HDEC read/write.
    s32  ReadHDEC(u8 ppuId) const;
    void WriteHDEC(u8 ppuId, s32 value);

private:

    //
    // Internal helpers
    //

    // Convert a host duration (ns) to guest TB ticks.
    static u64 NsToTicks(u64 ns) {
        // ticks = ns * XB_FREQ / 1e9 -> ticks = (ns * 50) / 1000  (= ns/20)
        return ns / 20ULL;  // 1 tick = 20 ns at 50 MHz
    }

    // Convert guest TB ticks to ns.
    static u64 TicksToNs(u64 ticks) {
        return ticks * 20ULL;  // 1 tick = 20 ns at 50 MHz
    }

    // Re-anchor epoch to now.
    void ReanchorLocked();

    // Compute absolute host deadline for a DEC value written now.
    // Returns NO_DEADLINE if the TB is disabled or value is already negative.
    std::chrono::steady_clock::time_point
    ComputeDecDeadline(s32 decValue) const;

    // Arm or re-arm a DEC deadline for the given thread.
    void ArmDecDeadline(u8 threadId, s32 decValue);

    // Arm or re-arm an HDEC deadline for the given PPU.
    void ArmHDecDeadline(u8 ppuId, s32 hdecValue);

    // Entry point for the decrementer watcher thread.
    void DecThread();

    // Fire the decrementer interrupt on the given hardware thread.
    void FireDecInterrupt(u8 threadId);

    // Fire the hypervisor decrementer interrupt on the given PPU.
    void FireHDecInterrupt(u8 ppuId);

    mutable std::mutex epochMutex;

    // Host time at which tbEpoch was recorded.
    std::chrono::steady_clock::time_point hostEpoch;

    // Guest TB value at hostEpoch.
    u64 tbEpoch = 0;


    // Enable flag
    std::atomic<bool> tbEnabled{ true };

    // Per-thread DEC state
    mutable std::mutex decMutex;
    std::condition_variable decCV;

    struct DecState {
        // Host-clock absolute time when this decrementer reaches zero.
        std::chrono::steady_clock::time_point deadline{ NO_DEADLINE };
        // The DEC value at the moment it was written
        s32  valueAtWrite = 0x7FFFFFFF; // DEC is initialized at this value at init.
        // Host time at which the above value was written.
        std::chrono::steady_clock::time_point writeTime{};
        // True while a pending interrupt has been raised but not yet acknowledged (prevents re-firing on every wakeup).
        bool pending = false;
    };

    std::array<DecState, MAX_THREADS> decState{};

    // Per-PPU HDEC state
    struct HDecState {
        std::chrono::steady_clock::time_point deadline{ NO_DEADLINE };
        s32  valueAtWrite = 0x7FFFFFFF;
        std::chrono::steady_clock::time_point writeTime{};
        bool pending = false;
    };

    std::array<HDecState, 3> hdecState{};

    // Decrementer watcher thread
    std::thread           decThread;
    std::atomic<bool>     decThreadActive{ false };

    // PPE State Ptrs
    std::array<sPPEState*, 3> ppeStates{ nullptr, nullptr, nullptr };
};

} // namespace Xe::XCPU
