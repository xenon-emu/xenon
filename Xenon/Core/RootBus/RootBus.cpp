/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "RootBus.h"

#include "Base/Global.h"
#include "Base/Logging/Log.h"

#include <algorithm>
#include <cstring>

RootBus::RootBus() = default;

RootBus::~RootBus() {
  hostBridge.reset();
  ram.reset();
}

void RootBus::AddHostBridge(std::shared_ptr<HostBridge> newHostBridge) { hostBridge = std::move(newHostBridge); }

void RootBus::AddDevice(std::shared_ptr<SystemDevice> device) {
  if (!device) {
    LOG_CRITICAL(RootBus, "Failed to attach device!");
    Base::SystemPause();
    return;
  }
  if (device->GetDeviceName() != "RAM") {
    LOG_ERROR(RootBus, "Only RAM attaches to the root bus. '{}' ignored.", device->GetDeviceName());
    return;
  }
  LOG_INFO(RootBus, "Device attached: {}", device->GetDeviceName());
  ram = std::move(device);
  ramSize = ram->GetEndAddress() - ram->GetStartAddress();
}

void RootBus::NoteMasterAbort(u64 address, bool write) {
  std::lock_guard lock(abortMutex);
  if (reportedAbortPages.insert(address & ~0xFFFull).second) {
    LOG_ERROR(RootBus, "Master abort: nothing decodes {} {:#x}", write ? "write to" : "read from", address);
  }
}

bool RootBus::Read(u64 readAddress, u8 *data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Read", MP_AUTO);
  if (!soc && readAddress < ramSize) {
    const u64 inRam = std::min<u64>(size, ramSize - readAddress);
    ram->Read(readAddress, data, inRam);
    if (inRam == size) { return true; }
    // Straddles the end of RAM: the rest is unclaimed.
    memset(data + inRam, 0xFF, size - inRam);
    NoteMasterAbort(ramSize, false);
    return false;
  }
  if (hostBridge && hostBridge->Read(readAddress, data, size)) { return true; }
  memset(data, 0xFF, size);
  NoteMasterAbort(readAddress, false);
  return false;
}

bool RootBus::Write(u64 writeAddress, const u8 *data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Write", MP_AUTO);
  if (!soc && writeAddress < ramSize) {
    const u64 inRam = std::min<u64>(size, ramSize - writeAddress);
    ram->Write(writeAddress, data, inRam);
    if (inRam == size) { return true; }
    NoteMasterAbort(ramSize, true);
    return false;
  }
  if (hostBridge && hostBridge->Write(writeAddress, data, size)) { return true; }
  NoteMasterAbort(writeAddress, true);
  return false;
}

bool RootBus::MemSet(u64 writeAddress, s32 data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::MemSet", MP_AUTO);
  if (!soc && writeAddress < ramSize) {
    const u64 inRam = std::min<u64>(size, ramSize - writeAddress);
    ram->MemSet(writeAddress, data, inRam);
    if (inRam == size) { return true; }
    NoteMasterAbort(ramSize, true);
    return false;
  }
  if (hostBridge && hostBridge->MemSet(writeAddress, data, size)) { return true; }
  NoteMasterAbort(writeAddress, true);
  return false;
}
