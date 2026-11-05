/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PCIBridge.h"

#include "Base/Global.h"
#include "Base/Logging/Log.h"
#include "Core/PCI/Bridge/NorthbridgeRegisters.h"
#include "Core/PCI/Bridge/PCIBridgeConfig.h"
#include "Core/PCI/PCIBar.h"

#include <algorithm>
#include <bit>
#include <thread>
#include <vector>

using Xe::PCI::FillInChunks;
using Xe::PCI::ForEachDwordLane;

// Interrupt controller registers.
constexpr u32 kRegControl = 0x00;
constexpr u32 kRegConfig = 0x04;
constexpr u32 kRegSourceMask = 0x0C;
constexpr u32 kRegRoute0 = 0x10;

// True if offset is a register the bridge models. The others just read back what was written.
static constexpr bool IsModelledRegister(u32 offset, u32 numSources) {
  return offset == kRegControl || offset == kRegConfig || offset == kRegSourceMask
         || (offset >= kRegRoute0 && offset < kRegRoute0 + 4 * numSources);
}

PCIBridge::PCIBridge() {
  // The header the way a running console has it (HWINIT and the kernel program the same bus numbers and BAR0).
  for (u32 i = 0; i != 64; ++i) { header.SetDword(i * 4, kPciBridgeConfigMap[i]); }

  // Revision depends on the board. The kernel picks the EHCI/SATA PHY setup off it, and Trinity vs Corona too: below
  // 0x70 is Trinity (Panda SB), 0x80 and up has Corona (Koala SB) PHYs.
  u8 revision = 0x60;
  switch (Config::highlyExperimental.consoleRevison) {
    case Config::eConsoleRevision::Xenon: revision = 0x02; break;
    case Config::eConsoleRevision::Zephyr:
    case Config::eConsoleRevision::Falcon:
    case Config::eConsoleRevision::Jasper:
    case Config::eConsoleRevision::Trinity: revision = 0x60; break;
    case Config::eConsoleRevision::Corona:
    case Config::eConsoleRevision::Corona4GB:
    case Config::eConsoleRevision::Winchester: revision = 0x90; break;
  }
  header.bytes[Xe::PCI::ConfigRevisionId] = revision;
  revisionId = revision;

  // Nothing is masked until the guest programs the controller (the kernel masks everything at init, Linux
  // unmasks it all).
  RebuildDecode();
}

PCIBridge::~PCIBridge() {
  for (auto &slot : slots) { slot.reset(); }
}

void PCIBridge::RegisterIIC(Xe::XCPU::XenonIIC *xenonIICPtr) { xenonIIC = xenonIICPtr; }

// Devices

void PCIBridge::AddPCIDevice(std::shared_ptr<PCIDevice> device, u8 deviceNumber, u8 function) {
  if (!device || deviceNumber >= 32 || function >= 8) {
    LOG_CRITICAL(PCIBridge, "Failed to attach a device!");
    return;
  }
  std::lock_guard lock(configMutex);
  auto &slot = slots[deviceNumber * 8 + function];
  // Where a platform reset puts the config space back to, before the guest's BARs are carried over.
  device->SavePowerOnConfig();
  if (slot && slot != device) {
    // Same slot, new object: keep what the guest assigned.
    device->SetCommand(slot->GetCommand());
    for (u32 bar = 0; bar != Xe::PCI::ConfigBarCount; ++bar) {
      device->SetBar(bar, Xe::PCI::BarLatch(device->GetBar(bar), slot->GetBar(bar), device->BarSize(bar)));
    }
    LOG_INFO(PCIBridge, "Replaced 1:{:X}.{} ({} -> {})", deviceNumber, function, slot->GetDeviceName(),
             device->GetDeviceName());
  } else {
    LOG_INFO(PCIBridge, "Attached 1:{:X}.{}: {}", deviceNumber, function, device->GetDeviceName());
  }
  // The decode table points at the old device until it's rebuilt. Keep it alive until then.
  const std::shared_ptr<PCIDevice> previous = std::exchange(slot, std::move(device));
  RebuildDecode();
}

void PCIBridge::RemovePCIDevice(u8 deviceNumber, u8 function) {
  if (deviceNumber >= 32 || function >= 8) { return; }
  std::lock_guard lock(configMutex);
  const std::shared_ptr<PCIDevice> previous = std::exchange(slots[deviceNumber * 8 + function], nullptr);
  if (previous) {
    LOG_INFO(PCIBridge, "Detached 1:{:X}.{}: {}", deviceNumber, function, previous->GetDeviceName());
    RebuildDecode();
  }
}

// Platform reset

void PCIBridge::Reset() {
  // Devices first, without the config lock: their workers get joined and may be in the middle of an interrupt.
  std::vector<std::shared_ptr<PCIDevice>> devices;
  {
    std::lock_guard lock(configMutex);
    for (const auto &slot : slots) {
      if (slot) { devices.push_back(slot); }
    }
  }
  for (const auto &device : devices) { device->PlatformReset(); }

  std::lock_guard lock(configMutex);
  // Own header and interrupt controller back to power-on: routes and mask cleared, nothing held behind the mask.
  for (u32 i = 0; i != 64; ++i) { header.SetDword(i * 4, kPciBridgeConfigMap[i]); }
  header.bytes[Xe::PCI::ConfigRevisionId] = revisionId;
  for (auto &reg : interruptRegisters) { reg.store(0, std::memory_order_release); }
  heldWhileMasked.store(0, std::memory_order_release);
  // Every function forgets the BARs and COMMAND the guest gave it.
  for (const auto &slot : slots) {
    if (slot) { slot->RestorePowerOnConfig(); }
  }
  RebuildDecode();
  LOG_INFO(PCIBridge, "Platform reset: {} functions reset.", devices.size());
}

// Memory decode

void PCIBridge::RebuildDecode() {
  // Writers are serialized by configMutex. Readers retry while the sequence is odd or changed under them.
  const u32 sequence = decodeSequence.load(std::memory_order_relaxed);
  decodeSequence.store(sequence + 1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);

  const u32 bar0 = header.Dword(Xe::PCI::ConfigBar0);
  const u32 base = Xe::PCI::BarAssigned(bar0, kWindowSize) ? bar0 & ~Xe::PCI::BarFlagMask : 0;
  windowBase.store(base, std::memory_order_relaxed);
  for (auto &granule : windowMap) { granule.store(0, std::memory_order_relaxed); }

  u32 count = 0;
  for (const auto &slot : slots) {
    PCIDevice *device = slot.get();
    if (!device) { continue; }
    const bool memoryDisabled = !(device->GetCommand() & Xe::PCI::CommandMemorySpace);
    for (u32 bar = 0; bar != Xe::PCI::ConfigBarCount; ++bar) {
      const u32 size = device->BarSize(bar);
      const u32 value = device->GetBar(bar);
      if (!Xe::PCI::BarAssigned(value, size)) { continue; }
      if (count == kMaxRanges) {
        LOG_ERROR(PCIBridge, "Too many BARs to decode; {} BAR{} ignored", device->GetDeviceName(), bar);
        continue;
      }
      const u32 start = value & ~Xe::PCI::BarFlagMask;
      const u32 decode = Xe::PCI::BarDecodeSize(size);
      DecodeRange &range = ranges[count];
      range.base.store(start, std::memory_order_relaxed);
      range.size.store(decode, std::memory_order_relaxed);
      range.device.store(device, std::memory_order_relaxed);
      range.bar.store(static_cast<u8>(bar), std::memory_order_relaxed);
      range.memoryDisabled.store(memoryDisabled, std::memory_order_relaxed);
      const u32 offset = start - base;
      const bool inside =
        base != 0 && offset >= kRegisterBlockSize && offset < kWindowSize && decode <= kWindowSize - offset;
      range.outsideWindow.store(!inside, std::memory_order_relaxed);
      if (inside) {
        // Overlapping BARs: the first one claims the granule.
        for (u32 granule = offset >> kGranuleShift; granule != (offset + decode) >> kGranuleShift; ++granule) {
          if (windowMap[granule].load(std::memory_order_relaxed) == 0) {
            windowMap[granule].store(static_cast<u8>(count + 1), std::memory_order_relaxed);
          }
        }
      }
      ++count;
    }
  }
  rangeCount.store(count, std::memory_order_relaxed);
  decodeSequence.store(sequence + 2, std::memory_order_release);
}

bool PCIBridge::Decode(u32 address, Target &target) const {
  for (;;) {
    const u32 sequence = decodeSequence.load(std::memory_order_acquire);
    if (sequence & 1) {
      std::this_thread::yield();
      continue;
    }
    Target result{};
    bool found = false;
    const u32 base = windowBase.load(std::memory_order_relaxed);
    const u32 offset = address - base;
    if (base != 0 && offset < kWindowSize) {
      if (offset < kRegisterBlockSize) {
        result.registers = true;
        result.offset = offset;
        found = true;
      } else if (const u8 index = windowMap[offset >> kGranuleShift].load(std::memory_order_relaxed)) {
        const DecodeRange &range = ranges[index - 1];
        result.offset = address - range.base.load(std::memory_order_relaxed);
        result.device = range.device.load(std::memory_order_relaxed);
        result.bar = range.bar.load(std::memory_order_relaxed);
        result.memoryDisabled = range.memoryDisabled.load(std::memory_order_relaxed);
        found = true;
      }
    }
    if (!found) {
      // BARs outside the window (the SFCX flash window) or straddling the interrupt controller.
      const u32 count = rangeCount.load(std::memory_order_relaxed);
      for (u32 i = 0; i != count; ++i) {
        const DecodeRange &range = ranges[i];
        if (!range.outsideWindow.load(std::memory_order_relaxed)) { continue; }
        if (address - range.base.load(std::memory_order_relaxed) < range.size.load(std::memory_order_relaxed)) {
          result.offset = address - range.base.load(std::memory_order_relaxed);
          result.device = range.device.load(std::memory_order_relaxed);
          result.bar = range.bar.load(std::memory_order_relaxed);
          result.memoryDisabled = range.memoryDisabled.load(std::memory_order_relaxed);
          found = true;
          break;
        }
      }
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    if (decodeSequence.load(std::memory_order_relaxed) == sequence) {
      target = result;
      return found;
    }
  }
}

void PCIBridge::NoteMemoryDisabled(PCIDevice *device) {
  if (memoryDisabledWarned.First(device)) {
    LOG_WARNING(PCIBridge, "{} is accessed with COMMAND.MEM clear, decoding it anyway.", device->GetDeviceName());
  }
}

bool PCIBridge::Read(u64 readAddress, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "PCIBridge::Read", MP_AUTO);
  Target target;
  if ((readAddress >> 32) != 0 || !Decode(static_cast<u32>(readAddress), target)) { return false; }
  if (target.registers) {
    InterruptRegisterRead(target.offset, data, size);
    return true;
  }
  if (target.memoryDisabled) { NoteMemoryDisabled(target.device); }
  target.device->BarRead(target.bar, target.offset, data, size);
  return true;
}

bool PCIBridge::Write(u64 writeAddress, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "PCIBridge::Write", MP_AUTO);
  Target target;
  if ((writeAddress >> 32) != 0 || !Decode(static_cast<u32>(writeAddress), target)) { return false; }
  if (target.registers) {
    InterruptRegisterWrite(target.offset, data, size);
    return true;
  }
  if (target.memoryDisabled) { NoteMemoryDisabled(target.device); }
  target.device->BarWrite(target.bar, target.offset, data, size);
  return true;
}

bool PCIBridge::MemSet(u64 writeAddress, s32 data, u64 size) {
  Target target;
  if ((writeAddress >> 32) != 0 || !Decode(static_cast<u32>(writeAddress), target)) { return false; }
  if (target.registers) {
    FillInChunks(data, size, [&](u64 done, const u8 *bytes, u64 count) {
      InterruptRegisterWrite(target.offset + static_cast<u32>(done), bytes, count);
    });
    return true;
  }
  if (target.memoryDisabled) { NoteMemoryDisabled(target.device); }
  target.device->BarMemSet(target.bar, target.offset, data, size);
  return true;
}

// Configuration

bool PCIBridge::ConfigRead(u32 address, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "PCIBridge::ConfigRead", MP_AUTO);
  const u32 bus = Xe::PCI::ConfigBus(address);
  // Registers 0x100 and up alias the first 256 bytes.
  const u32 reg = Xe::PCI::ConfigRegister(address) & 0xFF;
  std::lock_guard lock(configMutex);
  if (bus == 0) {
    OwnConfigRead(reg, data, size);
    return true;
  }
  PCIDevice *device = bus == header.bytes[Xe::PCI::ConfigSecondaryBus]
                        ? Slot(Xe::PCI::ConfigDevice(address), Xe::PCI::ConfigFunction(address))
                        : nullptr;
  if (!device) {
    // Master abort: no function there, or a bus number behind the bridge with nothing on it.
    memset(data, 0xFF, size);
    return false;
  }
  const u32 functionBase = address & ~0xFFFu;
  ForEachDwordLane(reg, size, [&](u32 at, u64 done, u64 count) {
    device->ConfigRead(functionBase | (at & 0xFF), data + done, count);
  });
  return true;
}

bool PCIBridge::ConfigWrite(u32 address, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "PCIBridge::ConfigWrite", MP_AUTO);
  const u32 bus = Xe::PCI::ConfigBus(address);
  const u32 reg = Xe::PCI::ConfigRegister(address) & 0xFF;
  std::lock_guard lock(configMutex);
  if (bus == 0) {
    OwnConfigWrite(reg, data, size);
    return true;
  }
  PCIDevice *device = bus == header.bytes[Xe::PCI::ConfigSecondaryBus]
                        ? Slot(Xe::PCI::ConfigDevice(address), Xe::PCI::ConfigFunction(address))
                        : nullptr;
  if (!device) { return false; }
  const u32 functionBase = address & ~0xFFFu;
  ForEachDwordLane(reg, size, [&](u32 at, u64 done, u64 count) {
    DeviceConfigWrite(device, functionBase | (at & 0xFF), data + done, count);
  });
  return true;
}

void PCIBridge::DeviceConfigWrite(PCIDevice *device, u32 address, const u8 *data, u64 size) {
  const u32 reg = address & 0xFF;
  u32 before[Xe::PCI::ConfigBarCount];
  for (u32 bar = 0; bar != Xe::PCI::ConfigBarCount; ++bar) { before[bar] = device->GetBar(bar); }
  const u16 commandBefore = device->GetCommand();

  device->ConfigWrite(address, data, size);

  // BARs latch whatever base bits their size implements, no matter what the device's ConfigWrite stored. A size
  // probe then reads ~(size - 1) and unimplemented BARs stay 0.
  bool changed = false;
  if (reg < Xe::PCI::ConfigBar0 + 4 * Xe::PCI::ConfigBarCount && reg + size > Xe::PCI::ConfigBar0) {
    for (u32 bar = 0; bar != Xe::PCI::ConfigBarCount; ++bar) {
      const u32 barReg = Xe::PCI::ConfigBar0 + 4 * bar;
      if (reg >= barReg + 4 || reg + size <= barReg) { continue; }
      const u32 latched = Xe::PCI::BarLatch(before[bar], device->GetBar(bar), device->BarSize(bar));
      device->SetBar(bar, latched);
      changed |= latched != before[bar];
    }
  }
  changed |= ((device->GetCommand() ^ commandBefore) & Xe::PCI::CommandMemorySpace) != 0;
  if (changed) { RebuildDecode(); }
}

void PCIBridge::OwnConfigRead(u32 reg, u8 *data, u64 size) { header.Read(reg, data, size); }

void PCIBridge::OwnConfigWrite(u32 reg, const u8 *data, u64 size) {
  // BAR0 is the 64 KiB window. BAR1 isn't implemented.
  const u32 before = header.Dword(Xe::PCI::ConfigBar0);
  header.Write(reg, data, size, kWindowSize, 2);
  if (header.Dword(Xe::PCI::ConfigBar0) != before) { RebuildDecode(); }
}

// Interrupt controller

u32 PCIBridge::ReadInterruptRegister(u32 offset) {
  if (!IsModelledRegister(offset, kNumSources)) {
    if (reportedUnknownRegisters.First(offset)) {
      LOG_WARNING(PCIBridge,
                  "Interrupt controller register {:#x} isn't modelled, it reads back what was "
                  "written.",
                  offset);
    }
  }
  return interruptRegisters[offset / 4].load(std::memory_order_acquire);
}

void PCIBridge::WriteInterruptRegister(u32 offset, u32 value) {
  const bool route = offset >= kRegRoute0 && offset < kRegRoute0 + 4 * kNumSources;
  if (!IsModelledRegister(offset, kNumSources)) {
    if (reportedUnknownRegisters.First(offset)) {
      LOG_WARNING(PCIBridge,
                  "Interrupt controller register {:#x} isn't modelled (write {:#x}), it reads back "
                  "what was written.",
                  offset, value);
    }
  }
  interruptRegisters[offset / 4].store(value, std::memory_order_release);
  if (offset != kRegSourceMask && !route) { return; }
  // Mask or route changed: send what was held behind the mask and is now let through.
  const u32 held = heldWhileMasked.load(std::memory_order_acquire);
  const u32 mask = interruptRegisters[kRegSourceMask / 4].load(std::memory_order_acquire);
  for (u32 ready = held & ~mask; ready; ready &= ready - 1) {
    const u32 source = static_cast<u32>(std::countr_zero(ready));
    heldWhileMasked.fetch_and(~(1u << source), std::memory_order_acq_rel);
    DeliverSource(source);
  }
}

// Registers are 32-bit little endian, so byte n of the access is byte (offset & 3) + n of the register.
void PCIBridge::InterruptRegisterRead(u32 offset, u8 *data, u64 size) {
  ForEachDwordLane(offset, size, [&](u32 at, u64 done, u64 count) {
    if (at >= kRegisterBlockSize) {
      memset(data + done, 0xFF, count);
      return;
    }
    const u32 value = ReadInterruptRegister(at & ~3u);
    memcpy(data + done, reinterpret_cast<const u8 *>(&value) + (at & 3), count);
  });
}

void PCIBridge::InterruptRegisterWrite(u32 offset, const u8 *data, u64 size) {
  ForEachDwordLane(offset, size, [&](u32 at, u64 done, u64 count) {
    if (at >= kRegisterBlockSize) { return; }
    u32 value = interruptRegisters[at / 4].load(std::memory_order_acquire);
    memcpy(reinterpret_cast<u8 *>(&value) + (at & 3), data + done, count);
    WriteInterruptRegister(at & ~3u, value);
  });
}

bool PCIBridge::DeliverSource(u32 source) {
  const u32 bit = 1u << source;
  const u32 route = interruptRegisters[kRegRoute0 / 4 + source].load(std::memory_order_acquire);
  if (!(route & kRouteEnable)) {
    // Not connected: nobody listens (the kernel and Linux disconnect a source this way).
    return false;
  }
  if (interruptRegisters[kRegSourceMask / 4].load(std::memory_order_acquire) & bit) {
    heldWhileMasked.fetch_or(bit, std::memory_order_acq_rel);
    return false;
  }
  if (!xenonIIC) { return false; }
  const u8 vector = static_cast<u8>((route & 0x3F) << 2);
  const u8 threads = static_cast<u8>((route >> 8) & 0x3F);
  xenonIIC->GenerateInterrupt(vector, threads);
  return true;
}

bool PCIBridge::RouteInterrupt(u8 prio, u8 targetCPU) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "PCIBridge::RouteInterrupt", MP_AUTO);
  const s8 source = Xe::PCI::Southbridge::VectorToSource[(prio >> 2) & 0x1F];
  if (source >= 0) { return DeliverSource(static_cast<u32>(source)); }
  if (!xenonIIC) { return false; }
  // North of the bridge: the IOC routes it.
  if (northbridge) {
    std::array<Xe::PCI::NorthbridgeRegisters::Delivery, 6> deliveries;
    u32 count = 0;
    if (northbridge->ResolveInterrupt(prio, targetCPU == 0xFF ? 0 : targetCPU, deliveries, count)) {
      for (u32 i = 0; i != count; ++i) { xenonIIC->GenerateInterrupt(deliveries[i].vector, deliveries[i].threads); }
      return count != 0;
    }
  }
  if (targetCPU == 0xFF) {
    LOG_ERROR(PCIBridge, "Interrupt {:#x} has no route and no target", prio);
    return false;
  }
  xenonIIC->GenerateInterrupt(prio, targetCPU);
  return true;
}

void PCIBridge::CancelInterrupt(u8 prio) {
  const s8 source = Xe::PCI::Southbridge::VectorToSource[(prio >> 2) & 0x1F];
  if (source >= 0) { heldWhileMasked.fetch_and(~(1u << source), std::memory_order_acq_rel); }
}
