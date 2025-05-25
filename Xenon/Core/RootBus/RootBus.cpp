// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Global.h"
#include "Core/XCPU/Xenon.h"

#include "RootBus.h"

#define PCI_CONFIG_SPACE_BEGIN 0xD0000000
#define PCI_CONFIG_SPACE_END 0xD1000000

#define PCI_BRIDGE_START_ADDR 0xEA000000
#define PCI_BRIDGE_END_ADDR 0xEA010000

RootBus::RootBus() {
  connectedDevices.clear();
}

RootBus::~RootBus() {
  connectedDevices.clear();
  biuData.reset();
}

void RootBus::AddHostBridge(std::shared_ptr<HostBridge> newHostBridge) {
  hostBridge = newHostBridge;
}

void RootBus::AddDevice(std::shared_ptr<SystemDevice> device) {
  if (!device) {
    LOG_CRITICAL(RootBus, "Failed to attach device!");
    Base::SystemPause();
    return;
  }

  deviceCount++;
  LOG_INFO(RootBus, "Device attached: {}", device->GetDeviceName());
  connectedDevices.insert({ device->GetDeviceName(), device });
}

void RootBus::ResetDevice(std::shared_ptr<SystemDevice> device) {
  if (!device) {
    LOG_CRITICAL(RootBus, "Failed to reset device!");
    Base::SystemPause();
    return;
  }

  std::string name = device->GetDeviceName();
  if (auto it = connectedDevices.find(name); it != connectedDevices.end()) {
    LOG_INFO(RootBus, "Resetting device: {}", it->first);
    it->second.reset();
    connectedDevices.erase(it);
    connectedDevices.insert({ device->GetDeviceName(), device });
  } else {
    LOG_CRITICAL(RootBus, "Failed to reset device! '{}' never existed.", it->first);
  }
}

bool RootBus::Read(u64 readAddress, u8 *data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Read", MP_AUTO);
  // Configuration Read?
  if (readAddress >= PCI_CONFIG_REGION_ADDRESS &&
      readAddress <= PCI_CONFIG_REGION_ADDRESS + PCI_CONFIG_REGION_SIZE) {
    ConfigRead(readAddress, data, size);
    return true;
  }

  for (auto &[name, dev] : connectedDevices) {
    if (readAddress >= dev->GetStartAddress() &&
        readAddress <= dev->GetEndAddress()) {
      // Hit
      dev->Read(readAddress, data, size);
      return true;
    }
  }

  // Check on the other busses
  if (hostBridge->Read(readAddress, data, size)) {
    return true;
  }

  if (!soc) {
    // Device not found
    LOG_ERROR(RootBus, "Read failed at address 0x{:X}", readAddress);

    // Any reads to bus that don't belong to any device are always 0xFF
    memset(data, 0xFF, size);
  } else {
    memset(data, 0, size);
  }

  return false;
}

bool RootBus::MemSet(u64 writeAddress, s32 data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::MemSet", MP_AUTO);
  for (auto &[name, dev] : connectedDevices) {
    if (writeAddress >= dev->GetStartAddress() &&
        writeAddress <= dev->GetEndAddress()) {
      // Hit
      dev->MemSet(writeAddress, data, size);
      return true;
    }
  }

  // Check on the other busses
  if (hostBridge->MemSet(writeAddress, data, size)) {
    return true;
  }

  // Device or address not found
  if (false) {
    LOG_ERROR(RootBus, "MemSet failed at address: 0x{:X}, data: 0x{:X}", writeAddress, data);
    if (XeMain::GetCPU())
      XeMain::GetCPU()->Halt(); // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger on bad fault
  }
  return false;
}

bool RootBus::Write(u64 writeAddress, const u8 *data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Write", MP_AUTO);
  // PCI Configuration Write?
  if (writeAddress >= PCI_CONFIG_REGION_ADDRESS &&
      writeAddress <= PCI_CONFIG_REGION_ADDRESS + PCI_CONFIG_REGION_SIZE) {
    ConfigWrite(writeAddress, data, size);
    return true;
  }

  for (auto &[name, dev] : connectedDevices) {
    if (writeAddress >= dev->GetStartAddress() &&
        writeAddress <= dev->GetEndAddress()) {
      // Hit
      dev->Write(writeAddress, data, size);
      return true;
    }
  }

  // Check on the other busses
  if (hostBridge->Write(writeAddress, data, size)) {
    return true;
  }

  // Device or address not found
  if (!soc) {
    LOG_ERROR(RootBus, "Write to 0x{:X} failed", writeAddress, *reinterpret_cast<const u64*>(data));
  }
  return false;
}

//
// Configuration R/W
//

bool RootBus::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  return hostBridge->ConfigRead(readAddress, data, size);
}

bool RootBus::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  return hostBridge->ConfigWrite(writeAddress, data, size);
}
