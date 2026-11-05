/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/XCPU/Context/XenonIIC/XenonIIC.h"

#include <array>

// The southbridge's map: where each function sits on the bus behind the bridge, and which interrupt vector it
// raises and which of the bridge's interrupt sources carries it.
//
// Bus 1 as the kernel programs it:
//   0.0 XMA EA001800          1.0 SATA ODD EA001200/EA001220   2.0 SATA HDD EA001300/EA001320
//   4.0 OHCI0 EA002000        4.1 EHCI0 EA003000               5.0 OHCI1 EA004000   5.1 EHCI1 EA005000
//   7.0 ENET EA001400         8.0 SFCX EA00C000 + C8000000     9.0 AUDIO EA001600   A.0 SMC EA001000
//
// Sources: interrupt source numbers from the Xbox 360 Linux Kernel patches (xenon_pci_irq_map), the kernel's own
// table agrees.

// Device numbers on bus 1 as the kernel's PCI enumeration places them. XeMain puts the devices there, the bridge's
// slots only know what's plugged in. The USB blocks are two functions each: OHCI 0, EHCI 1.
constexpr u8 XMA_DEV_NUM = 0x0;
constexpr u8 ODD_DEV_NUM = 0x1;
constexpr u8 HDD_DEV_NUM = 0x2;
constexpr u8 USB0_DEV_NUM = 0x4;
constexpr u8 USB1_DEV_NUM = 0x5;
constexpr u8 ENET_DEV_NUM = 0x7;
constexpr u8 SFCX_DEV_NUM = 0x8;
constexpr u8 AUDIO_DEV_NUM = 0x9;
constexpr u8 SMC_DEV_NUM = 0xA;

// CPU interrupt priorities (vectors) the PCI side raises, as the interrupt controller defines them.
constexpr u8 PRIO_SMM = Xe::XCPU::prioSMM;
constexpr u8 PRIO_SFCX = Xe::XCPU::prioSFCX;
constexpr u8 PRIO_SATA_HDD = Xe::XCPU::prioSATAHDD;
constexpr u8 PRIO_SATA_ODD = Xe::XCPU::prioSATAODD;
constexpr u8 PRIO_OHCI_0 = Xe::XCPU::prioOHCI0;
constexpr u8 PRIO_EHCI_0 = Xe::XCPU::prioEHCI0;
constexpr u8 PRIO_OHCI_1 = Xe::XCPU::prioOHCI1;
constexpr u8 PRIO_EHCI_1 = Xe::XCPU::prioEHCI1;
constexpr u8 PRIO_XMA = Xe::XCPU::prioXMA;
constexpr u8 PRIO_AUDIO = Xe::XCPU::prioAUDIO;
constexpr u8 PRIO_ENET = Xe::XCPU::prioENET;
constexpr u8 PRIO_CLOCK = Xe::XCPU::prioCLOCK;
// North of the bridge: the sources the IOC routes.
constexpr u8 PRIO_XPS = Xe::XCPU::prioXPS;
constexpr u8 PRIO_GRAPHICS = Xe::XCPU::prioGRAPHICS;
constexpr u8 PRIO_BIU = Xe::XCPU::prioBIU;
constexpr u8 PRIO_IOC = Xe::XCPU::prioIOC;

namespace Xe::PCI::Southbridge {

  // The bridge's interrupt sources and the vectors they carry.
  struct SourceRoute {
    u8 prio;
    u8 source;
  };
  constexpr SourceRoute Sources[] = {
    {PRIO_CLOCK, 0},  {PRIO_SATA_ODD, 1}, {PRIO_SATA_HDD, 2}, {PRIO_SMM, 3},  {PRIO_OHCI_0, 4}, {PRIO_EHCI_0, 5},
    {PRIO_OHCI_1, 6}, {PRIO_EHCI_1, 7},   {PRIO_ENET, 10},    {PRIO_XMA, 11}, {PRIO_AUDIO, 12}, {PRIO_SFCX, 13},
  };

  // Bridge source of each CPU interrupt vector (index vector >> 2), -1 = not routed by the bridge.
  inline constexpr std::array<s8, 32> VectorToSource = [] {
    std::array<s8, 32> map{};
    map.fill(-1);
    for (const SourceRoute &route : Sources) { map[route.prio >> 2] = static_cast<s8>(route.source); }
    return map;
  }();

} // namespace Xe::PCI::Southbridge
