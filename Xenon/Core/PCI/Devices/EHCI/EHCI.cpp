/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "EHCI.h"

#include "Base/Logging/Log.h"
#include "Core/PCI/Bridge/PCIBridge.h"

#include <algorithm>
#include <array>
#include <bit>

// Every register access is logged with this on (a lot of output):
// #define EHCI_DEBUG

#ifdef EHCI_DEBUG
  #define EHCI_DEBUGP(x, ...) LOG_DEBUG(EHCI, "[EHCI] " x, ##__VA_ARGS__)
#else
  #define EHCI_DEBUGP(x, ...)
#endif

constexpr u32 kCapLength = 0x20;
constexpr u32 kHciVersion = 0x0100;

// Operational registers (from BAR + CAPLENGTH)
constexpr u32 kRegUsbCmd = 0x00;
constexpr u32 kRegUsbSts = 0x04;
constexpr u32 kRegUsbIntr = 0x08;
constexpr u32 kRegFrIndex = 0x0C;
constexpr u32 kRegCtrlDsSegment = 0x10;
constexpr u32 kRegPeriodicListBase = 0x14;
constexpr u32 kRegAsyncListAddr = 0x18;
constexpr u32 kRegConfigFlag = 0x40;
constexpr u32 kRegPortSC = 0x44;

// USBCMD
constexpr u32 kCmdRS = 1u << 0;
constexpr u32 kCmdHCReset = 1u << 1;
constexpr u32 kCmdPSE = 1u << 4;
constexpr u32 kCmdASE = 1u << 5;
constexpr u32 kCmdIAAD = 1u << 6;
constexpr u32 kCmdWritable = 0x00FF0B71u; // ITC, park, IAAD, ASE, PSE, RS (FLS fixed at 1024)

// USBSTS
constexpr u32 kStsUsbInt = 1u << 0;
constexpr u32 kStsUsbErrInt = 1u << 1;
constexpr u32 kStsPCD = 1u << 2;
constexpr u32 kStsFLR = 1u << 3;
constexpr u32 kStsIAA = 1u << 5;
constexpr u32 kStsLatched = 0x3Fu;
constexpr u32 kStsHalted = 1u << 12;
constexpr u32 kStsPSS = 1u << 14;
constexpr u32 kStsASS = 1u << 15;

// PORTSC
constexpr u32 kPortCCS = 1u << 0;
constexpr u32 kPortCSC = 1u << 1;
constexpr u32 kPortPE = 1u << 2;
constexpr u32 kPortPEC = 1u << 3;
constexpr u32 kPortOCC = 1u << 5;
constexpr u32 kPortFPR = 1u << 6;
constexpr u32 kPortSuspend = 1u << 7;
constexpr u32 kPortPR = 1u << 8;
constexpr u32 kPortLineK = 1u << 10;
constexpr u32 kPortLineJ = 2u << 10;
constexpr u32 kPortLineMask = 3u << 10;
constexpr u32 kPortPP = 1u << 12;
constexpr u32 kPortPO = 1u << 13;
constexpr u32 kPortChanges = kPortCSC | kPortPEC | kPortOCC;
constexpr u32 kPortStored = 0x007FC000u; // indicator, test control, wake enables

// Link pointers
constexpr u32 kLinkTerminate = 1u << 0;
constexpr u32 kTypeITD = 0;
constexpr u32 kTypeQH = 1;

// QH dword 1
constexpr u32 kQhDTC = 1u << 14;
// qTD token
constexpr u32 kTokActive = 1u << 7;
constexpr u32 kTokHalted = 1u << 6;
constexpr u32 kTokXactErr = 1u << 3;
constexpr u32 kTokIOC = 1u << 15;
constexpr u32 kTokDT = 1u << 31;
// iTD transaction
constexpr u32 kItdActive = 1u << 31;
constexpr u32 kItdXactErr = 1u << 28;
constexpr u32 kItdIOC = 1u << 15;

constexpr u32 kMaxLinks = 256;
constexpr u32 kMaxQtdsPerQh = 16;
constexpr u32 kMaxAsyncPackets = 1024;
constexpr u32 kMaxPacket = 3 * 1024;
// A qTD moves at most 0x5000 bytes (five buffer pages).
constexpr u32 kMaxQtdBuffer = 0x5000;

// Packets a whole-transfer transaction stood for (each flips the data toggle): every full max-packet, plus the short or
// zero-length one that ended it.
static u32 PacketsFor(u32 bytes, u32 maxPacket, bool endedShort) {
  const u32 full = bytes / maxPacket;
  return (bytes % maxPacket || endedShort || bytes == 0) ? full + 1 : full;
}

Xe::PCIDev::EHCI::EHCI(const std::string &deviceName, u64 size, s32 instance, PCIBridge *parentPCIBridge, RAM *ram,
                       std::shared_ptr<USB::PortGroup> portGroup)
    : PCIDevice(deviceName, size)
    , instance(instance)
    , interruptLine(parentPCIBridge, instance == 0 ? PRIO_EHCI_0 : PRIO_EHCI_1)
    , ports(std::move(portGroup))
    , dma(ram) {

  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.id.hexData = instance == 0 ? 0x58051414 : 0x58071414;
  pciConfigSpace.configSpaceHeader.commandStatus.hexData = 0x02900106;
  pciConfigSpace.configSpaceHeader.classRev.hexData = 0x0C032001;
  pciConfigSpace.configSpaceHeader.subsystem.hexData = instance == 0 ? 0x58051414 : 0x58071414;
  pciConfigSpace.configSpaceHeader.capabilities.hexData = instance == 0 ? 0x00000050 : 0x00000000;
  pciConfigSpace.configSpaceHeader.interrupt.hexData = 0x50000400;

  // Set our PCI Dev Sizes
  pciDevSizes[0] = DevSize; // BAR0

  numPorts = std::min(ports->NumPorts(), kMaxPorts);
  // N_PORTS, no port power control, all ports behind one companion.
  hcsParams = numPorts | (numPorts << 8) | (1u << 12);
  // 32-bit addressing, fixed 1024-entry frame list, no park mode, no extended capabilities.
  hccParams = 0;

  {
    std::lock_guard lock(ports->Mutex());
    ports->SetEhci(this);
    Reset();
  }
  clock.Start(deviceName, [this](u32 frames) { OnFrames(frames); });
}

Xe::PCIDev::EHCI::~EHCI() {
  clock.Stop();
  std::lock_guard lock(ports->Mutex());
  ports->SetEhci(nullptr);
}

// The console's reset line: HCRESET, with the frame clock stopped (nothing walks the schedules in guest memory any more
// once the lock is released), the devices given a bus reset and the ports re-evaluated.
void Xe::PCIDev::EHCI::PlatformReset() {
  std::lock_guard lock(ports->Mutex());
  Reset();
  ports->ResetDevices();
  for (u32 port = 0; port < numPorts; ++port) { OnPortChanged(port); }
}

// Power-on and HCRESET: operational registers to their defaults, CONFIGFLAG 0, every port to the companion.
void Xe::PCIDev::EHCI::Reset() {
  usbCmd = 0x00080000; // ITC 8 microframes
  usbSts = 0;
  usbIntr = 0;
  frameIndex = 0;
  ctrlDsSegment = 0;
  periodicListBase = 0;
  asyncListAddr = 0;
  configFlag = 0;
  pendingConnects = 0;
  for (u32 port = 0; port < numPorts; ++port) {
    portSC[port] = kPortPP | kPortPO;
    ports->SetCompanionOwned(port, true);
  }
  UpdateInterruptLine();
  UpdateClock();
}

// MMIO

void Xe::PCIDev::EHCI::Read(u64 barOffset, u8 *data, u64 size) {
  const u32 offset = static_cast<u32>(barOffset);
  u32 value = 0;
  {
    std::lock_guard lock(ports->Mutex());
    value = ReadRegister(offset & ~3u);
  }
  EHCI_DEBUGP("{} Read({:#x}) == {:#x}", instance, offset, value);
  value = byteswap_le<u32>(value) >> ((offset & 3) * 8);
  memcpy(data, &value, std::min<u64>(size, 4));
}

void Xe::PCIDev::EHCI::Write(u64 barOffset, const u8 *data, u64 size) {
  const u32 offset = static_cast<u32>(barOffset);
  if (size != 4 || (offset & 3)) {
    LOG_WARNING(EHCI, "{} unsupported {}-byte write at {:#x}", instance, size, offset);
    return;
  }
  u32 value = 0;
  memcpy(&value, data, 4);
  value = byteswap_le<u32>(value);
  EHCI_DEBUGP("{} Write({:#x}, {:#x})", instance, offset, value);
  std::lock_guard lock(ports->Mutex());
  WriteRegister(offset, value);
}

u32 Xe::PCIDev::EHCI::ReadRegister(u32 offset) {
  switch (offset) {
    case 0x00: return (kHciVersion << 16) | kCapLength;
    case 0x04: return hcsParams;
    case 0x08: return hccParams;
    case 0x0C: return 0; // HCSP-PORTROUTE (unused: N_PCC covers all ports)
    default: break;
  }
  if (offset < kCapLength) { return 0; }
  offset -= kCapLength;
  switch (offset) {
    case kRegUsbCmd: return usbCmd;
    case kRegUsbSts: return GetStatus();
    case kRegUsbIntr: return usbIntr;
    case kRegFrIndex: return frameIndex;
    case kRegCtrlDsSegment: return ctrlDsSegment;
    case kRegPeriodicListBase: return periodicListBase;
    case kRegAsyncListAddr: return asyncListAddr;
    case kRegConfigFlag: return configFlag;
    default:
      if (offset >= kRegPortSC && offset < kRegPortSC + numPorts * 4) { return portSC[(offset - kRegPortSC) / 4]; }
      return 0;
  }
}

void Xe::PCIDev::EHCI::WriteRegister(u32 offset, u32 value) {
  if (offset < kCapLength) {
    LOG_WARNING(EHCI, "{} write of {:#x} to capability register {:#x}", instance, value, offset);
    return;
  }
  offset -= kCapLength;
  switch (offset) {
    case kRegUsbCmd: WriteCommand(value); break;
    case kRegUsbSts: {
      const bool cleared = (usbSts & value & kStsLatched) != 0;
      usbSts &= ~(value & kStsLatched);
      // Edge forwarding, same as in the OHCI: Linux (ehci_irq) acknowledges only the bits it read without clearing
      // USBINTR, so a bit set between its read and its acknowledgement keeps the line up with no new edge. Still
      // asserted after an acknowledgement means new work, so send the interrupt again.
      UpdateInterruptLine(cleared);
      break;
    }
    case kRegUsbIntr:
      usbIntr = value & kStsLatched;
      UpdateInterruptLine();
      break;
    case kRegFrIndex: frameIndex = value & 0x3FFF; break;
    case kRegCtrlDsSegment: ctrlDsSegment = value; break;
    case kRegPeriodicListBase: periodicListBase = value & ~0xFFFu; break;
    case kRegAsyncListAddr: asyncListAddr = value & ~0x1Fu; break;
    case kRegConfigFlag: WriteConfigFlag(value); break;
    default:
      if (offset >= kRegPortSC && offset < kRegPortSC + numPorts * 4) {
        WritePortStatus((offset - kRegPortSC) / 4, value);
        break;
      }
      LOG_WARNING(EHCI, "{} write of {:#x} to unknown register {:#x}", instance, value, offset + kCapLength);
      break;
  }
}

void Xe::PCIDev::EHCI::WriteCommand(u32 value) {
  if (value & kCmdHCReset) {
    LOG_INFO(EHCI, "{} HCRESET", instance);
    Reset();
    return;
  }
  const u32 old = usbCmd;
  usbCmd = (usbCmd & ~kCmdWritable) | (value & kCmdWritable);
  // IAAD may only be set by software. The controller clears it.
  if (!(value & kCmdIAAD)) { usbCmd = (usbCmd & ~kCmdIAAD) | (old & kCmdIAAD); }
  if ((old ^ usbCmd) & kCmdRS) { LOG_INFO(EHCI, "{} {}", instance, IsRunning() ? "run" : "stop"); }
  // With the async schedule not running there is nothing to advance past: answer the doorbell at once.
  if ((usbCmd & kCmdIAAD) && (!IsRunning() || !(usbCmd & kCmdASE))) {
    usbCmd &= ~kCmdIAAD;
    SetStatus(kStsIAA);
  }
  UpdateClock();
}

// CONFIGFLAG 0 -> 1 routes every port to the EHCI, 1 -> 0 to the companion.
void Xe::PCIDev::EHCI::WriteConfigFlag(u32 value) {
  const u32 flag = value & 1;
  if (flag == configFlag) { return; }
  configFlag = flag;
  for (u32 port = 0; port < numPorts; ++port) {
    portSC[port] = flag ? portSC[port] & ~kPortPO : portSC[port] | kPortPO;
    ports->SetCompanionOwned(port, !flag);
  }
}

void Xe::PCIDev::EHCI::WritePortStatus(u32 port, u32 value) {
  u32 &sc = portSC[port];
  sc &= ~(value & kPortChanges);
  sc = (sc & ~kPortStored) | (value & kPortStored);
  if (!(value & kPortPE)) { sc &= ~kPortPE; } // software can only disable

  // Port reset: PR set holds the port in reset (and disabled), clearing it ends the reset. A high-speed device comes
  // out enabled, anything else stays disabled for the driver to hand to the companion.
  USB::Device *device = ports->EhciDevice(port);
  if ((value & kPortPR) && !(sc & kPortPR)) {
    sc |= kPortPR;
    sc &= ~(kPortPE | kPortSuspend);
    if (device) { device->BusReset(); }
  } else if (!(value & kPortPR) && (sc & kPortPR)) {
    sc &= ~kPortPR;
    if (device && (sc & kPortCCS) && device->GetSpeed() == USB::Speed::High) {
      sc |= kPortPE;
      sc &= ~kPortLineMask;
    }
  }

  // Suspend / resume.
  if ((value & kPortSuspend) && (sc & kPortPE)) { sc |= kPortSuspend; }
  if (value & kPortFPR) {
    sc |= kPortFPR;
  } else if (sc & kPortFPR) {
    sc &= ~(kPortFPR | kPortSuspend);
  }

  // Port owner: moving the port disconnects it from this controller and connects it to the other.
  const u32 owner = value & kPortPO;
  if (owner != (sc & kPortPO)) {
    sc = (sc & ~kPortPO) | owner;
    ports->SetCompanionOwned(port, owner != 0);
  }
}

void Xe::PCIDev::EHCI::SetPortChange(u32 port, u32 changeBits) {
  portSC[port] |= changeBits;
  SetStatus(kStsPCD);
}

void Xe::PCIDev::EHCI::OnPortChanged(u32 port) {
  if (port >= numPorts) { return; }
  u32 &sc = portSC[port];
  USB::Device *device = ports->EhciDevice(port);
  if (device && !(sc & kPortCCS)) {
    // Before reset every device looks full/low speed: low speed idles in K, full and high speed in J.
    sc = (sc & ~kPortLineMask) | kPortCCS | (device->GetSpeed() == USB::Speed::Low ? kPortLineK : kPortLineJ);
    LOG_INFO(EHCI, "{} port {}: {} connected", instance, port, device->GetName());
    // The connect is reported on the next frame tick, not inside the register write that routed the port here
    // (CONFIGFLAG in the kernel's restart): the 2853 kernel's EHCI and OHCI share one unlocked device-node pool, and a
    // connect interrupt that soon runs its DPC while the driver's entry point initializes the OHCI takes a node from
    // that pool for the OHCI root hub, giving one node to both.
    pendingConnects |= 1u << port;
    clock.SetActive(true);
  } else if (!device && (sc & kPortCCS)) {
    pendingConnects &= ~(1u << port);
    sc &= ~(kPortCCS | kPortPE | kPortSuspend | kPortPR | kPortFPR | kPortLineMask);
    LOG_INFO(EHCI, "{} port {}: released", instance, port);
    SetPortChange(port, kPortCSC);
  }
  // A disconnect on a companion-owned port gives it back to the EHCI.
  if (configFlag && (sc & kPortPO) && !ports->GetDevice(port)) {
    sc &= ~kPortPO;
    ports->SetCompanionOwned(port, false);
  }
}

// Interrupts

u32 Xe::PCIDev::EHCI::GetStatus() const {
  u32 status = usbSts & kStsLatched;
  if (!IsRunning()) { status |= kStsHalted; }
  if (usbCmd & kCmdPSE) { status |= kStsPSS; }
  if (usbCmd & kCmdASE) { status |= kStsASS; }
  return status;
}

void Xe::PCIDev::EHCI::SetStatus(u32 bits) {
  usbSts |= bits & kStsLatched;
  UpdateInterruptLine();
}

// The ISR drops the line by clearing USBINTR.
void Xe::PCIDev::EHCI::UpdateInterruptLine(bool acknowledged) {
  interruptLine.Set((usbSts & usbIntr & kStsLatched) != 0, acknowledged);
}

// Frames

void Xe::PCIDev::EHCI::UpdateClock() { clock.SetActive(IsRunning() || pendingConnects); }

void Xe::PCIDev::EHCI::OnFrames(u32 frames) {
  std::lock_guard lock(ports->Mutex());
  for (u32 port = 0; pendingConnects; ++port) {
    if (pendingConnects & (1u << port)) {
      pendingConnects &= ~(1u << port);
      SetPortChange(port, kPortCSC);
    }
  }
  for (u32 i = 0; i < frames && IsRunning(); ++i) { RunFrame(); }
  UpdateClock();
}

void Xe::PCIDev::EHCI::RunFrame() {
  if (usbCmd & kCmdPSE) { ProcessPeriodicSchedule(); }
  if (usbCmd & kCmdASE) { ProcessAsyncSchedule(); }
  // The async schedule was walked end to end: whatever the driver unlinked before ringing is no longer cached.
  if (usbCmd & kCmdIAAD) {
    usbCmd &= ~kCmdIAAD;
    SetStatus(kStsIAA);
  }
  // FRINDEX counts microframes. With a 1024-entry list FLR fires when bit 13 flips.
  const u32 old = frameIndex;
  frameIndex = (frameIndex + 8) & 0x3FFF;
  if ((old ^ frameIndex) & 0x2000) { SetStatus(kStsFLR); }
}

Xe::PCIDev::USB::Device *Xe::PCIDev::EHCI::FindDevice(u8 address) const {
  for (u32 port = 0; port < numPorts; ++port) {
    if ((portSC[port] & (kPortPE | kPortSuspend)) != kPortPE) { continue; }
    USB::Device *device = ports->EhciDevice(port);
    if (!device) { continue; }
    if (device->GetAddress() == address) { return device; }
    if (USB::Device *behind = device->Downstream(address)) { return behind; }
  }
  return nullptr;
}

void Xe::PCIDev::EHCI::ProcessAsyncSchedule() {
  const u32 start = asyncListAddr;
  if (!start) { return; }
  u32 qhAddress = start;
  for (u32 count = 0; count < kMaxLinks; ++count) {
    ProcessQueueHead(qhAddress, kMaxAsyncPackets);
    const u32 link = dma.Read32(qhAddress);
    if ((link & kLinkTerminate) || ((link >> 1) & 3) != kTypeQH) { break; }
    qhAddress = link & ~0x1Fu;
    if (qhAddress == start) { break; }
  }
}

void Xe::PCIDev::EHCI::ProcessPeriodicSchedule() {
  if (!periodicListBase) { return; }
  u32 link = dma.Read32(periodicListBase + ((frameIndex >> 3) & 1023) * 4);
  for (u32 count = 0; count < kMaxLinks && !(link & kLinkTerminate); ++count) {
    const u32 address = link & ~0x1Fu;
    switch ((link >> 1) & 3) {
      case kTypeITD: {
        u32 itd[16];
        if (!dma.ReadDwords(address, itd, 16)) { return; }
        ProcessIsochronousTd(address, itd);
        link = itd[0];
        break;
      }
      case kTypeQH: {
        const u32 sMask = dma.Read32(address + 8) & 0xFF;
        // Interrupt endpoint: one transaction per microframe its S-mask selects. Split transactions (C-mask) to a
        // full/low-speed device behind a hub aren't modelled, they run as plain transactions.
        if (sMask) { ProcessQueueHead(address, static_cast<u32>(std::popcount(sMask))); }
        link = dma.Read32(address);
        break;
      }
      default:
        // siTD (split isochronous) and FSTN: not modelled. Follow the link.
        link = dma.Read32(address);
        break;
    }
  }
}

// QH: 0 link, 1 characteristics, 2 capabilities, 3 current qTD, 4-11 overlay (next, alt next, token, 5 buffers).
void Xe::PCIDev::EHCI::ProcessQueueHead(u32 qhAddress, u32 packets) {
  u32 qh[12];
  if (!dma.ReadDwords(qhAddress, qh, 12)) { return; }

  for (u32 count = 0; count < kMaxQtdsPerQh && packets; ++count) {
    if (qh[6] & kTokHalted) { return; }
    if (!(qh[6] & kTokActive)) {
      // Advance: the alternate next qTD after a short packet (bytes left), otherwise the next one.
      const u32 bytesLeft = (qh[6] >> 16) & 0x7FFF;
      const u32 next = (!(qh[5] & kLinkTerminate) && bytesLeft) ? qh[5] : qh[4];
      if (next & kLinkTerminate) { return; }
      u32 qtd[8];
      if (!dma.ReadDwords(next & ~0x1Fu, qtd, 8) || !(qtd[2] & kTokActive)) { return; }
      // With DTC clear the QH keeps the data toggle across qTDs.
      const u32 toggle = (qh[1] & kQhDTC) ? qtd[2] & kTokDT : qh[6] & kTokDT;
      qh[3] = next & ~0x1Fu;
      qh[4] = qtd[0];
      qh[5] = qtd[1];
      qh[6] = (qtd[2] & ~kTokDT) | toggle;
      for (u32 i = 0; i < 5; ++i) { qh[7 + i] = qtd[3 + i]; }
    }

    const QtdResult result = ExecuteOverlay(qh, packets);
    dma.WriteDwords(qhAddress + 12, qh + 3, 9);
    if (result == QtdResult::Nak) { return; }
    // Retire the qTD: its token gets the overlay's final state.
    dma.Write32(qh[3] + 8, qh[6]);
    if (qh[6] & kTokIOC) { SetStatus(kStsUsbInt); }
    if (result == QtdResult::Halted) {
      SetStatus(kStsUsbErrInt);
      return;
    }
  }
}

// Runs the overlay's transactions until the qTD completes, halts, NAKs or packets runs out (then Nak: still active,
// progress kept in the overlay).
Xe::PCIDev::EHCI::QtdResult Xe::PCIDev::EHCI::ExecuteOverlay(u32 *qh, u32 &packets) {
  u32 &token = qh[6];
  const u8 address = qh[1] & 0x7F;
  const u8 endpoint = (qh[1] >> 8) & 0xF;
  const u32 maxPacket = std::clamp<u32>((qh[1] >> 16) & 0x7FF, 1, kMaxPacket);
  const u32 pidCode = (token >> 8) & 3;

  auto halt = [&](u32 errorBits) {
    token = (token & ~(kTokActive | (3u << 10))) | kTokHalted | errorBits;
    return QtdResult::Halted;
  };

  USB::Device *device = FindDevice(address);
  if (!device || pidCode == 3) { return halt(kTokXactErr); }
  const USB::Pid pid = pidCode == 0 ? USB::Pid::Out : pidCode == 1 ? USB::Pid::In : USB::Pid::Setup;

  u32 bytesLeft = (token >> 16) & 0x7FFF;
  u32 page = (token >> 12) & 7;
  u32 offset = qh[7] & 0xFFF;
  auto guestCopy = [&](u8 *data, u32 length, bool toGuest) {
    u32 pg = page, off = offset;
    while (length) {
      if (pg > 4) { return false; }
      const u32 run = std::min(length, 0x1000 - off);
      const u32 guest = (qh[7 + pg] & ~0xFFFu) + off;
      if (!(toGuest ? dma.Write(guest, data, run) : dma.Read(guest, data, run))) { return false; }
      data += run;
      length -= run;
      off = 0;
      ++pg;
    }
    return true;
  };

  // A device that takes whole transfers gets everything the qTD has left in one transaction.
  const bool whole = endpoint != 0 && pid != USB::Pid::Setup && device->WholeTransfers();
  thread_local std::vector<u8> packet(kMaxQtdBuffer);
  bool done = false;
  while (!done && packets) {
    const u32 chunk = pid == USB::Pid::Setup
                        ? std::min<u32>(bytesLeft, 8)
                        : std::min(whole ? bytesLeft : std::min(bytesLeft, maxPacket), kMaxQtdBuffer);
    if (pid != USB::Pid::In && chunk && !guestCopy(packet.data(), chunk, false)) { return halt(1u << 5); }
    const USB::TransactionResult result = device->Transact(pid, endpoint, packet.data(), chunk);
    if (result.handshake == USB::Handshake::Nak) { break; }
    if (result.handshake == USB::Handshake::Stall) { return halt(0); }
    if (result.handshake == USB::Handshake::Timeout) { return halt(kTokXactErr); }
    const u32 moved = pid == USB::Pid::In ? std::min(result.actual, chunk) : chunk;
    if (pid == USB::Pid::In && moved && !guestCopy(packet.data(), moved, true)) { return halt(1u << 5); }
    const u32 count = whole ? PacketsFor(moved, maxPacket, pid == USB::Pid::In && moved < chunk) : 1;
    packets -= std::min(packets, count);
    if (count & 1) { token ^= kTokDT; }
    bytesLeft -= moved;
    offset += moved;
    page += offset >> 12;
    offset &= 0xFFF;
    // Complete when everything moved, or on a short IN packet.
    done = bytesLeft == 0 || (pid == USB::Pid::In && moved < chunk);
  }

  token = (token & ~((0x7FFFu << 16) | (7u << 12))) | (bytesLeft << 16) | (std::min<u32>(page, 7) << 12);
  qh[7] = (qh[7] & ~0xFFFu) | offset;
  if (!done) { return QtdResult::Nak; }
  token &= ~kTokActive;
  return QtdResult::Complete;
}

// iTD: 0 link, 1-8 transactions (Active, errors, length 16-27, IOC, PG 12-14, offset 0-11), 9-15 buffer pages (page 0
// also holds the device address and endpoint, page 1 direction and max packet, page 2 Mult). The frame list slot is
// visited once per frame, so all eight microframe transactions run then.
void Xe::PCIDev::EHCI::ProcessIsochronousTd(u32 itdAddress, u32 *itd) {
  const u8 address = itd[9] & 0x7F;
  const u8 endpoint = (itd[9] >> 8) & 0xF;
  const bool in = itd[10] & (1u << 11);
  const u32 maxPacket = itd[10] & 0x7FF;
  const u32 mult = std::max<u32>(itd[11] & 3, 1);
  USB::Device *device = FindDevice(address);

  bool touched = false;
  std::array<u8, kMaxPacket> packet{};
  for (u32 i = 0; i < 8; ++i) {
    u32 &transaction = itd[1 + i];
    if (!(transaction & kItdActive)) { continue; }
    touched = true;
    const u32 length = std::min<u32>({(transaction >> 16) & 0xFFF, maxPacket * mult, kMaxPacket});
    const u32 page = (transaction >> 12) & 7;
    const u32 offset = transaction & 0xFFF;
    auto guestCopy = [&](u8 *data, u32 count, bool toGuest) {
      u32 pg = page, off = offset;
      while (count && pg < 7) {
        const u32 run = std::min(count, 0x1000 - off);
        const u32 guest = (itd[9 + pg] & ~0xFFFu) + off;
        if (!(toGuest ? dma.Write(guest, data, run) : dma.Read(guest, data, run))) { return; }
        data += run;
        count -= run;
        off = 0;
        ++pg;
      }
    };

    u32 status = 0;
    u32 actual = length;
    if (!device) {
      status = kItdXactErr;
      actual = 0;
    } else {
      if (!in && length) { guestCopy(packet.data(), length, false); }
      const USB::TransactionResult result =
        device->Transact(in ? USB::Pid::In : USB::Pid::Out, endpoint, packet.data(), length);
      if (result.handshake == USB::Handshake::Ack) {
        if (in) {
          actual = std::min(result.actual, length);
          if (actual) { guestCopy(packet.data(), actual, true); }
        }
      } else if (result.handshake == USB::Handshake::Nak) {
        actual = in ? 0 : length;
      } else {
        status = kItdXactErr;
        actual = 0;
      }
    }
    transaction = (transaction & ~(0xFu << 28) & ~(0xFFFu << 16)) | status | (actual << 16);
    if (transaction & kItdIOC) { SetStatus(kStsUsbInt); }
    if (status) { SetStatus(kStsUsbErrInt); }
  }
  if (touched) { dma.WriteDwords(itdAddress + 4, itd + 1, 8); }
}
