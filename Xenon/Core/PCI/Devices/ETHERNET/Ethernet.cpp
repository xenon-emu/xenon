/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Xenon Fast Ethernet Adapter emulation

#include "Ethernet.h"

#include "Base/Config.h"
#include "Base/Global.h"
#include "Base/Logging/Log.h"
#include "Base/Thread.h"
#include "Network/NetworkBridge.h"

#include <algorithm>
#include <chrono>
#include <cstddef>

// Every register write, MDIO access, interrupt and frame of the device.
// #define ETH_DEBUG

#ifdef ETH_DEBUG
  #define ETH_DEBUGP(x, ...) LOG_DEBUG(ETH, "[ETH] " x, ##__VA_ARGS__)
#else
  #define ETH_DEBUGP(x, ...)
#endif

// PHY register defaults.
static constexpr u16 kPhyBmcrDefault = 0x1140;   // AN enable, full duplex
static constexpr u16 kPhyBmsrBase = 0x78E9;      // 10/100 HD/FD capable, AN capable, AN complete, ext caps
static constexpr u16 kPhyBmsrLinkUp = 0x0004;    // Link status
static constexpr u16 kPhyAnarDefault = 0x05E1;   // 10/100 HD/FD + PAUSE (kernel asserts PAUSE is advertised)
static constexpr u16 kPhyAnlparLinkUp = 0x45E1;  // Partner: 10/100 HD/FD + PAUSE, ACK
static constexpr u16 kPhyMarvellStatus = 0xA800; // 100 Mbps, full duplex, resolved (bit 10 = real time link)

// PHY ID 2 (model/revision) of a common part of each vendor. The kernel only checks ID 1.
static u16 PhyId2For(u16 id1) {
  switch (id1) {
    case Xe::PCIDev::PHY_ICS: return 0xF450;      // ICS1893
    case Xe::PCIDev::PHY_MICREL: return 0x1513;   // KSZ8041
    case Xe::PCIDev::PHY_ATHEROS: return 0xD023;  // AR8032
    case Xe::PCIDev::PHY_MARVELL: return 0x0CC2;  // 88E3016 family
    case Xe::PCIDev::PHY_BROADCOM: return 0xBC70; // AC131
    default: return 0;
  }
}

static const char *PhyVendorName(u16 id1) {
  switch (id1) {
    case Xe::PCIDev::PHY_ICS: return "ICS";
    case Xe::PCIDev::PHY_MICREL: return "Micrel";
    case Xe::PCIDev::PHY_ATHEROS: return "Atheros";
    case Xe::PCIDev::PHY_MARVELL: return "Marvell";
    case Xe::PCIDev::PHY_BROADCOM: return "Broadcom";
    default: return "unknown";
  }
}

// The PHY of each board. Every board must use one the kernels it ships with accept. Xenon, Zephyr, Falcon and Jasper
// take ICS or Broadcom (kernels up to 6717 accept nothing else), and with Broadcom the driver also rewrites its PAUSE
// advertisement itself, which keeps the PHY init's final check consistent. Trinity uses Broadcom, accepted by every
// kernel. Corona (16 MB and 4 GB) and Winchester use Marvell, the only PHY with a Corona-specific init.
static u16 PhyForConsole(Config::eConsoleRevision revision) {
  switch (revision) {
    case Config::eConsoleRevision::Corona:
    case Config::eConsoleRevision::Corona4GB:
    case Config::eConsoleRevision::Winchester: return Xe::PCIDev::PHY_MARVELL;
    default: return Xe::PCIDev::PHY_BROADCOM;
  }
}

// Constructor
Xe::PCIDev::ETHERNET::ETHERNET(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram)
    : PCIDevice(deviceName, size)
    , interruptLine(parentPCIBridge, PRIO_ENET)
    , dma(ram) {

  // PCI config: Microsoft (0x1414), Xbox 360 Fast Ethernet (0x580A)
  pciConfigSpace.configSpaceHeader.id.hexData = 0x580A1414;
  pciConfigSpace.configSpaceHeader.commandStatus.hexData = 0x02100006;
  pciConfigSpace.configSpaceHeader.classRev.hexData = 0x02000001; // Ethernet controller class
  pciConfigSpace.configSpaceHeader.capabilities.hexData = 0x00000040;
  pciConfigSpace.configSpaceHeader.interrupt.hexData = 0x00000100;

  pciDevSizes[0] = DevSize; // BAR0

  // The PHY depends on the board.
  phyId1 = PhyForConsole(Config::highlyExperimental.consoleRevison);
  phyId2 = PhyId2For(phyId1);
  PhyReset();
  phyRegs[4] = kPhyAnarDefault;

  // Power-on register defaults.
  SetReg32(REG_MAX_PACKET_SIZE, ETH_MAX_FRAME_SIZE);
  static constexpr u8 defaultMac[6] = {0x00, 0x1D, 0xD8, 0xB7, 0x1C, 0x00};
  memcpy(&regFile[REG_MAC_ADDR0], defaultMac, sizeof(defaultMac));

  rxBacklog = std::make_unique<BacklogFrame[]>(kRxBacklogFrames);

  // Initialize the network bridge (if enabled in config).
  InitializeNetworkBridge();

  // Init the worker thread.
  workerRunning = true;
  workerThread.Start("Ethernet", [this] { WorkerLoop(); });

  LOG_INFO(ETH, "Ethernet controller initialized. PHY {} ({:#06x}:{:#06x}), link {}", PhyVendorName(phyId1), phyId1,
           phyId2, linkUp ? "UP" : "DOWN");
}

// Destructor
Xe::PCIDev::ETHERNET::~ETHERNET() {
  // Stop the worker thread and clean up.
  workerRunning = false;
  workerCV.notify_all();
  workerThread.Join();

  // Detach from the network bridge if attached.
  Network::GetNetworkBridge().DetachEthernetDevice();

  LOG_INFO(ETH,
           "Ethernet controller shutdown. Stats: TX: {} packets ({} errors), RX: {} packets ({} dropped), IRQs: {}",
           stats.txPackets.load(), stats.txErrors.load(), stats.rxPackets.load(), stats.rxDropped.load(),
           stats.interrupts.load());
}

// Initialize the Network Bridge.
void Xe::PCIDev::ETHERNET::InitializeNetworkBridge() {
  Network::BridgeConfig bridgeConfig;
  bridgeConfig.enabled = Config::network.enabled;
  bridgeConfig.backendType = Network::StringToBackendType(Config::network.backend);
  bridgeConfig.backendConfig = Config::network.backendConfig;

  auto &bridge = Network::GetNetworkBridge();
  if (bridge.Initialize(bridgeConfig)) {
    // Syncs the backend link state.
    bridge.AttachEthernetDevice(this);
  } else {
    linkUp = true;
    UpdatePhyLinkBits();
  }
}

// Register file helpers

u32 Xe::PCIDev::ETHERNET::GetReg32(u8 offset) const {
  u32 value = 0;
  memcpy(&value, &regFile[offset], sizeof(value));
  return value;
}

void Xe::PCIDev::ETHERNET::SetReg32(u8 offset, u32 value) { memcpy(&regFile[offset], &value, sizeof(value)); }

u16 Xe::PCIDev::ETHERNET::GetReg16(u8 offset) const {
  u16 value = 0;
  memcpy(&value, &regFile[offset], sizeof(value));
  return value;
}

// Reset

// Soft reset (CONFIG0 bit 15). Stops both DMA engines, clears interrupt state and the ring pointers. Base addresses,
// MAC addresses and the rest of the configuration are kept, the kernel reprograms them after every reset anyway.
void Xe::PCIDev::ETHERNET::Reset() {
  txHead[0] = 0;
  txHead[1] = 0;
  rxHead = 0;

  SetReg32(REG_TX_CONFIG, GetReg32(REG_TX_CONFIG) & ~(TX_CFG_ENABLE | TX_CFG_Q0_GO | TX_CFG_Q1_GO));
  SetReg32(REG_RX_CONFIG, GetReg32(REG_RX_CONFIG) & ~(RX_CFG_ENABLE | RX_CFG_ACTIVE));

  intStatus = 0;
  SetReg32(REG_INT_MASK, 0);
  UpdateInterruptLine();

  rxBacklogHead = 0;
  rxBacklogCount = 0;
  rxBacklogged.store(false, std::memory_order_relaxed);

  ETH_DEBUGP("SOFT-RESET complete.");
}

// Full RESET, POR defaults.
void Xe::PCIDev::ETHERNET::SystemReset() {
  std::lock_guard<std::mutex> lock(stateMutex);

  Reset();

  // Register file back to power-on defaults. MAC address 0 persists, the kernel programs it again when it sets the
  // device up anyway.
  u8 mac0[6];
  memcpy(mac0, &regFile[REG_MAC_ADDR0], sizeof(mac0));
  regFile.fill(0);
  memcpy(&regFile[REG_MAC_ADDR0], mac0, sizeof(mac0));
  SetReg32(REG_MAX_PACKET_SIZE, ETH_MAX_FRAME_SIZE);
  txDescBase[0] = 0;
  txDescBase[1] = 0;

  PhyReset();
  phyRegs[4] = kPhyAnarDefault;
  phyDebugRegs.fill(0);
  workPending.store(false, std::memory_order_release);

  LOG_INFO(ETH, "System reset: DMA stopped, registers cleared.");
}

// The worker only touches rings and guest memory under stateMutex, so nothing is in flight once SystemReset returns.
void Xe::PCIDev::ETHERNET::PlatformReset() { SystemReset(); }

// MMIO

void Xe::PCIDev::ETHERNET::Read(u64 barOffset, u8 *data, u64 size) {
  const u8 offset = static_cast<u8>(barOffset);
  const u64 available = std::min<u64>(size, DevSize - offset);

  std::lock_guard<std::mutex> lock(stateMutex);

  const auto covers = [&](u8 reg) { return offset < reg + 4 && offset + available > reg; };

  // Refresh the registers whose value is owned by the hardware.
  if (covers(REG_TX_DESC_BASE)) {
    const int queue = (GetReg32(REG_TX_CONFIG) & TX_CFG_QUEUE_SEL) ? 1 : 0;
    SetReg32(REG_TX_DESC_BASE, txDescBase[queue]);
  }
  if (covers(REG_RX_DESC_CURRENT)) {
    SetReg32(REG_RX_DESC_CURRENT, GetReg32(REG_RX_DESC_BASE) + rxHead * sizeof(EmacDescriptor));
  }
  const bool readsStatus = covers(REG_INT_STATUS);
  if (readsStatus) { SetReg32(REG_INT_STATUS, intStatus); }

  memcpy(data, &regFile[offset], available);
  if (available < size) { memset(data + available, 0, size - available); }

  // INT_STATUS is clear on read. The ISR reads it once after masking, no driver ever writes it.
  if (readsStatus) {
    ETH_DEBUGP("[Read] INT_STATUS = {:#010x}", intStatus);
    intStatus = 0;
    SetReg32(REG_INT_STATUS, 0);
    UpdateInterruptLine();
  }
}

void Xe::PCIDev::ETHERNET::Write(u64 barOffset, const u8 *data, u64 size) {
  const u8 offset = static_cast<u8>(barOffset);
  const u64 available = std::min<u64>(size, DevSize - offset);
  if (available == 0) { return; }

  std::lock_guard<std::mutex> lock(stateMutex);

  const u8 reg = offset & ~3;
  const u32 oldTxConfig = GetReg32(REG_TX_CONFIG);
  const u32 oldRxConfig = GetReg32(REG_RX_CONFIG);

  // Registers that are not plain storage.
  if (reg == REG_INT_STATUS) {
    // The kernel never writes INT_STATUS. Treat writes as write-one-to-clear for other guests.
    u32 value = 0;
    memcpy(reinterpret_cast<u8 *>(&value) + (offset - reg), data, std::min<u64>(available, 4 - (offset - reg)));
    intStatus &= ~value;
    UpdateInterruptLine();
    return;
  }
  if (reg == REG_TX_DESC_BASE) {
    const int queue = (oldTxConfig & TX_CFG_QUEUE_SEL) ? 1 : 0;
    memcpy(reinterpret_cast<u8 *>(&txDescBase[queue]) + (offset - reg), data,
           std::min<u64>(available, 4 - (offset - reg)));
    txHead[queue] = 0;
    ETH_DEBUGP("[Write] TX_DESC_BASE[{}] = {:#010x}", queue, txDescBase[queue]);
    return;
  }

  memcpy(&regFile[offset], data, available);

  // Side effects for every register touched by this access.
  for (u8 r = reg; r < offset + available; r += 4) { OnRegisterWrite(r, oldTxConfig, oldRxConfig); }
}

void Xe::PCIDev::ETHERNET::OnRegisterWrite(u8 reg, u32 oldTxConfig, u32 oldRxConfig) {
  switch (reg) {
    case REG_TX_CONFIG: {
      u32 txConfig = GetReg32(REG_TX_CONFIG);
      ETH_DEBUGP("[Write] TX_CONFIG = {:#010x}", txConfig);
      if (!(txConfig & TX_CFG_ENABLE)) {
        // TX disabled: engine idle, ring pointers back to the ring base, its completion causes withdrawn.
        txConfig &= ~(TX_CFG_Q0_GO | TX_CFG_Q1_GO);
        SetReg32(REG_TX_CONFIG, txConfig);
        txHead[0] = 0;
        txHead[1] = 0;
        intStatus &= ~(INT_TX_Q0_INT | INT_TX_Q0_EMPTY | INT_TX_Q1_INT | INT_TX_Q1_EMPTY);
        UpdateInterruptLine();
      } else if (txConfig & (TX_CFG_Q0_GO | TX_CFG_Q1_GO)) {
        // GO bits are sticky until the queue is found drained. Writing a GO bit doesn't stop the other queue. The
        // queues are walked right here: the descriptors are in memory (written before the GO write) and each frame goes
        // to the host as it is read, so a GO bit reads back clear again once its queue is empty.
        txConfig |= oldTxConfig & (TX_CFG_Q0_GO | TX_CFG_Q1_GO);
        SetReg32(REG_TX_CONFIG, txConfig);
        ProcessTxQueue(0);
        ProcessTxQueue(1);
      }
    } break;

    case REG_RX_CONFIG: {
      const u32 rxConfig = GetReg32(REG_RX_CONFIG);
      ETH_DEBUGP("[Write] RX_CONFIG = {:#010x}", rxConfig);
      if (rxConfig & RX_CFG_ENABLE) {
        // The DPC rewrites RX_CONFIG after draining the ring, descriptors may be free again for the backlog.
        DrainRxBacklog();
      } else {
        // RX disabled: ring pointer back to the ring base, its completion causes withdrawn.
        rxHead = 0;
        intStatus &= ~(INT_RX_INT | INT_RX_EMPTY);
        UpdateInterruptLine();
        if (oldRxConfig & RX_CFG_ENABLE) { ETH_DEBUGP("RX stopped"); }
      }
    } break;

    case REG_RX_DESC_BASE:
      rxHead = 0;
      ETH_DEBUGP("[Write] RX_DESC_BASE = {:#010x}", GetReg32(REG_RX_DESC_BASE));
      break;

    case REG_INT_MASK:
      ETH_DEBUGP("[Write] INT_MASK = {:#010x}", GetReg32(REG_INT_MASK));
      UpdateInterruptLine();
      break;

    case REG_CONFIG0:
      if (GetReg32(REG_CONFIG0) & CONFIG0_SOFT_RESET) { Reset(); }
      break;

    case REG_PHY_CONTROL: HandleMdioCommand(GetReg32(REG_PHY_CONTROL)); break;

    default: break;
  }
}

// MDIO / PHY interface
//
// PHY_CONTROL: data << 16 | reg << 11 | phyAddr << 6 | WRITE(0x20) | BUSY(0x10). The PHY address is 1 on southbridge
// revisions below 0x70 and 0 otherwise, the emulated PHY answers on any address. Commands complete immediately: a
// BUSY bit left set makes the kernel stop with fatal error 75085.

void Xe::PCIDev::ETHERNET::HandleMdioCommand(u32 command) {
  if (!(command & MDIO_BUSY)) { return; }

  const u8 reg = static_cast<u8>((command >> MDIO_REG_SHIFT) & 0x1F);
  u32 result = command & ~MDIO_BUSY;

  if (command & MDIO_WRITE) {
    const u16 value = static_cast<u16>(command >> MDIO_DATA_SHIFT);
    PhyWrite(reg, value);
    ETH_DEBUGP("MDIO WRITE: reg={} val={:#06x}", reg, value);
  } else {
    const u16 value = PhyRead(reg);
    result = (result & 0xFFFF) | (static_cast<u32>(value) << MDIO_DATA_SHIFT);
    ETH_DEBUGP("MDIO READ: reg={} val={:#06x}", reg, value);
  }

  SetReg32(REG_PHY_CONTROL, result);
}

u16 Xe::PCIDev::ETHERNET::PhyRead(u8 reg) {
  reg &= 0x1F;
  if (phyId1 == PHY_ATHEROS && reg == 0x1E) { return phyDebugRegs[phyRegs[0x1D] & 0x3F]; }
  return phyRegs[reg];
}

void Xe::PCIDev::ETHERNET::PhyWrite(u8 reg, u16 value) {
  switch (reg) {
    case 0: // BMCR
      if (value & 0x8000) {
        // Software reset: self clearing, status registers back to defaults. The written control bits take effect.
        const u16 anar = phyRegs[4];
        PhyReset();
        phyRegs[4] = anar;
      }
      // Reset (15) and restart auto-negotiation (9) are self clearing, negotiation completes instantly.
      phyRegs[0] = value & ~0x8200;
      break;
    case 1: // BMSR, read only
    case 2: // PHY ID 1, read only
    case 3: // PHY ID 2, read only
    case 5: // ANLPAR, read only
    case 6: // ANER, read only
      break;
    case 0x1E:
      // Atheros debug port data (the kernel's PHY setup adjusts debug registers 0x0B, 0x12 and 0x29 through it).
      if (phyId1 == PHY_ATHEROS) {
        phyDebugRegs[phyRegs[0x1D] & 0x3F] = value;
      } else {
        phyRegs[0x1E] = value;
      }
      break;
    default: phyRegs[reg & 0x1F] = value; break;
  }
}

void Xe::PCIDev::ETHERNET::PhyReset() {
  const u16 anar = phyRegs[4];
  memset(phyRegs, 0, sizeof(phyRegs));
  phyRegs[0] = kPhyBmcrDefault;
  phyRegs[2] = phyId1;
  phyRegs[3] = phyId2;
  phyRegs[4] = anar ? anar : kPhyAnarDefault;
  phyRegs[6] = 0x000F;
  phyRegs[9] = 0x0300;
  phyRegs[10] = 0x7C00;
  UpdatePhyLinkBits();
}

void Xe::PCIDev::ETHERNET::UpdatePhyLinkBits() {
  const bool up = linkUp.load();
  // BMSR bit 5 (AN complete) stays set so the kernel treats the link state as valid in both directions.
  phyRegs[1] = kPhyBmsrBase | (up ? kPhyBmsrLinkUp : 0);
  phyRegs[5] = up ? kPhyAnlparLinkUp : 0;
  // Marvell's PHY specific status (speed/duplex resolved, real-time link). Other vendors keep their own registers.
  if (phyId1 == PHY_MARVELL) { phyRegs[17] = up ? (kPhyMarvellStatus | 0x0400) : 0; }
}

// Link state

void Xe::PCIDev::ETHERNET::SetLinkUp(bool up) {
  std::lock_guard<std::mutex> lock(stateMutex);
  const bool wasUp = linkUp.exchange(up);
  UpdatePhyLinkBits();
  if (wasUp != up) {
    LOG_INFO(ETH, "Link status: {}", up ? "UP" : "DOWN");
    SetInterruptStatus(INT_LINK);
  }
}

// Interrupts

// Every cause latches into INT_STATUS, whatever INT_MASK says: the mask only gates the line (SiS190 family, which the
// Xbox 360 Linux Kernel patches bring up with the stock sis190 driver). The kernel relies on it: its ISR masks
// everything and reads the status, and its DPC walks the RX ring and restores the mask without walking the ring again,
// so a frame placed in the ring while the mask was 0 has to raise the line once the mask is back (else it waits for the
// next frame, and a TCP sender waits for its acknowledgement until its retransmission timeout).
//
// Stopping an engine (TX/RX_CONFIG enable clear) resets it: its ring pointer goes back to the base and its completion
// causes are withdrawn. libxenon runs the EMAC polled with the mask at 0, its descriptors requesting interrupts, and
// stops both engines before it starts a kernel. Linux's xenon_net then requests its shared interrupt before it has
// rings, and the handler's first (debug) call must find no RX cause. sis190 soft-resets the chip and clears INT_STATUS
// before it unmasks.
void Xe::PCIDev::ETHERNET::SetInterruptStatus(u32 bits) {
  intStatus |= bits;
  UpdateInterruptLine();
}

// The EMAC line is level sensitive, it drops when the ISR masks or reads the status.
void Xe::PCIDev::ETHERNET::UpdateInterruptLine() {
  const u32 mask = GetReg32(REG_INT_MASK);
  if (interruptLine.Set((intStatus & mask) != 0)) {
    ETH_DEBUGP("IRQ assert: status={:#010x} mask={:#010x}", intStatus, mask);
    stats.interrupts++;
  }
}

// DMA helpers

bool Xe::PCIDev::ETHERNET::ReadDescriptor(u32 address, EmacDescriptor &desc) {
  const u8 *ptr = dma.ReadPointer(address, sizeof(EmacDescriptor));
  if (!ptr) { return false; }
  // Ownership is written last by the guest. Read it first, then the rest of the descriptor.
  memcpy(&desc.control, ptr + offsetof(EmacDescriptor, control), sizeof(u32));
  std::atomic_thread_fence(std::memory_order_acquire);
  memcpy(&desc.status, ptr + offsetof(EmacDescriptor, status), sizeof(u32));
  memcpy(&desc.bufferAddr, ptr + offsetof(EmacDescriptor, bufferAddr), sizeof(u32));
  memcpy(&desc.bufferSize, ptr + offsetof(EmacDescriptor, bufferSize), sizeof(u32));
  return true;
}

// TX

void Xe::PCIDev::ETHERNET::ProcessTxQueue(int queue) {
  const u32 goBit = queue ? TX_CFG_Q1_GO : TX_CFG_Q0_GO;
  u32 txConfig = GetReg32(REG_TX_CONFIG);
  if (!(txConfig & TX_CFG_ENABLE) || !(txConfig & goBit)) { return; }

  const u32 base = txDescBase[queue];
  bool raiseInterrupt = false;
  bool drained = false;
  u8 frame[ETH_MAX_FRAME_SIZE];

  for (u32 frames = 0; frames < MAX_RING_DESCRIPTORS; frames++) {
    const u32 firstAddr = base + txHead[queue] * sizeof(EmacDescriptor);
    EmacDescriptor first{};
    if (base == 0 || !ReadDescriptor(firstAddr, first)) {
      LOG_ERROR(ETH, "TX[Q{}]: invalid descriptor address {:#010x}", queue, firstAddr);
      SetInterruptStatus(INT_BUS_ABORT_DETECTED);
      drained = true;
      break;
    }
    if (!(first.control & DESC_OWNERSHIP)) {
      drained = true;
      break;
    }

    // Gather the frame. DW0 of the first descriptor holds the frame length, fragments follow in owned descriptors (the
    // driver splits buffers crossing a page into two descriptors).
    const u32 frameLen = first.status & DESC_SIZE_MASK;
    u32 gathered = 0;
    bool ok = frameLen != 0 && frameLen <= ETH_MAX_FRAME_SIZE;
    u32 head = txHead[queue];
    EmacDescriptor current = first;

    for (u32 fragments = 0; fragments < 8; fragments++) {
      const u32 fragLen = std::min<u32>(current.bufferSize & DESC_SIZE_MASK, frameLen - gathered);
      if (ok && fragLen != 0) {
        const u8 *ptr = dma.ReadPointer(current.bufferAddr, fragLen);
        if (ptr) {
          memcpy(frame + gathered, ptr, fragLen);
          gathered += fragLen;
        } else {
          ok = false;
        }
      }
      head = (current.bufferSize & DESC_WRAP) ? 0 : (head + 1) % MAX_RING_DESCRIPTORS;

      if (!ok || gathered >= frameLen) { break; }

      // Continuation descriptor. It must already be owned, the driver hands over the first descriptor last.
      if (!ReadDescriptor(base + head * sizeof(EmacDescriptor), current) || !(current.control & DESC_OWNERSHIP)) {
        ok = false;
        break;
      }
    }
    txHead[queue] = head;

    // Write back status to the first descriptor only: ownership, error bits and retry count cleared on success. The
    // submitted control word carries 0x00230000 which overlaps the status field, so it must be cleared here or the
    // driver counts every frame as a collision/underrun.
    u32 control = first.control & ~(static_cast<u32>(DESC_OWNERSHIP) | TX_STS_ERROR_MASK | TX_STS_TRY_COUNT_MASK);
    control |= ok ? 1u : TX_STS_TIMEOUT;
    std::atomic_thread_fence(std::memory_order_release);
    if (u8 *ptr = dma.BeginWrite(firstAddr, sizeof(EmacDescriptor))) {
      memcpy(ptr + offsetof(EmacDescriptor, control), &control, sizeof(control));
      dma.EndWrite(firstAddr, sizeof(EmacDescriptor));
    }

    if (ok) {
      // Short frames go out zero padded to the Ethernet minimum.
      u32 sent = frameLen;
      if (sent < ETH_MIN_FRAME_SIZE) {
        memset(frame + sent, 0, ETH_MIN_FRAME_SIZE - sent);
        sent = ETH_MIN_FRAME_SIZE;
      }
      stats.txPackets++;
      HandleTxPacket(frame, sent);
      ETH_DEBUGP("TX[Q{}]: len={} next={}", queue, frameLen, txHead[queue]);
    } else {
      stats.txErrors++;
      LOG_WARNING(ETH, "TX[Q{}]: malformed frame at {:#010x} (len {})", queue, firstAddr, frameLen);
    }

    if (first.control & DESC_INTERRUPT) { raiseInterrupt = true; }
  }

  if (drained) {
    // Queue idle: clear its GO bit so the next transmit restarts it instead of deferring.
    SetReg32(REG_TX_CONFIG, GetReg32(REG_TX_CONFIG) & ~goBit);
  }
  if (raiseInterrupt) { SetInterruptStatus(queue ? INT_TX_Q1_INT : INT_TX_Q0_INT); }
}

// The backend's send only queues the frame, so it runs here, under the device lock, from the guest's GO write.
void Xe::PCIDev::ETHERNET::HandleTxPacket(const u8 *data, u32 len) {
  if (!data || len == 0) { return; }

  auto &bridge = Network::GetNetworkBridge();
  if (bridge.IsActive() && bridge.GetBackend()) { bridge.GetBackend()->SendPacket(data, len); }

  ETH_DEBUGP("TX: dst={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X} len={}", data[0], data[1], data[2], data[3], data[4],
             data[5], len);
}

// RX

// Hardware address filter. Unicast frames must match one of the two station addresses unless promiscuous, broadcast is
// always accepted, multicast needs the multicast enable bits (the driver does exact matching in software).
bool Xe::PCIDev::ETHERNET::AcceptFrame(const u8 *frame, u32 length) const {
  if (length < 6) { return false; }
  const u16 filter = GetReg16(REG_RX_FILTER);
  if (filter & RX_FILTER_PROMISCUOUS) { return true; }

  static constexpr u8 broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (memcmp(frame, broadcast, 6) == 0) { return true; }
  if (frame[0] & 1) { return (filter & (RX_FILTER_MCAST_HASH | RX_FILTER_ALL_MCAST)) != 0; }

  return memcmp(frame, &regFile[REG_MAC_ADDR0], 6) == 0 || memcmp(frame, &regFile[REG_MAC_ADDR1], 6) == 0;
}

bool Xe::PCIDev::ETHERNET::DeliverRxFrame(const u8 *frame, u32 length, bool &interrupt) {
  const u32 base = GetReg32(REG_RX_DESC_BASE);
  if (base == 0) { return false; }

  const u32 descAddr = base + rxHead * sizeof(EmacDescriptor);
  EmacDescriptor desc{};
  if (!ReadDescriptor(descAddr, desc)) {
    LOG_ERROR(ETH, "RX: invalid descriptor address {:#010x}", descAddr);
    SetInterruptStatus(INT_BUS_ABORT_DETECTED);
    return false;
  }
  if (!(desc.control & DESC_OWNERSHIP)) {
    // Ring full, the frame waits until the driver returns descriptors.
    return false;
  }

  // Short frames are zero padded to the Ethernet minimum, the reported length excludes the FCS.
  const u32 frameLen = std::max<u32>(length, ETH_MIN_FRAME_SIZE);
  const u32 bufferSize = desc.bufferSize & DESC_SIZE_MASK;
  u32 status = 1u << RX_STS_DESC_COUNT_SHIFT;

  u8 *buffer = frameLen <= bufferSize ? dma.BeginWrite(desc.bufferAddr, frameLen) : nullptr;
  if (buffer) {
    memcpy(buffer, frame, length);
    if (frameLen > length) { memset(buffer + length, 0, frameLen - length); }
    dma.EndWrite(desc.bufferAddr, frameLen);
    status |= frameLen | RX_STS_CRC_OK;
    stats.rxPackets++;
  } else {
    LOG_ERROR(ETH, "RX: descriptor {} has an unusable buffer {:#010x} (size {})", rxHead, desc.bufferAddr, bufferSize);
    status |= RX_STS_ABORT;
  }

  // Status first, then hand the descriptor back. DW3 (buffer size) is never modified, the driver asserts on it.
  u8 *descPtr = dma.BeginWrite(descAddr, sizeof(EmacDescriptor));
  memcpy(descPtr + offsetof(EmacDescriptor, status), &status, sizeof(status));
  std::atomic_thread_fence(std::memory_order_release);
  const u32 control = desc.control & ~DESC_OWNERSHIP;
  memcpy(descPtr + offsetof(EmacDescriptor, control), &control, sizeof(control));
  dma.EndWrite(descAddr, sizeof(EmacDescriptor));

  ETH_DEBUGP("RX: desc={} len={} buf={:#010x}", rxHead, frameLen, desc.bufferAddr);

  rxHead = (desc.bufferSize & DESC_WRAP) ? 0 : (rxHead + 1) % MAX_RING_DESCRIPTORS;
  if (desc.control & DESC_INTERRUPT) { interrupt = true; }
  return true;
}

void Xe::PCIDev::ETHERNET::DrainRxBacklog() {
  if (!(GetReg32(REG_RX_CONFIG) & RX_CFG_ENABLE)) { return; }
  bool interrupt = false;
  while (rxBacklogCount != 0) {
    const BacklogFrame &waiting = rxBacklog[rxBacklogHead];
    if (!DeliverRxFrame(waiting.data.data(), waiting.length, interrupt)) { break; }
    rxBacklogHead = (rxBacklogHead + 1) % kRxBacklogFrames;
    --rxBacklogCount;
  }
  rxBacklogged.store(rxBacklogCount != 0, std::memory_order_relaxed);
  if (interrupt) { SetInterruptStatus(INT_RX_INT); }
}

// Runs on the backend's reader thread, the frame goes into the ring right here unless
// frames are already waiting for free descriptors, in which case it queues behind them.
void Xe::PCIDev::ETHERNET::EnqueueRxPacket(const u8 *data, u32 length) {
  if (!data || length == 0) { return; }

  std::lock_guard<std::mutex> lock(stateMutex);
  // RX disabled = no reception on real hardware. Dropping here also keeps pre-boot host traffic from piling up against
  // a ring the guest hasn't set up yet.
  if (!(GetReg32(REG_RX_CONFIG) & RX_CFG_ENABLE)) {
    stats.rxDropped++;
    return;
  }
  if (!AcceptFrame(data, length)) { return; }
  if (length > ETH_MAX_FRAME_SIZE) {
    stats.rxDropped++;
    return;
  }

  if (rxBacklogCount == 0) {
    bool interrupt = false;
    const bool delivered = DeliverRxFrame(data, length, interrupt);
    if (interrupt) { SetInterruptStatus(INT_RX_INT); }
    if (delivered) { return; }
  }

  if (rxBacklogCount == kRxBacklogFrames) {
    stats.rxDropped++;
    return;
  }
  BacklogFrame &slot = rxBacklog[(rxBacklogHead + rxBacklogCount) % kRxBacklogFrames];
  memcpy(slot.data.data(), data, length);
  slot.length = length;
  ++rxBacklogCount;
  DrainRxBacklog();
  // Still waiting: the worker retries it (a polled driver returns descriptors without a register write).
  if (rxBacklogCount != 0) { KickWorker(); }
}

// Worker thread

void Xe::PCIDev::ETHERNET::KickWorker() {
  workPending.store(true, std::memory_order_release);
  workerCV.notify_one();
}

void Xe::PCIDev::ETHERNET::WorkerLoop() {

  while (workerRunning && XeRunning) {
    {
      // Frames waiting for descriptors are retried every millisecond, a polled driver (libxenon) returns descriptors
      // without a register write.
      const auto wait =
        rxBacklogged.load(std::memory_order_relaxed) ? std::chrono::milliseconds(1) : std::chrono::milliseconds(100);
      std::unique_lock<std::mutex> lock(workerMutex);
      workerCV.wait_for(lock, wait,
                        [this] { return !workerRunning || !XeRunning || workPending.load(std::memory_order_acquire); });
    }
    if (!workerRunning || !XeRunning) { break; }
    workPending.store(false, std::memory_order_release);

    if (rxBacklogged.load(std::memory_order_relaxed)) {
      std::lock_guard<std::mutex> lock(stateMutex);
      DrainRxBacklog();
    }
  }
}
