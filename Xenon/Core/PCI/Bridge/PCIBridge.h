/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Bridge/BridgeHeader.h"
#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/PCIe.h"
#include "Core/PCI/Southbridge.h"
#include "Core/PCI/WarnOnce.h"
#include "Core/XCPU/Context/XenonIIC/XenonIIC.h"

#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>

namespace Xe::PCI {
  class NorthbridgeRegisters;
}

// The southbridge's PCI-PCI bridge (bus 0, device 0).
//
// Config: it has its own type 1 header, and type 0 cycles go to the functions on its secondary bus (bus 1, the
// number HWINIT and the kernel both program). A missing function or an empty bus master-aborts, reads come back
// all ones.
//
// Memory: it claims its interrupt controller (the first 4 KiB of the BAR0 window at 0xEA000000) and every
// assigned BAR of the functions behind it, which is the rest of the 64 KiB window plus the SFCX flash window
// (BAR1, 0xC8000000). Anything else is left to the caller to master-abort. The decode table is rebuilt whenever a
// BAR changes (16-byte granules over the window, plus a short list of BARs outside it) and read lock-free under
// a sequence counter. COMMAND.MEM isn't enforced.
//
// Interrupt controller registers are 32-bit little endian:
//   +0x00  control (0, then 3 once the routes are set up)
//   +0x04  0x40000000
//   +0x0C  source mask, bit n set masks source n (the kernel masks a source by only setting its bit)
//   +0x10 + 4n  route of source n: 0x00800000 enable, 0x00200000 latched (edge), 0x0000FF00 target threads
//               (1 << thread), 0x00000080 level sensitive, 0x0000003F CPU vector >> 2
class PCIBridge {
public:
  PCIBridge();
  ~PCIBridge();

  // Puts device at bus 1, slot deviceNumber.function. One already there is replaced, and the new one takes over its
  // COMMAND and BARs (the guest assigned those to the slot, not to the object).
  void AddPCIDevice(std::shared_ptr<PCIDevice> device, u8 deviceNumber, u8 function);
  // Empties the slot (accesses to it master-abort from now on).
  void RemovePCIDevice(u8 deviceNumber, u8 function);

  // Memory space. False when the bridge doesn't claim the address.
  bool Read(u64 readAddress, u8 *data, u64 size);
  bool Write(u64 writeAddress, const u8 *data, u64 size);
  bool MemSet(u64 writeAddress, s32 data, u64 size);

  // Config cycles the host bridge hands over: bus 0 device 0 (this bridge's own header) and every bus but 0. address
  // is the config window address, any access size. False means master abort, and reads return all ones.
  bool ConfigRead(u32 address, u8 *data, u64 size);
  bool ConfigWrite(u32 address, const u8 *data, u64 size);

  // Platform reset (reboot or power off). Resets every function behind the bridge, then the bridge's own header and
  // interrupt controller, and puts back the BARs and COMMAND the guest had assigned. Everything stops, so call it
  // with the CPU halted and before guest memory is cleared.
  void Reset();

  void RegisterIIC(Xe::XCPU::XenonIIC *xenonIICPtr);
  // The IOC, which routes the sources north of the bridge.
  void SetNorthbridge(Xe::PCI::NorthbridgeRegisters *nb) { northbridge = nb; }

  // A device raises its interrupt. Southbridge sources go where their route says and are held
  // while masked. GRAPHICS, XPS, BIU and IOC go through the IOC routes instead, and targetCPU (a hardware thread
  // mask) is where they land while the guest hasn't programmed the route (for XPS, the threads whose XPS sources
  // assert). Anything else is sent to targetCPU as is.
  bool RouteInterrupt(u8 prio, u8 targetCPU = 0xFF);
  // A device drops its interrupt. One still held behind the mask is withdrawn, one already sent is a message the IIC
  // has taken so it stays.
  void CancelInterrupt(u8 prio);

private:
  static constexpr u32 kWindowSize = 0x10000;       // BAR0
  static constexpr u32 kRegisterBlockSize = 0x1000; // interrupt controller at the start of the window
  static constexpr u32 kGranuleShift = 4;           // smallest BAR (16 bytes)
  static constexpr u32 kGranules = kWindowSize >> kGranuleShift;
  static constexpr u32 kMaxRanges = 64;
  static constexpr u32 kSlots = 32 * 8;
  static constexpr u32 kNumSources = 16;
  static constexpr u32 kRouteEnable = 0x00800000;

  // An assigned BAR of a function behind the bridge.
  struct DecodeRange {
    std::atomic<u32> base{0};
    std::atomic<u32> size{0};
    std::atomic<PCIDevice *> device{nullptr};
    std::atomic<u8> bar{0};
    std::atomic<bool> memoryDisabled{false};
    std::atomic<bool> outsideWindow{false};
  };
  struct Target {
    PCIDevice *device = nullptr; // nullptr with registers set = the interrupt controller
    u32 bar = 0;
    u32 offset = 0; // into the interrupt controller, or into the device's BAR
    bool registers = false;
    bool memoryDisabled = false;
  };

  // Decode address.
  bool Decode(u32 address, Target &target) const;
  // Rebuild the decode table from the current BARs.
  void RebuildDecode();
  void NoteMemoryDisabled(PCIDevice *device);

  // Configuration.
  void OwnConfigRead(u32 reg, u8 *data, u64 size);
  void OwnConfigWrite(u32 reg, const u8 *data, u64 size);
  void DeviceConfigWrite(PCIDevice *device, u32 address, const u8 *data, u64 size);
  PCIDevice *Slot(u32 deviceNumber, u32 function) const { return slots[deviceNumber * 8 + function].get(); }

  // Interrupt controller.
  u32 ReadInterruptRegister(u32 offset);
  void WriteInterruptRegister(u32 offset, u32 value);
  void InterruptRegisterRead(u32 offset, u8 *data, u64 size);
  void InterruptRegisterWrite(u32 offset, const u8 *data, u64 size);
  // Sends the interrupt of source if its route is enabled and it isn't masked, otherwise holds it. Returns whether
  // it was sent.
  bool DeliverSource(u32 source);

  // IIC Pointer used for interrupts
  Xe::XCPU::XenonIIC *xenonIIC = nullptr;
  Xe::PCI::NorthbridgeRegisters *northbridge = nullptr;

  // Guards the slots, every function's config space and the decode table writers.
  std::mutex configMutex;
  std::array<std::shared_ptr<PCIDevice>, kSlots> slots{};
  // Own type 1 header.
  Xe::PCI::BridgeHeader header;
  u8 revisionId = 0;

  // Memory decode. decodeSequence is odd while the table is being rebuilt.
  std::atomic<u32> decodeSequence{0};
  std::atomic<u32> windowBase{0};
  std::array<std::atomic<u8>, kGranules> windowMap{}; // range index + 1, 0 = nothing
  std::array<DecodeRange, kMaxRanges> ranges{};
  std::atomic<u32> rangeCount{0};
  Xe::PCI::WarnOnce<PCIDevice *> memoryDisabledWarned;

  // Interrupt controller registers (dwords of the 4 KiB block), mask and routes included.
  std::array<std::atomic<u32>, kRegisterBlockSize / 4> interruptRegisters{};
  // Sources that asserted while masked. They go out once unmasked (a masked line stays asserted) and
  // CancelInterrupt clears them.
  std::atomic<u32> heldWhileMasked{0};
  Xe::PCI::WarnOnce<u32> reportedUnknownRegisters;
};
