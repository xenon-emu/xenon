/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// Xenon Fast Ethernet Adapter Emulation
// Based on Xbox 360 Linux kernel patches and Marvell 88E1111 PHY
//

#include "Ethernet.h"
#include "Network/NetworkBridge.h"

#include "Base/Logging/Log.h"
#include "Base/Thread.h"
#include "Base/Global.h"
#include "Base/Config.h"

// Ethernet interrupt priority
#define XE_NET_STATUS_INT 0x0000004C

// Enable debug logging for ethernet operations
//#define ETH_DEBUG

Xe::PCIDev::ETHERNET::ETHERNET(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram) :
  PCIDevice(deviceName, size), ramPtr(ram), parentBus(parentPCIBridge) {
  
  // Set PCI Properties (Xbox 360 Fast Ethernet Adapter)
  // Vendor: 0x1414 (Microsoft), Device: 0x580A
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580A1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02100006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x02000001; // Ethernet controller
  pciConfigSpace.configSpaceHeader.regD.hexData = 0x00000040;
  pciConfigSpace.configSpaceHeader.regF.hexData = 0x00000100;
  
  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x80; // BAR0 - Main register space
  
  // Initialize MDIO PHY registers
  memset(mdioRegisters, 0, sizeof(mdioRegisters));
  
  // Setup MDIO registers for all PHY addresses
  // xboxkrnld looks for these values, returns E75 error if not present
  for (int phy = 0; phy < 32; ++phy) {
    // Reg 0: Control Register
    // Bit 15: Reset (self-clearing)
    // Bit 12: Auto-Negotiation Enable
    // Bit 8: Duplex Mode (1 = Full)
    // Bits 6,13: Speed Selection (10: 1000Mbps)
    mdioRegisters[phy][0] = 0x1140; // Auto-Neg Enable, Full Duplex
    
    // Reg 1: Status Register
    // Bit 5: Auto-Negotiation Complete
    // Bit 4: Remote Fault
    // Bit 3: Auto-Negotiation Ability
    // Bit 2: Link Status
    // Bit 0: Extended Capability
    mdioRegisters[phy][1] = 0x78ED; // Auto-Neg Complete, Link Up, Extended Caps
    
    // Reg 2: PHY Identifier 1 (OUI bits 3-18)
    // Marvell OUI: 0x005043
    mdioRegisters[phy][2] = 0x0141; // Marvell OUI MSBs
    
    // Reg 3: PHY Identifier 2 (OUI bits 19-24, Model, Revision)
    // 88E1111 Model: 0x0C, Revision: 0x02
    mdioRegisters[phy][3] = 0x0CC2; // 88E1111 Model/Revision
    
    // Reg 4: Auto-Negotiation Advertisement
    // Advertise 10/100/1000 Full/Half capabilities
    mdioRegisters[phy][4] = 0x01E1;
    
    // Reg 5: Auto-Negotiation Link Partner Ability
    mdioRegisters[phy][5] = 0x4DE1;
    
    // Reg 6: Auto-Negotiation Expansion
    mdioRegisters[phy][6] = 0x000F;
    
    // Reg 9: 1000BASE-T Control
    mdioRegisters[phy][9] = 0x0300;
    
    // Reg 10: 1000BASE-T Status
    mdioRegisters[phy][10] = 0x7C00;
    
    // Reg 17: PHY Specific Status (Marvell)
    // Indicates link up, 1000Mbps, full duplex
    mdioRegisters[phy][17] = 0xAC00;
  }
  
  // Set default MAC address (Xbox-like format)
  ethPciState.macAddress[0] = 0x00;
  ethPciState.macAddress[1] = 0x1D;
  ethPciState.macAddress[2] = 0xD8;
  ethPciState.macAddress[3] = 0xB7;
  ethPciState.macAddress[4] = 0x1C;
  ethPciState.macAddress[5] = 0x00;
  
  // Initialize max packet size
  ethPciState.maxPacketSizeReg = ETH_MAX_FRAME_SIZE;
  
  // Initialize network bridge if configured
  InitializeNetworkBridge();
  
  // Start worker thread
  workerRunning = true;
  workerThread = std::thread(&ETHERNET::WorkerThreadLoop, this);
  
  LOG_INFO(ETH, "Ethernet controller initialized. MAC: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
    ethPciState.macAddress[0], ethPciState.macAddress[1], ethPciState.macAddress[2],
    ethPciState.macAddress[3], ethPciState.macAddress[4], ethPciState.macAddress[5]);
}

Xe::PCIDev::ETHERNET::~ETHERNET() {
  // Stop worker thread
  workerRunning = false;
  workerCV.notify_all();
  
  if (workerThread.joinable()) {
    workerThread.join();
  }
  
  // Detach from network bridge
  Network::GetNetworkBridge().DetachEthernetDevice();
  
  LOG_INFO(ETH, "Ethernet controller shutdown. TX: {} packets, RX: {} packets",
    stats.txPackets.load(), stats.rxPackets.load());
}

void Xe::PCIDev::ETHERNET::InitializeNetworkBridge() {
  // Configure and initialize the network bridge based on config
  Network::BridgeConfig bridgeConfig;
  bridgeConfig.enabled = Config::network.enabled;
  bridgeConfig.backendType = Network::StringToBackendType(Config::network.backend);
  bridgeConfig.backendConfig = Config::network.backendConfig;
  
  auto& bridge = Network::GetNetworkBridge();
  
  if (bridge.Initialize(bridgeConfig)) {
    // Attach this device to the bridge
    bridge.AttachEthernetDevice(this);
    
    // Update link state based on backend
    if (bridge.GetBackend() && bridge.GetBackend()->IsReady()) {
      linkUp = bridge.GetBackend()->IsLinkUp();
      
      // Update PHY registers based on link state
      for (int phy = 0; phy < 32; ++phy) {
        if (linkUp) {
          mdioRegisters[phy][1] |= 0x0004;
          mdioRegisters[phy][17] |= 0x0400;
        } else {
          mdioRegisters[phy][1] &= ~0x0004;
          mdioRegisters[phy][17] &= ~0x0400;
        }
      }
    }
  } else {
    // Bridge disabled or failed, link is "up" but no real connectivity
    linkUp = true;
  }
}

void Xe::PCIDev::ETHERNET::Reset() {
  // Reset all state
  ethPciState = {};
  
  // Reset descriptor indices
  txRing0Head = 0;
  txRing1Head = 0;
  txRing0Tail = 0;
  txRing1Tail = 0;
  rxHead = 0;
  rxTail = 0;
  
  // Clear queues
  {
    std::lock_guard<std::mutex> lock(rxQueueMutex);
    while (!pendingRxPackets.empty()) pendingRxPackets.pop();
  }
  {
    std::lock_guard<std::mutex> lock(txQueueMutex);
    while (!pendingTxPackets.empty()) pendingTxPackets.pop();
  }
  
  // Disable TX/RX
  txRing0Enabled = false;
  txRing1Enabled = false;
  rxEnabled = false;
  
  // Re-initialize default values
  ethPciState.maxPacketSizeReg = ETH_MAX_FRAME_SIZE;
  
  // Set default MAC
  ethPciState.macAddress[0] = 0x00;
  ethPciState.macAddress[1] = 0x1D;
  ethPciState.macAddress[2] = 0xD8;
  ethPciState.macAddress[3] = 0xB7;
  ethPciState.macAddress[4] = 0x1C;
  ethPciState.macAddress[5] = 0x00;
  
  LOG_DEBUG(ETH, "Device reset complete");
}

void Xe::PCIDev::ETHERNET::Read(u64 readAddress, u8 *data, u64 size) {
  u8 offset = readAddress & 0xFF;

  switch (offset) {
  case TX_CONFIG:
    if (txRing0Enabled) {
      ethPciState.txConfigReg |= TX_CFG_RING0_EN;
    }
    if (txRing1Enabled) {
      ethPciState.txConfigReg |= TX_CFG_RING1_EN;
    }
    memcpy(data, &ethPciState.txConfigReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] TX_CONFIG = 0x{:08X}", ethPciState.txConfigReg);
#endif
    break;
    
  case TX_DESCRIPTOR_BASE: {
    u8 ringID = (ethPciState.txConfigReg & TX_CFG_RING_SEL) ? 1 : 0;
    u32 descriptorBase = ringID ? ethPciState.txDescriptor1BaseReg : ethPciState.txDescriptor0BaseReg;
    memcpy(data, &descriptorBase, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] TX_DESCRIPTOR_BASE[RING {:#d}] = {:#x}", descriptorBase, ringID);
#endif
  }
    break;
    
  case NEXT_FREE_TX_DESCR:
    break;
    
  case RX_CONFIG:
    if (rxEnabled) {
      ethPciState.rxConfigReg |= RX_CFG_ENABLE;
    }
    memcpy(data, &ethPciState.rxConfigReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] RX_CONFIG = 0x{:08X}", ethPciState.rxConfigReg);
#endif
    break;
    
  case RX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.rxDescriptorBaseReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] RX_DESCRIPTOR_BASE = 0x{:08X}", ethPciState.rxDescriptorBaseReg);
#endif
    break;
    
  case INTERRUPT_STATUS:
    // Return current status - driver reads this to check what caused interrupt
    memcpy(data, &ethPciState.interruptStatusReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] INTERRUPT_STATUS = 0x{:08X} (mask=0x{:08X})", 
      ethPciState.interruptStatusReg, ethPciState.interruptMaskReg);
#endif
    ethPciState.interruptStatusReg = 0;
    break;
    
  case INTERRUPT_MASK:
    memcpy(data, &ethPciState.interruptMaskReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] INTERRUPT_MASK = 0x{:08X}", ethPciState.interruptMaskReg);
#endif
    break;
    
  case CONFIG_0:
    memcpy(data, &ethPciState.config0Reg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] CONFIG_0 = 0x{:08X}", ethPciState.config0Reg);
#endif
    break;
    
  case POWER:
    memcpy(data, &ethPciState.powerReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] POWER = 0x{:08X}", ethPciState.powerReg);
#endif
    break;
    
  case PHY_CONFIG:
    memcpy(data, &ethPciState.phyConfigReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] PHY_CONFIG = 0x{:08X}", ethPciState.phyConfigReg);
#endif
    break;
    
  case PHY_CONTROL: {
    MdioRead(ethPciState.phyControlReg);
    memcpy(data, &ethPciState.phyControlReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] PHY_CONTROL (MDIO) = 0x{:08X}", ethPciState.phyControlReg);
#endif
  } break;
  
  case CONFIG_1:
    memcpy(data, &ethPciState.config1Reg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] CONFIG_1 = 0x{:08X}", ethPciState.config1Reg);
#endif
    break;
    
  case RETRY_COUNT:
    memcpy(data, &ethPciState.retryCountReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] RETRY_COUNT = 0x{:08X}", ethPciState.retryCountReg);
#endif
    break;
    
  case MULTICAST_FILTER_CONTROL:
    memcpy(data, &ethPciState.multicastFilterControlReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] MULTICAST_FILTER_CONTROL = 0x{:08X}", ethPciState.multicastFilterControlReg);
#endif
    break;
    
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(data, &ethPciState.macAddress[(offset - ADDRESS_0)], size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] MAC_ADDRESS[{}] = 0x{:02X}", (offset - ADDRESS_0), 
      ethPciState.macAddress[(offset - ADDRESS_0)]);
#endif
    break;
    
  case MULTICAST_HASH + 0x0:
    memcpy(data, &ethPciState.multicastHashFilter0, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] MULTICAST_HASH_0 = 0x{:08X}", ethPciState.multicastHashFilter0);
#endif
    break;
    
  case MULTICAST_HASH + 0x4:
    memcpy(data, &ethPciState.multicastHashFilter1, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] MULTICAST_HASH_1 = 0x{:08X}", ethPciState.multicastHashFilter1);
#endif
    break;
    
  case MAX_PACKET_SIZE:
    memcpy(data, &ethPciState.maxPacketSizeReg, size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] MAX_PACKET_SIZE = 0x{:08X}", ethPciState.maxPacketSizeReg);
#endif
    break;
    
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(data, &ethPciState.macAddress2[(offset - ADDRESS_1)], size);
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Read] MAC_ADDRESS2[{}] = 0x{:02X}", (offset - ADDRESS_1), 
      ethPciState.macAddress2[(offset - ADDRESS_1)]);
#endif
    break;
    
  default:
    LOG_WARNING(ETH, "Register '0x{:02X}' is unknown! Attempted to read {} bytes", 
      static_cast<u16>(offset), size);
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
  case TX_CONFIG: {
    ethPciState.txConfigReg = val;

    u8 ringID = (ethPciState.txConfigReg & TX_CFG_RING_SEL) ? 1 : 0;

#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] TX_CONFIG = 0x{:08X}", val);
#endif

    // Check if TX is being enabled (bit 0 = DMA enable, bit 4 = TX enable)
    // Linux driver uses 0x00001c01 to enable TX
    if (val & TX_CFG_RING0_EN) {
      if (ethPciState.txDescriptor0BaseReg == 0) {
        LOG_WARNING(ETH, "TX enabled but TX_DESCRIPTOR_BASE is unset!");
      }
      if (!txRing0Head) {
        txRing0Enabled = true;
        LOG_DEBUG(ETH, "TX enabled, descriptor base: 0x{:08X}", ethPciState.txDescriptor0BaseReg);
      }
      // Notify worker thread to process pending TX
      workerCV.notify_one();
    }
    else if (txRing0Enabled && !(val & 0x01)) {
      txRing0Enabled = false;
      LOG_DEBUG(ETH, "TX disabled");
    }

    if (val & TX_CFG_RING1_EN) {
      if (ethPciState.txDescriptor1BaseReg == 0) {
        LOG_WARNING(ETH, "TX enabled but TX_DESCRIPTOR_BASE is unset!");
      }
      if (!txRing1Head) {
        txRing1Enabled = true;
        LOG_DEBUG(ETH, "TX enabled, descriptor base: 0x{:08X}", ethPciState.txDescriptor1BaseReg);
      }
      // Notify worker thread to process pending TX
      workerCV.notify_one();
    }
    else if (txRing1Enabled && !(val & 0x10)) {
      txRing1Enabled = false;
      LOG_DEBUG(ETH, "TX disabled");
    }
  }
    
    break;
    
  case TX_DESCRIPTOR_BASE:

    if (!(ethPciState.txConfigReg & TX_CFG_RING_SEL)) {
      ethPciState.txDescriptor0BaseReg = val;
    }
    else {
      ethPciState.txDescriptor1BaseReg = val;
    }

#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] TX_DESCRIPTOR_BASE = 0x{:08X}", val);
#endif
    // Don't reset indices here - driver may set base before enabling
    break;
    
  case NEXT_FREE_TX_DESCR:
    // Writing to status typically triggers TX processing
    ethPciState.txDescriptorStatusReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] NEXT_FREE_TX_DESCR = 0x{:08X}", val);
#endif
    break;
    
  case RX_CONFIG:
    ethPciState.rxConfigReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] RX_CONFIG = 0x{:08X}", val);
#endif
    
    // Check if RX is being enabled (bit 0 = DMA enable, bit 4 = RX enable)
    // Linux driver uses 0x00101c11 to enable RX
    if ((val & 0x01) && (val & 0x10)) {
      if (ethPciState.rxDescriptorBaseReg == 0) {
        LOG_WARNING(ETH, "RX enabled but RX_DESCRIPTOR_BASE is unset!");
      }
      if (!rxEnabled) {
        rxEnabled = true;
        rxHead = 0;
        LOG_DEBUG(ETH, "RX enabled, descriptor base: 0x{:08X}", ethPciState.rxDescriptorBaseReg);
      }
    } else if (rxEnabled && !(val & 0x01)) {
      rxEnabled = false;
      LOG_DEBUG(ETH, "RX disabled");
    }
    break;
    
  case RX_DESCRIPTOR_BASE:
    ethPciState.rxDescriptorBaseReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] RX_DESCRIPTOR_BASE = 0x{:08X}", val);
#endif
    // Don't reset indices here - driver may set base before enabling
    break;
    
  case INTERRUPT_STATUS:
    {
      u32 oldStatus = ethPciState.interruptStatusReg;
      //ethPciState.interruptStatusReg = val;
#ifdef ETH_DEBUG
      LOG_DEBUG(ETH, "[Write] INTERRUPT_STATUS val=0x{:08X}, 0x{:08X} -> 0x{:08X}", 
        val, oldStatus, ethPciState.interruptStatusReg);
#endif
    }
    break;
    
  case INTERRUPT_MASK:
    {
      u32 oldMask = ethPciState.interruptMaskReg;
      ethPciState.interruptMaskReg = val;
#ifdef ETH_DEBUG
      LOG_DEBUG(ETH, "[Write] INTERRUPT_MASK = 0x{:08X} (was 0x{:08X})", val, oldMask);
#endif
    }
    break;
    
  case CONFIG_0:
    ethPciState.config0Reg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] CONFIG_0 = 0x{:08X}", val);
#endif
    // From Linux driver analysis:
    // 0x08558001 = Reset + Enable (bit 15 = soft reset, bit 0 = enable)
    // 0x08550001 = Normal operation (enable without reset)
    // Bit 15 (0x8000) triggers a soft reset when set along with bit 0
    if ((val & 0x00008000) && (val & 0x00000001)) {
      LOG_DEBUG(ETH, "Soft reset triggered via CONFIG_0");
      // Don't call full Reset() - just reset the TX/RX state
      // The driver expects registers to retain their values
      txRing0Head = 0;
      txRing1Head = 0;
      txRing0Tail = 0;
      txRing1Tail = 0;
      rxHead = 0;
      rxTail = 0;
      txRing0Enabled = false;
      txRing1Enabled = false;
      rxEnabled = false;
      
      // Clear pending packets
      {
        std::lock_guard<std::mutex> lock(rxQueueMutex);
        while (!pendingRxPackets.empty()) pendingRxPackets.pop();
      }
      {
        std::lock_guard<std::mutex> lock(txQueueMutex);
        while (!pendingTxPackets.empty()) pendingTxPackets.pop();
      }
    }
    break;
    
  case POWER:
    ethPciState.powerReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] POWER = 0x{:08X}", val);
#endif
    break;
    
  case PHY_CONFIG:
    ethPciState.phyConfigReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] PHY_CONFIG = 0x{:08X}", val);
#endif
    break;
    
  case PHY_CONTROL:
    MdioWrite(val);
    break;
    
  case CONFIG_1:
    ethPciState.config1Reg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] CONFIG_1 = 0x{:08X}", val);
#endif
    break;
    
  case RETRY_COUNT:
    ethPciState.retryCountReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] RETRY_COUNT = 0x{:08X}", val);
#endif
    break;
    
  case MULTICAST_FILTER_CONTROL:
    ethPciState.multicastFilterControlReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] MULTICAST_FILTER_CONTROL = 0x{:08X}", val);
#endif
    break;
    
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(&ethPciState.macAddress[(offset - ADDRESS_0)], data, size);
    LOG_DEBUG(ETH, "[Write] MAC_ADDRESS[{}] = 0x{:02X}", (offset - ADDRESS_0), static_cast<u32>(*data));
    break;
    
  case MULTICAST_HASH + 0x0:
    ethPciState.multicastHashFilter0 = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] MULTICAST_HASH_0 = 0x{:08X}", val);
#endif
    break;
    
  case MULTICAST_HASH + 0x4:
    ethPciState.multicastHashFilter1 = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] MULTICAST_HASH_1 = 0x{:08X}", val);
#endif
    break;
    
  case MAX_PACKET_SIZE:
    ethPciState.maxPacketSizeReg = val;
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "[Write] MAX_PACKET_SIZE = 0x{:08X}", val);
#endif
    break;
    
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(&ethPciState.macAddress2[(offset - ADDRESS_1)], data, size);
    LOG_DEBUG(ETH, "[Write] MAC_ADDRESS2[{}] = 0x{:02X}", (offset - ADDRESS_1), static_cast<u32>(*data));
    break;
    
  default:
    LOG_WARNING(ETH, "Register '0x{:02X}' is unknown! Data = 0x{:08X} ({}b)", 
      static_cast<u16>(offset), val, size);
    break;
  }
}

void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {
  // Convert memset to write
  u8 buffer[8];
  memset(buffer, data, std::min(size, (u64)sizeof(buffer)));
  Write(writeAddress, buffer, size);
}

u32 Xe::PCIDev::ETHERNET::MdioRead(u32 addr) {
  // MDIO frame format:
  // Bits 15-11: Register number
  // Bits 10-6: PHY address
  // Bit 5: Read/Write (1 = read)
  // Bits 4-0: Start/opcode bits
  
  // Only do the read if we're told so
  if ((addr & 0xFF) == 0x50) {
    u8 phyAddr = 0; // static_cast<u8>((addr >> 6) & 0x1F);
    u8 regNum = static_cast<u8>((addr >> 11) & 0x1F);
    u16 readVal = mdioRegisters[phyAddr][regNum];

#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "MDIO_READ[PHY={}][REG={}] = 0x{:04X}", phyAddr, regNum, readVal);
#endif

    // Place read result in upper 16 bits of phyControlReg
    // Clear busy bit (bit 4)
    ethPciState.phyControlReg = (static_cast<u32>(readVal) << 16);
    ethPciState.phyControlReg &= ~0x10; // Clear MDIO busy/status bit
    return readVal;
  }

  return addr;
}

void Xe::PCIDev::ETHERNET::MdioWrite(u32 val) {
  u16 writeVal = static_cast<u16>(val>>16) & 0xFFFF;
  u8 phyAddr = 0;// static_cast<u8>((val >> 6) & 0x1F);
  u8 regNum = static_cast<u8>((val >> 11) & 0x1F);

#ifdef ETH_DEBUG
  LOG_DEBUG(ETH, "MDIO_WRITE[PHY={}][REG={}] = 0x{:04X}", phyAddr, regNum, writeVal);
#endif

  if ((val & 0xFF) == 0x50) {
    // This is meant to be a MDIO Read. Just copy over the value and return.
    ethPciState.phyControlReg = val;
    return;
  }

  // Handle special register behaviors
  switch (regNum) {
  case 0: // Control Register
    if (writeVal & 0x8000) {
      // Reset bit set - perform PHY reset
      writeVal &= ~0x8000; // Clear reset bit (self-clearing)
      
      // Reset PHY to default state
      mdioRegisters[phyAddr][0] = 0x1140;
      mdioRegisters[phyAddr][1] = 0x78ED;
      
      LOG_DEBUG(ETH, "PHY {} reset", phyAddr);
    } else {
      mdioRegisters[phyAddr][regNum] = writeVal;
    }
    break;
    
  case 2: // PHY ID 1 - Read only
  case 3: // PHY ID 2 - Read only
    // Ignore writes to read-only registers
    break;
    
  case 4: // Auto-Negotiation Advertisement
    mdioRegisters[phyAddr][regNum] = writeVal;
    // Ensure bit 10 is set for Xbox compatibility
    if (phyAddr == 1) {
      mdioRegisters[phyAddr][regNum] |= 0x0400;
    }
    break;
    
  default:
    mdioRegisters[phyAddr][regNum] = writeVal;
    break;
  }

  // Update phyControlReg: preserve upper bits, update data, clear busy
  //ethPciState.phyControlReg = (val & 0xFFFF0000) | writeVal;
  //ethPciState.phyControlReg &= ~0x10; // Clear the busy/status bit
  
#ifdef ETH_DEBUG
  LOG_DEBUG(ETH, "PHY_CONTROL = 0x{:08X}", ethPciState.phyControlReg);
#endif
}

bool Xe::PCIDev::ETHERNET::ReadTxDescriptor(bool ring0, u32 index, XE_TX_DESCRIPTOR& desc) {
  
  u32 txDescriptorBaseReg = ring0 ? ethPciState.txDescriptor0BaseReg : ethPciState.txDescriptor1BaseReg;

  if (txDescriptorBaseReg == 0) {
    return false;
  }
  
  u32 descAddr = txDescriptorBaseReg + (index * sizeof(XE_TX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to TX descriptor at 0x{:08X}", descAddr);
    return false;
  }
  
  memcpy(&desc, ptr, sizeof(XE_TX_DESCRIPTOR));
  
  return true;
}

bool Xe::PCIDev::ETHERNET::WriteTxDescriptor(bool ring0, u32 index, const XE_TX_DESCRIPTOR& desc) {

  u32 txDescriptorBaseReg = ring0 ? ethPciState.txDescriptor0BaseReg : ethPciState.txDescriptor1BaseReg;

  if (txDescriptorBaseReg == 0) {
    return false;
  }
  
  u32 descAddr = txDescriptorBaseReg + (index * sizeof(XE_TX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to TX descriptor at 0x{:08X}", descAddr);
    return false;
  }
  
  memcpy(ptr, &desc, sizeof(XE_TX_DESCRIPTOR));
  
  return true;
}

bool Xe::PCIDev::ETHERNET::ReadRxDescriptor(u32 index, XE_RX_DESCRIPTOR& desc) {
  if (ethPciState.rxDescriptorBaseReg == 0) {
    return false;
  }
  
  u32 descAddr = ethPciState.rxDescriptorBaseReg + (index * sizeof(XE_RX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to RX descriptor at 0x{:08X}", descAddr);
    return false;
  }
  
  memcpy(&desc, ptr, sizeof(XE_RX_DESCRIPTOR));
  
  return true;
}

bool Xe::PCIDev::ETHERNET::WriteRxDescriptor(u32 index, const XE_RX_DESCRIPTOR& desc) {
  if (ethPciState.rxDescriptorBaseReg == 0) {
    return false;
  }
  
  u32 descAddr = ethPciState.rxDescriptorBaseReg + (index * sizeof(XE_RX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to RX descriptor at 0x{:08X}", descAddr);
    return false;
  }
  
  memcpy(ptr, &desc, sizeof(XE_RX_DESCRIPTOR));
  
  return true;
}

void Xe::PCIDev::ETHERNET::ProcessTxDescriptors(bool ring0) {
  bool txEnabled = ring0 ? txRing0Enabled : txRing1Enabled;
  bool baseRegValid = ring0 ? ethPciState.txDescriptor0BaseReg != 0 : ethPciState.txDescriptor1BaseReg != 0;
  
  if (!txEnabled || !baseRegValid) {
    return;
  }
  
  u8 descriptorCount = ring0 ? NUM_RING0_TX_DESCRIPTORS : NUM_RING1_TX_DESCRIPTORS;

  u32 processedCount = 0;
  
  while (processedCount < descriptorCount) {
    XE_TX_DESCRIPTOR desc;
    u32 index = ring0 ? txRing0Head : txRing1Head;
    
    if (!ReadTxDescriptor(ring0, index, desc)) {
      break;
    }
    
    // Check OWN bit (bit 31 of descr[1]/status)
    if (!(desc.status & TX_DESC_OWN)) {
      break;
    }
    
    // Get packet length from descr[0]
    u32 packetLen = desc.length & 0xFFFF;
    
    if (packetLen == 0 || packetLen > ETH_MAX_FRAME_SIZE) {
      desc.status &= ~TX_DESC_OWN;
      WriteTxDescriptor(ring0, index, desc);
      stats.txErrors++;
      (ring0 ? txRing0Head : txRing1Head) = (desc.lengthWrap & 0x80000000) ? 0 : (((ring0 ? txRing0Head : txRing1Head) + 1) % descriptorCount);
      processedCount++;
      continue;
    }
    
    // Get packet data (descr[2] = address)
    u8* packetPtr = ramPtr->GetPointerToAddress(desc.bufferAddress);
    if (!packetPtr) {
      desc.status &= ~TX_DESC_OWN;
      WriteTxDescriptor(ring0, index, desc);
      stats.txErrors++;
      (ring0 ? txRing0Head : txRing1Head) = (desc.lengthWrap & 0x80000000) ? 0 : (((ring0 ? txRing0Head : txRing1Head) + 1) % descriptorCount);
      processedCount++;
      continue;
    }
    
    HandleTxPacket(packetPtr, packetLen);
    
    stats.txPackets++;
    stats.txBytes += packetLen;
    
    // Clear OWN bit
    desc.status &= ~TX_DESC_OWN;
    WriteTxDescriptor(ring0, index, desc);
    
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "TX: desc={}, len={}, buf=0x{:08X}", index, packetLen, desc.bufferAddress);
#endif
    
    // Check wrap bit in descr[3]
    (ring0 ? txRing0Head : txRing1Head) = (desc.lengthWrap & 0x80000000) ? 0 : (((ring0 ? txRing0Head : txRing1Head) + 1) % descriptorCount);
    processedCount++;
  }
  
  // Raise TX interrupt ONCE after batch processing (not per-packet)
  // This matches real hardware behavior and prevents interrupt storms
  if (processedCount > 0) {
    RaiseInterrupt(ring0 ? INT_TX_RING0: INT_TX_RING1);
  }
}

void Xe::PCIDev::ETHERNET::ProcessRxDescriptors() {
  if (!rxEnabled || ethPciState.rxDescriptorBaseReg == 0) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(rxQueueMutex);
  
  u32 processedCount = 0;
  
  while (!pendingRxPackets.empty()) {
    XE_RX_DESCRIPTOR desc;
    u32 index = rxHead;
    
    if (!ReadRxDescriptor(index, desc)) {
      LOG_ERROR(ETH, "Failed to read RX descriptor {}", index);
      break;
    }
    
    // Check OWN bit - must be set (owned by HW/available) to receive into
    if (!(desc.status & RX_DESC_OWN)) {
      LOG_WARNING(ETH, "RX ring full, dropping packet");
      stats.rxDropped++;
      pendingRxPackets.pop();
      continue;
    }
    
    // Get buffer size from descr[3] (bufferSizeWrap, lower 16 bits)
    u32 bufferSize = desc.bufferSizeWrap & 0xFFFF;
    
    EthernetPacket& packet = pendingRxPackets.front();
    u32 copyLen = packet.length;
    
    if (copyLen > bufferSize) {
      LOG_WARNING(ETH, "RX packet ({}) exceeds buffer size ({}), truncating", 
        packet.length, bufferSize);
      copyLen = bufferSize;
      stats.rxOverruns++;
    }
    
    // Get buffer address from descr[2]
    u8* bufferPtr = ramPtr->GetPointerToAddress(desc.bufferAddress);
    if (!bufferPtr) {
      LOG_ERROR(ETH, "RX descriptor {} has invalid buffer address: 0x{:08X}", 
        index, desc.bufferAddress);
      
      // Dumps from HW show this.
      desc.receivedLength |= 0x0101 << 16;

      // Clear OWN bit to return to software
      desc.status &= ~RX_DESC_OWN;

      // Hardware has this bit set for all done RX descriptors.
      desc.status |= 0x00060000;
      WriteRxDescriptor(index, desc);
      
      stats.rxErrors++;
      pendingRxPackets.pop();
      
      // Check wrap bit in descr[3]
      rxHead = (desc.bufferSizeWrap & 0x80000000) ? 0 : ((rxHead + 1) % NUM_RX_DESCRIPTORS);
      continue;
    }
    
    memcpy(bufferPtr, packet.data.data(), copyLen);
    
    // Update descriptor:
    // descr[0] (receivedLength) = actual received length
    // descr[1] (status) = clear OWN bit (return to software)
    desc.receivedLength = copyLen;
    desc.status &= ~RX_DESC_OWN;

    // Hardware has this bit set for all done RX descriptors.
    desc.status |= 0x00060000;
    
    // Dumps from HW show this.
    desc.receivedLength |= 0x0101 << 16;
    desc.receivedLength |= 0x00030000;

    WriteRxDescriptor(index, desc);
    
    stats.rxPackets++;
    stats.rxBytes += copyLen;
    processedCount++;
    
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "RX: desc={}, len={}, buf=0x{:08X}", index, copyLen, desc.bufferAddress);
#endif
    
    pendingRxPackets.pop();
    
    // Check wrap bit in descr[3] (bit 31)
    rxHead = (desc.bufferSizeWrap & 0x80000000) ? 0 : ((rxHead + 1) % NUM_RX_DESCRIPTORS);
  }
  
  // Raise RX interrupt ONCE after batch processing
  if (processedCount > 0) {
    RaiseInterrupt(INT_RX_DONE);
  }
}

void Xe::PCIDev::ETHERNET::HandleTxPacket(const u8* data, u32 len) {
  if (!data || len == 0) {
    return;
  }
  
  // Try to send through network bridge first
  auto& bridge = Network::GetNetworkBridge();
  if (bridge.IsActive() && bridge.GetBackend()) {
    if (bridge.GetBackend()->SendPacket(data, len)) {
#ifdef ETH_DEBUG
      LOG_DEBUG(ETH, "TX packet sent to bridge: len={}", len);
#endif
    } else {
      stats.txDropped++;
    }
  }
  
  // Also queue for internal use (loopback, diagnostics)
  EthernetPacket packet;
  packet.data.resize(len);
  memcpy(packet.data.data(), data, len);
  packet.length = len;
  
  {
    std::lock_guard<std::mutex> lock(txQueueMutex);
    
    // Limit queue size to prevent memory issues
    if (pendingTxPackets.size() < 256) {
      pendingTxPackets.push(std::move(packet));
    }
  }
  
#ifdef ETH_DEBUG
  // Log packet info
  if (len >= 14) {
    LOG_DEBUG(ETH, "TX: dst={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X} "
      "src={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X} type=0x{:04X} len={}",
      data[0], data[1], data[2], data[3], data[4], data[5],
      data[6], data[7], data[8], data[9], data[10], data[11],
      (data[12] << 8) | data[13], len);
  }
#endif
}

void Xe::PCIDev::ETHERNET::HandleRxPacket(const u8* data, u32 len) {
  if (!data || len == 0 || len > ETH_MAX_FRAME_SIZE) {
    return;
  }
  
  // Validate minimum frame size
  if (len < ETH_MIN_FRAME_SIZE) {
    stats.rxErrors++;
    return;
  }
  
  // Check if we should accept this packet
  bool accept = false;
  
  // Check for broadcast
  if (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF &&
      data[3] == 0xFF && data[4] == 0xFF && data[5] == 0xFF) {
    if (ethPciState.rxConfigReg & RX_CFG_BROADCAST) {
      accept = true;
    }
  }
  // Check for multicast
  else if (data[0] & 0x01) {
    if (ethPciState.rxConfigReg & RX_CFG_ALL_MULTI) {
      accept = true;
    }
    // TODO: Check multicast hash table
  }
  // Check for our MAC address
  else if (memcmp(data, ethPciState.macAddress, 6) == 0 ||
           memcmp(data, ethPciState.macAddress2, 6) == 0) {
    accept = true;
  }
  // Promiscuous mode
  else if (ethPciState.rxConfigReg & RX_CFG_PROMISC) {
    accept = true;
  }
  
  if (!accept) {
    //return;
  }
  
  // Create and queue packet
  EnqueueRxPacket(data, len);
}

void Xe::PCIDev::ETHERNET::EnqueueRxPacket(const u8* data, u32 length) {
  if (!data || length == 0) {
    return;
  }
  
  EthernetPacket packet;
  packet.data.resize(length);
  memcpy(packet.data.data(), data, length);
  packet.length = length;
  
  {
    std::lock_guard<std::mutex> lock(rxQueueMutex);
    
    // Limit queue size
    if (pendingRxPackets.size() < 256) {
      pendingRxPackets.push(std::move(packet));
      
      // Notify worker thread
      workerCV.notify_one();
    } else {
      stats.rxDropped++;
    }
  }
}

bool Xe::PCIDev::ETHERNET::DequeueRxPacket(EthernetPacket& packet) {
  std::lock_guard<std::mutex> lock(txQueueMutex);
  
  if (pendingTxPackets.empty()) {
    return false;
  }
  
  packet = std::move(pendingTxPackets.front());
  pendingTxPackets.pop();
  return true;
}

void Xe::PCIDev::ETHERNET::SetLinkUp(bool up) {
  bool wasUp = linkUp.exchange(up);
  
  if (wasUp != up) {
    LOG_INFO(ETH, "Link status changed: {}", up ? "UP" : "DOWN");
    
    // Update PHY status registers
    for (int phy = 0; phy < 32; ++phy) {
      if (up) {
        mdioRegisters[phy][1] |= 0x0004;  // Link status bit
        mdioRegisters[phy][17] |= 0x0400; // Real-time link status
      } else {
        mdioRegisters[phy][1] &= ~0x0004;
        mdioRegisters[phy][17] &= ~0x0400;
      }
    }
    
    // Generate link change interrupt
    RaiseInterrupt(INT_LINK_CHANGE);
  }
}

void Xe::PCIDev::ETHERNET::RaiseInterrupt(u32 bits) {
  // Set the interrupt status bits
  u32 oldStatus = ethPciState.interruptStatusReg;
  ethPciState.interruptStatusReg |= bits;
  
#ifdef ETH_DEBUG
  if (oldStatus != ethPciState.interruptStatusReg) {
    LOG_DEBUG(ETH, "RaiseInterrupt: bits=0x{:08X}, status 0x{:08X} -> 0x{:08X}, mask=0x{:08X}",
      bits, oldStatus, ethPciState.interruptStatusReg, ethPciState.interruptMaskReg);
  }
#endif
  
  CheckAndFireInterrupt();
}

void Xe::PCIDev::ETHERNET::CheckAndFireInterrupt() {
  // Only fire interrupt if:
  // 1. There are pending interrupt status bits
  // 2. Those bits are enabled in the mask
  // 3. The mask is not zero (driver hasn't disabled interrupts)
  
  u32 pending = ethPciState.interruptStatusReg & ethPciState.interruptMaskReg;
  
  if (pending != 0 && ethPciState.interruptMaskReg != 0) {
#ifdef ETH_DEBUG
    LOG_DEBUG(ETH, "Firing interrupt: pending=0x{:08X} (status=0x{:08X} & mask=0x{:08X})",
      pending, ethPciState.interruptStatusReg, ethPciState.interruptMaskReg);
#endif
    parentBus->RouteInterrupt(PRIO_ENET);
    ethPciState.interruptMaskReg = 0;
  }
}

void Xe::PCIDev::ETHERNET::WorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Ethernet");
  
  LOG_DEBUG(ETH, "Ethernet worker thread started");
  
  while (workerRunning && XeRunning) {
    // Wait for work or timeout
    {
      std::unique_lock<std::mutex> lock(workerMutex);
      workerCV.wait_for(lock, std::chrono::milliseconds(1), [this] {
        return !workerRunning || !XeRunning ||
               ((txRing0Enabled || txRing1Enabled) ) || //&& txRing0Head != txRing0Tail
               (!pendingRxPackets.empty() && rxEnabled);
      });
    }
    
    // Check for shutdown
    if (!workerRunning || !XeRunning) {
      break;
    }
    
    // Process TX queue
    if (txRing0Enabled) {
      ProcessTxDescriptors(true);
    }
    
    if (txRing1Enabled) {
      ProcessTxDescriptors(false);
    }

    // Process RX queue
    if (rxEnabled) {
      ProcessRxDescriptors();
    }
  }
  
  LOG_DEBUG(ETH, "Ethernet worker thread stopped");
}

void Xe::PCIDev::ETHERNET::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  // Check if we're being scanned for BAR size
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
