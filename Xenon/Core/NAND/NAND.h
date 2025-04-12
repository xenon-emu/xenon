// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fstream>
#include <filesystem>
#include <vector>

#include "Base/SystemDevice.h"
#include "Core/RootBus/HostBridge/PCIBridge/SFCX/SFCX.h"

#define NAND_START_ADDR 0xC8000000
#define NAND_END_ADDR 0xCC000000 // 64 Mb region

class NAND : public SystemDevice {
public:
  NAND(const char *deviceName, SFCX* sfcxDevicePtr,
    u64 startAddress, u64 endAddress,
    bool isSOCDevice);
  ~NAND();

  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

private:
  SFCX* sfcxDevice;
};
