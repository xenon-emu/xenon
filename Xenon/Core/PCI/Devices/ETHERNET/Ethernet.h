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
#define NUM_TX_DESCRIPTORS 16
#define NUM_RX_DESCRIPTORS 16

// Descriptor ring mask
#define TX_DESC_MASK (NUM_TX_DESCRIPTORS - 1)
#define RX_DESC_MASK (NUM_RX_DESCRIPTORS - 1)

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

// Interrupt Status/Mask bits
enum XE_ETH_INTERRUPT_BITS {
  INT_TX_DONE = 0x00000001,       // TX descriptor processed
  INT_TX_ERROR = 0x00000002,      // TX error occurred
  INT_TX_EMPTY = 0x00000004,      // TX descriptor ring empty
  INT_RX_DONE = 0x00000010,       // RX descriptor processed
  INT_RX_ERROR = 0x00000020,      // RX error occurred
  INT_RX_OVERFLOW = 0x00000040,   // RX buffer overflow
  INT_LINK_CHANGE = 0x00000100,   // PHY link status changed
  INT_PHY_INT = 0x00000200,       // PHY interrupt
  INT_ALL = 0x000003FF            // All interrupts
};

// TX Config Register bits
enum XE_ETH_TX_CONFIG_BITS {
  TX_CFG_ENABLE = 0x80000000,     // TX enabled
  TX_CFG_DMA_EN = 0x00000001,     // TX DMA enabled
  TX_CFG_RING_SIZE_MASK = 0x00000F00, // Ring size bits
  TX_CFG_RING_SIZE_SHIFT = 8
};

// RX Config Register bits
enum XE_ETH_RX_CONFIG_BITS {
  RX_CFG_ENABLE = 0x80000000,     // RX enabled
  RX_CFG_DMA_EN = 0x00000001,     // RX DMA enabled
  RX_CFG_PROMISC = 0x00000002,    // Promiscuous mode
  RX_CFG_ALL_MULTI = 0x00000004,  // Accept all multicast
  RX_CFG_BROADCAST = 0x00000008,  // Accept broadcast
  RX_CFG_RING_SIZE_MASK = 0x00000F00,
  RX_CFG_RING_SIZE_SHIFT = 8
};

// TX Descriptor structure (hardware format)
// Each descriptor is 16 bytes
#pragma pack(push, 1)
struct XE_TX_DESCRIPTOR {
  u32 bufferAddress;    // Physical address of packet buffer
  u32 bufferSize;       // Size of buffer / packet length
  u32 status;           // Status and control flags
  u32 reserved;         // Reserved/padding
};

// TX Descriptor Status bits
enum XE_TX_DESC_STATUS {
  TX_DESC_OWN = 0x80000000,       // Owned by hardware (1) or software (0)
  TX_DESC_FIRST = 0x40000000,     // First descriptor of packet
  TX_DESC_LAST = 0x20000000,      // Last descriptor of packet
  TX_DESC_INT = 0x10000000,       // Generate interrupt on completion
  TX_DESC_CRC = 0x08000000,       // Append CRC
  TX_DESC_PAD = 0x04000000,       // Pad short frames
  TX_DESC_DONE = 0x00000001,      // Transfer complete
  TX_DESC_ERROR = 0x00000002,     // Error occurred
  TX_DESC_RETRY = 0x00000004,     // Retry limit exceeded
  TX_DESC_COLLISION = 0x00000008, // Collision detected
  TX_DESC_LEN_MASK = 0x0000FFFF   // Packet length mask (lower 16 bits of bufferSize)
};

// RX Descriptor structure (hardware format)
// Each descriptor is 16 bytes
struct XE_RX_DESCRIPTOR {
  u32 bufferAddress;    // Physical address of buffer
  u32 bufferSize;       // Size of buffer
  u32 status;           // Status and received length
  u32 reserved;         // Reserved/padding
};

// RX Descriptor Status bits
enum XE_RX_DESC_STATUS {
  RX_DESC_OWN = 0x80000000,       // Owned by hardware (1) or software (0)
  RX_DESC_FIRST = 0x40000000,     // First descriptor of packet
  RX_DESC_LAST = 0x20000000,      // Last descriptor of packet
  RX_DESC_INT = 0x10000000,       // Generate interrupt
  RX_DESC_DONE = 0x00000001,      // Transfer complete
  RX_DESC_ERROR = 0x00000002,     // Error occurred
  RX_DESC_CRC_ERR = 0x00000004,   // CRC error
  RX_DESC_OVERFLOW = 0x00000008,  // Buffer overflow
  RX_DESC_RUNT = 0x00000010,      // Runt frame (too short)
  RX_DESC_MULTICAST = 0x00000020, // Multicast frame
  RX_DESC_BROADCAST = 0x00000040, // Broadcast frame
  RX_DESC_LEN_MASK = 0x0000FFFF   // Received length mask
};
#pragma pack(pop)

// Xenon Fast Ethernet PCI Device State struct.
struct XE_PCI_STATE {
  // Transmission
  u32 txConfigReg = 0;
  u32 txDescriptorBaseReg = 0;
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
  // MDIO (PHY) operations
  u32 MdioRead(u32 addr);
  void MdioWrite(u32 val);
  
  // Descriptor ring processing
  void ProcessRxDescriptors();
  void ProcessTxDescriptors();
  
  // Packet handling
  void HandleTxPacket(const u8 *data, u32 len);
  void HandleRxPacket(const u8* data, u32 len);
  
  // Interrupt management
  void RaiseInterrupt(u32 bits);
  void UpdateInterruptStatus();
  void CheckAndFireInterrupt();
  
  // Worker thread
  void WorkerThreadLoop();
  
  // Descriptor operations
  bool ReadTxDescriptor(u32 index, XE_TX_DESCRIPTOR& desc);
  bool WriteTxDescriptor(u32 index, const XE_TX_DESCRIPTOR& desc);
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
  std::atomic<bool> txEnabled{false};
  std::atomic<bool> linkUp{true};
  
  // TX descriptor ring tracking
  u32 txHead = 0;  // Next descriptor to process (hardware)
  u32 txTail = 0;  // Next descriptor to be filled (software)
  
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
