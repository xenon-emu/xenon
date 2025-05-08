// Copyright 2025 Xenon Emulator Project

#include "Ethernet.h"

#include "Base/Logging/Log.h"
#include "Base/Thread.h"

#define XE_NET_STATUS_INT 0x0000004C

Xe::PCIDev::ETHERNET::ETHERNET(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram) :
  PCIDevice(deviceName, size), ramPtr(ram), parentBus(parentPCIBridge) {
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580A1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02100006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x02000001;
  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x80; // BAR0
  memset(mdioRegisters, 0, sizeof(mdioRegisters));
  // xboxkrnld looks for these values, and will return E75 if it doesn't get it
  // Setup MDIO
  for (int phy = 0; phy < 32; ++phy) {
    for (int r = 0; r < 32; ++r) {
      mdioRegisters[phy][r] = 0;
    }
    mdioRegisters[phy][0] = 0x1140; // Basic Control (Auto-Negotiation Enabled)
    mdioRegisters[phy][1] = 0x796D; // Basic Status (Auto-Neg Capable, Link Status)
    mdioRegisters[phy][2] = 0x0141; // Marvell OUI MSBs
    mdioRegisters[phy][3] = 0x0CC2; // 88E1111 Model/Revision
  }
}

void Xe::PCIDev::ETHERNET::Read(u64 readAddress, u8 *data, u64 size) {
  u8 offset = readAddress & 0xFF;

  switch (offset) {
  case TX_CONFIG:
    if (txEnabled)
      ethPciState.txConfigReg |= 0x1 << 31;
    ProcessTxDescriptors();
    memcpy(data, &ethPciState.txConfigReg, size);
    LOG_DEBUG(ETH, "[Read] TX_CONFIG = 0x{:X}", ethPciState.txConfigReg);
    break;
  case TX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.txDescriptorBaseReg, size);
    LOG_DEBUG(ETH, "[Read] TX_DESCRIPTOR_BASE = 0x{:X}", ethPciState.txDescriptorBaseReg);
    break;
  case TX_DESCRIPTOR_STATUS:
    memcpy(data, &ethPciState.txDescriptorStatusReg, size);
    LOG_DEBUG(ETH, "[Read] TX_DESCRIPTOR_STATUS = 0x{:X}", ethPciState.txDescriptorStatusReg);
    ProcessTxDescriptors();
    break;
  case RX_CONFIG:
    if (rxEnabled)
      ethPciState.rxConfigReg |= 0x1 << 31;
    ProcessRxDescriptors();
    memcpy(data, &ethPciState.rxConfigReg, size);
    LOG_DEBUG(ETH, "[Read] RX_CONFIG = 0x{:X}", ethPciState.rxConfigReg);
    break;
  case RX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.rxDescriptorBaseReg, size);
    LOG_DEBUG(ETH, "[Read] RX_DESCRIPTOR_BASE = 0x{:X}", ethPciState.rxDescriptorBaseReg);
    break;
  case INTERRUPT_STATUS:
    memcpy(data, &ethPciState.interruptStatusReg, size);
    LOG_DEBUG(ETH, "[Read] INTERRUPT_STATUS = 0x{:X}", ethPciState.interruptStatusReg);
    break;
  case INTERRUPT_MASK:
    memcpy(data, &ethPciState.interruptMaskReg, size);
    LOG_DEBUG(ETH, "[Read] INTERRUPT_MASK = 0x{:X}", ethPciState.interruptMaskReg);
    break;
  case CONFIG_0:
    memcpy(data, &ethPciState.config0Reg, size);
    LOG_DEBUG(ETH, "[Read] CONFIG_0 = 0x{:X}", ethPciState.config0Reg);
    break;
  case POWER:
    memcpy(data, &ethPciState.powerReg, size);
    LOG_DEBUG(ETH, "[Read] POWER = 0x{:X}", ethPciState.powerReg);
    break;
  case PHY_CONFIG:
    memcpy(data, &ethPciState.phyConfigReg, size);
    LOG_DEBUG(ETH, "[Read] PHY_CONFIG = 0x{:X}", ethPciState.phyConfigReg);
    break;
  case PHY_CONTROL: {
    MdioRead(ethPciState.phyControlReg);
    memcpy(data, &ethPciState.phyControlReg, size);
    LOG_DEBUG(ETH, "[Read] PHY_CONTROL (MDIO) = 0x{:08X}", ethPciState.phyControlReg);
  } break;
  case CONFIG_1:
    memcpy(data, &ethPciState.config1Reg, size);
    LOG_DEBUG(ETH, "[Read] CONFIG_1 = 0x{:X}", ethPciState.config1Reg);
    break;
  case RETRY_COUNT:
    memcpy(data, &ethPciState.retryCountReg, size);
    LOG_DEBUG(ETH, "[Read] RETRY_COUNT = 0x{:X}", ethPciState.retryCountReg);
    break;
  case MULTICAST_FILTER_CONTROL:
    memcpy(data, &ethPciState.multicastFilterControlReg, size);
    LOG_DEBUG(ETH, "[Read] MULTICAST_FILTER_CONTROL = 0x{:X}", ethPciState.multicastFilterControlReg);
    break;
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(data, &ethPciState.macAddress[(offset - ADDRESS_0)], size);
    LOG_DEBUG(ETH, "[Read] ADDRESS[1][{}] = 0x{:X}", (offset - ADDRESS_0), ethPciState.macAddress[(offset - ADDRESS_0)]);
    break;
  case MULTICAST_HASH + 0x0:
    memcpy(data, &ethPciState.multicastHashFilter0, size);
    LOG_DEBUG(ETH, "[Read] MULTICAST_HASH = 0x{:X}", ethPciState.multicastHashFilter0);
    break;
  case MULTICAST_HASH + 0x4:
    LOG_DEBUG(ETH, "[Read] MULTICAST_HASH1 = 0x{:X}", ethPciState.multicastHashFilter1);
    memcpy(data, &ethPciState.multicastHashFilter1, size);
    break;
  case MAX_PACKET_SIZE:
    memcpy(data, &ethPciState.maxPacketSizeReg, size);
    LOG_DEBUG(ETH, "[Read] MAX_PACKET_SIZE = 0x{:X}", ethPciState.maxPacketSizeReg);
    break;
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(data, &ethPciState.macAddress2[(offset - ADDRESS_1)], size);
    LOG_DEBUG(ETH, "[Read] ADDRESS[1][{}] = 0x{:X}", (offset - ADDRESS_1), ethPciState.macAddress2[(offset - ADDRESS_1)]);
    break;
  default:
    LOG_ERROR(ETH, "Register '0x{:X}' is unknown! Attempted to read {} bytes", static_cast<u16>(offset), size);
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
  memcpy(&val, data, size);
  switch (offset) {
  case TX_CONFIG:
    ethPciState.txConfigReg = val;
    LOG_DEBUG(ETH, "TX_CONFIG = 0x{:X}", val);
    if (ethPciState.txDescriptorBaseReg == 0) {
      LOG_WARNING(ETH, "TX_CONFIG written but TX_DESCRIPTOR_BASE is unset!");
    }
    txEnabled = true;
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
    if (ethPciState.rxDescriptorBaseReg == 0) {
      LOG_WARNING(ETH, "RX_CONFIG written but RX_DESCRIPTOR_BASE is unset!");
    }
    rxEnabled = true;
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
    MdioWrite(val);
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
    LOG_DEBUG(ETH, "macAddress[{}] = 0x{:X}", (offset - ADDRESS_0), static_cast<u32>(*data));
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
    LOG_DEBUG(ETH, "macAddress2[{}] = 0x{:X}", (offset - ADDRESS_1), static_cast<u32>(*data));
    break;
  default:
    LOG_ERROR(ETH, "Register '0x{:X}' is unknown! Data = 0x{:X} ({}b)", static_cast<u16>(offset), val, size);
    break;
  }
}

void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {}

u32 Xe::PCIDev::ETHERNET::MdioRead(u32 addr) {
  u8 phyAddr = static_cast<u8>((addr >> 6) & 0x1F);
  u8 regNum = static_cast<u8>((addr >> 11) & 0x1F);
  u16 readVal = mdioRegisters[phyAddr][regNum];
  LOG_DEBUG(ETH, "PHY_READ[{}][{}] = 0x{:04X}", phyAddr, regNum, readVal);

  // Merge the read result into the lower 16 bits; preserve upper control bits
  ethPciState.phyControlReg = (readVal << 16);
  ethPciState.phyControlReg &= ~0x10; // Clear MDIO busy/status bit
  return readVal;
}

void Xe::PCIDev::ETHERNET::MdioWrite(u32 val) {
  u16 writeVal = static_cast<u16>(val & 0xFFFF);

  u8 phyAddr = static_cast<u8>((val >> 6) & 0x1F);
  u8 regNum  = static_cast<u8>((val >> 11) & 0x1F);

  LOG_DEBUG(ETH, "PHY_WRITE[{}][{}] = 0x{:08X}(0x{:04X})",
    phyAddr, regNum, val, writeVal);

  // Optional: emulate hardware behavior (e.g., reset)
  switch (phyAddr) {
  case 1: {
    switch (regNum) {
    case 4:
      if (phyAddr == 1)
        mdioRegisters[phyAddr][regNum] |= 0x0400; // Ensure bit 10 is set
      break;
    }
  } break;
  default:
    switch (regNum) {
    // Reset
    case 0:
      if (writeVal & 0x8000)
        writeVal &= ~0x8000;
      mdioRegisters[phyAddr][regNum] = writeVal;
      break;
    // PHY Id, RO
    case 2:
    case 3:
      break;
    default:
      mdioRegisters[phyAddr][regNum] = writeVal;
      break;
    }
    break;
  }

  // Update phyControlReg: preserve upper bits (command/address), update data
  ethPciState.phyControlReg = (val & 0xFFFF0000) | writeVal;
  ethPciState.phyControlReg &= ~0x10; // Clear the busy/status bit
  LOG_DEBUG(ETH, "PHY_CONTROL = 0x{:08X}", ethPciState.phyControlReg);
}

#define NUM_TX_DESCRIPTORS 0x10
#define NUM_RX_DESCRIPTORS 0x10

void Xe::PCIDev::ETHERNET::ProcessRxDescriptors() {
  if (!rxEnabled || ethPciState.rxDescriptorBaseReg == 0) {
    LOG_WARNING(ETH, "Skipping RX descriptor processing: RX not enabled or base not set");
    return;
  }

  // For now, just log it's being accessed
  LOG_DEBUG(ETH, "Processing RX descriptors from base 0x{:X}", ethPciState.rxDescriptorBaseReg);
}

void Xe::PCIDev::ETHERNET::ProcessTxDescriptors() {
  if (!txEnabled || ethPciState.txDescriptorBaseReg == 0) {
    LOG_WARNING(ETH, "Skipping TX descriptor processing: RX not enabled or base not set");
    return;
  }

  // For now, just log it's being accessed
  LOG_DEBUG(ETH, "Processing TX descriptors from base 0x{:X}", ethPciState.txDescriptorBaseReg);
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
