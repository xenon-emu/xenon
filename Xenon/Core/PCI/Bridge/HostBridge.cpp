/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HostBridge.h"

#include "Base/Config.h"
#include "Base/Global.h"
#include "Base/Logging/Log.h"
#include "Core/PCI/PCIBar.h"

#include <algorithm>

using Xe::PCI::FillInChunks;
using Xe::PCI::ForEachDwordLane;

// BAR0 at reset: where HWINIT puts it (0xD0008010 = 0xE0000000) and where the kernel finds it (0x7FD2xxxx ->
// 0xE102xxxx).
constexpr u32 kNorthbridgeBase = 0xE0000000;

// Bus 0 devices.
constexpr u32 kDevBridge = 0;
constexpr u32 kDevHost = 1;
constexpr u32 kDevGPU = 2;

HostBridge::HostBridge(u64 ramSize)
    : northbridge(static_cast<u32>(ramSize)) {
  ResetConfig();
}

void HostBridge::ResetConfig() {
  header.bytes.fill(0);
  // Device/Vendor ID. HWINIT takes bits 4-7 of the device ID as the northbridge type and spins forever on one it
  // doesn't know.
  u32 id = 0x58301414;
  switch (Config::highlyExperimental.consoleRevison) {
    case Config::eConsoleRevision::Falcon: id = 0x58101414; break;
    case Config::eConsoleRevision::Trinity:
    case Config::eConsoleRevision::Corona:
    case Config::eConsoleRevision::Corona4GB: id = 0x58401414; break;
    case Config::eConsoleRevision::Winchester: id = 0x58501414; break;
    default: break;
  }
  header.SetDword(0x00, id);
  // Class code 0x060000 (host bridge), revision 0x10.
  const u32 classRevision = 0x06000010;
  header.SetDword(0x08, classRevision);
  // Only BAR0 is implemented.
  header.SetDword(Xe::PCI::ConfigBar0, kNorthbridgeBase);
  northbridgeBase.store(kNorthbridgeBase, std::memory_order_relaxed);
}

void HostBridge::Reset() {
  // The GPU first: its command processor and DMA stop before the devices behind the PCI bridge do.
  std::shared_ptr<Xe::Xenos::XenosGPU> gpu;
  std::shared_ptr<PCIBridge> southbridge;
  {
    std::lock_guard lock(mutex);
    gpu = xenosGPU;
    southbridge = pciBridge;
  }
  if (gpu) { gpu->PlatformReset(); }
  if (southbridge) { southbridge->Reset(); }
  std::lock_guard lock(mutex);
  northbridge.Reset();
  ResetConfig();
}

HostBridge::~HostBridge() {
  xenosGPU.reset();
  pciBridge.reset();
}

void HostBridge::RegisterXenosGPU(std::shared_ptr<Xe::Xenos::XenosGPU> gpu) {
  std::lock_guard lock(mutex);
  xenosGPU = std::move(gpu);
  // The memory controllers put DRAM chip select 1 at the upper half of the size HWINIT programs here.
  northbridge.SetMemorySizeCallback([this](u32 size) {
    if (xenosGPU) { xenosGPU->SetBiuMemorySize(size); }
  });
  // Self-refresh requests from the BIU.
  northbridge.SetPowerControlCallback([this](u32 value) {
    if (xenosGPU) {
      xenosGPU->SetSelfRefreshRequest((value & Xe::PCI::NorthbridgeRegisters::PowerControlSelfRefresh) != 0);
    }
  });
  // The GPU interrupt is level sensitive, so one raised before software enabled the route goes out once it does.
  northbridge.SetRouteEnabledCallback([this](u32 route) {
    if (route == Xe::PCI::NorthbridgeRegisters::RouteGraphics && xenosGPU) { xenosGPU->GraphicsRouteEnabled(); }
  });
}

void HostBridge::RegisterPCIBridge(std::shared_ptr<PCIBridge> bridge) {
  std::lock_guard lock(mutex);

  pciBridge = std::move(bridge);
  // The IOC routes the GPU's and the northbridge's own interrupts.
  pciBridge->SetNorthbridge(&northbridge);
}

HostBridge::eTarget HostBridge::Decode(u32 address, u32 &offset) {
  if (address - Xe::PCI::ConfigWindowBase < Xe::PCI::ConfigWindowSize) { return eTarget::Config; }
  const u32 base = northbridgeBase.load(std::memory_order_relaxed);
  if (base != 0 && address - base < Xe::PCI::NorthbridgeRegisters::WindowSize) {
    offset = address - base;
    return eTarget::Northbridge;
  }
  if (xenosGPU && xenosGPU->IsAddressMappedInBAR(address)) { return eTarget::XenosGPU; }
  if (pciBridge) { return eTarget::PCIBridge; }
  return eTarget::None;
}

bool HostBridge::Read(u64 readAddress, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::Read", MP_AUTO);
  if ((readAddress >> 32) != 0) { return false; }
  u32 offset = 0;
  switch (Decode(static_cast<u32>(readAddress), offset)) {
    // A configuration master abort is an ordinary answer (all ones), not a bus error.
    case eTarget::Config: ConfigRead(static_cast<u32>(readAddress), data, size); return true;
    case eTarget::Northbridge: northbridge.Read(offset, data, size); return true;
    case eTarget::XenosGPU: return xenosGPU->Read(readAddress, data, size);
    case eTarget::PCIBridge: return pciBridge->Read(readAddress, data, size);
    default: return false;
  }
}

bool HostBridge::Write(u64 writeAddress, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::Write", MP_AUTO);
  if ((writeAddress >> 32) != 0) { return false; }
  u32 offset = 0;
  switch (Decode(static_cast<u32>(writeAddress), offset)) {
    case eTarget::Config: ConfigWrite(static_cast<u32>(writeAddress), data, size); return true;
    case eTarget::Northbridge: northbridge.Write(offset, data, size); return true;
    case eTarget::XenosGPU: return xenosGPU->Write(writeAddress, data, size);
    case eTarget::PCIBridge: return pciBridge->Write(writeAddress, data, size);
    default: return false;
  }
}

bool HostBridge::MemSet(u64 writeAddress, s32 data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::MemSet", MP_AUTO);
  if ((writeAddress >> 32) != 0) { return false; }
  u32 offset = 0;
  switch (Decode(static_cast<u32>(writeAddress), offset)) {
    case eTarget::Config:
      FillInChunks(data, size, [&](u64 done, const u8 *bytes, u64 count) {
        ConfigWrite(static_cast<u32>(writeAddress + done), bytes, count);
      });
      return true;
    case eTarget::Northbridge: northbridge.Fill(offset, static_cast<u8>(data), size); return true;
    case eTarget::XenosGPU: return xenosGPU->MemSet(writeAddress, data, size);
    case eTarget::PCIBridge: return pciBridge->MemSet(writeAddress, data, size);
    default: return false;
  }
}

// Configuration

bool HostBridge::ConfigRead(u32 address, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::ConfigRead", MP_AUTO);
  const u32 bus = Xe::PCI::ConfigBus(address);
  const u32 device = Xe::PCI::ConfigDevice(address);
  if (bus != 0 || (device == kDevBridge && Xe::PCI::ConfigFunction(address) == 0)) {
    return pciBridge && pciBridge->ConfigRead(address, data, size);
  }
  const u32 reg = Xe::PCI::ConfigRegister(address) & 0xFF;
  if (Xe::PCI::ConfigFunction(address) == 0) {
    if (device == kDevHost) {
      std::lock_guard lock(mutex);
      header.Read(reg, data, size);
      return true;
    }
    if (device == kDevGPU && xenosGPU) {
      ForEachDwordLane(reg, size, [&](u32 at, u64 done, u64 count) {
        xenosGPU->ConfigRead((address & ~0xFFFu) | (at & 0xFF), data + done, count);
      });
      return true;
    }
  }
  // No device: master abort, all ones.
  memset(data, 0xFF, size);
  return false;
}

bool HostBridge::ConfigWrite(u32 address, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "HostBridge::ConfigWrite", MP_AUTO);
  const u32 bus = Xe::PCI::ConfigBus(address);
  const u32 device = Xe::PCI::ConfigDevice(address);
  if (bus != 0 || (device == kDevBridge && Xe::PCI::ConfigFunction(address) == 0)) {
    return pciBridge && pciBridge->ConfigWrite(address, data, size);
  }
  const u32 reg = Xe::PCI::ConfigRegister(address) & 0xFF;
  if (Xe::PCI::ConfigFunction(address) == 0) {
    if (device == kDevHost) {
      std::lock_guard lock(mutex);
      OwnConfigWrite(reg, data, size);
      return true;
    }
    if (device == kDevGPU && xenosGPU) {
      ForEachDwordLane(reg, size, [&](u32 at, u64 done, u64 count) {
        xenosGPU->ConfigWrite((address & ~0xFFFu) | (at & 0xFF), data + done, count);
      });
      return true;
    }
  }
  return false;
}

void HostBridge::OwnConfigWrite(u32 reg, const u8 *data, u64 size) {
  // BAR0 is the 32 MiB register file. BAR1-5 aren't implemented.
  header.Write(reg, data, size, Xe::PCI::NorthbridgeRegisters::WindowSize, Xe::PCI::ConfigBarCount);
  const u32 bar0 = header.Dword(Xe::PCI::ConfigBar0);
  northbridgeBase.store(
    Xe::PCI::BarAssigned(bar0, Xe::PCI::NorthbridgeRegisters::WindowSize) ? bar0 & ~Xe::PCI::BarFlagMask : 0,
    std::memory_order_relaxed);
}
