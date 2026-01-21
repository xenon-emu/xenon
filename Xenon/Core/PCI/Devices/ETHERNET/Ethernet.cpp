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

#ifndef ETH_DEBUG
#define DEBUGP(x, ...)
#else
#define DEBUGP(x, ...) LOG_DEBUG(ETH, x, ##__VA_ARGS__);
#endif

// Class Destructor
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
  
  // Setup MDIO registers for the first PHY
  // Reversing suggests that the rest of the PHY's are disabled/unused
  // NOTE: xboxkrnl looks for these values, returns E75 error if not present
  // These are expected on similar drivers, we need to dump them from real hardware.
  // Reg 0: Control Register
  mdioRegisters[0] = 0x1140; // Auto-Neg Enable, Full Duplex
  // Reg 1: Status Register
  mdioRegisters[1] = 0x78ED; // Auto-Neg Complete, Link Up, Extended Caps
  // Reg 2: PHY Identifier 1 (OUI bits 3-18)
  mdioRegisters[2] = 0x0141; // Marvell OUI MSBs
  // Reg 3: PHY Identifier 2 (OUI bits 19-24, Model, Revision)
  // 88E1111 Model: 0x0C, Revision: 0x02
  mdioRegisters[3] = 0x0CC2; // 88E1111 Model/Revision
  // Reg 4: Auto-Negotiation Advertisement
  mdioRegisters[4] = 0x01E1;
  // Reg 5: Auto-Negotiation Link Partner Ability
  mdioRegisters[5] = 0x4DE1;
  // Reg 6: Auto-Negotiation Expansion
  mdioRegisters[6] = 0x000F;
  // Reg 9: 1000BASE-T Control
  mdioRegisters[9] = 0x0300;
  // Reg 10: 1000BASE-T Status
  mdioRegisters[10] = 0x7C00;
  // Reg 17: PHY Specific Status (Marvell)
  mdioRegisters[17] = 0xAC00;
  
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
  
  LOG_INFO(ETH, "Ethernet controller initialized. MAC: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
    ethPciState.macAddress[0], ethPciState.macAddress[1], ethPciState.macAddress[2],
    ethPciState.macAddress[3], ethPciState.macAddress[4], ethPciState.macAddress[5]);
}

// Class Destructor
Xe::PCIDev::ETHERNET::~ETHERNET() {
  // Stop worker thread
  workerRunning = false;
  workerCV.notify_all();
  
  if (workerThread.joinable()) {
    workerThread.join();
  }
  
  // Detach from network bridge
  Network::GetNetworkBridge().DetachEthernetDevice();
  
  LOG_INFO(ETH, "Ethernet controller shutdown. Transmitted: {} TX packets, {} RX packets",
    stats.txPackets.load(), stats.rxPackets.load());
}

// Initializes the Network bridge
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
      if (linkUp) {
        mdioRegisters[1] |= 0x0004;
        mdioRegisters[17] |= 0x0400;
      } else {
        mdioRegisters[1] &= ~0x0004;
        mdioRegisters[17] &= ~0x0400;
      }
    }
  } else {
    // Bridge disabled or failed, link is "up" but no real connectivity
    linkUp = true;
  }
}

// Reset the emulated ethernet adapter
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
  
  // Clear RX queue
  {
    std::lock_guard<std::mutex> lock(rxQueueMutex);
    while (!pendingRxPackets.empty()) pendingRxPackets.pop();
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
  
  DEBUGP("Device reset complete");
}

//
// PCI Read/Write routines
//

// PCI Read
void Xe::PCIDev::ETHERNET::Read(u64 readAddress, u8 *data, u64 size) {
  // Register index
  u8 regIdx = readAddress & 0xFF;

  switch (regIdx) {
  case TX_CONFIG:
    // Whenever software tries to send a new TX packet via either queue, it reads TX_CONFIG reg, to identify wheter 
    // the destination queue is enabled, if not, it will automatically enable it.
    if (txRing0Enabled) { ethPciState.txConfigReg |= TX_CFG_RING0_EN; }
    if (txRing1Enabled) { ethPciState.txConfigReg |= TX_CFG_RING1_EN; }

    memcpy(data, &ethPciState.txConfigReg, size);
    DEBUGP("[Read] TX_CONFIG = {:#x}", ethPciState.txConfigReg);
    break;
  case TX_DESCRIPTOR_BASE: {
    // Reading the TX descriptor base is done by first updating what ring it wants to pull data from via a write to 
    // the TX_CONFIG reg, and then reading this register.

    // Get the current ring to pull data from
    u8 ringID = (ethPciState.txConfigReg & TX_CFG_RING_SEL) ? 1 : 0;

    // Current ring TX descriptor base reg
    u32 descriptorBase = ringID ? ethPciState.txDescriptor1BaseReg : ethPciState.txDescriptor0BaseReg;
    
    memcpy(data, &descriptorBase, size);
    DEBUGP("[Read] TX_DESCRIPTOR_BASE[RING {:#d}] = {:#x}", descriptorBase, ringID);
    }
    break;
  case RX_CONFIG:
    // Same as TX_CONFIG
    if (rxEnabled) { ethPciState.rxConfigReg |= RX_CFG_ENABLE; }

    memcpy(data, &ethPciState.rxConfigReg, size);
    DEBUGP("[Read] RX_CONFIG = {:#08x}", ethPciState.rxConfigReg);
    break;
  case RX_DESCRIPTOR_BASE:
    memcpy(data, &ethPciState.rxDescriptorBaseReg, size);
    DEBUGP("[Read] RX_DESCRIPTOR_BASE = {:#08x}", ethPciState.rxDescriptorBaseReg);
    break;
  case INTERRUPT_STATUS:
    // Return current status - driver reads this to check what caused interrupt
    memcpy(data, &ethPciState.interruptStatusReg, size);
    DEBUGP("[Read] INTERRUPT_STATUS = {:#08x}",  ethPciState.interruptStatusReg);
    break;
  case INTERRUPT_MASK:
    memcpy(data, &ethPciState.interruptMaskReg, size);
    DEBUGP("[Read] INTERRUPT_MASK = {:#08x}", ethPciState.interruptMaskReg);
    break;   
  case CONFIG_0:
    memcpy(data, &ethPciState.config0Reg, size);
    DEBUGP("[Read] CONFIG_0 = {:#08x}", ethPciState.config0Reg);
    break;   
  case POWER:
    memcpy(data, &ethPciState.powerReg, size);
    DEBUGP("[Read] POWER = {:#08x}", ethPciState.powerReg);
    break;   
  case PHY_CONFIG:
    memcpy(data, &ethPciState.phyConfigReg, size);
    DEBUGP("[Read] PHY_CONFIG = {:#08x}", ethPciState.phyConfigReg);
    break;  
  case PHY_CONTROL: {
    MdioRead(ethPciState.phyControlReg);
    memcpy(data, &ethPciState.phyControlReg, size);
    DEBUGP("[Read] PHY_CONTROL (MDIO) = {:#08x}", ethPciState.phyControlReg);
  } break;
  case CONFIG_1:
    memcpy(data, &ethPciState.config1Reg, size);
    DEBUGP("[Read] CONFIG_1 = {:#08x}", ethPciState.config1Reg);
    break;  
  case RETRY_COUNT:
    memcpy(data, &ethPciState.retryCountReg, size);
    DEBUGP("[Read] RETRY_COUNT = {:#08x}", ethPciState.retryCountReg);
    break; 
  case MULTICAST_FILTER_CONTROL:
    memcpy(data, &ethPciState.multicastFilterControlReg, size);
    DEBUGP("[Read] MULTICAST_FILTER_CONTROL = {:#08x}", ethPciState.multicastFilterControlReg);
    break;  
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(data, &ethPciState.macAddress[(regIdx - ADDRESS_0)], size);
    DEBUGP("[Read] MAC_ADDRESS[{}] = {:#02x}", (regIdx - ADDRESS_0), ethPciState.macAddress[(regIdx - ADDRESS_0)]);
    break;
  case MULTICAST_HASH + 0x0:
    memcpy(data, &ethPciState.multicastHashFilter0, size);
    DEBUGP("[Read] MULTICAST_HASH_0 = {:#08x}", ethPciState.multicastHashFilter0);
    break;  
  case MULTICAST_HASH + 0x4:
    memcpy(data, &ethPciState.multicastHashFilter1, size);
    DEBUGP("[Read] MULTICAST_HASH_1 = {:#08x}", ethPciState.multicastHashFilter1);
    break;  
  case MAX_PACKET_SIZE:
    memcpy(data, &ethPciState.maxPacketSizeReg, size);
    DEBUGP("[Read] MAX_PACKET_SIZE = {:#08x}", ethPciState.maxPacketSizeReg);
    break; 
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(data, &ethPciState.macAddress2[(regIdx - ADDRESS_1)], size);
    DEBUGP("[Read] MAC_ADDRESS2[{}] = {:#02x}", (regIdx - ADDRESS_1), ethPciState.macAddress2[(regIdx - ADDRESS_1)]);
    break;
  default:
    LOG_WARNING(ETH, "Register '{:#02x}' is unknown! Attempted to read {} bytes", static_cast<u16>(regIdx), size);
    memset(data, 0xFF, size);
    break;
  }
}

// PCI Write
void Xe::PCIDev::ETHERNET::Write(u64 writeAddress, const u8 *data, u64 size) {
  u8 offset = writeAddress & 0xFF;

  u32 val = 0;
  memcpy(&val, data, size);
  
  switch (offset) {
  case TX_CONFIG: {
    ethPciState.txConfigReg = val;

    u8 ringID = (ethPciState.txConfigReg & TX_CFG_RING_SEL) ? 1 : 0;

    DEBUGP("[Write] TX_CONFIG = {:#08x}", val);

    // Check if TX is being enabled for RING 0
    if (val & TX_CFG_RING0_EN) {
      if (ethPciState.txDescriptor0BaseReg == 0) {
        LOG_WARNING(ETH, "TX enabled but TX_DESCRIPTOR_BASE is unset!");
      }
      if (!txRing0Enabled) {
        txRing0Enabled = true;
        DEBUGP("TX enabled, descriptor base: {:#08x}", ethPciState.txDescriptor0BaseReg);
      }
      // Notify worker thread to process pending TX
      workerCV.notify_one();
    } else if (txRing0Enabled && !(val & 0x01)) {
      // TX is being disabled.
      txRing0Enabled = false;
      DEBUGP("TX disabled");
    }

    // Same for RING 1
    if (val & TX_CFG_RING1_EN) {
      if (ethPciState.txDescriptor1BaseReg == 0) {
        LOG_WARNING(ETH, "TX enabled but TX_DESCRIPTOR_BASE is unset!");
      }
      if (!txRing1Enabled) {
        txRing1Enabled = true;
        DEBUGP("TX enabled, descriptor base: {:#08x}", ethPciState.txDescriptor1BaseReg);
      }
      // Notify worker thread to process pending TX
      workerCV.notify_one();
    } else if (txRing1Enabled && !(val & 0x10)) {
      // TX is being disabled.
      txRing1Enabled = false;
      DEBUGP("TX disabled");
      }
    }
    break; 
  case TX_DESCRIPTOR_BASE:
    // Write the base to the specified RING from TX_CONFIG
    if (!(ethPciState.txConfigReg & TX_CFG_RING_SEL)) { ethPciState.txDescriptor0BaseReg = val; }
    else { ethPciState.txDescriptor1BaseReg = val; }
    DEBUGP("[Write] TX_DESCRIPTOR_BASE = {:#08x}", val);
    break;   
  case NEXT_FREE_TX_DESCR:
    ethPciState.txDescriptorStatusReg = val;
    DEBUGP("[Write] NEXT_FREE_TX_DESCR = {:#08x}", val);
    break;
  case RX_CONFIG:
    ethPciState.rxConfigReg = val;
    DEBUGP("[Write] RX_CONFIG = {:#08x}", val);
    
    // Check if RX is being enabled
    if ((val & 0x01) && (val & 0x10)) {
      if (ethPciState.rxDescriptorBaseReg == 0) {
        LOG_WARNING(ETH, "RX enabled but RX_DESCRIPTOR_BASE is unset!");
      }
      if (!rxEnabled) {
        rxEnabled = true;
        rxHead = 0;
        DEBUGP("RX enabled, descriptor base: {:#08x}", ethPciState.rxDescriptorBaseReg);
      }
    } else if (rxEnabled && !(val & 0x01)) {
      // RX is being disabled
      rxEnabled = false;
      DEBUGP("RX disabled");
    }
    break;  
  case RX_DESCRIPTOR_BASE:
    ethPciState.rxDescriptorBaseReg = val;
    DEBUGP("[Write] RX_DESCRIPTOR_BASE = {:#08x}", val);
    break;
  case INTERRUPT_STATUS:
    {
    u32 oldStatus = ethPciState.interruptStatusReg; 
    DEBUGP("[Write] INTERRUPT_STATUS val={:#08x}, {:#08x} -> {:#08x}", val, oldStatus, ethPciState.interruptStatusReg);
    }
    break;
  case INTERRUPT_MASK:
    {
      u32 oldMask = ethPciState.interruptMaskReg;
      ethPciState.interruptMaskReg = val;
      // Enable interrupts if mask is not zero.
      if (val != 0) {
        enableInterrutps.store(true);
      }

      DEBUGP("[Write] INTERRUPT_MASK = {:#08x} (was {:#08x})", val, oldMask);
    }
    break;
  case CONFIG_0:
    ethPciState.config0Reg = val;

    DEBUGP("[Write] CONFIG_0 = {:#08x}", val);
    
    // From Linux driver:
    // 0x08558001 = Reset + Enable (bit 15 = soft reset, bit 0 = enable)
    // 0x08550001 = Normal operation (enable without reset)
    // Bit 15 (0x8000) triggers a soft reset when set along with bit 0
    if ((val & 0x00008000) && (val & 0x00000001)) {
      DEBUGP("Soft reset triggered via CONFIG_0");
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
      
      // Clear pending RX packets
      {
        std::lock_guard<std::mutex> lock(rxQueueMutex);
        while (!pendingRxPackets.empty()) pendingRxPackets.pop();
      }
    }
    break;  
  case POWER:
    ethPciState.powerReg = val;
    DEBUGP("[Write] POWER = {:#08x}", val);
    break;
  case PHY_CONFIG:
    ethPciState.phyConfigReg = val;
    DEBUGP("[Write] PHY_CONFIG = {:#08x}", val);
    break;
  case PHY_CONTROL:
    MdioWrite(val);
    break;
  case CONFIG_1:
    ethPciState.config1Reg = val;
    DEBUGP("[Write] CONFIG_1 = {:#08x}", val);
    break;
  case RETRY_COUNT:
    ethPciState.retryCountReg = val;
    DEBUGP("[Write] RETRY_COUNT = {:#08x}", val);
    break;
  case MULTICAST_FILTER_CONTROL:
    ethPciState.multicastFilterControlReg = val;
    DEBUGP("[Write] MULTICAST_FILTER_CONTROL = {:#08x}", val);
    break;
  case ADDRESS_0:
  case ADDRESS_0 + 1:
  case ADDRESS_0 + 2:
  case ADDRESS_0 + 3:
  case ADDRESS_0 + 4:
  case ADDRESS_0 + 5:
    memcpy(&ethPciState.macAddress[(offset - ADDRESS_0)], data, size);
    DEBUGP("[Write] MAC_ADDRESS[{}] = {:#02x}", (offset - ADDRESS_0), static_cast<u32>(*data));
    break;
  case MULTICAST_HASH + 0x0:
    ethPciState.multicastHashFilter0 = val;
    DEBUGP("[Write] MULTICAST_HASH_0 = {:#08x}", val);
    break;
  case MULTICAST_HASH + 0x4:
    ethPciState.multicastHashFilter1 = val;
    DEBUGP("[Write] MULTICAST_HASH_1 = {:#08x}", val);
    break;
  case MAX_PACKET_SIZE:
    ethPciState.maxPacketSizeReg = val;
    DEBUGP("[Write] MAX_PACKET_SIZE = {:#08x}", val);
    break;
  case ADDRESS_1:
  case ADDRESS_1 + 1:
  case ADDRESS_1 + 2:
  case ADDRESS_1 + 3:
  case ADDRESS_1 + 4:
  case ADDRESS_1 + 5:
    memcpy(&ethPciState.macAddress2[(offset - ADDRESS_1)], data, size);
    DEBUGP("[Write] MAC_ADDRESS2[{}] = {:#02x}", (offset - ADDRESS_1), static_cast<u32>(*data));
    break; 
  default:
    LOG_WARNING(ETH, "Register '{:#02x}' is unknown! Data = {:#08x} ({}b)", static_cast<u16>(offset), val, size);
    break;
  }
}

// MemSet
void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {
  // Convert memset to write
  u8 buffer[8];
  memset(buffer, data, std::min(size, (u64)sizeof(buffer)));
  Write(writeAddress, buffer, size);
}

// MDIO Read
u32 Xe::PCIDev::ETHERNET::MdioRead(u32 addr) {
  // Only do the read if we're told so
  if ((addr & 0xFF) == 0x50) {
    u8 regNum = static_cast<u8>((addr >> 11) & 0x1F);
    u16 readVal = mdioRegisters[regNum];

    DEBUGP("MDIO_READ[REG={}] = {:#04x}", regNum, readVal);

    // Place read result in upper 16 bits of phyControlReg
    // Clear busy bit (bit 4)
    ethPciState.phyControlReg = (static_cast<u32>(readVal) << 16);
    ethPciState.phyControlReg &= ~0x10; // Clear MDIO busy/status bit
    return readVal;
  }

  return addr;
}

// MDIO Write
void Xe::PCIDev::ETHERNET::MdioWrite(u32 val) {
  u16 writeVal = static_cast<u16>(val>>16) & 0xFFFF;
  u8 regNum = static_cast<u8>((val >> 11) & 0x1F);

  DEBUGP("MDIO_WRITE[REG={}] = {:#04x}", regNum, writeVal);

  if ((val & 0xFF) == 0x50) {
    // This is meant to be a MDIO Read. Just copy over the value (MDIO read address) and return.
    ethPciState.phyControlReg = val;
    return;
  } else if((val & 0xFF) == 0x70){
    // This is indeed an MDIO Write
    // Handle special register behaviors
    switch (regNum) {
    case 0: // Control Register
      if (writeVal & 0x8000) {
        // Reset bit set - perform PHY reset
        writeVal &= ~0x8000; // Clear reset bit (self-clearing)

        // Reset PHY to default state
        mdioRegisters[0] = 0x1140;
        mdioRegisters[1] = 0x78ED;

        DEBUGP("PHY Reset!");
      }
      else { mdioRegisters[regNum] = writeVal; }
      break;
    case 2: // PHY ID 1 - Read only
    case 3: // PHY ID 2 - Read only
      // Ignore writes to read-only registers
      break;
    default:
      mdioRegisters[regNum] = writeVal;
      break;
    }

    return;
  }
}

//
// Descriptor Processing
//

// Read a TX descriptor from memory
bool Xe::PCIDev::ETHERNET::ReadTxDescriptor(bool ring0, u32 index, XE_TX_DESCRIPTOR& desc) {
  
  u32 txDescriptorBaseReg = ring0 ? ethPciState.txDescriptor0BaseReg : ethPciState.txDescriptor1BaseReg;

  if (txDescriptorBaseReg == 0) { return false; }
  
  u32 descAddr = txDescriptorBaseReg + (index * sizeof(XE_TX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to TX descriptor at {:#08x}", descAddr);
    return false;
  }
  
  memcpy(&desc, ptr, sizeof(XE_TX_DESCRIPTOR));
  return true;
}
// Write a TX descriptor to memory
bool Xe::PCIDev::ETHERNET::WriteTxDescriptor(bool ring0, u32 index, const XE_TX_DESCRIPTOR& desc) {
  u32 txDescriptorBaseReg = ring0 ? ethPciState.txDescriptor0BaseReg : ethPciState.txDescriptor1BaseReg;
  if (txDescriptorBaseReg == 0) { return false; }
  
  u32 descAddr = txDescriptorBaseReg + (index * sizeof(XE_TX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to TX descriptor at {:#08x}", descAddr);
    return false;
  }
  
  memcpy(ptr, &desc, sizeof(XE_TX_DESCRIPTOR));
  return true;
}

// Read an RX descriptor from memory
bool Xe::PCIDev::ETHERNET::ReadRxDescriptor(u32 index, XE_RX_DESCRIPTOR& desc) {
  if (ethPciState.rxDescriptorBaseReg == 0) { return false; }
  
  u32 descAddr = ethPciState.rxDescriptorBaseReg + (index * sizeof(XE_RX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to RX descriptor at {:#08x}", descAddr);
    return false;
  }
  
  memcpy(&desc, ptr, sizeof(XE_RX_DESCRIPTOR));  
  return true;
}
// Write an RX descriptor to memory
bool Xe::PCIDev::ETHERNET::WriteRxDescriptor(u32 index, const XE_RX_DESCRIPTOR& desc) {
  if (ethPciState.rxDescriptorBaseReg == 0) {
    return false;
  }
  
  u32 descAddr = ethPciState.rxDescriptorBaseReg + (index * sizeof(XE_RX_DESCRIPTOR));
  u8* ptr = ramPtr->GetPointerToAddress(descAddr);
  if (!ptr) {
    LOG_ERROR(ETH, "Failed to get pointer to RX descriptor at {:#08x}", descAddr);
    return false;
  }
  
  memcpy(ptr, &desc, sizeof(XE_RX_DESCRIPTOR));
  
  return true;
}

// TX Descriptors Processing
void Xe::PCIDev::ETHERNET::ProcessTxDescriptors(bool ring0) {
  bool txEnabled = ring0 ? txRing0Enabled : txRing1Enabled;
  bool baseRegValid = ring0 ? ethPciState.txDescriptor0BaseReg != 0 : ethPciState.txDescriptor1BaseReg != 0;
  
  // Check TX enabled and valid base reg
  if (!txEnabled || !baseRegValid) { return; }
  
  // Get descriptor count
  u8 descriptorCount = ring0 ? NUM_RING0_TX_DESCRIPTORS : NUM_RING1_TX_DESCRIPTORS;
  // Current processed count
  u32 processedCount = 0;
  
  while (processedCount < descriptorCount) {
    XE_TX_DESCRIPTOR desc;
    u32 index = ring0 ? txRing0Head : txRing1Head;
    
    if (!ReadTxDescriptor(ring0, index, desc)) {
      break;
    }
    
    // Check OWN bit (bit 31 of descr[1]/status)
    if (!(desc.status & TX_DESC_OWN)) { break; }
    
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
    
    DEBUGP("TX: desc={}, len={}, buf={:#08x}", index, packetLen, desc.bufferAddress);
    
    // Check wrap bit in descr[3]
    (ring0 ? txRing0Head : txRing1Head) = (desc.lengthWrap & 0x80000000) ? 0 : (((ring0 ? txRing0Head : txRing1Head) + 1) % descriptorCount);
    processedCount++;
  }
  
  // Raise TX interrupt ONCE after batch processing (not per-packet)
  // This matches real hardware behavior and prevents interrupt storms
  // TODO: Fixed batch sets?
  if (processedCount > 0) {
    RaiseInterrupt(ring0 ? INT_TX_RING0: INT_TX_RING1);
  }
}
// RX Descriptors Processing
void Xe::PCIDev::ETHERNET::ProcessRxDescriptors() {

  if (!rxEnabled || ethPciState.rxDescriptorBaseReg == 0) { return; }
  
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
      LOG_WARNING(ETH, "RX ring full, dropping packet and signaling interrupt to guest OS.");
      stats.rxDropped++;
      pendingRxPackets.pop();
      // Re enable interrupts, since if we're here, it means that the OS ethernet interrupt handler didn't got triggered.
      enableInterrutps.store(true);
      // Raise interrupt siganling we're full
      RaiseInterrupt(INT_RX_DONE);
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
      LOG_ERROR(ETH, "RX descriptor {} has invalid buffer address: {:#08x}", 
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
    
    DEBUGP("RX: desc={}, len={}, buf={:#08x}", index, copyLen, desc.bufferAddress);
    
    pendingRxPackets.pop();
    
    // Check wrap bit in descr[3] (bit 31)
    rxHead = (desc.bufferSizeWrap & 0x80000000) ? 0 : ((rxHead + 1) % NUM_RX_DESCRIPTORS);
  }
  
  // Raise RX interrupt ONCE after batch processing
  // TODO: Fixed batch sets?
  if (processedCount > 0) {
    RaiseInterrupt(INT_RX_DONE);
  }
}

// Handle incomming TX packet from guest
void Xe::PCIDev::ETHERNET::HandleTxPacket(const u8* data, u32 len) {
  if (!data || len == 0) { return; }
  
  // Try to send through network bridge first
  auto& bridge = Network::GetNetworkBridge();
  if (bridge.IsActive() && bridge.GetBackend()) {
    if (bridge.GetBackend()->SendPacket(data, len)) {
      DEBUGP("TX packet sent to bridge: len={}", len);
    } else {
      stats.txDropped++;
    }
  }

  // Log packet info
  if (len >= 14) {
    DEBUGP("TX: dst={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X} "
      "src={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X} type=0x{:04X} len={}",
      data[0], data[1], data[2], data[3], data[4], data[5],
      data[6], data[7], data[8], data[9], data[10], data[11],
      (data[12] << 8) | data[13], len);
  }
}

// Enqueues a new RX packet
void Xe::PCIDev::ETHERNET::EnqueueRxPacket(const u8* data, u32 length) {
  if (!data || length == 0) { return; }
  
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

// Dequeues a pending RX packet
bool Xe::PCIDev::ETHERNET::DequeueRxPacket(EthernetPacket& packet) {
  std::lock_guard<std::mutex> lock(rxQueueMutex);
  
  if (pendingRxPackets.empty()) { return false; }

  packet = std::move(pendingRxPackets.front());
  pendingRxPackets.pop();
  return true;
}

// Changes link state and update internal regs
void Xe::PCIDev::ETHERNET::SetLinkUp(bool up) {
  bool wasUp = linkUp.exchange(up);
  
  if (wasUp != up) {
    LOG_INFO(ETH, "Link status changed: {}", up ? "UP" : "DOWN");
    
    // Update PHY status registers
    if (up) {
      mdioRegisters[1] |= 0x0004;  // Link status bit
      mdioRegisters[17] |= 0x0400; // Real-time link status
    } else {
      mdioRegisters[1] &= ~0x0004;
      mdioRegisters[17] &= ~0x0400;
    }
    
    // Generate link change interrupt
    RaiseInterrupt(INT_LINK_CHANGE);
  }
}

// Raise interrupt if enabled by interrupt mask
void Xe::PCIDev::ETHERNET::RaiseInterrupt(u32 bits) {
  // Set the interrupt status bits
  ethPciState.interruptStatusReg |= bits;
  
  // Only fire interrupt if:
  // 1. There are pending interrupt status bits
  // 2. Those bits are enabled in the mask
  // 3. The mask is not zero (driver hasn't disabled interrupts)

  u32 pending = ethPciState.interruptStatusReg & ethPciState.interruptMaskReg;

  if (pending != 0 && ethPciState.interruptMaskReg != 0 && enableInterrutps.load()) {
    DEBUGP("Firing interrupt: pending={:#08x} (status={:#08x} & mask={:#08x})",
      pending, ethPciState.interruptStatusReg, ethPciState.interruptMaskReg);

    parentBus->RouteInterrupt(PRIO_ENET);
    // Disable interrupts without clearing the mask.
    enableInterrutps.store(false);
  }
}

// PCI Config Read
void Xe::PCIDev::ETHERNET::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

// PCI Config Write
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

// Worker Thread loop
void Xe::PCIDev::ETHERNET::WorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Ethernet");

  DEBUGP("Ethernet worker thread started");

  while (workerRunning && XeRunning) {
    // Wait for work or timeout
    {
      std::unique_lock<std::mutex> lock(workerMutex);
      workerCV.wait_for(lock, std::chrono::milliseconds(1), [this] {
        return !workerRunning || !XeRunning || ((txRing0Enabled || txRing1Enabled)) || (!pendingRxPackets.empty() && rxEnabled);
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

  DEBUGP("Ethernet worker thread stopped");
}