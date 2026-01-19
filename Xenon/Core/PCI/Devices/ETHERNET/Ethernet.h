/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

//
// Xenon Fast Ethernet Adapter Emulation
// Based on Xbox 360 Linux kernel patches and hardware documentation
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <array>

#include "Core/RAM/RAM.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/PCIDevice.h"

#define ETHERNET_DEV_SIZE 0x80

// Maximum Ethernet frame size (including VLAN tag)
#define ETH_MAX_FRAME_SIZE 1522
// Standard MTU
#define ETH_MTU 1500
// Minimum frame size
#define ETH_MIN_FRAME_SIZE 64
// Maximum packet buffer size
#define ETH_MAX_PACKET_SIZE 2048

// Number of TX/RX descriptors (must be power of 2)
// NOTE: Xbox 360 Kernel allocates 64 RX descriptors, linux driver only uses 16
#define NUM_RING0_TX_DESCRIPTORS 32
#define NUM_RING1_TX_DESCRIPTORS 8
#define NUM_RX_DESCRIPTORS 64 

namespace Xe {
namespace PCIDev {

// Register Set and offsets.
// Taken from Linux kernel patches for the Xbox 360.
enum XE_ETH_REGISTERS {
  TX_CONFIG = 0x00,             // 0x7fea1400 -> 0x00001c01
  TX_DESCRIPTOR_BASE = 0x04,    // 0x7fea1404 -> Changes (0x002xxxxx)
                                // 0x7fea1408 -> 0x0
  NEXT_FREE_TX_DESCR = 0x0C,    // 0x7fea140c -> 0x00264198 (next available TX descriptor?)
  RX_CONFIG = 0x10,             // 0x7fea1410 -> 0x00101c11
  RX_DESCRIPTOR_BASE = 0x14,    // 0x7fea1414 -> Changes (0x002xxxxx)
                                // 0x7fea1418 -> 0x0
  NEXT_FREE_RX_DESCR = 0x1C,    // 0x7fea141C -> 0x002643e4 (next available RX descriptor?)
  INTERRUPT_STATUS = 0x20,      // 0x7fea1420 -> 0x20000040
  INTERRUPT_MASK = 0x24,        // 0x7fea1424 -> 0x00010054
  CONFIG_0 = 0x28,              // 0x7fea1428 -> 0x04550001
                                // 0x7fea142C -> 0x71b86ed3
  POWER = 0x30,                 // 0x7fea1430 -> 0x0
                                // 0x7fea1434 -> 0x0
                                // 0x7fea1438 -> 0x0
                                // 0x7fea143c -> 0x0
  PHY_CONFIG = 0x40,            // 0x7fea1440 -> 0x04001901
  PHY_CONTROL = 0x44,           // 0x7fea1444 -> 0x78ed0800
                                // 0x7fea1448 -> 0x0
                                // 0x7fea144c -> 0x0
  CONFIG_1 = 0x50,              // 0x7fea1450 -> 0x00002360
  RETRY_COUNT = 0x54,           // 0x7fea1454 -> 0x0
                                // 0x7fea1458 -> 0x0
                                // 0x7fea145c -> 0x0
  // Uninteresting: (working ok)
  MULTICAST_FILTER_CONTROL = 0x60,
  ADDRESS_0 = 0x62,                
  MULTICAST_HASH = 0x68,
  MAX_PACKET_SIZE = 0x78,
  ADDRESS_1 = 0x7A
};

// Interrupt Status/Mask bits (from Linux driver)
// The driver uses mask 0x00010044 and checks status & 0x0000004C in ISR
// 0x00000004 = TX ring 0 complete
// 0x00000008 = TX ring 1 complete  
// 0x00000040 = RX complete
// 0x00010000 = multicast related
// Combined check in ISR: 0x0000004C = TX0 | TX1 | RX
enum XE_ETH_INTERRUPT_BITS {
  INT_TX_RING0 = 0x00000004,      // TX descriptor ring 0 processed
  INT_TX_RING1 = 0x00000008,      // TX descriptor ring 1 processed
  INT_RX_DONE = 0x20000040,       // RX descriptor processed
  INT_MULTICAST = 0x00010000,     // Multicast related
  INT_UNK_2 = 0x20000000,     // Unknown, hardware shows it when RX INT triggers
  
  // Combined masks used by driver
  INT_TX_RX_MASK = 0x0000004C,    // Mask for TX0 | TX1 | RX (used in ISR check)
  INT_DEFAULT_MASK = 0x00010044,  // Default mask: TX0 | RX | MULTICAST
  
  // Legacy/compatibility aliases
  INT_TX_DONE = INT_TX_RING0,     // Alias for compatibility
  INT_TX_ERROR = 0x00000002,      // TX error occurred
  INT_TX_EMPTY = 0x00000001,      // TX descriptor ring empty
  INT_RX_ERROR = 0x00000020,      // RX error occurred
  INT_RX_OVERFLOW = 0x00000080,   // RX buffer overflow
  INT_LINK_CHANGE = 0x00000100,   // PHY link status changed
  INT_PHY_INT = 0x00000200,       // PHY interrupt
  INT_ALL = 0x000103FF            // All interrupts
};

// CONFIG_0 Register bits (from Linux driver analysis)
// Values seen: 0x08558001 (reset), 0x08550001 (normal)
enum XE_ETH_CONFIG0_BITS {
  CFG0_ENABLE = 0x00000001,       // Bit 0: Enable
  CFG0_SOFT_RESET = 0x00008000,   // Bit 15: Soft reset (when combined with enable)
};

// TX Config Register bits (from Linux driver analysis)
// Bit 0: TX DMA enable
// Bit 4: TX enable (when combined with bit 0)
// Bits 8-11: Ring size
// The value 0x00001c01 enables TX
enum XE_ETH_TX_CONFIG_BITS {
  TX_CFG_RING0_EN = 0x00000010,     // TX RING 0 Enable
  TX_CFG_RING1_EN = 0x00000020,     // TX RING 1 Enable
  TX_CFG_RING_SEL = 0x00010000      // Select ring 1 vs ring 0
};

// RX Config Register bits (from Linux driver analysis)
// Bit 0: RX DMA enable
// Bit 4: RX enable
// The value 0x00101c11 enables RX
enum XE_ETH_RX_CONFIG_BITS {
  RX_CFG_DMA_EN = 0x00000001,     // RX DMA enabled
  RX_CFG_ENABLE = 0x00000011,     // RX fully enabled (bits 0 and 4)
  RX_CFG_PROMISC = 0x00000002,    // Promiscuous mode
  RX_CFG_ALL_MULTI = 0x00000004,  // Accept all multicast
  RX_CFG_BROADCAST = 0x00000008,  // Accept broadcast
  RX_CFG_RING_SIZE_MASK = 0x00000F00,
  RX_CFG_RING_SIZE_SHIFT = 8
};

// TX Descriptor structure (hardware format from Linux driver)
// Each descriptor is 16 bytes (0x10)
// Layout:
//   [0x00] descr[0] = length (for TX: packet length to send)
//   [0x04] descr[1] = flags/status (bit 31 = owned by HW, 0xc0230000 for valid TX)
//   [0x08] descr[2] = buffer physical address
//   [0x0C] descr[3] = length | wrap_bit (bit 31 = wrap/last in ring)
#pragma pack(push, 1)
struct XE_TX_DESCRIPTOR {
  u32 length;         // [0x00] Packet length
  u32 status;         // [0x04] Status/flags (bit 31 = OWN)
  u32 bufferAddress;  // [0x08] Physical address of packet buffer
  u32 lengthWrap;     // [0x0C] Length | wrap bit (bit 31)
};

// TX Descriptor Status bits (descr[1])
enum XE_TX_DESC_STATUS {
  TX_DESC_OWN = 0x80000000,       // Owned by hardware (1) or software (0)
  TX_DESC_VALID = 0xC0230000,     // Valid TX descriptor (from Linux driver)
  TX_DESC_DONE = 0x00000000,      // Transfer complete (OWN cleared)
  TX_DESC_WRAP = 0x80000000,      // Wrap bit in descr[3] (last descriptor in ring)
};

// RX Descriptor structure (hardware format from Linux driver)
// Layout:
//   [0x00] descr[0] = received length (filled by HW)
//   [0x04] descr[1] = flags/status (bit 31 = owned by HW, 0xc0000000 for valid RX)
//   [0x08] descr[2] = buffer physical address
//   [0x0C] descr[3] = buffer_length | wrap_bit
struct XE_RX_DESCRIPTOR {
  u32 receivedLength; // [0x00] Received packet length (filled by HW)
  u32 status;         // [0x04] Status/flags (bit 31 = OWN)
  u32 bufferAddress;  // [0x08] Physical address of buffer
  u32 bufferSizeWrap; // [0x0C] Buffer size | wrap bit (bit 31)
};

// RX Descriptor Status bits (descr[1])
enum XE_RX_DESC_STATUS {
  RX_DESC_OWN = 0x80000000,       // Owned by hardware (1) or software (0)
  RX_DESC_VALID = 0xC0000000,     // Valid RX descriptor (from Linux driver)
  RX_DESC_LEN_MASK = 0x0000FFFF,  // Received length mask in descr[0]
};
#pragma pack(pop)

// Xenon Fast Ethernet PCI Device State struct.
struct XE_PCI_STATE {
  // Transmission
  u32 txConfigReg = 0;
  u32 txDescriptor0BaseReg = 0;
  u32 txDescriptor1BaseReg = 0;
  u32 txDescriptorStatusReg = 0;

  // Reception
  u32 rxConfigReg = 0;
  u32 rxDescriptorBaseReg = 0;

  // Interrupts
  u32 interruptStatusReg = 0;
  u32 interruptMaskReg = 0;

  // Configuration and power
  u32 config0Reg = 0;
  u32 powerReg = 0;
  u32 phyConfigReg = 0;
  u32 phyControlReg = 0;
  u32 config1Reg = 0;
  u32 retryCountReg = 0;

  // Multicast filter control
  u32 multicastFilterControlReg = 0;

  // MAC address (6 bytes stored as array)
  u8 macAddress[6] = {0x00, 0x1D, 0xD8, 0xB7, 0x1C, 0x00}; // Default Xbox-like MAC

  // Multicast hash filters
  u32 multicastHashFilter0 = 0;
  u32 multicastHashFilter1 = 0;

  // Packet limits and secondary address
  u32 maxPacketSizeReg = ETH_MAX_FRAME_SIZE;
  u8 macAddress2[6] = {0};
};

// Packet buffer structure for pending TX/RX
struct EthernetPacket {
  std::vector<u8> data;
  u32 length = 0;
  u64 timestamp = 0;
};

// Ethernet device statistics
struct EthernetStats {
  std::atomic<u64> txPackets{0};
  std::atomic<u64> rxPackets{0};
  std::atomic<u64> txBytes{0};
  std::atomic<u64> rxBytes{0};
  std::atomic<u64> txErrors{0};
  std::atomic<u64> rxErrors{0};
  std::atomic<u64> txDropped{0};
  std::atomic<u64> rxDropped{0};
  std::atomic<u64> collisions{0};
  std::atomic<u64> rxCrcErrors{0};
  std::atomic<u64> rxOverruns{0};
};

class ETHERNET : public PCIDevice {
public:
  ETHERNET(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram);
  ~ETHERNET();
  
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

  // External interface for future bridge support
  void EnqueueRxPacket(const u8* data, u32 length);
  bool DequeueRxPacket(EthernetPacket& packet);
  
  // Get statistics
  const EthernetStats& GetStats() const { return stats; }
  
  // Link status
  bool IsLinkUp() const { return linkUp; }
  void SetLinkUp(bool up);

private:
  // Network bridge initialization
  void InitializeNetworkBridge();
  
  // MDIO (PHY) operations
  u32 MdioRead(u32 addr);
  void MdioWrite(u32 val);
  
  // Descriptor ring processing
  void ProcessRxDescriptors();
  void ProcessTxDescriptors(bool ring0);
  
  // Packet handling
  void HandleTxPacket(const u8 *data, u32 len);
  void HandleRxPacket(const u8* data, u32 len);
  
  // Interrupt management
  void RaiseInterrupt(u32 bits);
  void CheckAndFireInterrupt();
  
  // Worker thread
  void WorkerThreadLoop();
  
  // Descriptor operations
  bool ReadTxDescriptor(bool ring0, u32 index, XE_TX_DESCRIPTOR& desc);
  bool WriteTxDescriptor(bool ring0, u32 index, const XE_TX_DESCRIPTOR& desc);
  bool ReadRxDescriptor(u32 index, XE_RX_DESCRIPTOR& desc);
  bool WriteRxDescriptor(u32 index, const XE_RX_DESCRIPTOR& desc);
  
  // Reset device
  void Reset();

  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus = nullptr;
  // RAM Pointer
  RAM *ramPtr = nullptr;
  
  // MDIO Registers (32 PHYs x 32 registers)
  u16 mdioRegisters[32][32] = {};
  
  // PCI device state
  XE_PCI_STATE ethPciState = {};
  
  // TX/RX state
  std::atomic<bool> rxEnabled{false};
  std::atomic<bool> txRing0Enabled{false};
  std::atomic<bool> txRing1Enabled{false};
  std::atomic<bool> linkUp{true};
  
  // TX descriptor ring tracking
  u32 txRing0Head = 0;  // Next descriptor to process for RING 0 (hardware)
  u32 txRing1Head = 0;  // Next descriptor to process for RING 1 (hardware)
  u32 txRing0Tail = 0;  // Next descriptor to be filled for RING 0 (software)
  u32 txRing1Tail = 0;  // Next descriptor to be filled for RING 1 (software)
  
  // RX descriptor ring tracking  
  u32 rxHead = 0;  // Next descriptor to process (hardware)
  u32 rxTail = 0;  // Next descriptor to be filled (software)
  
  // Pending RX packets (received from bridge/external)
  std::queue<EthernetPacket> pendingRxPackets;
  std::mutex rxQueueMutex;
  
  // Pending TX packets (waiting to be sent to bridge)
  std::queue<EthernetPacket> pendingTxPackets;
  std::mutex txQueueMutex;
  
  // Statistics
  EthernetStats stats;
  
  // Worker thread
  std::thread workerThread;
  std::atomic<bool> workerRunning{false};
  std::condition_variable workerCV;
  std::mutex workerMutex;
};

} // namespace PCIDev
} // namespace Xe
