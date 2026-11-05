/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/PCI/WarnOnce.h"

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

// Northbridge register file: BAR0 of the host bridge (bus 0 device 1), 32 MiB at 0xE0000000 once the CB's HWINIT
// programs it (0xD0008010 = 0xE0000000). The kernel maps 0xE1000000-0xE10FFFFF at 0x7FD00000 (BIU 0x7FD2xxxx, IOC
// 0x7FD4xxxx), the hypervisor reaches the whole BAR.

// A register reads back whatever was last written (0 after reset) unless it's listed below with other behaviour.

// The IOC routes the interrupt sources north of the PCI bridge: BIU and IOC errors and the GPU (GRAPHICS, and XPS
// once per hardware thread). The kernel programs a route as
//   0x80000000 enable | (1 << thread) << 8 | 0x80 level sensitive | vector >> 2
// and disables a source by writing 0. Unlike the southbridge there's no latched bit and no mask register.

namespace Xe::PCI {

  class NorthbridgeRegisters {
  public:
    // Register file size: BAR0 of the host bridge.
    static constexpr u32 WindowSize = 0x02000000;

    // Offsets from BAR0.
    static constexpr u32 HostE0020000 = 0x00020000; // written by the hypervisor and the CB, purpose unknown
    static constexpr u32 HostE0020004 = 0x00020004;
    static constexpr u32 BiuE1003000 = 0x01003000; // FSB/BIU setup written by the CB/HWINIT (0, 0x30, 0, 0xE0F7FD00)
    static constexpr u32 BiuE1003100 = 0x01003100;
    static constexpr u32 BiuE1003200 = 0x01003200;
    static constexpr u32 BiuE1003300 = 0x01003300;
    static constexpr u32 NbFlush = 0x01007000;     // a power-mode change writes 0x0D150300 and reads it back
    static constexpr u32 BiuE1010000 = 0x01010000; // reads back at +0x10 as well
    static constexpr u32 BiuE1010010 = 0x01010010;
    static constexpr u32 BiuE1010020 = 0x01010020;
    static constexpr u32 BiuE1013000 = 0x01013000;
    static constexpr u32 BiuE1013100 = 0x01013100;
    static constexpr u32 BiuE1013200 = 0x01013200;
    static constexpr u32 BiuE1013300 = 0x01013300;
    static constexpr u32 BiuE1016000 = 0x01016000; // HWINIT NB-type-specific setup (0xE1016000/6100/6200)
    static constexpr u32 BiuE1016100 = 0x01016100;
    static constexpr u32 BiuE1016200 = 0x01016200;
    static constexpr u32 BiuE1018000 = 0x01018000; // mirror of 0xE1018020
    static constexpr u32 BiuE1018020 = 0x01018020;
    static constexpr u32 BiuMemorySize = 0x01020000;   // HWINIT writes 0x20000000 before training
    static constexpr u32 BiuPowerControl = 0x01020004; // bit 0 during a power-mode change, 0x18 = self-refresh
    static constexpr u32 BiuE1020008 = 0x01020008;
    static constexpr u32 BiuErrorEnable = 0x01020020; // the kernel enables the errors with 0xFFFFFFF0
    static constexpr u32 BiuErrorStatus = 0x01020024; // the kernel's BIU error handler hangs on nonzero
    static constexpr u32 BiuErrorClear = 0x01020028;  // write 1s to clear BiuErrorStatus
    static constexpr u32 IocMemorySize = 0x01040000;  // trained memory size, written by HWINIT, reset = installed RAM
    static constexpr u32 IocE1040024 = 0x01040024;    // HWINIT sets bit 27
    static constexpr u32 IocErrorEnable = 0x0104002C; // 0x7FFFFFFF
    static constexpr u32 IocErrorStatus = 0x01040030; // the kernel's IOC error handler hangs on nonzero
    static constexpr u32 IocErrorClear = 0x01040034;  // write 1s to clear IocErrorStatus
    static constexpr u32 IocRoute0 = 0x01040044;      // interrupt routes, IocRoute0 + 4 * Route*
    static constexpr u32 IocSouthbridgeLink0 = 0x01040074; // written by the kernel's interrupt setup: 0x40000000
    static constexpr u32 IocSouthbridgeLink1 = 0x01040078; // 0xEA000050, a southbridge register address
    static constexpr u32 NbE1060030 = 0x01060030;          // index/data pair written at the end of HWINIT
    static constexpr u32 NbE1060034 = 0x01060034;

    // IOC routes: the kernel's interrupt source number - 0x40.
    static constexpr u32 RouteBiu = 0;
    static constexpr u32 RouteGraphics = 2;
    static constexpr u32 RouteIoc = 4;
    static constexpr u32 RouteXps0 = 5; // + hardware thread
    static constexpr u32 RouteCount = 11;
    static constexpr u32 RouteEnable = 0x80000000;

    // One interrupt a route sends: a CPU vector to a hardware thread mask.
    struct Delivery {
      u8 vector = 0;
      u8 threads = 0;
    };

    explicit NorthbridgeRegisters(u32 installedRamSize);

    // Platform reset: every register back to power-on (the memory size reads the installed RAM again) and the interrupt
    // routes cleared. Software programs them again.
    void Reset();

    // offset is relative to BAR0, any size and alignment (accesses get split into 32-bit registers).
    void Read(u32 offset, u8 *data, u64 size);
    void Write(u32 offset, const u8 *data, u64 size);
    void Fill(u32 offset, u8 value, u64 size);

    // Called with the new value when software writes the memory size.
    void SetMemorySizeCallback(std::function<void(u32)> callback) { memorySizeCallback = std::move(callback); }
    // Called with the new value when software writes BiuPowerControl. Self-refresh requests reach the memory
    // controllers.
    void SetPowerControlCallback(std::function<void(u32)> callback) { powerControlCallback = std::move(callback); }
    // Called with the route when software writes an enabled IOC route. The source is level sensitive, so one
    // that is already asserted gets delivered now.
    void SetRouteEnabledCallback(std::function<void(u32)> callback) { routeEnabledCallback = std::move(callback); }

    // BiuPowerControl bits 3-4: the hypervisor asks the memory controllers to put DRAM in self-refresh before
    // retraining it, and clears them when it's back.
    static constexpr u32 PowerControlSelfRefresh = 0x18;

    u32 GetMemorySize();

    // What the IOC sends when its source for CPU priority prio asserts: GRAPHICS (0x58), BIU (0x64), IOC (0x68), and
    // XPS (0x54) for each hardware thread set in legacyThreads. Returns false if the IOC doesn't route prio.
    // A route software never wrote sends prio to legacyThreads (to the thread itself for XPS).
    bool ResolveInterrupt(u8 prio, u8 legacyThreads, std::array<Delivery, 6> &out, u32 &count) const;

  private:
    u32 ReadRegister(u32 offset);
    void WriteRegister(u32 offset, u32 value);
    void NoteAccess(u32 offset, bool write, u32 value);
    bool ResolveRoute(u32 route, u8 legacyVector, u8 legacyThreads, Delivery &delivery) const;

    u32 installedRam = 0;
    std::mutex mutex;
    std::unordered_map<u32, u32> registers;
    WarnOnce<u32> reportedUnknown;
    std::function<void(u32)> memorySizeCallback;
    std::function<void(u32)> powerControlCallback;
    std::function<void(u32)> routeEnabledCallback;

    // Lock-free copies of the routes for the interrupt path, plus which ones software has written.
    std::array<std::atomic<u32>, RouteCount> routes{};
    std::atomic<u32> routesWritten{0};
  };

} // namespace Xe::PCI
