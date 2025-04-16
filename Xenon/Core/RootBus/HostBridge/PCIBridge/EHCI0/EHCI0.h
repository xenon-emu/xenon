// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define EHCI0_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace EHCI0 {

class EHCI0 : public PCIDevice {
public:
  EHCI0(const std::string &deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
};

} // namespace EHCI0
} // namespace PCIDev
} // namespace Xe
