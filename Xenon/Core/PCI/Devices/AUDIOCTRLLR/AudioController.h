// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Core/PCI/PCIDevice.h"

#define AUDIO_CTRLR_DEV_SIZE 0x40

namespace Xe {
namespace PCIDev {

class AUDIOCTRLR : public PCIDevice {
public:
  AUDIOCTRLR(const std::string &deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
};

} // namespace PCIDev
} // namespace Xe
