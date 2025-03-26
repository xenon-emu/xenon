// Copyright 2025 Xenon Emulator Project

#include "Ethernet.h"

#include "Base/Logging/Log.h"

#define XE_NET_STATUS_INT 0x0000004C

Xe::PCIDev::ETHERNET::ETHERNET::ETHERNET(const char *deviceName, u64 size) :
  PCIDevice(deviceName, size) {
  // Set PCI Properties.
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580A1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02100006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x02000001;
  // Set our PCI Dev Sizes.
  pciDevSizes[0] = 0x80; // BAR0
}

void Xe::PCIDev::ETHERNET::ETHERNET::Read(u64 readAddress, u8 *data, u8 byteCount) {
  u8 offset = readAddress & 0xFF;

  return; // For now.

  switch (offset) {
  case Xe::PCIDev::ETHERNET::TX_CONFIG:
    memcpy(data, &ethPciState.txConfigReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::TX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.txDescriptorBaseReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::TX_DESCRIPTOR_STATUS:
    memcpy(data, &ethPciState.txDescriptorStatusReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::RX_CONFIG:
    memcpy(data, &ethPciState.rxConfigReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::RX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.rxDescriptorBaseReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::INTERRUPT_STATUS:
    memcpy(data, &ethPciState.interruptStatusReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::INTERRUPT_MASK:
    memcpy(data, &ethPciState.interruptMaskReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::CONFIG_0:
    memcpy(data, &ethPciState.config0Reg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::POWER:
    memcpy(data, &ethPciState.powerReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::PHY_CONFIG:
    memcpy(data, &ethPciState.phyConfigReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::PHY_CONTROL:
    memcpy(data, &ethPciState.phyControlReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::CONFIG_1:
    memcpy(data, &ethPciState.config1Reg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::RETRY_COUNT:
    memcpy(data, &ethPciState.retryCountReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::MULTICAST_FILTER_CONTROL:
    memcpy(data, &ethPciState.multicastFilterControlReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::ADDRESS_0:
    memcpy(data, &ethPciState.address0Reg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::MULTICAST_HASH:
    memcpy(data, &ethPciState.multicastHashReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::MAX_PACKET_SIZE:
    memcpy(data, &ethPciState.maxPacketSizeReg, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::ADDRESS_1:
    memcpy(data, &ethPciState.address1Reg, byteCount);
    break;
  default:
    LOG_ERROR(ETH, "Unknown PCI Reg being read {:#x}", static_cast<u16>(offset));
    memset(data, 0xFF, byteCount);
    break;
  }
}

void Xe::PCIDev::ETHERNET::ETHERNET::ConfigRead(u64 readAddress, u8 *data, u8 byteCount) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], byteCount);
}

void Xe::PCIDev::ETHERNET::ETHERNET::Write(u64 writeAddress, const u8 *data, u8 byteCount) {
  u8 offset = writeAddress & 0xFF;

  switch (offset) {
  case Xe::PCIDev::ETHERNET::TX_CONFIG:
    memcpy(&ethPciState.txConfigReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::TX_DESCRIPTOR_BASE:
    memcpy(&ethPciState.txDescriptorBaseReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::TX_DESCRIPTOR_STATUS:
    memcpy(&ethPciState.txDescriptorStatusReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::RX_CONFIG:
    memcpy(&ethPciState.rxConfigReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::RX_DESCRIPTOR_BASE:
    memcpy(&ethPciState.rxDescriptorBaseReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::INTERRUPT_STATUS:
    memcpy(&ethPciState.interruptStatusReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::INTERRUPT_MASK:
    memcpy(&ethPciState.interruptMaskReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::CONFIG_0:
    memcpy(&ethPciState.config0Reg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::POWER:
    memcpy(&ethPciState.powerReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::PHY_CONFIG:
    memcpy(&ethPciState.phyConfigReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::PHY_CONTROL:
    memcpy(&ethPciState.phyControlReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::CONFIG_1:
    memcpy(&ethPciState.config1Reg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::RETRY_COUNT:
    memcpy(&ethPciState.retryCountReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::MULTICAST_FILTER_CONTROL:
    memcpy(&ethPciState.multicastFilterControlReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::ADDRESS_0:
    memcpy(&ethPciState.address0Reg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::MULTICAST_HASH:
    memcpy(&ethPciState.multicastHashReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::MAX_PACKET_SIZE:
    memcpy(&ethPciState.maxPacketSizeReg, data, byteCount);
    break;
  case Xe::PCIDev::ETHERNET::ADDRESS_1:
    memcpy(&ethPciState.address1Reg, data, byteCount);
    break;
  default:
    u64 tmp = 0;
    memcpy(&tmp, data, byteCount);
    LOG_ERROR(ETH, "Unknown PCI Reg being written {:#x} data = {:#x}", static_cast<u16>(offset), tmp);
    break;
  }
}

void Xe::PCIDev::ETHERNET::ETHERNET::ConfigWrite(u64 writeAddress, const u8 *data, u8 byteCount) {
  // Check if we're being scanned.
  u64 tmp = 0;
  memcpy(&tmp, data, byteCount);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery.
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
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address.
      tmp = 0; // Register not implemented.
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, byteCount);
}
