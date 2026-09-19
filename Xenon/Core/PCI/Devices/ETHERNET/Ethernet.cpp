/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Xenon Fast Ethernet Adapter Emulation

#include "Ethernet.h"

#include "Base/Config.h"
#include "Base/Global.h"
#include "Base/Logging/Log.h"
#include "Base/Thread.h"
#include "Network/NetworkBridge.h"

// #define ETH_DEBUG

#ifndef ETH_DEBUG
  #define DEBUGP(x, ...)
#else
  #define DEBUGP(x, ...) LOG_DEBUG(ETH, x, ##__VA_ARGS__);
#endif

// Constructor
Xe::PCIDev::ETHERNET::ETHERNET(const std::string& deviceName, u64 size, PCIBridge* parentPCIBridge, RAM* ram)
    : PCIDevice(deviceName, size)
    , ramPtr(ram)
    , parentBus(parentPCIBridge) {

  // PCI config: Microsoft (0x1414), Xbox 360 Fast Ethernet (0x580A)
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580A1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02100006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x02000001; // Ethernet controller class
  pciConfigSpace.configSpaceHeader.regD.hexData = 0x00000040;
  pciConfigSpace.configSpaceHeader.regF.hexData = 0x00000100;

  pciDevSizes[0] = 0x80; // BAR0

  // Initialize PHY registers based on console revision
  memset(phyRegs, 0, sizeof(phyRegs));

  // Reg 0: BMCR - Auto-Neg Enable, Full Duplex, 100Mbps
  phyRegs[0] = 0x1140;
  // Reg 1: BMSR - Auto-Neg Complete, Link Up, Extended Caps
  phyRegs[1] = 0x78ED;

  // Reg 2: PHY ID 1 - vendor-specific based on console revision
  // From kernel PhyInit: 5 different PHY vendors depending on hardware
  switch (Config::highlyExperimental.consoleRevison) {
    case Config::eConsoleRevision::Xenon:
      phyRegs[2] = PHY_BROADCOM; // 0x0143 - Xenon boards
      break;
    default:
      phyRegs[2] = PHY_MARVELL; // 0x0141 - Jasper and later (most common)
      break;
  }

  // Reg 3: PHY ID 2 (model/revision)
  phyRegs[3] = 0x0CC2;
  // Reg 4: Auto-Negotiation Advertisement (100Mbps FD + pause)
  phyRegs[4] = 0x05E1;
  // Reg 5: Link Partner Ability (partner supports 100Mbps FD)
  phyRegs[5] = 0x45E1;
  // Reg 6: Auto-Negotiation Expansion
  phyRegs[6] = 0x000F;
  // Reg 9: 1000BASE-T Control
  phyRegs[9] = 0x0300;
  // Reg 10: 1000BASE-T Status
  phyRegs[10] = 0x7C00;
  // Reg 17: PHY Specific Status (Marvell: link up, 100Mbps, full duplex)
  phyRegs[17] = 0xAC00;

  // Default register values from kernel init sequence
  regs.rxMaxFrame = ETH_MAX_FRAME_SIZE;

  // Initialize the network bridge (if enabled in config).
  InitializeNetworkBridge();

  // Init the worker thread.
  workerRunning = true;
  workerThread = std::thread(&ETHERNET::WorkerThreadLoop, this);

  LOG_INFO(ETH, "Ethernet controller initialized. Default MAC: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
           regs.macAddr0[0], regs.macAddr0[1], regs.macAddr0[2], regs.macAddr0[3], regs.macAddr0[4], regs.macAddr0[5]);
}

// Destructor
Xe::PCIDev::ETHERNET::~ETHERNET() {
  // Stop the worker thread and clean up.
  workerRunning = false;
  workerCV.notify_all();
  if (workerThread.joinable()) { workerThread.join(); }

  // Detatch from the network bridge if attached.
  Network::GetNetworkBridge().DetachEthernetDevice();

  LOG_INFO(ETH, "Ethernet controller shutdown. Stats: TX: {} packets, RX: {} packets", stats.txPackets.load(),
           stats.rxPackets.load());
}

// Intialize the Network Bridge.
void Xe::PCIDev::ETHERNET::InitializeNetworkBridge() {
  Network::BridgeConfig bridgeConfig;
  bridgeConfig.enabled = Config::network.enabled;
  bridgeConfig.backendType = Network::StringToBackendType(Config::network.backend);
  bridgeConfig.backendConfig = Config::network.backendConfig;

  auto& bridge = Network::GetNetworkBridge();
  if (bridge.Initialize(bridgeConfig)) {
    bridge.AttachEthernetDevice(this);
    if (bridge.GetBackend() && bridge.GetBackend()->IsReady()) {
      linkUp = bridge.GetBackend()->IsLinkUp();
      if (linkUp) {
        phyRegs[1] |= 0x0004;  // BMSR link status
        phyRegs[17] |= 0x0400; // Marvell real-time link
      } else {
        phyRegs[1] &= ~0x0004;
        phyRegs[17] &= ~0x0400;
      }
    }
  } else {
    linkUp = true;
  }
}

// Reset
void Xe::PCIDev::ETHERNET::Reset() {
  // Reset TX/RX state without clearing register values, kernel expects registers to retain their values across soft
  // reset).
  txHead[0] = 0;
  txHead[1] = 0;
  rxHead = 0;

  txQ0Active = false;
  txQ1Active = false;
  rxActive = false;
  isrAckedBits = 0;

  // Clear pending RX queue
  {
    std::lock_guard<std::mutex> lock(rxQueueMutex);
    while (!pendingRxPackets.empty()) pendingRxPackets.pop();
    hasRxPending.store(false);
  }

  DEBUGP("SOFT-RESET complete.");
}

// Full RESET, POR defaults.
void Xe::PCIDev::ETHERNET::SystemReset() {
  std::lock_guard<std::mutex> lock(stateMutex);

  // Stop DMA, clear ring indices and the pending RX queue.
  Reset();

  // Register file back to power-on defaults. MAC addresses persist (they come from config, the kernel re-programs them
  // during NicInit anyway).
  regs.txConfig = 0;
  regs.txDescBase[0] = 0;
  regs.txDescBase[1] = 0;
  regs.txDescStatus = 0;
  regs.rxConfig = 0;
  regs.rxDescBase = 0;
  regs.rxDescStatus = 0;
  regs.isrStatus.store(0);
  regs.intEnable = 0;
  regs.softReset = 0;
  regs.power = 0;
  regs.macConfig = 0;
  regs.miiCmd = 0;
  regs.miiConfig = 0;
  regs.macSpeed = 0;
  regs.flowCtrl = 0;
  regs.rxFilter = 0;
  regs.txDescQ0 = 0;
  regs.txDescQ1 = 0;
  regs.rxMaxFrame = ETH_MAX_FRAME_SIZE;
  interruptsEnabled.store(false);

  LOG_INFO(ETH, "System reset: DMA stopped, registers cleared.");
}

// MMIO Read
void Xe::PCIDev::ETHERNET::Read(u64 readAddress, u8* data, u64 size) {
  u8 offset = readAddress & 0xFF;

  std::lock_guard<std::mutex> lock(stateMutex);

  switch (offset) {
    case REG_TX_CONFIG: {
      // Kernel reads TX_CONFIG to check if queues are active/enabled.
      // Bit 0 (active) reflects whether TX DMA is running.
      u32 val = regs.txConfig;
      if (txQ0Active) val |= TX_CFG_ACTIVE | TX_CFG_Q0_EN;
      if (txQ1Active) val |= TX_CFG_ACTIVE | TX_CFG_Q1_EN;
      memcpy(data, &val, size);
      DEBUGP("[Read] TX_CONFIG = {:#010x}", val);
    } break;

    case REG_TX_DESC_BASE: {
      // Ring selection via TX_CONFIG bit 16.
      int ring = (regs.txConfig & TX_CFG_RING_SEL) ? 1 : 0;
      memcpy(data, &regs.txDescBase[ring], size);
      DEBUGP("[Read] TX_DESC_BASE[{}] = {:#010x}", ring, regs.txDescBase[ring]);
    } break;

    case REG_TX_DESC_STATUS: memcpy(data, &regs.txDescStatus, size); break;

    case REG_RX_CONFIG: {
      // Bit 0 (active) reflects whether RX DMA is running.
      u32 val = regs.rxConfig;
      if (rxActive) val |= RX_CFG_ACTIVE;
      memcpy(data, &val, size);
      DEBUGP("[Read] RX_CONFIG = {:#010x}", val);
    } break;

    case REG_RX_DESC_BASE: memcpy(data, &regs.rxDescBase, size); break;

    case REG_RX_DESC_STATUS: memcpy(data, &regs.rxDescStatus, size); break;

    case REG_ISR_STATUS: {
      // Kernel NicProcessIsr reads ISR_STATUS to get pending interrupt bits.
      // If the value is 0, the kernel traps (spurious interrupt assertion).
      // We snapshot the current bits and mark them as "acked" so they get cleared when the DPC re-enables INT_ENABLE.
      // New bits set by the worker thread between now and re-enable are preserved.

      u32 stale = 0;
      if (!rxActive.load()) stale |= ISR_RX;
      if (!txQ0Active.load()) stale |= ISR_TX_Q0;
      if (!txQ1Active.load()) stale |= ISR_TX_Q1;
      if (stale) regs.isrStatus.fetch_and(~stale);

      u32 status = regs.isrStatus.load();
      isrAckedBits = status; // Remember what the ISR saw.
      memcpy(data, &status, size);
      DEBUGP("[Read] ISR_STATUS = {:#010x}", status);
    } break;

    case REG_INT_ENABLE:
      memcpy(data, &regs.intEnable, size);
      DEBUGP("[Read] INT_ENABLE = {:#010x}", regs.intEnable);
      break;

    case REG_SOFT_RESET: memcpy(data, &regs.softReset, size); break;

    case REG_POWER: memcpy(data, &regs.power, size); break;

    case REG_MAC_CONFIG: memcpy(data, &regs.macConfig, size); break;

    case REG_MII_CMD: {
      // Kernel MII read protocol:
      // 1. Write command with read bit to MII_CMD
      // 2. Poll busy bit (bit 4) until clear
      // 3. Read MII_CMD to get data in upper 16 bits
      HandleMdioRead();
      memcpy(data, &regs.miiCmd, size);
      DEBUGP("[Read] MII_CMD = {:#010x}", regs.miiCmd);
    } break;

    case REG_MII_CONFIG: memcpy(data, &regs.miiConfig, size); break;

    case REG_MAC_SPEED: memcpy(data, &regs.macSpeed, size); break;

    case REG_FLOW_CTRL: memcpy(data, &regs.flowCtrl, size); break;

    case REG_RX_FILTER: memcpy(data, &regs.rxFilter, size); break;

    // Primary MAC address (byte-addressable)
    case REG_MAC_ADDR0:
    case REG_MAC_ADDR0 + 1:
    case REG_MAC_ADDR0 + 2:
    case REG_MAC_ADDR0 + 3:
    case REG_MAC_ADDR0 + 4:
    case REG_MAC_ADDR0 + 5: memcpy(data, &regs.macAddr0[offset - REG_MAC_ADDR0], size); break;

    case REG_TX_DESC_Q0: memcpy(data, &regs.txDescQ0, size); break;

    case REG_TX_DESC_Q1: memcpy(data, &regs.txDescQ1, size); break;

    case REG_RX_MAX_FRAME: memcpy(data, &regs.rxMaxFrame, size); break;

    // Secondary MAC address (byte-addressable)
    case REG_MAC_ADDR1:
    case REG_MAC_ADDR1 + 1:
    case REG_MAC_ADDR1 + 2:
    case REG_MAC_ADDR1 + 3:
    case REG_MAC_ADDR1 + 4:
    case REG_MAC_ADDR1 + 5: memcpy(data, &regs.macAddr1[offset - REG_MAC_ADDR1], size); break;

    default:
      LOG_WARNING(ETH, "Unknown register read: offset={:#04x} size={}", static_cast<u16>(offset), size);
      memset(data, 0, size);
      break;
  }
}

// MMIO Write
void Xe::PCIDev::ETHERNET::Write(u64 writeAddress, const u8* data, u64 size) {
  u8 offset = writeAddress & 0xFF;
  u32 val = 0;
  memcpy(&val, data, std::min(size, (u64)4));

  std::lock_guard<std::mutex> lock(stateMutex);

  switch (offset) {
    case REG_TX_CONFIG: {
      regs.txConfig = val;
      DEBUGP("[Write] TX_CONFIG = {:#010x}", val);

      // Kernel NicStartTx writes: (1 << (queueIndex + 4)) | 0x1C01
      // Check Q0 enable (bit 4)
      if (val & TX_CFG_Q0_EN) {
        if (!txQ0Active) {
          txQ0Active = true;
          DEBUGP("TX Q0 enabled, desc base: {:#010x}", regs.txDescBase[0]);
        }
        workerCV.notify_one();
      }

      // Check Q1 enable (bit 5)
      if (val & TX_CFG_Q1_EN) {
        if (!txQ1Active) {
          txQ1Active = true;
          DEBUGP("TX Q1 enabled, desc base: {:#010x}", regs.txDescBase[1]);
        }
        workerCV.notify_one();
      }

      // Kernel NicStopXmitRecv writes 0 to TX_CONFIG to disable
      if (val == 0) {
        txQ0Active = false;
        txQ1Active = false;
        // Clear active bit immediately (kernel polls bit 0 for stop confirmation)
        regs.txConfig &= ~TX_CFG_ACTIVE;
      }
    } break;

    case REG_TX_DESC_BASE: {
      // Ring selected by TX_CONFIG bit 16
      int ring = (regs.txConfig & TX_CFG_RING_SEL) ? 1 : 0;
      regs.txDescBase[ring] = val;
      DEBUGP("[Write] TX_DESC_BASE[{}] = {:#010x}", ring, val);
    } break;

    case REG_TX_DESC_STATUS: regs.txDescStatus = val; break;

    case REG_RX_CONFIG: {
      regs.rxConfig = val;
      DEBUGP("[Write] RX_CONFIG = {:#010x}", val);

      // Kernel NicInit writes 0x111C1000 to enable RX
      // Any non-zero write with significant bits enables RX
      if (val != 0 && val != RX_CFG_ACTIVE) {
        if (!rxActive) {
          rxActive = true;
          rxHead = 0;
          DEBUGP("RX enabled, desc base: {:#010x}", regs.rxDescBase);
        }
      }

      // Kernel NicStopXmitRecv writes 0 to RX_CONFIG to disable
      if (val == 0) {
        rxActive = false;
        regs.rxConfig &= ~RX_CFG_ACTIVE;
      }
    } break;

    case REG_RX_DESC_BASE:
      regs.rxDescBase = val;
      DEBUGP("[Write] RX_DESC_BASE = {:#010x}", val);
      break;

    case REG_RX_DESC_STATUS: regs.rxDescStatus = val; break;

    case REG_ISR_STATUS:
      // ISR_STATUS is read-only from the kernel's perspective.
      DEBUGP("[Write] ISR_STATUS = {:#010x} (ignored, read-only)", val);
      break;

    case REG_INT_ENABLE: {
      // Kernel interrupt model:
      //   ISR: writes 0 to INT_ENABLE to disable all interrupts
      //   ISR: reads ISR_STATUS (we snapshot those bits in isrAckedBits)
      //   DPC: processes events based on the snapshot
      //   DPC: writes 0x54000100 to INT_ENABLE to re-enable
      //
      // On re-enable, clear only the bits the ISR actually read. Any new bits set by the worker thread between ISR read
      // and DPC re-enable are preserved.
      regs.intEnable = val;

      if (val == INT_ENABLE_NONE) {
        interruptsEnabled.store(false);
        DEBUGP("[Write] INT_ENABLE = 0 (interrupts disabled)");
      } else {
        // DPC re-enabling interrupts: clear only the bits the ISR processed.
        if (isrAckedBits != 0) {
          regs.isrStatus.fetch_and(~isrAckedBits);
          isrAckedBits = 0;
        }
        interruptsEnabled.store(true);
        DEBUGP("[Write] INT_ENABLE = {:#010x} (interrupts enabled)", val);

        // If new bits accumulated while the DPC was running, fire immediately.
        if (regs.isrStatus.load() != 0) { FireInterrupt(); }
      }
    } break;

    case REG_SOFT_RESET: {
      regs.softReset = val;
      DEBUGP("[Write] SOFT_RESET = {:#010x}", val);

      // Kernel NicReset sequence:
      //   1. Write 0x01805504 (assert reset - bit 19 set)
      //   2. Stall 10us
      //   3. Write 0x01005504 (release reset - bit 19 clear)
      if (val == SOFT_RESET_ASSERT || (val & SOFT_RESET_BIT)) {
        DEBUGP("Soft reset asserted");
        Reset();
      }
    } break;

    case REG_POWER: regs.power = val; break;

    case REG_MAC_CONFIG:
      regs.macConfig = val;
      DEBUGP("[Write] MAC_CONFIG = {:#010x}", val);
      break;

    case REG_MII_CMD: HandleMdioWrite(val); break;

    case REG_MII_CONFIG:
      regs.miiConfig = val;
      DEBUGP("[Write] MII_CONFIG = {:#010x}", val);
      break;

    case REG_MAC_SPEED:
      regs.macSpeed = val;
      DEBUGP("[Write] MAC_SPEED = {:#010x}", val);
      break;

    case REG_FLOW_CTRL:
      regs.flowCtrl = val;
      DEBUGP("[Write] FLOW_CTRL = {:#010x}", val);
      break;

    case REG_RX_FILTER:
      regs.rxFilter = val;
      DEBUGP("[Write] RX_FILTER = {:#010x}", val);
      break;

    // Primary MAC address.
    case REG_MAC_ADDR0:
    case REG_MAC_ADDR0 + 1:
    case REG_MAC_ADDR0 + 2:
    case REG_MAC_ADDR0 + 3:
    case REG_MAC_ADDR0 + 4:
    case REG_MAC_ADDR0 + 5:
      memcpy(&regs.macAddr0[offset - REG_MAC_ADDR0], data, size);
      DEBUGP("[Write] MAC0[{}] = {:#04x}", offset - REG_MAC_ADDR0, static_cast<u32>(*data));
      break;

    case REG_TX_DESC_Q0:
      regs.txDescQ0 = val;
      DEBUGP("[Write] TX_DESC_Q0 = {:#010x}", val);
      break;

    case REG_TX_DESC_Q1:
      regs.txDescQ1 = val;
      DEBUGP("[Write] TX_DESC_Q1 = {:#010x}", val);
      break;

    case REG_RX_MAX_FRAME:
      regs.rxMaxFrame = val;
      DEBUGP("[Write] RX_MAX_FRAME = {:#010x}", val);
      break;

    // Secondary MAC address.
    case REG_MAC_ADDR1:
    case REG_MAC_ADDR1 + 1:
    case REG_MAC_ADDR1 + 2:
    case REG_MAC_ADDR1 + 3:
    case REG_MAC_ADDR1 + 4:
    case REG_MAC_ADDR1 + 5:
      memcpy(&regs.macAddr1[offset - REG_MAC_ADDR1], data, size);
      DEBUGP("[Write] MAC1[{}] = {:#04x}", offset - REG_MAC_ADDR1, static_cast<u32>(*data));
      break;

    default:
      LOG_WARNING(ETH, "Unknown register write: offset={:#04x} val={:#010x} size={}", static_cast<u16>(offset), val,
                  size);
      break;
  }
}

void Xe::PCIDev::ETHERNET::MemSet(u64 writeAddress, s32 data, u64 size) {
  u8 buffer[8];
  memset(buffer, data, std::min(size, (u64)sizeof(buffer)));
  Write(writeAddress, buffer, size);
}

//
// MDIO / PHY Interface
//

// Kernel MII read protocol (PhyReadReg):
//   1. Write command: ((regAddr << 5 | phySelectBit) << 6) | 0x10
//   2. Poll bit 4 (busy) until clear
//   3. Read MII_CMD: data in bits [31:16]

// Kernel MII write protocol (PhyWriteReg):
//   1. Write command: ((data << 5 | regAddr << 5 | phySelectBit) << 6) | 0x30
//   2. Poll bit 4 (busy) until clear

void Xe::PCIDev::ETHERNET::HandleMdioRead() {
  // If busy bit (bit 4) is still set, the previous write was a read command.
  // Extract the register address and return data in upper 16 bits.
  if (regs.miiCmd & 0x10) {
    // Extract register number from the command:
    // Command format: ((regAddr << 5 | phySelect) << 6) | flags
    // regAddr is at bits [15:11] after the shift encoding
    u8 regNum = static_cast<u8>((regs.miiCmd >> 11) & 0x1F);
    u16 readVal = phyRegs[regNum];

    DEBUGP("MDIO READ: reg={} val={:#06x}", regNum, readVal);

    // Place result in upper 16 bits, clear busy bit.
    regs.miiCmd = (static_cast<u32>(readVal) << 16);
  }
}

void Xe::PCIDev::ETHERNET::HandleMdioWrite(u32 val) {
  regs.miiCmd = val;

  // Check bit 5 for write operation (PhyWriteReg sets bit 5)
  // Bit 4 = busy, Bit 0 = direction: 0=read, 1=write
  // Write command has bits 4+5 set (0x30), read command has bit 4 set (0x10)
  if (val & 0x20) {
    // This is a write operation.
    u16 writeVal = static_cast<u16>((val >> 16) & 0xFFFF);
    u8 regNum = static_cast<u8>((val >> 11) & 0x1F);

    DEBUGP("MDIO WRITE: reg={} val={:#06x}", regNum, writeVal);

    switch (regNum) {
      case 0: // BMCR
        if (writeVal & 0x8000) {
          // Reset bit - self-clearing, restore defaults
          phyRegs[0] = 0x1140;
          phyRegs[1] = 0x78ED;
          DEBUGP("PHY reset");
        } else {
          phyRegs[regNum] = writeVal;
        }
        break;
      case 2: // PHY ID 1 - read only
      case 3: // PHY ID 2 - read only
        break;
      default: phyRegs[regNum] = writeVal; break;
    }

    // Clear busy bit after write completes
    regs.miiCmd &= ~0x10;
  }
  // If bit 5 is not set, this is a read setup command.
  // The busy bit (bit 4) stays set until the next read of MII_CMD.
}

//
// Descriptor Processing
//

void Xe::PCIDev::ETHERNET::ProcessTxRing(int queueIndex) {
  bool active = (queueIndex == 0) ? txQ0Active.load() : txQ1Active.load();
  u32 descBase = regs.txDescBase[queueIndex];

  if (!active || descBase == 0) return;

  u32 maxDescs = (queueIndex == 0) ? NUM_RING0_TX_DESCRIPTORS : NUM_RING1_TX_DESCRIPTORS;
  u32 processed = 0;

  while (processed < maxDescs) {
    u32 descAddr = descBase + (txHead[queueIndex] * sizeof(EmacDescriptor));
    u8* descPtr = ramPtr->GetPointerToAddress(descAddr);
    if (!descPtr) break;

    EmacDescriptor desc;
    memcpy(&desc, descPtr, sizeof(EmacDescriptor));

    // Check if HW owns this descriptor (bit 31 of ownership/DW1)
    if (!(desc.ownership & TX_OWN_HW)) break;

    // Extract frame length from status DW0 [10:0]
    u32 frameLen = desc.status & 0x7FF;

    if (frameLen == 0 || frameLen > ETH_MAX_FRAME_SIZE) {
      // Invalid frame - clear ownership and skip
      desc.ownership &= ~TX_OWN_HW;
      memcpy(descPtr, &desc, sizeof(EmacDescriptor));
      stats.txErrors++;
    } else {
      // Get packet data from buffer address (DW2)
      u8* packetPtr = ramPtr->GetPointerToAddress(desc.bufferAddr);
      if (packetPtr) {
        HandleTxPacket(packetPtr, frameLen);
        stats.txPackets++;
        stats.txBytes += frameLen;
      } else {
        stats.txErrors++;
      }

      // Clear HW ownership - return descriptor to software
      desc.ownership &= ~TX_OWN_HW;
      memcpy(descPtr, &desc, sizeof(EmacDescriptor));
    }

    DEBUGP("TX[Q{}]: desc={} len={} buf={:#010x}", queueIndex, txHead[queueIndex], frameLen, desc.bufferAddr);

    // Advance to next descriptor (wrap if bit 31 of DW3 is set)
    if (desc.sizeFlags & DESC_WRAP) {
      txHead[queueIndex] = 0;
    } else {
      txHead[queueIndex] = (txHead[queueIndex] + 1) % maxDescs;
    }
    processed++;
  }

  // Raise TX interrupt once for the batch (bit 4 for Q0, bit 5 for Q1)
  if (processed > 0) { SetInterruptStatus(queueIndex == 0 ? ISR_TX_Q0 : ISR_TX_Q1); }
}

void Xe::PCIDev::ETHERNET::ProcessRxDescriptors() {
  if (!rxActive || regs.rxDescBase == 0) return;

  // Fast check without lock
  if (!hasRxPending.load(std::memory_order_acquire)) return;

  // Extract packets from queue under lock
  std::vector<EthernetPacket> packets;
  {
    std::lock_guard<std::mutex> lock(rxQueueMutex);
    if (pendingRxPackets.empty()) return;

    packets.reserve(pendingRxPackets.size());
    while (!pendingRxPackets.empty()) {
      packets.push_back(std::move(pendingRxPackets.front()));
      pendingRxPackets.pop();
    }
    hasRxPending.store(false, std::memory_order_release);
  }

  u32 processed = 0;

  for (auto& pkt : packets) {
    u32 descAddr = regs.rxDescBase + (rxHead * sizeof(EmacDescriptor));
    u8* descPtr = ramPtr->GetPointerToAddress(descAddr);
    if (!descPtr) {
      LOG_ERROR(ETH, "Invalid RX descriptor address: {:#010x}", descAddr);
      break;
    }

    EmacDescriptor desc;
    memcpy(&desc, descPtr, sizeof(EmacDescriptor));

    // Check HW ownership (bit 31 of DW1)
    if (!(desc.ownership & RX_OWN_HW)) {
      LOG_WARNING(ETH, "RX ring full at desc {}, dropping packet", rxHead);
      stats.rxDropped++;
      // Force an interrupt so kernel processes existing frames
      SetInterruptStatus(ISR_RX);
      continue;
    }

    // Get buffer size from DW3 [10:0]
    u32 bufSize = desc.sizeFlags & DESC_SIZE_MASK;
    u32 copyLen = std::min(pkt.length, bufSize);

    // Get buffer pointer
    u8* bufPtr = ramPtr->GetPointerToAddress(desc.bufferAddr);
    if (!bufPtr) {
      LOG_ERROR(ETH, "RX desc {} invalid buffer: {:#010x}", rxHead, desc.bufferAddr);
      // Mark as error, return to software
      desc.status = RX_STS_OVERFLOW | (1 << RX_STS_DESC_COUNT_SHIFT);
      desc.ownership = 0; // SW owns
      memcpy(descPtr, &desc, sizeof(EmacDescriptor));
      stats.rxErrors++;
    } else {
      // Copy packet data to guest buffer
      memcpy(bufPtr, pkt.data.data(), copyLen);

      // Fill in RX descriptor status (DW0):
      // [10:0]  = frame length
      // Bit 16  = FRAME_OK
      // [29:24] = descriptor count (1 for single-descriptor frames)
      desc.status = (copyLen & RX_STS_FRAME_LEN_MASK) | RX_STS_FRAME_OK | (1 << RX_STS_DESC_COUNT_SHIFT);

      // Clear HW ownership - return to software
      desc.ownership = 0;
      memcpy(descPtr, &desc, sizeof(EmacDescriptor));

      stats.rxPackets++;
      stats.rxBytes += copyLen;
      processed++;
    }

    DEBUGP("RX: desc={} len={} buf={:#010x}", rxHead, copyLen, desc.bufferAddr);

    // Advance (wrap if bit 31 of DW3)
    if (desc.sizeFlags & DESC_WRAP) {
      rxHead = 0;
    } else {
      rxHead = (rxHead + 1) % NUM_RX_DESCRIPTORS;
    }
  }

  if (processed > 0) { SetInterruptStatus(ISR_RX); }
}

//
// Packet Handling
//

void Xe::PCIDev::ETHERNET::HandleTxPacket(const u8* data, u32 len) {
  if (!data || len == 0) return;

  auto& bridge = Network::GetNetworkBridge();
  if (bridge.IsActive() && bridge.GetBackend()) {
    if (!bridge.GetBackend()->SendPacket(data, len)) { stats.txDropped++; }
  }

  DEBUGP("TX: dst={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X} len={}", data[0], data[1], data[2], data[3], data[4],
         data[5], len);
}

void Xe::PCIDev::ETHERNET::EnqueueRxPacket(const u8* data, u32 length) {
  if (!data || length == 0) return;

  // RX DMA disabled = no reception on real hardware. Dropping here also keeps pre-boot host traffic from piling up
  // against a ring the guest has not set up yet.
  if (!rxActive.load(std::memory_order_acquire)) {
    stats.rxDropped++;
    return;
  }

  EthernetPacket pkt;
  pkt.data.assign(data, data + length);
  pkt.length = length;

  {
    std::lock_guard<std::mutex> lock(rxQueueMutex);
    if (pendingRxPackets.size() < 256) {
      pendingRxPackets.push(std::move(pkt));
      hasRxPending.store(true);
      workerCV.notify_one();
    } else {
      stats.rxDropped++;
    }
  }
}

bool Xe::PCIDev::ETHERNET::DequeueRxPacket(EthernetPacket& packet) {
  std::lock_guard<std::mutex> lock(rxQueueMutex);
  if (pendingRxPackets.empty()) return false;

  packet = std::move(pendingRxPackets.front());
  pendingRxPackets.pop();
  hasRxPending.store(!pendingRxPackets.empty());
  return true;
}

//
// Link State
//

void Xe::PCIDev::ETHERNET::SetLinkUp(bool up) {
  bool wasUp = linkUp.exchange(up);
  if (wasUp != up) {
    LOG_INFO(ETH, "Link status: {}", up ? "UP" : "DOWN");

    if (up) {
      phyRegs[1] |= 0x0004;
      phyRegs[17] |= 0x0400;
    } else {
      phyRegs[1] &= ~0x0004;
      phyRegs[17] &= ~0x0400;
    }

    // MII interrupt (bit 16) for link state change
    SetInterruptStatus(ISR_MII);
  }
}

//
// Interrupt Processing
//

// Kernel interrupt model:
//   1. HW sets bits in ISR_STATUS
//   2. HW fires interrupt if INT_ENABLE is non-zero
//   3. Kernel ISR writes 0 to INT_ENABLE (disables interrupts)
//   4. Kernel ISR reads ISR_STATUS to get pending bits
//   5. Kernel DPC processes the bits
//   6. Kernel DPC writes 0x54000100 to INT_ENABLE (re-enables)
//   7. On re-enable, we clear ISR_STATUS (handled in Write for INT_ENABLE)

void Xe::PCIDev::ETHERNET::SetInterruptStatus(u32 bits) {
  // Set status bits atomically
  regs.isrStatus.fetch_or(bits);

  // Fire interrupt if enabled
  FireInterrupt();
}

void Xe::PCIDev::ETHERNET::FireInterrupt() {
  if (!interruptsEnabled.load()) return;

  u32 status = regs.isrStatus.load();
  if (status == 0) return;

  // Only fire if there are enabled bits.
  // INT_ENABLE acts as a global gate, not a per-bit mask.
  if (regs.intEnable != 0) {
    DEBUGP("Firing interrupt: status={:#010x} enable={:#010x}", status, regs.intEnable);
    parentBus->RouteInterrupt(PRIO_ENET);
  }
}

//
// PCI Config Space
//

void Xe::PCIDev::ETHERNET::ConfigRead(u64 readAddress, u8* data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::ETHERNET::ConfigWrite(u64 writeAddress, const u8* data, u64 size) {
  u64 tmp = 0;
  memcpy(&tmp, data, size);

  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // BAR size discovery
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          tmp &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) break;
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) {
      tmp = 0; // Expansion ROM not implemented
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
}

// Worker Thread
void Xe::PCIDev::ETHERNET::WorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Ethernet");

  while (workerRunning && XeRunning) {
    {
      std::unique_lock<std::mutex> lock(workerMutex);
      workerCV.wait_for(lock, std::chrono::microseconds(500), [this] {
        return !workerRunning || !XeRunning || txQ0Active || txQ1Active || (hasRxPending.load() && rxActive);
      });
    }

    if (!workerRunning || !XeRunning) break;

    // Process TX rings
    if (txQ0Active) ProcessTxRing(0);
    if (txQ1Active) ProcessTxRing(1);

    // Process RX
    if (rxActive) ProcessRxDescriptors();
  }
}
