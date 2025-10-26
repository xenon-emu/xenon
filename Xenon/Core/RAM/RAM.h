/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>

#include "Base/SystemDevice.h"

#define RAM_START_ADDR 0

class RAM : public SystemDevice {
public:
  RAM(const std::string &deviceName, u64 startAddress, std::string size,
    bool isSOCDevice);
  ~RAM();
  void Reset();
  void Resize(u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

  u8 *GetPointerToAddress(u32 address);
  u64 GetSize() {
    return ramSize;
  }
private:
  u64 ramSize = 0;
  std::unique_ptr<u8[]> ramData{};
};
