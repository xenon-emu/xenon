// Copyright 2025 Xenon Emulator Project. All rights reserved.

//
// Xenon Fast Ethernet Adapter Emulation
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define ETHERNET_DEV_SIZE 0x80

namespace Xe {
namespace PCIDev {

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
  // Transmission
  u32 txConfigReg;
  u32 txDescriptorBaseReg;
  u32 txDescriptorStatusReg;

  // Reception
  u32 rxConfigReg;
  u32 rxDescriptorBaseReg;

  // Interrupts
  u32 interruptStatusReg;
  u32 interruptMaskReg;

  // Configuration and power
  u32 config0Reg;
  u32 powerReg;
  u32 phyConfigReg;
  u32 phyControlReg;
  u32 config1Reg;
  u32 retryCountReg;

  // Multicast filter control
  u32 multicastFilterControlReg;

  // MAC address (6 bytes stored as array)
  u8 macAddress[6];

  // Multicast hash filters
  u32 multicastHashFilter0; // Possibly at 0x68
  u32 multicastHashFilter1; // Possibly at 0x6C

  // Packet limits and secondary address
  u32 maxPacketSizeReg;
  // MAC address 2 (6 bytes stored as array)
  u8 macAddress2[6];
};

class ETHERNET : public PCIDevice {
public:
  ETHERNET(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
  // MDIO Read
  u32 MdioRead(u32 addr);
  // MDIO Write
  void MdioWrite(u32 val);
  // RX Descriptors
  void ProcessRxDescriptors();
  // TX Descriptors
  void ProcessTxDescriptors();
  // Handle RX Packets
  void HandleTxPacket(const u8 *data, u32 len);
  // Handle TX Packets
  void HandleRxPacket(const u8* data, u32 len);

  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus = nullptr;
  // RAM Pointer
  RAM *ramPtr = nullptr;
  // MDIO Registers
  u16 mdioRegisters[32][32] = {};
  XE_PCI_STATE ethPciState = {};
  bool rxEnabled = false;
  bool txEnabled = false;
};

} // namespace PCIDev
} // namespace Xe
