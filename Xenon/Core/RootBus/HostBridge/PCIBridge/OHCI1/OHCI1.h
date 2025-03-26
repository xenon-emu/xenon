// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define OHCI1_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace OHCI1 {

class OHCI1 : public PCIDevice {
public:
  OHCI1(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u8 byteCount) override;
  void ConfigRead(u64 readAddress, u8 *data, u8 byteCount) override;
  void Write(u64 writeAddress, const u8 *data, u8 byteCount) override;
  void ConfigWrite(u64 writeAddress, const u8 *data, u8 byteCount) override;

private:
};

} // namespace OHCI1
} // namespace PCIDev
} // namespace Xe
