/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Logging/Log.h"
#include "Base/Global.h"
#include "Core/XCPU/XenonCPU.h"

#include "RootBus.h"

#define PCI_CONFIG_SPACE_BEGIN 0xD0000000
#define PCI_CONFIG_SPACE_END 0xD1000000

#define PCI_BRIDGE_START_ADDR 0xEA000000
#define PCI_BRIDGE_END_ADDR 0xEA010000

RootBus::RootBus() {
  connectedDevices.clear();
}

RootBus::~RootBus() {
  for (auto &[name, device] : connectedDevices) {
    device.reset();
  }
  connectedDevices.clear();

  hostBridge.reset();

  biuData.reset();
}

std::weak_ptr<HostBridge> RootBus::AddHostBridge(std::unique_ptr<HostBridge> newHostBridge) {
  hostBridge = std::move(newHostBridge);

  return hostBridge;
}

void RootBus::AddDevice(std::unique_ptr<SystemDevice> device) {
  if (!device) {
    LOG_CRITICAL(RootBus, "Failed to attach device!");
    Base::SystemPause();
    return;
  }

  u32 hash = device->GetHash();
  if (auto it = connectedDevices.find(hash); it == connectedDevices.end()) {
    deviceCount++;
    LOG_INFO(RootBus, "Device attached: {}", device->GetDeviceName());
    connectedDevices.insert({ hash, std::move(device) });
  } else {
    LOG_CRITICAL(RootBus, "Device already exists! You cannot attach a devce twice without first detaching.");
    Base::SystemPause();
  }

  // Get specific pointers
  if (auto it = connectedDevices.find("RAM"_j); it != connectedDevices.end())
    ramDevice = it->second;
  if (auto it = connectedDevices.find("NAND"_j); it != connectedDevices.end())
    sfcxDevice = it->second;
}

void RootBus::ResetDevice(std::unique_ptr<SystemDevice> device) {
  if (!device) {
    LOG_CRITICAL(RootBus, "Failed to reset device!");
    Base::SystemPause();
    return;
  }

  u32 hash = device->GetHash();
  if (auto it = connectedDevices.find(hash); it != connectedDevices.end()) {
    LOG_INFO(RootBus, "Resetting device: {}", it->second->GetDeviceName());
    it->second.reset();
    connectedDevices.erase(it);
    connectedDevices.insert({ hash, std::move(device) });
  } else {
    LOG_CRITICAL(RootBus, "Failed to reset device! '{}' never existed.", it->first);
  }

  // Get specific pointers
  if (auto it = connectedDevices.find("RAM"_j); it != connectedDevices.end())
    ramDevice = it->second;
  if (auto it = connectedDevices.find("NAND"_j); it != connectedDevices.end())
    sfcxDevice = it->second;
}

bool RootBus::Read(u64 address, u8 *data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Read", MP_AUTO);

  // Fast path, most reads go to RAM, so check there first.
  if (auto ram = ramDevice.lock()) {
    if (!soc && address < PHYS_MEMORY_END) {
      ram->Read(address, data, size);
      return true;
    }
  } else {
    return false;
  }

  // SFCX
  if (auto sfcx = sfcxDevice.lock()) {
    if (address >= sfcx->GetStartAddress() && address <= sfcx->GetEndAddress()) {
      // Hit
      sfcx->Read(address, data, size);
      return true;
    }
  } else {
    return false;
  }

  // Configuration Read?
  if (address >= PCI_CONFIG_REGION_ADDRESS && address <= PCI_CONFIG_REGION_ADDRESS + PCI_CONFIG_REGION_SIZE) {
    ConfigRead(address, data, size);
    return true;
  }

  // Check on the other busses
  if (hostBridge->Read(address, data, size)) {
    return true;
  }

  // Device not found
  LOG_ERROR(RootBus, "Read failed at address 0x{:X}", address);

  // Any reads to bus that don't belong to any device are always 0xFF
  memset(data, 0xFF, size);

  return false;
}

bool RootBus::MemSet(u64 address, s32 data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::MemSet", MP_AUTO);
  for (auto &[name, dev] : connectedDevices) {
    if (address >= dev->GetStartAddress() && address <= dev->GetEndAddress()) {
      // Hit
      dev->MemSet(address, data, size);
      return true;
    }
  }

  // Check on the other busses
  if (hostBridge->MemSet(address, data, size)) {
    return true;
  }

  // Device or address not found
  if (false) {
    LOG_ERROR(RootBus, "MemSet failed at address: 0x{:X}, data: 0x{:X}", address, data);
    if (XeMain::GetCPU())
      XeMain::GetCPU()->Halt(); // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger on bad fault
  }

  return false;
}

bool RootBus::Write(u64 address, const u8 *data, u64 size, bool soc) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Write", MP_AUTO);

  if (auto ram = ramDevice.lock()) {
    if (!soc && address < 0x3FFFFFFF) {
      ram->Write(address, data, size);
      return true;
    }
  } else {
    return false;
  }

  // SFCX
  if (auto sfcx = sfcxDevice.lock()) {
    if (address >= sfcx->GetStartAddress() && address <= sfcx->GetEndAddress()) {
      // Hit
      sfcx->Write(address, data, size);
      return true;
    }
  } else {
    return false;
  }

  // PCI Configuration Write?
  if (address >= PCI_CONFIG_REGION_ADDRESS &&
      address <= PCI_CONFIG_REGION_ADDRESS + PCI_CONFIG_REGION_SIZE) {
    ConfigWrite(address, data, size);
    return true;
  }

  // Check on the other busses
  if (hostBridge->Write(address, data, size)) {
    return true;
  }

  // Device or address not found
  LOG_ERROR(RootBus, "Write to {:#x} failed, data {:#x}", address, *reinterpret_cast<const u64*>(data));
  return false;
}

//
// Configuration R/W
//

bool RootBus::ConfigRead(u64 address, u8 *data, u64 size) {
  return hostBridge->ConfigRead(address, data, size);
}

bool RootBus::ConfigWrite(u64 address, const u8 *data, u64 size) {
  return hostBridge->ConfigWrite(address, data, size);
}