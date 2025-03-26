// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define XMA_DEV_SIZE 0x400

class XMA : public PCIDevice {
public:
  XMA(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u8 byteCount) override;
  void ConfigRead(u64 readAddress, u8*data, u8 byteCount) override;
  void Write(u64 writeAddress, const u8 *data, u8 byteCount) override;
  void ConfigWrite(u64 writeAddress, const u8 *data, u8 byteCount) override;

private:
};
