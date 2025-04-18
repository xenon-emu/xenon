// Copyright 2025 Xenon Emulator Project

#include "RootBus.h"

#include "Core/Xe_Main.h"
#include "Base/Logging/Log.h"

#define PCI_CONFIG_SPACE_BEGIN 0xD0000000
#define PCI_CONFIG_SPACE_END 0xD1000000

#define PCI_BRIDGE_START_ADDR 0xEA000000
#define PCI_BRIDGE_END_ADDR 0xEA010000

RootBus::RootBus() {
  deviceCount = 0;
  conectedDevices.resize(deviceCount);
}

RootBus::~RootBus() {
  biuData.reset();
}

void RootBus::AddHostBridge(HostBridge *newHostBridge) {
  hostBridge = newHostBridge;
}

void RootBus::AddDevice(SystemDevice *device) {
  deviceCount++;
  LOG_INFO(RootBus, "Device attached: {}", device->GetDeviceName());
  conectedDevices.push_back(device);
}

void RootBus::ResetDevice(SystemDevice *device) {
  LOG_INFO(RootBus, "Resetting device: {}", device->GetDeviceName());

  for (u64 i = 0; i != conectedDevices.size(); ++i) {
    SystemDevice *dev = conectedDevices[i];
    if (dev->GetDeviceName() == device->GetDeviceName()) {
      conectedDevices.erase(conectedDevices.begin() + i);
    }
  }

  conectedDevices.push_back(device);
}

void RootBus::Read(u64 readAddress, u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Read", MP_AUTO);
  // Configuration Read?
  if (readAddress >= PCI_CONFIG_REGION_ADDRESS &&
      readAddress <= PCI_CONFIG_REGION_ADDRESS + PCI_CONFIG_REGION_SIZE) {
    ConfigRead(readAddress, data, size);
    return;
  }

  for (auto &device : conectedDevices) {
    if (readAddress >= device->GetStartAddress() &&
        readAddress <= device->GetEndAddress()) {
      // Hit
      device->Read(readAddress, data, size);
      return;
    }
  }

  // Check on the other Busses.
  if (hostBridge->Read(readAddress, data, size)) {
    return;
  }

  // Device not found.
  LOG_ERROR(RootBus, "Read failed at address {:#x}", readAddress);

  // Any reads to bus that don't belong to any device are always 0xFF.
  memset(data, 0xFF, size);
}

void RootBus::MemSet(u64 writeAddress, s32 data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::MemSet", MP_AUTO);
  for (auto &device : conectedDevices) {
    if (writeAddress >= device->GetStartAddress() &&
        writeAddress <= device->GetEndAddress()) {
      // Hit
      device->MemSet(writeAddress, data, size);
      return;
    }
  }

  // Check on the other Busses.
  if (hostBridge->MemSet(writeAddress, data, size)) {
    return;
  }

  // Device or address not found.
  if (false) {
    LOG_ERROR(RootBus, "MemSet failed at address: {:#x}, data: {:#x}", writeAddress, data);
    LOG_CRITICAL(Xenon, "Halting...");
    Xe_Main->getCPU()->Halt(); // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger on bad fault
  }
}

void RootBus::Write(u64 writeAddress, const u8 *data, u64 size) {
  MICROPROFILE_SCOPEI("[Xe::PCI]", "RootBus::Write", MP_AUTO);
  // PCI Configuration Write?
  if (writeAddress >= PCI_CONFIG_REGION_ADDRESS &&
      writeAddress <= PCI_CONFIG_REGION_ADDRESS + PCI_CONFIG_REGION_SIZE) {
    ConfigWrite(writeAddress, data, size);
    return;
  }

  for (auto &device : conectedDevices) {
    if (writeAddress >= device->GetStartAddress() &&
        writeAddress <= device->GetEndAddress()) {
      // Hit
      device->Write(writeAddress, data, size);
      return;
    }
  }

  // Check on the other Busses.
  if (hostBridge->Write(writeAddress, data, size)) {
    return;
  }

  // Device or address not found.
  if (false) {
    LOG_ERROR(RootBus, "Write failed at address: {:#x}, data: {:#x}", writeAddress, *reinterpret_cast<const u64*>(data));
    LOG_CRITICAL(Xenon, "Halting...");
    Xe_Main->getCPU()->Halt(); // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger on bad fault
  }
}

//
// Configuration R/W.
//

void RootBus::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  hostBridge->ConfigRead(readAddress, data, size);
}

void RootBus::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  hostBridge->ConfigWrite(writeAddress, data, size);
}
