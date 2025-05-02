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
  // xboxkrnld looks for these values, and will return E75 if it doesn't get it
  mdioRegisters[1][2] = 0x0015; // PHY ID1 (expected OUI MSBs)
  mdioRegisters[1][3] = 0x0141; // PHY ID2 (expected OUI LSBs + model/rev)
}

void Xe::PCIDev::ETHERNET::Read(u64 readAddress, u8 *data, u64 size) {
  u8 offset = readAddress & 0xFF;
  
  switch (offset) {
  case TX_CONFIG:
    memcpy(data, &ethPciState.txConfigReg, size);
    break;
  case TX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.txDescriptorBaseReg, size);
    break;
  case TX_DESCRIPTOR_STATUS:
    memcpy(data, &ethPciState.txDescriptorStatusReg, size);
    break;
  case RX_CONFIG:
    memcpy(data, &ethPciState.rxConfigReg, size);
    break;
  case RX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.rxDescriptorBaseReg, size);
    break;
  case INTERRUPT_STATUS:
    memcpy(data, &ethPciState.interruptStatusReg, size);
    break;
  case INTERRUPT_MASK:
    memcpy(data, &ethPciState.interruptMaskReg, size);
    break;
  case CONFIG_0:
    memcpy(data, &ethPciState.config0Reg, size);
    break;
  case POWER:
    memcpy(data, &ethPciState.powerReg, size);
    break;
  case PHY_CONFIG:
    memcpy(data, &ethPciState.phyConfigReg, size);
    break;
  case PHY_CONTROL: {
    u32 regVal = PhyReadReg();
    memcpy(data, &regVal, size);
  } break;
  case CONFIG_1:
    memcpy(data, &ethPciState.config1Reg, size);
    break;
  case RETRY_COUNT:
    memcpy(data, &ethPciState.retryCountReg, size);
    break;
  case MULTICAST_FILTER_CONTROL:
    memcpy(data, &ethPciState.multicastFilterControlReg, size);
    break;
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(data, &ethPciState.macAddress[(offset - ADDRESS_0)], size);
    break;
  case MULTICAST_HASH + 0x0:
    memcpy(data, &ethPciState.multicastHashFilter0, size);
    break;
  case MULTICAST_HASH + 0x4:
    memcpy(data, &ethPciState.multicastHashFilter1, size);
    break;
  case MAX_PACKET_SIZE:
    memcpy(data, &ethPciState.maxPacketSizeReg, size);
    break;
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(data, &ethPciState.macAddress2[(offset - ADDRESS_1)], size);
    break;
  default:
    LOG_ERROR(ETH, "Register '{:#x}' is unknown! Attempted to read {} bytes", static_cast<u16>(offset), size);
    memset(data, 0xFF, size);
    break;
  }
}

void Xe::PCIDev::ETHERNET::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::ETHERNET::Write(u64 writeAddress, const u8 *data, u64 size) {
  u8 offset = writeAddress & 0xFF;

  u32 val = 0;
  val = byteswap_be(val);
  memcpy(&val, data, size);
  switch (offset) {
  case TX_CONFIG:
    ethPciState.txConfigReg = val;
    LOG_DEBUG(ETH, "TX_CONFIG = 0x{:X}", val);
    break;
  case TX_DESCRIPTOR_BASE:
    ethPciState.txDescriptorBaseReg = val;
    LOG_DEBUG(ETH, "TX_DESCRIPTOR_BASE = 0x{:X}", val);
    break;
  case TX_DESCRIPTOR_STATUS:
    ethPciState.txDescriptorStatusReg = val;
    LOG_DEBUG(ETH, "TX_DESCRIPTOR_STATUS = 0x{:X}", val);
    break;
  case RX_CONFIG:
    ethPciState.rxConfigReg = val;
    LOG_DEBUG(ETH, "RX_CONFIG = 0x{:X}", val);
    break;
  case RX_DESCRIPTOR_BASE:
    ethPciState.rxDescriptorBaseReg = val;
    LOG_DEBUG(ETH, "RX_DESCRIPTOR_BASE = 0x{:X}", val);
    break;
  case INTERRUPT_STATUS:
    ethPciState.interruptStatusReg &= ~val;
    LOG_DEBUG(ETH, "INTERRUPT_STATUS (ACK) = 0x{:X} -> 0x{:X}", val, ethPciState.interruptStatusReg);
    break;
  case INTERRUPT_MASK:
    ethPciState.interruptMaskReg = val;
    LOG_DEBUG(ETH, "INTERRUPT_MASK = 0x{:X}", val);
    break;
  case CONFIG_0:
    ethPciState.config0Reg = val;
    LOG_DEBUG(ETH, "CONFIG_0 = 0x{:X}", val);
    break;
  case POWER:
    ethPciState.powerReg = val;
    LOG_DEBUG(ETH, "POWER = 0x{:X}", val);
    break;
  case PHY_CONFIG:
    ethPciState.phyConfigReg = val;
    LOG_DEBUG(ETH, "PHY_CONFIG = 0x{:X}", val);
    break;
  case PHY_CONTROL: {
    ethPciState.phyControlReg = val;
    u8 phyAddr = (ethPciState.phyConfigReg >> 8) & 0x1F;
    u8 regNum  = (ethPciState.phyConfigReg >> 0) & 0x1F;
    bool isWrite = ethPciState.phyConfigReg & (1 << 16);

    if (phyAddr < 32 && regNum < 32) {
      if (isWrite) {
        u16 writeVal = static_cast<u16>(val & 0xFFFF);
        switch (regNum) {
        case 0: {
          if (writeVal & 0x8000) {
            // Bit 15 is the reset bit - emulate reset behavior
            writeVal &= ~0x8000; // Clear the reset bit immediately
          }
        } break;
        default: {
          // Handle as normal until needed
        } break;
        }
        LOG_INFO(ETH, "PHY addr {} reg {} with a value of 0x{:X}", phyAddr, regNum, writeVal);
        mdioRegisters[phyAddr][regNum] = writeVal;
      } else {
        ethPciState.phyControlReg = (1u << 31) | mdioRegisters[phyAddr][regNum];
      }
    } else {
      LOG_WARNING(ETH, "MDIO access to invalid PHY addr {} reg {}", phyAddr, regNum);
    }
  } break;
  case CONFIG_1:
    ethPciState.config1Reg = val;
    LOG_DEBUG(ETH, "CONFIG_1 = 0x{:X}", val);
    break;
  case RETRY_COUNT:
    ethPciState.retryCountReg = val;
    LOG_DEBUG(ETH, "RETRY_COUNT = 0x{:X}", val);
    break;
  case MULTICAST_FILTER_CONTROL:
    ethPciState.multicastFilterControlReg = val;
    LOG_DEBUG(ETH, "MULTICAST_FILTER_CONTROL = 0x{:X}", val);
    break;
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(&ethPciState.macAddress[(offset - ADDRESS_0)], data, size);
    LOG_DEBUG(ETH, "macAddress[{}] = 0x{:X}", (offset - ADDRESS_0), (u32)*data);
    break;
  case MULTICAST_HASH + 0x0:
    ethPciState.multicastHashFilter0 = val;
    LOG_DEBUG(ETH, "MULTICAST_HASH_0 = 0x{:X}", val);
    break;
  case MULTICAST_HASH + 0x4:
    ethPciState.multicastHashFilter1 = val;
    LOG_DEBUG(ETH, "MULTICAST_HASH_1 = 0x{:X}", val);
    break;
  case MAX_PACKET_SIZE:
    ethPciState.maxPacketSizeReg = val;
    LOG_DEBUG(ETH, "MAX_PACKET_SIZE = 0x{:X}", val);
    break;
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(&ethPciState.macAddress2[(offset - ADDRESS_1)], data, size);
    LOG_DEBUG(ETH, "macAddress2[{}] = 0x{:X}", (offset - ADDRESS_1), (u32)*data);
    break;
  default:
    LOG_ERROR(ETH, "Register '{:#x}' is unknown! Data = {:#x} ({}b)", static_cast<u16>(offset), val, size);
    break;
  }
}

void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {
  u8 offset = writeAddress & 0xFF;

  switch (offset) {
  case TX_CONFIG:
    ethPciState.txConfigReg = data;
    LOG_DEBUG(ETH, "TX_CONFIG = 0x{:X}", data);
    break;
  case TX_DESCRIPTOR_BASE:
    ethPciState.txDescriptorBaseReg = data;
    LOG_DEBUG(ETH, "TX_DESCRIPTOR_BASE = 0x{:X}", data);
    break;
  case TX_DESCRIPTOR_STATUS:
    ethPciState.txDescriptorStatusReg = data;
    LOG_DEBUG(ETH, "TX_DESCRIPTOR_STATUS = 0x{:X}", data);
    break;
  case RX_CONFIG:
    ethPciState.rxConfigReg = data;
    LOG_DEBUG(ETH, "RX_CONFIG = 0x{:X}", data);
    break;
  case RX_DESCRIPTOR_BASE:
    ethPciState.rxDescriptorBaseReg = data;
    LOG_DEBUG(ETH, "RX_DESCRIPTOR_BASE = 0x{:X}", data);
    break;
  case INTERRUPT_STATUS:
    ethPciState.interruptStatusReg = data;
    LOG_DEBUG(ETH, "INTERRUPT_STATUS (ACK) = 0x{:X} -> 0x{:X}", data, ethPciState.interruptStatusReg);
    break;
  case INTERRUPT_MASK:
    ethPciState.interruptMaskReg = data;
    LOG_DEBUG(ETH, "INTERRUPT_MASK = 0x{:X}", data);
    break;
  case CONFIG_0:
    ethPciState.config0Reg = data;
    LOG_DEBUG(ETH, "CONFIG_0 = 0x{:X}", data);
    break;
  case POWER:
    ethPciState.powerReg = data;
    LOG_DEBUG(ETH, "POWER = 0x{:X}", data);
    break;
  case PHY_CONFIG:
    ethPciState.phyConfigReg = data;
    LOG_DEBUG(ETH, "PHY_CONFIG = 0x{:X}", data);
    break;
  case PHY_CONTROL:
    ethPciState.phyControlReg = data;
    LOG_DEBUG(ETH, "PHY_CONTROL = 0x{:X}", data);
    break;
  case CONFIG_1:
    ethPciState.config1Reg = data;
    LOG_DEBUG(ETH, "CONFIG_1 = 0x{:X}", data);
    break;
  case RETRY_COUNT:
    ethPciState.retryCountReg = data;
    LOG_DEBUG(ETH, "RETRY_COUNT = 0x{:X}", data);
    break;
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memset(&ethPciState.macAddress[(offset - ADDRESS_0)], data, size);
    LOG_DEBUG(ETH, "macAddress[{}] = 0x{:X}", (offset - ADDRESS_0), (u32)data);
    break;
  case MULTICAST_FILTER_CONTROL:
    ethPciState.multicastFilterControlReg = data;
    LOG_DEBUG(ETH, "MULTICAST_FILTER_CONTROL = 0x{:X}", data);
    break;
  case MULTICAST_HASH + 0x0:
    ethPciState.multicastHashFilter0 = data;
    LOG_DEBUG(ETH, "MULTICAST_HASH_0 = 0x{:X}", data);
    break;
  case MULTICAST_HASH + 0x4:
    ethPciState.multicastHashFilter1 = data;
    LOG_DEBUG(ETH, "MULTICAST_HASH_1 = 0x{:X}", data);
    break;
  case MAX_PACKET_SIZE:
    ethPciState.maxPacketSizeReg = data;
    LOG_DEBUG(ETH, "MAX_PACKET_SIZE = 0x{:X}", data);
    break;
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memset(&ethPciState.macAddress2[(offset - ADDRESS_1)], data, size);
    LOG_DEBUG(ETH, "macAddress2[{}] = 0x{:X}", (offset - ADDRESS_1), (u32)data);
    break;
  default:
    LOG_ERROR(ETH, "Register '{:#x}' is unknown! Data = {:#x} ({}b)", static_cast<u16>(offset), data, size);
    break;
  }
}

u32 Xe::PCIDev::ETHERNET::PhyReadReg() {
  u8 phyAddr = (ethPciState.phyConfigReg >> 8) & 0x1F;
  u8 regNum  = (ethPciState.phyConfigReg >> 0) & 0x1F;

  if (phyAddr >= 32 || regNum >= 32) {
    LOG_WARNING(ETH, "MDIO read from invalid PHY addr {} reg {}", phyAddr, regNum);
    return 0xFFFFFFFF; // Error pattern
  }

  u16 val = mdioRegisters[phyAddr][regNum];
  LOG_INFO(ETH, "PHY Read: addr {} reg {} = 0x{:X}", phyAddr, regNum, val);
  return (1u << 31) | val; // Bit 31 signals read complete
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
