// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define OHCI0_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace OHCI0 {

class OHCI0 : public PCIDevice {
public:
  OHCI0(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u8 byteCount) override;
  void ConfigRead(u64 readAddress, u8 *data, u8 byteCount) override;
  void Write(u64 writeAddress, const u8 *data, u8 byteCount) override;
  void ConfigWrite(u64 writeAddress, const u8 *data, u8 byteCount) override;

private:
};

} // namespace OHCI0
} // namespace PCIDev
} // namespace Xe
