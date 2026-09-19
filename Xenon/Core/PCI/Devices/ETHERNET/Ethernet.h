/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Xenon Fast Ethernet Adapter Emulation.

#pragma once

#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/PCIDevice.h"
#include "Core/RAM/RAM.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#define ETHERNET_DEV_SIZE 0x80

// Frame size constants
#define ETH_MAX_FRAME_SIZE 1522 // Including VLAN tag
#define ETH_MTU            1500
#define ETH_MIN_FRAME_SIZE 64
#define ETH_RX_BUFFER_SIZE 1528 // 1522 data + 6 padding (kernel uses this)

// Descriptor ring sizes (kernel allocates 64 RX descriptors)
#define NUM_RING0_TX_DESCRIPTORS 32
#define NUM_RING1_TX_DESCRIPTORS 8
#define NUM_RX_DESCRIPTORS       64

namespace Xe {
  namespace PCIDev {

    // EMAC Register Map (offsets from BAR0, base 0x7FEA1400)
    enum EMAC_REG : u8 {
      REG_TX_CONFIG = 0x00,      // TX DMA config. Bit 0: active (HW status). Bit 4: Q0 enable. Bit 5: Q1 enable.
      REG_TX_DESC_BASE = 0x04,   // TX descriptor ring base (selected by TX_CONFIG ring select bit)
      REG_TX_DESC_STATUS = 0x0C, // TX descriptor status / next free
      REG_RX_CONFIG = 0x10,      // RX DMA config. Bit 0: active (HW status). Kernel writes 0x111C1000 to enable.
      REG_RX_DESC_BASE = 0x14,   // RX descriptor ring base address
      REG_RX_DESC_STATUS = 0x1C, // RX descriptor status / next free
      REG_ISR_STATUS = 0x20,     // Interrupt Status (read-only from kernel, written by HW)
      REG_INT_ENABLE = 0x24,     // Interrupt Enable/Mask. Write 0 = disable all. Write 0x54000100 = enable.
      REG_SOFT_RESET = 0x28,     // Soft reset. Write 0x01805504 = assert, 0x01005504 = release.
      REG_POWER = 0x30,          // Power management
      REG_MAC_CONFIG = 0x40,     // MAC configuration (speed, duplex, flow control)
      REG_MII_CMD = 0x44,        // MII/MDIO command register (PHY access)
      REG_MII_CONFIG = 0x48,     // MII configuration (FORCE_LINK toggle for Jasper)
      REG_MAC_SPEED = 0x50,      // MAC speed/duplex setting
      REG_FLOW_CTRL = 0x54,      // Flow control (0x10000000 when link down, 0 when up)
      REG_RX_FILTER = 0x60,      // Receive filter control
      REG_MAC_ADDR0 = 0x62,      // Primary MAC address (6 bytes, byte-written with eieio)
      REG_TX_DESC_Q0 = 0x68,     // TX descriptor ring Q0 offset
      REG_TX_DESC_Q1 = 0x6C,     // TX descriptor ring Q1 offset
      REG_MCAST_HASH0 = 0x68,    // Multicast hash filter 0 (alias, context-dependent)
      REG_MCAST_HASH1 = 0x6C,    // Multicast hash filter 1 (alias, context-dependent)
      REG_RX_MAX_FRAME = 0x78,   // Max frame size / RX config
      REG_MAC_ADDR1 = 0x7A,      // Secondary MAC address (6 bytes)
    };

    // ISR_STATUS bits (0x7FEA1420)
    enum EMAC_ISR_BITS : u32 {
      ISR_TX_Q0 = 0x00000010, // Bit 4: TX queue 0 complete
      ISR_TX_Q1 = 0x00000020, // Bit 5: TX queue 1 complete
      ISR_RX = 0x00000040,    // Bit 6: RX frame received
      ISR_MII = 0x00010000,   // Bit 16: PHY/MII status change
    };

// INT_ENABLE values (0x7FEA1424) - from kernel ISR/DPC flow
#define INT_ENABLE_ALL  0x54000100u // Enable TX/RX/MII interrupts
#define INT_ENABLE_NONE 0x00000000u // Disable all interrupts

    // TX_CONFIG bits (0x7FEA1400)
    enum EMAC_TX_CFG : u32 {
      TX_CFG_ACTIVE = 0x00000001,    // Bit 0: TX DMA active (HW status, poll to check stop)
      TX_CFG_Q0_EN = 0x00000010,     // Bit 4: TX queue 0 enable
      TX_CFG_Q1_EN = 0x00000020,     // Bit 5: TX queue 1 enable
      TX_CFG_BASE_BITS = 0x00001C00, // Bits [12:10]: base configuration
      TX_CFG_RING_SEL = 0x00010000,  // Bit 16: select ring 1 for descriptor base read/write
      TX_CFG_START = 0x00001C01,     // Base value for NicStartTx (active + base bits)
    };

    // RX_CONFIG bits (0x7FEA1410)
    enum EMAC_RX_CFG : u32 {
      RX_CFG_ACTIVE = 0x00000001, // Bit 0: RX DMA active (HW status, poll to check stop)
      RX_CFG_ENABLE = 0x111C1000, // Full enable value written by kernel
    };

// DMA Descriptor (16 bytes)
#pragma pack(push, 1)
    struct EmacDescriptor {
      u32 status;     // +0x00: Status/control flags
      u32 ownership;  // +0x04: Bit 31 = HW owns (1=HW, 0=SW)
      u32 bufferAddr; // +0x08: Physical address of data buffer
      u32 sizeFlags;  // +0x0C: [10:0] buffer size, bit 31 = wrap (last in ring)
    };
#pragma pack(pop)

    // RX Descriptor Status (DW0) bits
    enum RX_STATUS_BITS : u32 {
      RX_STS_FRAME_LEN_MASK = 0x000007FF, // [10:0] received frame length
      RX_STS_FRAME_OK = 0x00010000,       // Bit 16: frame received OK
      RX_STS_CRC_ERROR = 0x00020000,      // Bit 17: CRC error
      RX_STS_ALIGN_ERROR = 0x00080000,    // Bit 19: alignment error
      RX_STS_OVERFLOW = 0x00200000,       // Bit 21: RX overflow
      RX_STS_TOO_LONG = 0x00400000,       // Bit 22: frame too long
      RX_STS_DESC_COUNT_SHIFT = 24,
    };

    // TX Descriptor Ownership (DW1) flags
    enum TX_OWN_BITS : u32 {
      TX_OWN_HW = 0x80000000,          // Bit 31: HW owns descriptor
      TX_OWN_CTRL_NORMAL = 0x40200000, // SOP + EOP + CRC insert (normal frame)
      TX_OWN_CTRL_VLAN = 0x40300000,   // SOP + EOP + CRC + VLAN tagged
    };

    // RX Descriptor Ownership (DW1)
    enum RX_OWN_BITS : u32 {
      RX_OWN_HW = 0x80000000,   // Bit 31: HW owns (ready for receive)
      RX_OWN_INIT = 0x000000C0, // Initial ownership value
    };

    // Descriptor Size/Flags (DW3)
    enum DESC_SIZE_BITS : u32 {
      DESC_SIZE_MASK = 0x000007FF, // [10:0] buffer size
      DESC_WRAP = 0x80000000,      // Bit 31: wrap to first descriptor
    };

// SOFT_RESET register values (0x7FEA1428)
#define SOFT_RESET_ASSERT  0x01805504u // Assert reset
#define SOFT_RESET_RELEASE 0x01005504u // Release reset
#define SOFT_RESET_BIT     0x00800000u // The bit that differs between assert/release

    // PHY vendor IDs (MII register 2)
    enum PHY_VENDOR : u16 {
      PHY_ICS = 0x0015,      // ICS (Xenon/Zephyr boards)
      PHY_MICREL = 0x0022,   // Micrel
      PHY_ATHEROS = 0x004D,  // Atheros AR8035 (Slim/E models)
      PHY_MARVELL = 0x0141,  // Marvell 88E1116 (Jasper)
      PHY_BROADCOM = 0x0143, // Broadcom (Xenon)
    };

    // Packet buffer for pending TX/RX
    struct EthernetPacket {
      std::vector<u8> data;
      u32 length = 0;
    };

    // Statistics
    struct EthernetStats {
      std::atomic<u64> txPackets{0};
      std::atomic<u64> rxPackets{0};
      std::atomic<u64> txBytes{0};
      std::atomic<u64> rxBytes{0};
      std::atomic<u64> txErrors{0};
      std::atomic<u64> rxErrors{0};
      std::atomic<u64> txDropped{0};
      std::atomic<u64> rxDropped{0};
    };

    // Ethernet
    class ETHERNET : public PCIDevice {
    public:
      ETHERNET(const std::string& deviceName, u64 size, PCIBridge* parentPCIBridge, RAM* ram);
      ~ETHERNET();

      // MMIO interface.
      void Read(u64 readAddress, u8* data, u64 size) override;
      void Write(u64 writeAddress, const u8* data, u64 size) override;
      void MemSet(u64 writeAddress, s32 data, u64 size) override;
      void ConfigRead(u64 readAddress, u8* data, u64 size) override;
      void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

      // External interface for network bridge.
      void EnqueueRxPacket(const u8* data, u32 length);
      bool DequeueRxPacket(EthernetPacket& packet);

      // Returns a reference to the current statistics.
      const EthernetStats& GetStats() const { return stats; }

      // Returns true if the link is up.
      bool IsLinkUp() const { return linkUp; }
      // Sets current link state.
      void SetLinkUp(bool up);

      // Full Power On Reset, called on system reboot. Unlike the guest-initiated soft reset (REG_SOFT_RESET), this also
      // returns the register file to power-on defaults. NOTE: A reboot must stop DMA, or the stale RX ring base from
      // the previous  kernel keeps scribbling status writes over the new kernel's freshly reset RAM.
      void SystemReset();

    private:
      // Initializes the network bridge.
      void InitializeNetworkBridge();

      // MDIO (PHY) operations.
      void HandleMdioRead();
      void HandleMdioWrite(u32 val);

      // Descriptor ring processing.
      void ProcessTxRing(int queueIndex);
      void ProcessRxDescriptors();

      // Packet handling.
      void HandleTxPacket(const u8* data, u32 len);

      // Interrupt management.
      void SetInterruptStatus(u32 bits);
      void FireInterrupt();

      // Worker thread.
      void WorkerThreadLoop();

      // Hardware reset.
      void Reset();

      // PCI bridge for interrupt routing.
      PCIBridge* parentBus = nullptr;
      // RAM Pointer for descriptor and buffer access.
      RAM* ramPtr = nullptr;

      // MMIO register file.
      struct {
        u32 txConfig = 0;              // 0x00
        u32 txDescBase[2] = {};        // 0x04 (per-ring, selected by ring_sel)
        u32 txDescStatus = 0;          // 0x0C
        u32 rxConfig = 0;              // 0x10
        u32 rxDescBase = 0;            // 0x14
        u32 rxDescStatus = 0;          // 0x1C
        std::atomic<u32> isrStatus{0}; // 0x20
        u32 intEnable = 0;             // 0x24
        u32 softReset = 0;             // 0x28
        u32 power = 0;                 // 0x30
        u32 macConfig = 0;             // 0x40
        u32 miiCmd = 0;                // 0x44
        u32 miiConfig = 0;             // 0x48
        u32 macSpeed = 0;              // 0x50
        u32 flowCtrl = 0;              // 0x54
        u32 rxFilter = 0;              // 0x60
        u8 macAddr0[6] = {0x00, 0x1D, 0xD8, 0xB7, 0x1C, 0x00};
        u32 txDescQ0 = 0;    // 0x68
        u32 txDescQ1 = 0;    // 0x6C
        u32 rxMaxFrame = 0;  // 0x78
        u8 macAddr1[6] = {}; // 0x7A
      } regs;

      // MII/MDIO PHY registers (32 registers)
      u16 phyRegs[32] = {};

      // Mutex for protecting shared state.
      std::mutex stateMutex;

      // DMA state
      std::atomic<bool> rxActive{false};
      std::atomic<bool> txQ0Active{false};
      std::atomic<bool> txQ1Active{false};
      std::atomic<bool> linkUp{true};

      // Interrupt state.
      std::atomic<bool> interruptsEnabled{false};
      u32 isrAckedBits = 0; // Bits the ISR has read (cleared on INT_ENABLE re-enable)

      // Descriptor ring indices.
      u32 txHead[2] = {};
      u32 rxHead = 0;

      // Pending RX packets.
      std::queue<EthernetPacket> pendingRxPackets;
      std::mutex rxQueueMutex;
      std::atomic<bool> hasRxPending{false};

      // Current Statistics counters.
      EthernetStats stats;

      // Worker thread for processing TX/RX descriptors.
      std::thread workerThread;
      // Flag to indicate if the worker thread is running.
      std::atomic<bool> workerRunning{false};
      // Condition variable and mutex for worker thread synchronization.
      std::condition_variable workerCV;
      std::mutex workerMutex;
    };

  } // namespace PCIDev
} // namespace Xe
