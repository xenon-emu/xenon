// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define AUDIO_CTRLR_DEV_SIZE 0x40

namespace Xe {
namespace PCIDev {
namespace AUDIOCTRLR {

class AUDIOCTRLR : public PCIDevice {
public:
  AUDIOCTRLR(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u8 byteCount) override;
  void ConfigRead(u64 readAddress, u8 *data, u8 byteCount) override;
  void Write(u64 writeAddress, const u8 *data, u8 byteCount) override;
  void ConfigWrite(u64 writeAddress, const u8 *data, u8 byteCount) override;

private:
};

} // namespace AUDIOCTRLR
} // namespace PCIDev
} // namespace Xe
