// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define EHCI1_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {
namespace EHCI1 {

class EHCI1 : public PCIDevice {
public:
  EHCI1(const std::string &deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
};

} // namespace EHCI1
} // namespace PCIDev
} // namespace Xe
