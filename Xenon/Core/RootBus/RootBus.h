// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <unordered_map>

#include "Base/SystemDevice.h"
#include "Core/PCI/Bridge/HostBridge.h"

// PCI Configuration region
#define PCI_CONFIG_REGION_ADDRESS 0xD0000000
#define PCI_CONFIG_REGION_SIZE 0x1000000

class RootBus {
public:
  RootBus();
  ~RootBus();

  void AddHostBridge(std::shared_ptr<HostBridge> newHostBridge);

  void AddDevice(std::shared_ptr<SystemDevice> device);

  void ResetDevice(std::shared_ptr<SystemDevice> device);

  bool Read(u64 readAddress, u8 *data, u64 size, bool soc = false);
  bool Write(u64 writeAddress, const u8 *data, u64 size, bool soc = false);
  bool MemSet(u64 writeAddress, s32 data, u64 size);

  // Configuration Space R/W
  bool ConfigRead(u64 readAddress, u8 *data, u64 size);
  bool ConfigWrite(u64 writeAddress, const u8 *data, u64 size);

private:
  std::shared_ptr<HostBridge> hostBridge{};
  u32 deviceCount;
  std::unordered_map<std::string, std::shared_ptr<SystemDevice>> connectedDevices;

  std::unique_ptr<u8> biuData{ std::make_unique<STRIP_UNIQUE(biuData)>(0x10000) };
};
