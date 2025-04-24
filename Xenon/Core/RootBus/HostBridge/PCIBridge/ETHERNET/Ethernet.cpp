// Copyright 2025 Xenon Emulator Project

#include "Ethernet.h"

#include "Base/Logging/Log.h"

#define XE_NET_STATUS_INT 0x0000004C

Xe::PCIDev::ETHERNET::ETHERNET(const std::string &deviceName, u64 size) :
  PCIDevice(deviceName, size) {
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580A1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02100006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x02000001;
  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x80; // BAR0
}

void Xe::PCIDev::ETHERNET::Read(u64 readAddress, u8 *data, u64 size) {
  u8 offset = readAddress & 0xFF;

  return; // For now.

  switch (offset) {
  case Xe::PCIDev::TX_CONFIG:
    memcpy(data, &ethPciState.txConfigReg, size);
    break;
  case Xe::PCIDev::TX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.txDescriptorBaseReg, size);
    break;
  case Xe::PCIDev::TX_DESCRIPTOR_STATUS:
    memcpy(data, &ethPciState.txDescriptorStatusReg, size);
    break;
  case Xe::PCIDev::RX_CONFIG:
    memcpy(data, &ethPciState.rxConfigReg, size);
    break;
  case Xe::PCIDev::RX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.rxDescriptorBaseReg, size);
    break;
  case Xe::PCIDev::INTERRUPT_STATUS:
    memcpy(data, &ethPciState.interruptStatusReg, size);
    break;
  case Xe::PCIDev::INTERRUPT_MASK:
    memcpy(data, &ethPciState.interruptMaskReg, size);
    break;
  case Xe::PCIDev::CONFIG_0:
    memcpy(data, &ethPciState.config0Reg, size);
    break;
  case Xe::PCIDev::POWER:
    memcpy(data, &ethPciState.powerReg, size);
    break;
  case Xe::PCIDev::PHY_CONFIG:
    memcpy(data, &ethPciState.phyConfigReg, size);
    break;
  case Xe::PCIDev::PHY_CONTROL:
    memcpy(data, &ethPciState.phyControlReg, size);
    break;
  case Xe::PCIDev::CONFIG_1:
    memcpy(data, &ethPciState.config1Reg, size);
    break;
  case Xe::PCIDev::RETRY_COUNT:
    memcpy(data, &ethPciState.retryCountReg, size);
    break;
  case Xe::PCIDev::MULTICAST_FILTER_CONTROL:
    memcpy(data, &ethPciState.multicastFilterControlReg, size);
    break;
  case Xe::PCIDev::ADDRESS_0:
    memcpy(data, &ethPciState.address0Reg, size);
    break;
  case Xe::PCIDev::MULTICAST_HASH:
    memcpy(data, &ethPciState.multicastHashReg, size);
    break;
  case Xe::PCIDev::MAX_PACKET_SIZE:
    memcpy(data, &ethPciState.maxPacketSizeReg, size);
    break;
  case Xe::PCIDev::ADDRESS_1:
    memcpy(data, &ethPciState.address1Reg, size);
    break;
  default:
    LOG_ERROR(ETH, "Unknown PCI Reg being read {:#x}", static_cast<u16>(offset));
    memset(data, 0xFF, size);
    break;
  }
}

void Xe::PCIDev::ETHERNET::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::ETHERNET::Write(u64 writeAddress, const u8 *data, u64 size) {
  u8 offset = writeAddress & 0xFF;

  switch (offset) {
  case Xe::PCIDev::TX_CONFIG:
    memcpy(&ethPciState.txConfigReg, data, size);
    break;
  case Xe::PCIDev::TX_DESCRIPTOR_BASE:
    memcpy(&ethPciState.txDescriptorBaseReg, data, size);
    break;
  case Xe::PCIDev::TX_DESCRIPTOR_STATUS:
    memcpy(&ethPciState.txDescriptorStatusReg, data, size);
    break;
  case Xe::PCIDev::RX_CONFIG:
    memcpy(&ethPciState.rxConfigReg, data, size);
    break;
  case Xe::PCIDev::RX_DESCRIPTOR_BASE:
    memcpy(&ethPciState.rxDescriptorBaseReg, data, size);
    break;
  case Xe::PCIDev::INTERRUPT_STATUS:
    memcpy(&ethPciState.interruptStatusReg, data, size);
    break;
  case Xe::PCIDev::INTERRUPT_MASK:
    memcpy(&ethPciState.interruptMaskReg, data, size);
    break;
  case Xe::PCIDev::CONFIG_0:
    memcpy(&ethPciState.config0Reg, data, size);
    break;
  case Xe::PCIDev::POWER:
    memcpy(&ethPciState.powerReg, data, size);
    break;
  case Xe::PCIDev::PHY_CONFIG:
    memcpy(&ethPciState.phyConfigReg, data, size);
    break;
  case Xe::PCIDev::PHY_CONTROL:
    memcpy(&ethPciState.phyControlReg, data, size);
    break;
  case Xe::PCIDev::CONFIG_1:
    memcpy(&ethPciState.config1Reg, data, size);
    break;
  case Xe::PCIDev::RETRY_COUNT:
    memcpy(&ethPciState.retryCountReg, data, size);
    break;
  case Xe::PCIDev::MULTICAST_FILTER_CONTROL:
    memcpy(&ethPciState.multicastFilterControlReg, data, size);
    break;
  case Xe::PCIDev::ADDRESS_0:
    memcpy(&ethPciState.address0Reg, data, size);
    break;
  case Xe::PCIDev::MULTICAST_HASH:
    memcpy(&ethPciState.multicastHashReg, data, size);
    break;
  case Xe::PCIDev::MAX_PACKET_SIZE:
    memcpy(&ethPciState.maxPacketSizeReg, data, size);
    break;
  case Xe::PCIDev::ADDRESS_1:
    memcpy(&ethPciState.address1Reg, data, size);
    break;
  default:
    u64 tmp = 0;
    memcpy(&tmp, data, size);
    LOG_ERROR(ETH, "Unknown PCI Reg being written {:#x} data = {:#x}", static_cast<u16>(offset), tmp);
    break;
  }
}

void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {
  u8 offset = writeAddress & 0xFF;

  switch (offset) {
  case Xe::PCIDev::TX_CONFIG:
    memset(&ethPciState.txConfigReg, data, size);
    break;
  case Xe::PCIDev::TX_DESCRIPTOR_BASE:
    memset(&ethPciState.txDescriptorBaseReg, data, size);
    break;
  case Xe::PCIDev::TX_DESCRIPTOR_STATUS:
    memset(&ethPciState.txDescriptorStatusReg, data, size);
    break;
  case Xe::PCIDev::RX_CONFIG:
    memset(&ethPciState.rxConfigReg, data, size);
    break;
  case Xe::PCIDev::RX_DESCRIPTOR_BASE:
    memset(&ethPciState.rxDescriptorBaseReg, data, size);
    break;
  case Xe::PCIDev::INTERRUPT_STATUS:
    memset(&ethPciState.interruptStatusReg, data, size);
    break;
  case Xe::PCIDev::INTERRUPT_MASK:
    memset(&ethPciState.interruptMaskReg, data, size);
    break;
  case Xe::PCIDev::CONFIG_0:
    memset(&ethPciState.config0Reg, data, size);
    break;
  case Xe::PCIDev::POWER:
    memset(&ethPciState.powerReg, data, size);
    break;
  case Xe::PCIDev::PHY_CONFIG:
    memset(&ethPciState.phyConfigReg, data, size);
    break;
  case Xe::PCIDev::PHY_CONTROL:
    memset(&ethPciState.phyControlReg, data, size);
    break;
  case Xe::PCIDev::CONFIG_1:
    memset(&ethPciState.config1Reg, data, size);
    break;
  case Xe::PCIDev::RETRY_COUNT:
    memset(&ethPciState.retryCountReg, data, size);
    break;
  case Xe::PCIDev::MULTICAST_FILTER_CONTROL:
    memset(&ethPciState.multicastFilterControlReg, data, size);
    break;
  case Xe::PCIDev::ADDRESS_0:
    memset(&ethPciState.address0Reg, data, size);
    break;
  case Xe::PCIDev::MULTICAST_HASH:
    memset(&ethPciState.multicastHashReg, data, size);
    break;
  case Xe::PCIDev::MAX_PACKET_SIZE:
    memset(&ethPciState.maxPacketSizeReg, data, size);
    break;
  case Xe::PCIDev::ADDRESS_1:
    memset(&ethPciState.address1Reg, data, size);
    break;
  default:
    u64 tmp = 0;
    memset(&tmp, data, size);
    LOG_ERROR(ETH, "Unknown PCI Reg being written {:#x} data = {:#x}", static_cast<u16>(offset), tmp);
    break;
  }
}

void Xe::PCIDev::ETHERNET::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  // Check if we're being scanned
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          tmp &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
      tmp = 0; // Register not implemented
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
}
