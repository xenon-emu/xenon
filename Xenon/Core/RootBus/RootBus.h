/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <unordered_map>

#include "Base/SystemDevice.h"
#include "Core/PCI/Bridge/HostBridge.h"

// Physical memory address space
#define PHYS_MEMORY_START 0x00000000  // Physical memory (RAM) start address
#define PHYS_MEMORY_END   0x3FFFFFFF  // Physical memory (RAM) end address

// PCI Configuration region
#define PCI_CONFIG_REGION_ADDRESS 0xD0000000
#define PCI_CONFIG_REGION_SIZE 0x1000000

class RootBus {
public:
  RootBus();
  ~RootBus();

  std::weak_ptr<HostBridge> AddHostBridge(std::unique_ptr<HostBridge> newHostBridge);
  void AddDevice(std::unique_ptr<SystemDevice> device);
  void ResetDevice(std::unique_ptr<SystemDevice> device);

  bool Read(u64 address, u8 *data, u64 size, bool soc = false);
  bool Write(u64 address, const u8 *data, u64 size, bool soc = false);
  bool MemSet(u64 address, s32 data, u64 size);

  // Configuration Space R/W
  bool ConfigRead(u64 address, u8 *data, u64 size);
  bool ConfigWrite(u64 address, const u8 *data, u64 size);

  template <typename T>
  std::weak_ptr<T> GetDevice(u32 hash) {
    return std::dynamic_pointer_cast<T>(connectedDevices[hash]);
  }
private:
  std::shared_ptr<HostBridge> hostBridge = {};

  u32 deviceCount = 0;
  std::unordered_map<u32, std::shared_ptr<SystemDevice>> connectedDevices = {};

  // Direct device pointers for both RAM and SFCX
  std::weak_ptr<SystemDevice> ramDevice = {};
  std::weak_ptr<SystemDevice> sfcxDevice = {};

  std::unique_ptr<u8> biuData{ std::make_unique<STRIP_UNIQUE(biuData)>(0x10000) };
};
