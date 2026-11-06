/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Xenon Fast Ethernet Adapter (southbridge EMAC) emulation.
//
// Behaviour follows the kernel's network driver (interrupt driven) and the kernel debugger's (polled). All device
// registers and DMA descriptors are little-endian. MMIO data arrives here in guest memory byte order, so reading it
// as a host little-endian integer gives the device value directly (the guest byte-swaps on its side).

#pragma once

#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/DmaEngine.h"
#include "Core/PCI/InterruptLine.h"
#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/Worker.h"
#include "Core/RAM/RAM.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Frame size constants. The kernel programs MAX_PACKET_SIZE = 1522.
constexpr u32 ETH_MAX_FRAME_SIZE = 1522; // Including VLAN tag, excluding FCS
constexpr u32 ETH_MIN_FRAME_SIZE = 60;   // Minimum frame without FCS, short frames are zero padded

// Most descriptors of a ring walk (the wrap bit is authoritative, this only bounds runaway rings)
constexpr u32 MAX_RING_DESCRIPTORS = 256;

namespace Xe::PCIDev {

  // EMAC Register Map (offsets from BAR0, phys 0xEA001400, kernel VA 0x7FEA1400)
  enum EMAC_REG : u8 {
    REG_TX_CONFIG = 0x00,       // Bit 0 TX enable, bit 4/5 queue 0/1 GO (HW clears when idle), bit 16 queue select
    REG_TX_DESC_BASE = 0x04,    // TX ring base of the queue selected by TX_CONFIG bit 16
    REG_RX_CONFIG = 0x10,       // Bit 0 RX enable, bit 4 RX DMA active
    REG_RX_DESC_BASE = 0x14,    // RX ring base
    REG_RX_DESC_CURRENT = 0x1C, // Current RX descriptor (phys). Kernel asserts it equals the base after RX stop
    REG_INT_STATUS = 0x20,      // Interrupt status, clear on read
    REG_INT_MASK = 0x24,        // Interrupt mask
    REG_CONFIG0 = 0x28,         // Bit 15: soft reset
    REG_PHY_CONTROL = 0x44,     // MDIO command/data
    REG_RX_FILTER = 0x60,       // u16 receive filter
    REG_MAC_ADDR0 = 0x62,       // Unicast address 0 (6 bytes)
    REG_MAX_PACKET_SIZE = 0x78, // u16, 1522
    REG_MAC_ADDR1 = 0x7A,       // Unicast address 1 (6 bytes)
  };

  // INT_STATUS / INT_MASK bits
  enum EMAC_INT_BITS : u32 {
    INT_TX_Q0_INT = 0x00000004,
    INT_TX_Q0_EMPTY = 0x00000008,
    INT_TX_Q1_INT = 0x00000010,
    INT_TX_Q1_EMPTY = 0x00000020,
    INT_RX_INT = 0x00000040,
    INT_RX_EMPTY = 0x00000080,
    INT_LINK = 0x00010000,
    INT_BUS_ABORT_DETECTED = 0x00200000,
  };

  // TX_CONFIG bits
  enum EMAC_TX_CFG : u32 {
    TX_CFG_ENABLE = 0x00000001,    // Bit 0: TX enable (reads back as written)
    TX_CFG_Q0_GO = 0x00000010,     // Bit 4: queue 0 start, cleared by HW once the queue is drained
    TX_CFG_Q1_GO = 0x00000020,     // Bit 5: queue 1 start, cleared by HW once the queue is drained
    TX_CFG_QUEUE_SEL = 0x00010000, // Bit 16: TX_DESC_BASE addresses queue 1
  };

  // RX_CONFIG bits
  enum EMAC_RX_CFG : u32 {
    RX_CFG_ENABLE = 0x00000001, // Bit 0: RX enable
    RX_CFG_ACTIVE = 0x00000010, // Bit 4: RX DMA active. Must stay set while running (kernel watchdog)
  };

  // CONFIG0 bits
  enum EMAC_CONFIG0_BITS : u32 {
    CONFIG0_SOFT_RESET = 0x00008000,
  };

  // RX_FILTER bits
  enum EMAC_RX_FILTER_BITS : u16 {
    RX_FILTER_PROMISCUOUS = 0x0100,
    RX_FILTER_MCAST_HASH = 0x0200,
    RX_FILTER_ALL_MCAST = 0x0400,
  };

// DMA Descriptor (16 bytes, little-endian, 16 byte aligned)
#pragma pack(push, 1)
  struct EmacDescriptor {
    u32 status;     // +0x00 RX: status written by HW. TX: frame length on the first descriptor, 0 on continuations
    u32 control;    // +0x04 Bit 31 ownership (1 = HW), bit 30 interrupt request, TX control/status bits
    u32 bufferAddr; // +0x08 Physical address of the data buffer
    u32 bufferSize; // +0x0C [10:0] buffer size, bit 31 = wrap (last in ring). Never modified by HW
  };
#pragma pack(pop)
  static_assert(sizeof(EmacDescriptor) == 16, "EMAC descriptors are 16 bytes");

  // Descriptor control (DW1) bits, shared by RX and TX
  enum DESC_CONTROL_BITS : u32 {
    DESC_OWNERSHIP = 0x80000000, // Owned by HW
    DESC_INTERRUPT = 0x40000000, // Raise the queue interrupt on completion
  };

  // TX descriptor status bits in DW1, written back by HW on the first descriptor of a frame. Bits 16-19 are, from
  // the lowest, collision, underrun, timeout and tryout.
  enum TX_STATUS_BITS : u32 {
    TX_STS_TRY_COUNT_MASK = 0x000000FF,
    TX_STS_TIMEOUT = 0x00040000,
    TX_STS_ERROR_MASK = 0x000F0000,
  };

  // RX descriptor status (DW0) bits
  enum RX_STATUS_BITS : u32 {
    RX_STS_CRC_OK = 0x00010000,
    RX_STS_ABORT = 0x00800000,
    RX_STS_DESC_COUNT_SHIFT = 24,
  };

  // Descriptor buffer size (DW3)
  enum DESC_SIZE_BITS : u32 {
    DESC_SIZE_MASK = 0x000007FF,
    DESC_WRAP = 0x80000000,
  };

  // PHY vendor IDs (MII register 2, the OUI's upper bits): the only ones the kernel's driver accepts when it sets the
  // PHY up, anything else is fatal error E75. Kernels 1888 through 6717 know ICS and Broadcom, 9199 adds Micrel and
  // Atheros, 17489 Marvell (with a Corona-only sequence for southbridge revision 0x90).
  enum PHY_VENDOR : u16 {
    PHY_ICS = 0x0015,      // OUI 00:A0:BE
    PHY_MICREL = 0x0022,   // OUI 00:10:A1
    PHY_ATHEROS = 0x004D,  // OUI 00:13:74
    PHY_MARVELL = 0x0141,  // OUI 00:50:43
    PHY_BROADCOM = 0x0143, // OUI 00:50:EF
  };

  // PHY_CONTROL (MDIO) bits
  enum MDIO_BITS : u32 {
    MDIO_BUSY = 0x00000010,
    MDIO_WRITE = 0x00000020,
    MDIO_REG_SHIFT = 11,
    MDIO_DATA_SHIFT = 16,
  };

  // Statistics, reported when the device shuts down
  struct EthernetStats {
    std::atomic<u64> txPackets{0};
    std::atomic<u64> rxPackets{0};
    std::atomic<u64> txErrors{0};
    std::atomic<u64> rxDropped{0};
    std::atomic<u64> interrupts{0};
  };

  // Ethernet
  class ETHERNET : public PCIDevice {
  public:
    // Size of the register file, which BAR0 decodes.
    static constexpr u32 DevSize = 0x80;

    ETHERNET(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram);
    ~ETHERNET();

    // MMIO interface.
    void Read(u64 offset, u8 *data, u64 size) override;
    void Write(u64 offset, const u8 *data, u64 size) override;
    void PlatformReset() override;

    // A frame from the host network: placed in the RX ring right away, or in the
    // backlog behind frames already waiting there for free descriptors.
    void EnqueueRxPacket(const u8 *data, u32 length);

    // Sets current link state.
    void SetLinkUp(bool up);

    // Full Power On Reset, called on system reboot. Stops DMA and returns the register file to power-on defaults.
    void SystemReset();

  private:
    void InitializeNetworkBridge();

    // Register file helpers (little-endian storage).
    u32 GetReg32(u8 offset) const;
    void SetReg32(u8 offset, u32 value);
    u16 GetReg16(u8 offset) const;

    // Side effects of a register write, called with the new register file contents in place.
    void OnRegisterWrite(u8 offset, u32 oldTxConfig, u32 oldRxConfig);

    // MDIO (PHY) operations.
    void HandleMdioCommand(u32 command);
    u16 PhyRead(u8 reg);
    void PhyWrite(u8 reg, u16 value);
    void PhyReset();
    void UpdatePhyLinkBits();

    // Descriptor ring processing.
    void ProcessTxQueue(int queue);
    // Places one frame in the RX ring: false when the ring has no free descriptor (or isn't set up yet) and the frame
    // has to wait. interrupt: its descriptor asked for the RX interrupt.
    bool DeliverRxFrame(const u8 *frame, u32 length, bool &interrupt);
    // Moves backlog frames into the ring while it has free descriptors.
    void DrainRxBacklog();
    bool AcceptFrame(const u8 *frame, u32 length) const;

    // Hands a transmitted frame to the host network.
    void HandleTxPacket(const u8 *data, u32 len);

    // Interrupt management.
    void SetInterruptStatus(u32 bits);
    void UpdateInterruptLine();

    // Worker thread.
    void WorkerLoop();
    void KickWorker();

    // Soft reset (CONFIG0 bit 15).
    void Reset();

    // RAM helpers.
    bool ReadDescriptor(u32 address, EmacDescriptor &desc);

    // The level sensitive interrupt line to the bridge.
    Xe::PCI::InterruptLine interruptLine;
    // Bus-master access to guest RAM for descriptor and buffer access.
    Xe::PCI::DmaEngine dma;

    // MMIO register file, little-endian byte image of the device registers.
    std::array<u8, DevSize> regFile{};
    // TX ring bases per queue (0x04 is banked by TX_CONFIG bit 16).
    u32 txDescBase[2] = {};

    // MII/MDIO PHY registers (32 registers)
    u16 phyRegs[32] = {};
    u16 phyId1 = PHY_BROADCOM;
    u16 phyId2 = 0;
    // Atheros debug port: register 0x1D selects, 0x1E reads/writes the selected debug register.
    std::array<u16, 0x40> phyDebugRegs{};

    // Protects registers, ring state and the interrupt line.
    std::mutex stateMutex;

    // Interrupt state.
    u32 intStatus = 0; // INT_STATUS, clear on read

    // Ring state (internal DMA pointers, reset to the ring base when the engine is disabled).
    u32 txHead[2] = {};
    u32 rxHead = 0;

    std::atomic<bool> linkUp{true};

    // RX backlog: frames that found no free descriptor, oldest first, in preallocated slots.
    // rxBacklogged mirrors a non-empty backlog for the worker, which retries it.
    static constexpr u32 kRxBacklogFrames = 256;
    struct BacklogFrame {
      std::array<u8, ETH_MAX_FRAME_SIZE> data;
      u32 length;
    };
    std::unique_ptr<BacklogFrame[]> rxBacklog;
    u32 rxBacklogHead = 0;
    u32 rxBacklogCount = 0;
    std::atomic<bool> rxBacklogged{false};

    // Current Statistics counters.
    EthernetStats stats;

    // Worker thread: retries the RX backlog (descriptors a polled guest returns without a register write).
    Xe::PCI::Worker workerThread;
    std::atomic<bool> workerRunning{false};
    std::atomic<bool> workPending{false};
    std::condition_variable workerCV;
    std::mutex workerMutex;
  };

} // namespace Xe::PCIDev
