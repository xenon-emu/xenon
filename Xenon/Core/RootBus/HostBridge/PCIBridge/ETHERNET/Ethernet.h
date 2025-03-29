// Copyright 2025 Xenon Emulator Project

//
// Xenon Fast Ethernet Adapter Emulation
//

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define ETHERNET_DEV_SIZE 0x80

namespace Xe {
namespace PCIDev {
namespace ETHERNET {

// Register Set and offsets.
// Taken from Linux kernel patches for the Xbox 360.
enum XE_ETH_REGISTERS {
  TX_CONFIG = 0x00,
  TX_DESCRIPTOR_BASE = 0x04,
  TX_DESCRIPTOR_STATUS = 0x0C,
  RX_CONFIG = 0x10,
  RX_DESCRIPTOR_BASE = 0x14,
  INTERRUPT_STATUS = 0x20,
  INTERRUPT_MASK = 0x24,
  CONFIG_0 = 0x28,
  POWER = 0x30,
  PHY_CONFIG = 0x40,
  PHY_CONTROL = 0x44,
  CONFIG_1 = 0x50,
  RETRY_COUNT = 0x54,
  MULTICAST_FILTER_CONTROL = 0x60,
  ADDRESS_0 = 0x62,
  MULTICAST_HASH = 0x68,
  MAX_PACKET_SIZE = 0x78,
  ADDRESS_1 = 0x7A
};

// Xenon Fast Ethernet PCI Device State struct.
struct XE_PCI_STATE {
  u32 txConfigReg;
  u32 txDescriptorBaseReg;
  u32 txDescriptorStatusReg;
  u32 rxConfigReg;
  u32 rxDescriptorBaseReg;
  u32 interruptStatusReg;
  u32 interruptMaskReg;
  u32 config0Reg;
  u32 powerReg;
  u32 phyConfigReg;
  u32 phyControlReg;
  u32 config1Reg;
  u32 retryCountReg;
  u32 multicastFilterControlReg;
  u32 address0Reg;
  u32 multicastHashReg;
  u32 maxPacketSizeReg;
  u32 address1Reg;
};

class ETHERNET : public PCIDevice {
public:
  ETHERNET(const char *deviceName, u64 size);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
  XE_PCI_STATE ethPciState{};
};

} // namespace ETHERNET
} // namespace PCIDev
} // namespace Xe
