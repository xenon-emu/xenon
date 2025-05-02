// Copyright 2025 Xenon Emulator Project

#include "Ethernet.h"

#include "Base/Logging/Log.h"

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
    mdioRegisters[phy][0] = byteswap_be<u16>(0x1100); // BMCR - autoneg=0, full-duplex, 100Mb
    mdioRegisters[phy][1] = byteswap_be<u16>(0x7849); // BSR - Link, auto-negotiation capable, etc.
    mdioRegisters[phy][2] = byteswap_be<u16>(0x0015); // PHY ID1 (expected OUI MSBs)
    mdioRegisters[phy][3] = byteswap_be<u16>(0x0141); // PHY ID2 (expected OUI LSBs + model/rev)
    mdioRegisters[phy][4] = byteswap_be<u16>((1 << 8) | (1 << 6) | 0x0001); // ANAR: 100FDX + 10FDX + selector
    mdioRegisters[phy][5] = mdioRegisters[phy][4]; // partner = same
    // Set bit-24
    mdioRegisters[phy][4] |= (1 << (24 - 16));
    mdioRegisters[phy][5] |= (1 << (24 - 16));
    mdioRegisters[phy][17] = byteswap_be<u16>(0x6024); // 0110 0000 0010 0100 = 100 Mb, full-duplex, AN-complete, link-up
  }
  // Create thread
  mdioThread = std::thread(&ETHERNET::MdioThreadFunc, this);
}
Xe::PCIDev::ETHERNET::~ETHERNET() {
  mdioRunning = false;
  mdioCV.notify_all();
  if (mdioThread.joinable()) {
    mdioThread.join();
  }
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
    u32 regVal = ethPciState.phyControlReg;
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
    bool isWrite = ethPciState.phyConfigReg & (1 << 16);
    if (isWrite) {
      MdioWrite(val);
    } else {
      ethPciState.phyControlReg = MdioRead();
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

void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {}

u32 Xe::PCIDev::ETHERNET::MdioRead() {
  std::unique_lock<std::mutex> lock(mdioMutex);
  mdioReq = {
    .isRead = true,
    .phyAddr = static_cast<u8>((ethPciState.phyConfigReg >> 8) & 0x1F),
    .regNum = static_cast<u8>(ethPciState.phyConfigReg & 0x1F),
    .writeVal = 0,
    .readResult = 0,
    .completed = false
  };
  mdioRequestPending = true;
  mdioCV.notify_one();

  mdioCV.wait(lock, [&]() { return mdioReq.completed; });
  return mdioReq.readResult;
}

void Xe::PCIDev::ETHERNET::MdioWrite(u32 val) {
  std::unique_lock<std::mutex> lock(mdioMutex);
  mdioReq = {
    .isRead = false,
    .phyAddr = static_cast<u8>((ethPciState.phyConfigReg >> 8) & 0x1F),
    .regNum = static_cast<u8>(ethPciState.phyConfigReg & 0x1F),
    .writeVal = val,
    .readResult = 0,
    .completed = false
  };
  mdioRequestPending = true;
  mdioCV.notify_one();

  mdioCV.wait(lock, [&]() { return mdioReq.completed; });
}

void Xe::PCIDev::ETHERNET::MdioThreadFunc() {
  while (mdioRunning) {
    std::unique_lock<std::mutex> lock(mdioMutex);
    mdioCV.wait(lock, [&]() { return mdioRequestPending || !mdioRunning; });

    if (!mdioRequestPending)
      continue;

    auto& req = mdioReq;

    if (req.phyAddr >= 32 || req.regNum >= 32) {
      LOG_WARNING(ETH, "Invalid MDIO access: PHY {} REG {}", req.phyAddr, req.regNum);
      req.readResult = 0xFFFFFFFF;
    } else {
      u16 *mdio = mdioRegisters[req.phyAddr];

      if (req.isRead) {
        u16 val = mdio[req.regNum];
        switch (req.regNum) {
        //case 1: {
        //  val &= ~(1 << 2); // Clear status bit
        //  mdio[req.regNum] = val;
        //} break;
        case 2: {
          LOG_DEBUG(ETH, ">> MDIO ID READ: PHY {} REG 2 returned 0x{:04X}", req.phyAddr, val);
        } break;
        case 17: {
          bool linkUp = (val & (1 << 2)) != 0;
          if (linkUp && !lastLinkState[req.phyAddr]) {
            ethPciState.interruptStatusReg |= XE_NET_STATUS_INT;
            parentBus->RouteInterrupt(PRIO_ENET);
          }
          lastLinkState[req.phyAddr] = linkUp;
        } break;
        }
        req.readResult = val;
        LOG_INFO(ETH, "MDIO READ: PHY {} REG {} = 0x{:04X}", req.phyAddr, req.regNum, val);
      } else {
        u16 writeVal = static_cast<u16>(req.writeVal);
        switch (req.regNum) {
        case 0: {
          if (writeVal & 0x8000) {
            // Reset PHY
            writeVal &= ~0x8000;
          }
        } break;
        }
        mdio[req.regNum] = writeVal;
        LOG_INFO(ETH, "MDIO WRITE: PHY {} REG {} <= 0x{:04X}", req.phyAddr, req.regNum, writeVal);
      }
    }

    // Clear request, and mark as done
    req.completed = true;
    ethPciState.phyConfigReg &= ~0x10;
    mdioRequestPending = false;
    mdioCV.notify_all();
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
