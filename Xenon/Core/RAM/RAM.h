// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/SystemDevice.h"
#include <memory>

#define RAM_START_ADDR 0
#define RAM_SIZE 0x20000000

class RAM : public SystemDevice {
public:
  RAM(const std::string &deviceName, u64 startAddress, u64 endAddress,
    bool isSOCDevice);
  ~RAM();
  void Reset();
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

  u8 *getPointerToAddress(u32 address);

private:
  std::unique_ptr<u8[]> RAMData{};
};
