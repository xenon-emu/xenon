/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "NorthbridgeRegisters.h"

#include "Base/Logging/Log.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/PCIBar.h"

#include <cstring>

// Every northbridge register access, with its name.
// #define NB_DEBUG

#ifdef NB_DEBUG
  #define NB_DEBUGP(x, ...) LOG_DEBUG(HostBridge, "[NB] " x, ##__VA_ARGS__)
#else
  #define NB_DEBUGP(x, ...)
#endif

using Xe::PCI::FillInChunks;
using Xe::PCI::ForEachDwordLane;
using Xe::PCI::NorthbridgeRegisters;

struct NamedRegister {
  u32 offset;
  const char *name;
};

static constexpr NamedRegister kNamedRegisters[] = {
  {NorthbridgeRegisters::HostE0020000, "HOST_E0020000"},
  {NorthbridgeRegisters::HostE0020004, "HOST_E0020004"},
  {NorthbridgeRegisters::BiuE1003000, "BIU_E1003000"},
  {NorthbridgeRegisters::BiuE1003100, "BIU_E1003100"},
  {NorthbridgeRegisters::BiuE1003200, "BIU_E1003200"},
  {NorthbridgeRegisters::BiuE1003300, "BIU_E1003300"},
  {NorthbridgeRegisters::NbFlush, "NB_FLUSH"},
  {NorthbridgeRegisters::BiuE1010000, "BIU_E1010000"},
  {NorthbridgeRegisters::BiuE1010010, "BIU_E1010010"},
  {NorthbridgeRegisters::BiuE1010020, "BIU_E1010020"},
  {NorthbridgeRegisters::BiuE1013000, "BIU_E1013000"},
  {NorthbridgeRegisters::BiuE1013100, "BIU_E1013100"},
  {NorthbridgeRegisters::BiuE1013200, "BIU_E1013200"},
  {NorthbridgeRegisters::BiuE1013300, "BIU_E1013300"},
  {NorthbridgeRegisters::BiuE1016000, "BIU_E1016000"},
  {NorthbridgeRegisters::BiuE1016100, "BIU_E1016100"},
  {NorthbridgeRegisters::BiuE1016200, "BIU_E1016200"},
  {NorthbridgeRegisters::BiuE1018000, "BIU_E1018000"},
  {NorthbridgeRegisters::BiuE1018020, "BIU_E1018020"},
  {NorthbridgeRegisters::BiuMemorySize, "BIU_MEMORY_SIZE"},
  {NorthbridgeRegisters::BiuPowerControl, "BIU_POWER_CONTROL"},
  {NorthbridgeRegisters::BiuE1020008, "BIU_E1020008"},
  {NorthbridgeRegisters::BiuErrorEnable, "BIU_ERROR_ENABLE"},
  {NorthbridgeRegisters::BiuErrorStatus, "BIU_ERROR_STATUS"},
  {NorthbridgeRegisters::BiuErrorClear, "BIU_ERROR_CLEAR"},
  {NorthbridgeRegisters::IocMemorySize, "IOC_MEMORY_SIZE"},
  {NorthbridgeRegisters::IocE1040024, "IOC_E1040024"},
  {NorthbridgeRegisters::IocErrorEnable, "IOC_ERROR_ENABLE"},
  {NorthbridgeRegisters::IocErrorStatus, "IOC_ERROR_STATUS"},
  {NorthbridgeRegisters::IocErrorClear, "IOC_ERROR_CLEAR"},
  {NorthbridgeRegisters::IocRoute0 + 4 * NorthbridgeRegisters::RouteBiu, "IOC_ROUTE_BIU"},
  {NorthbridgeRegisters::IocRoute0 + 4 * 1, "IOC_ROUTE_41"},
  {NorthbridgeRegisters::IocRoute0 + 4 * NorthbridgeRegisters::RouteGraphics, "IOC_ROUTE_GRAPHICS"},
  {NorthbridgeRegisters::IocRoute0 + 4 * 3, "IOC_ROUTE_43"},
  {NorthbridgeRegisters::IocRoute0 + 4 * NorthbridgeRegisters::RouteIoc, "IOC_ROUTE_IOC"},
  {NorthbridgeRegisters::IocRoute0 + 4 * (NorthbridgeRegisters::RouteXps0 + 0), "IOC_ROUTE_XPS0"},
  {NorthbridgeRegisters::IocRoute0 + 4 * (NorthbridgeRegisters::RouteXps0 + 1), "IOC_ROUTE_XPS1"},
  {NorthbridgeRegisters::IocRoute0 + 4 * (NorthbridgeRegisters::RouteXps0 + 2), "IOC_ROUTE_XPS2"},
  {NorthbridgeRegisters::IocRoute0 + 4 * (NorthbridgeRegisters::RouteXps0 + 3), "IOC_ROUTE_XPS3"},
  {NorthbridgeRegisters::IocRoute0 + 4 * (NorthbridgeRegisters::RouteXps0 + 4), "IOC_ROUTE_XPS4"},
  {NorthbridgeRegisters::IocRoute0 + 4 * (NorthbridgeRegisters::RouteXps0 + 5), "IOC_ROUTE_XPS5"},
  {NorthbridgeRegisters::IocSouthbridgeLink0, "IOC_SB_LINK0"},
  {NorthbridgeRegisters::IocSouthbridgeLink1, "IOC_SB_LINK1"},
  {NorthbridgeRegisters::NbE1060030, "NB_E1060030"},
  {NorthbridgeRegisters::NbE1060034, "NB_E1060034"},
};

static const char *RegisterName(u32 offset) {
  for (const NamedRegister &reg : kNamedRegisters) {
    if (reg.offset == offset) { return reg.name; }
  }
  return nullptr;
}

// Absolute address of a register for messages: BAR0 as HWINIT programs it.
constexpr u32 kTypicalBase = 0xE0000000;

static constexpr bool IsRoute(u32 offset) {
  return offset >= NorthbridgeRegisters::IocRoute0
         && offset < NorthbridgeRegisters::IocRoute0 + 4 * NorthbridgeRegisters::RouteCount;
}

NorthbridgeRegisters::NorthbridgeRegisters(u32 installedRamSize)
    : installedRam(installedRamSize) {
  registers[IocMemorySize] = installedRamSize;
}

void NorthbridgeRegisters::Reset() {
  std::lock_guard lock(mutex);
  registers.clear();
  registers[IocMemorySize] = installedRam;
  for (auto &route : routes) { route.store(0, std::memory_order_relaxed); }
  routesWritten.store(0, std::memory_order_release);
}

u32 NorthbridgeRegisters::GetMemorySize() {
  std::lock_guard lock(mutex);
  return ReadRegister(IocMemorySize);
}

void NorthbridgeRegisters::NoteAccess(u32 offset, bool write, [[maybe_unused]] u32 value) {
  const char *name = RegisterName(offset);
  if (!name && reportedUnknown.First(offset)) {
    LOG_WARNING(HostBridge,
                "Northbridge register {:#x} isn't modelled (first {} {:#x}), it reads back what was "
                "written.",
                kTypicalBase + offset, write ? "write" : "read", value);
  }
  NB_DEBUGP("{} {} ({:#x}) {:#x}", write ? "Write" : "Read", name ? name : "?", kTypicalBase + offset, value);
}

u32 NorthbridgeRegisters::ReadRegister(u32 offset) {
  const auto it = registers.find(offset);
  return it != registers.end() ? it->second : 0;
}

void NorthbridgeRegisters::WriteRegister(u32 offset, u32 value) {
  if (IsRoute(offset)) {
    const u32 route = (offset - IocRoute0) / 4;
    routes[route].store(value, std::memory_order_relaxed);
    routesWritten.fetch_or(1u << route, std::memory_order_release);
    registers[offset] = value;
    return;
  }
  switch (offset) {
    case BiuE1010000:
      // A retail console reads the value back at +0x10 as well.
      registers[BiuE1010000] = value;
      registers[BiuE1010010] = value;
      break;
    case BiuE1018020:
      registers[BiuE1018020] = value;
      registers[BiuE1018000] = value;
      break;
    case BiuErrorStatus:
    case IocErrorStatus:
      // Status: set by hardware, cleared through the CLEAR registers. No error is ever raised here.
      break;
    case BiuErrorClear:
      registers[BiuErrorStatus] = ReadRegister(BiuErrorStatus) & ~value;
      registers[offset] = value;
      break;
    case IocErrorClear:
      registers[IocErrorStatus] = ReadRegister(IocErrorStatus) & ~value;
      registers[offset] = value;
      break;
    default: registers[offset] = value; break;
  }
}

// Accesses are split into the 32-bit registers they touch. Byte n of a register is byte (offset & 3) + n of the access.
void NorthbridgeRegisters::Read(u32 offset, u8 *data, u64 size) {
  std::lock_guard lock(mutex);
  ForEachDwordLane(offset, size, [&](u32 at, u64 done, u64 count) {
    const u32 reg = at & ~3u;
    const u32 value = ReadRegister(reg);
    NoteAccess(reg, false, value);
    memcpy(data + done, reinterpret_cast<const u8 *>(&value) + (at & 3), count);
  });
}

void NorthbridgeRegisters::Write(u32 offset, const u8 *data, u64 size) {
  bool memorySizeWritten = false;
  u32 memorySize = 0;
  bool powerControlWritten = false;
  u32 powerControl = 0;
  u32 routesEnabled = 0;
  {
    std::lock_guard lock(mutex);
    ForEachDwordLane(offset, size, [&](u32 at, u64 done, u64 count) {
      const u32 reg = at & ~3u;
      u32 value = ReadRegister(reg);
      memcpy(reinterpret_cast<u8 *>(&value) + (at & 3), data + done, count);
      NoteAccess(reg, true, value);
      WriteRegister(reg, value);
      if (reg == IocMemorySize) {
        memorySizeWritten = true;
        memorySize = value;
      }
      if (reg == BiuPowerControl) {
        powerControlWritten = true;
        powerControl = value;
      }
      if (IsRoute(reg) && (value & RouteEnable)) { routesEnabled |= 1u << ((reg - IocRoute0) / 4); }
    });
  }
  if (memorySizeWritten && memorySizeCallback) { memorySizeCallback(memorySize); }
  if (powerControlWritten && powerControlCallback) { powerControlCallback(powerControl); }
  for (u32 route = 0; routesEnabled && routeEnabledCallback; ++route) {
    if (routesEnabled & (1u << route)) {
      routesEnabled &= ~(1u << route);
      routeEnabledCallback(route);
    }
  }
}

void NorthbridgeRegisters::Fill(u32 offset, u8 value, u64 size) {
  FillInChunks(value, size,
               [&](u64 done, const u8 *bytes, u64 count) { Write(offset + static_cast<u32>(done), bytes, count); });
}

bool NorthbridgeRegisters::ResolveRoute(u32 route, u8 legacyVector, u8 legacyThreads, Delivery &delivery) const {
  if (!(routesWritten.load(std::memory_order_acquire) & (1u << route))) {
    delivery = {legacyVector, legacyThreads};
    return legacyThreads != 0;
  }
  const u32 value = routes[route].load(std::memory_order_relaxed);
  if (!(value & RouteEnable)) { return false; } // disconnected
  delivery.vector = static_cast<u8>((value & 0x3F) << 2);
  delivery.threads = static_cast<u8>((value >> 8) & 0x3F);
  return delivery.threads != 0;
}

bool NorthbridgeRegisters::ResolveInterrupt(u8 prio, u8 legacyThreads, std::array<Delivery, 6> &out, u32 &count) const {
  count = 0;
  switch (prio) {
    case PRIO_BIU:
      if (ResolveRoute(RouteBiu, prio, legacyThreads, out[0])) { count = 1; }
      return true;
    case PRIO_GRAPHICS:
      if (ResolveRoute(RouteGraphics, prio, legacyThreads, out[0])) { count = 1; }
      return true;
    case PRIO_IOC:
      if (ResolveRoute(RouteIoc, prio, legacyThreads, out[0])) { count = 1; }
      return true;
    case PRIO_XPS:
      // One source per hardware thread. The GPU asserts the ones its INTERRUPT packet names.
      for (u32 thread = 0; thread != 6; ++thread) {
        if (!(legacyThreads & (1u << thread))) { continue; }
        if (ResolveRoute(RouteXps0 + thread, prio, static_cast<u8>(1u << thread), out[count])) { ++count; }
      }
      return true;
    default: return false;
  }
}
