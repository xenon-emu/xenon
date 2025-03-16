// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define EHCI0_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace EHCI0 {

class EHCI0 : public PCIDevice {
public:
  EHCI0(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u8 byteCount) override;
  void ConfigRead(u64 readAddress, u8 *data, u8 byteCount) override;
  void Write(u64 writeAddress, u8 *data, u8 byteCount) override;
  void ConfigWrite(u64 writeAddress, u8 *data, u8 byteCount) override;

private:
};

} // namespace EHCI0
} // namespace PCIDev
} // namespace Xe
