// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define XMA_DEV_SIZE 0x400

// Taken from:
// https://github.com/xenia-canary/xenia-canary/blob/canary_experimental/src/xenia/apu/xma_register_table.inc
enum XE_XMA_REGISTERS {
  CONTEXT_ARRAY_ADDRESS = 0x0600,
  CURRENT_CONTEXT_INDEX = 0x0606,
  NEXT_CONTEXT_INDEX = 0x0607,
  UNKNOWN_610 = 0x0610,
  UNKNOWN_620 = 0x0620,
  KICK = 0x0650,
  UNKNOWN_660 = 0x0660,
  LOCK = 0x0690,
  CLEAR = 0x06A0
};

class XMA : public PCIDevice {
public:
  XMA(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
};
