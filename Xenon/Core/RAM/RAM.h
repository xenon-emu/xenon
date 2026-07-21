/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>

#include "Base/SystemDevice.h"

#define RAM_START_ADDR 0

class RAM : public SystemDevice {
public:
  RAM(u64 startAddress, u64 size, bool isSOCDevice);
  ~RAM();

  void Reset();

  static u64 ParseRamSize(std::string size);

  void Resize(u64 size);
  void Read(u64 address, u8 *data, u64 size) override;
  void Write(u64 address, const u8 *data, u64 size) override;
  void MemSet(u64 address, s32 data, u64 size) override;

  u8 *GetPointerToAddress(u32 address);
  u64 GetSize() {
    return ramSize;
  }
private:
  u64 ramSize = 0;
  std::unique_ptr<u8[]> ramData{};
};
