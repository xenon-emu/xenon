/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OHCI.h"

#include "Base/Logging/Log.h"
#include "Core/PCI/Bridge/PCIBridge.h"

#include <algorithm>
#include <array>

// Every register access and completed transfer is logged with this on (a lot of output):
// #define OHCI_DEBUG

#ifdef OHCI_DEBUG
  #define OHCI_DEBUGP(x, ...) LOG_DEBUG(OHCI, "[OHCI] " x, ##__VA_ARGS__)
#else
  #define OHCI_DEBUGP(x, ...)
#endif

// Registers
constexpr u32 kRegRevision = 0x00;
constexpr u32 kRegControl = 0x04;
constexpr u32 kRegCommandStatus = 0x08;
constexpr u32 kRegInterruptStatus = 0x0C;
constexpr u32 kRegInterruptEnable = 0x10;
constexpr u32 kRegInterruptDisable = 0x14;
constexpr u32 kRegHCCA = 0x18;
constexpr u32 kRegPeriodCurrentED = 0x1C;
constexpr u32 kRegControlHeadED = 0x20;
constexpr u32 kRegControlCurrentED = 0x24;
constexpr u32 kRegBulkHeadED = 0x28;
constexpr u32 kRegBulkCurrentED = 0x2C;
constexpr u32 kRegDoneHead = 0x30;
constexpr u32 kRegFmInterval = 0x34;
constexpr u32 kRegFmRemaining = 0x38;
constexpr u32 kRegFmNumber = 0x3C;
constexpr u32 kRegPeriodicStart = 0x40;
constexpr u32 kRegLSThreshold = 0x44;
constexpr u32 kRegRhDescriptorA = 0x48;
constexpr u32 kRegRhDescriptorB = 0x4C;
constexpr u32 kRegRhStatus = 0x50;
constexpr u32 kRegRhPortStatus = 0x54;

// HcControl
constexpr u32 kCtlPLE = 1u << 2;
constexpr u32 kCtlIE = 1u << 3;
constexpr u32 kCtlCLE = 1u << 4;
constexpr u32 kCtlBLE = 1u << 5;
constexpr u32 kCtlHCFS = 3u << 6;
constexpr u32 kCtlUsbSuspend = 3u << 6;

// HcCommandStatus
constexpr u32 kCmdHCR = 1u << 0;
constexpr u32 kCmdCLF = 1u << 1;
constexpr u32 kCmdBLF = 1u << 2;
constexpr u32 kCmdOCR = 1u << 3;

// HcInterruptStatus / Enable
constexpr u32 kIntWDH = 1u << 1;
constexpr u32 kIntSF = 1u << 2;
constexpr u32 kIntFNO = 1u << 5;
constexpr u32 kIntRHSC = 1u << 6;
constexpr u32 kIntSources = 0x4000007Fu;
constexpr u32 kIntMIE = 1u << 31;

// HcRhDescriptorA
constexpr u32 kRhaNPS = 1u << 9;
constexpr u32 kRhaNOCP = 1u << 12;
constexpr u32 kRhaWritable = 0xFF001B00u; // POTPGT, NOCP, OCPM, NPS, PSM

// HcRhStatus
constexpr u32 kRhsOCIC = 1u << 17;
constexpr u32 kRhsDRWE = 1u << 15;
constexpr u32 kRhsCRWE = 1u << 31;

// HcRhPortStatus
constexpr u32 kPortCCS = 1u << 0;
constexpr u32 kPortPES = 1u << 1;
constexpr u32 kPortPSS = 1u << 2;
constexpr u32 kPortPOCI = 1u << 3;
constexpr u32 kPortPRS = 1u << 4;
constexpr u32 kPortPPS = 1u << 8;
constexpr u32 kPortLSDA = 1u << 9;
constexpr u32 kPortCSC = 1u << 16;
constexpr u32 kPortPSSC = 1u << 18;
constexpr u32 kPortPRSC = 1u << 20;
constexpr u32 kPortChanges = 0x1F0000u;

// ED dword 0
constexpr u32 kEdSkip = 1u << 14;
constexpr u32 kEdIso = 1u << 15;
// ED dword 2 (HeadP)
constexpr u32 kEdHalted = 1u << 0;
constexpr u32 kEdCarry = 1u << 1;

// Condition codes
constexpr u32 kCcNoError = 0;
constexpr u32 kCcStall = 4;
constexpr u32 kCcDeviceNotResponding = 5;
constexpr u32 kCcDataOverrun = 8;
constexpr u32 kCcDataUnderrun = 9;

// HCCA
constexpr u32 kHccaFrameNumber = 0x80;
constexpr u32 kHccaDoneHead = 0x84;

// Port reset length (the kernel waits up to 100 ms for PRSC).
constexpr u8 kPortResetFrames = 10;
// General TDs one control/bulk ED may complete per frame.
constexpr u32 kMaxTdsPerEd = 16;
constexpr u32 kMaxEdsPerList = 256;
constexpr u32 kMaxPacket = 4096;
// A general TD's buffer spans at most two pages.
constexpr u32 kMaxTdBuffer = 0x2000;

// Packets a whole-transfer transaction stood for: every full max-packet, plus the short or
// zero-length one that ended it.
static u32 PacketsFor(u32 bytes, u32 maxPacket, bool endedShort) {
  const u32 full = bytes / maxPacket;
  return (bytes % maxPacket || endedShort || bytes == 0) ? full + 1 : full;
}

// Page-split buffer of a general TD or an isochronous packet: bytes [0, firstLength) start at first, the rest at the
// start of secondPage.
struct SplitBuffer {
  u32 first = 0;
  u32 secondPage = 0;
  u32 firstLength = 0;
  u32 length = 0;

  // start..end inclusive, at most one page crossing. 'start' == 0 means empty.
  static SplitBuffer Make(u32 start, u32 end) {
    SplitBuffer buffer;
    if (start == 0) { return buffer; }
    buffer.first = start;
    buffer.secondPage = end & ~0xFFFu;
    if (((start ^ end) & ~0xFFFu) == 0) {
      buffer.length = end >= start ? end - start + 1 : 0;
      buffer.firstLength = buffer.length;
    } else {
      buffer.firstLength = 0x1000 - (start & 0xFFF);
      buffer.length = buffer.firstLength + (end & 0xFFF) + 1;
    }
    return buffer;
  }
  u32 AddressAt(u32 pos) const { return pos < firstLength ? first + pos : secondPage + (pos - firstLength); }
  // Copies [pos, pos + count) between guest memory and data.
  bool Copy(const Xe::PCI::DmaEngine &dma, u32 pos, u8 *data, u32 count, bool toGuest) const {
    while (count) {
      const u32 run = pos < firstLength ? std::min(count, firstLength - pos) : count;
      const u32 address = AddressAt(pos);
      if (!(toGuest ? dma.Write(address, data, run) : dma.Read(address, data, run))) { return false; }
      pos += run;
      data += run;
      count -= run;
    }
    return true;
  }
};

Xe::PCIDev::OHCI::OHCI(const std::string &deviceName, u64 size, s32 instance, PCIBridge *parentPCIBridge, RAM *ram,
                       std::shared_ptr<USB::PortGroup> portGroup)
    : PCIDevice(deviceName, size)
    , instance(instance)
    , interruptLine(parentPCIBridge, instance == 0 ? PRIO_OHCI_0 : PRIO_OHCI_1)
    , ports(std::move(portGroup))
    , dma(ram) {
  pciConfigSpace.configSpaceHeader.id.hexData = instance == 0 ? 0x58041414 : 0x58061414;
  pciConfigSpace.configSpaceHeader.commandStatus.hexData = 0x02800156;
  pciConfigSpace.configSpaceHeader.classRev.hexData = 0x0C03100F;
  pciConfigSpace.configSpaceHeader.headerInfo.hexData = 0x00800000;
  pciConfigSpace.configSpaceHeader.subsystem.hexData = instance == 0 ? 0x58041414 : 0x58061414;
  pciConfigSpace.configSpaceHeader.interrupt.hexData = 0x50000100;

  // Set our PCI Dev Sizes
  pciDevSizes[0] = DevSize; // BAR0

  numPorts = std::min(ports->NumPorts(), kMaxPorts);
  {
    std::lock_guard lock(ports->Mutex());
    HardReset();
    ports->SetOhci(this);
    for (u32 port = 0; port < numPorts; ++port) { OnPortChanged(port); }
  }
  clock.Start(deviceName, [this](u32 frames) { OnFrames(frames); });
}

Xe::PCIDev::OHCI::~OHCI() {
  clock.Stop();
  std::lock_guard lock(ports->Mutex());
  ports->SetOhci(nullptr);
}

// Reset

// The console's reset line: the controller goes to UsbReset with every register at its power-on value, the frame clock
// stops (so nothing walks the schedules in guest memory any more), the devices see a bus reset and the root hub
// re-evaluates what is plugged in.
void Xe::PCIDev::OHCI::PlatformReset() {
  std::lock_guard lock(ports->Mutex());
  hcControl = 0;
  HardReset();
  hcControl = 0; // IR is a firmware handoff bit, gone with the reset too
  ports->ResetDevices();
  for (u32 port = 0; port < numPorts; ++port) { OnPortChanged(port); }
  UpdateClock();
}

void Xe::PCIDev::OHCI::HardReset() {
  SoftReset();
  hcControl = 0; // UsbReset
  hcRhDescriptorA = numPorts | kRhaNPS | kRhaNOCP | (1u << 24);
  hcRhDescriptorB = 0;
  hcRhStatus = 0;
  for (u32 port = 0; port < kMaxPorts; ++port) {
    hcRhPortStatus[port] = port < numPorts ? kPortPPS : 0;
    portResetFrames[port] = 0;
  }
}

// HcCommandStatus.HCR. The root hub keeps its state. HcInterruptEnable resets to 0, and the kernel's driver then writes
// MIE | RHSC | FNO | WDH (0x80000062).
void Xe::PCIDev::OHCI::SoftReset() {
  hcControl = (hcControl & (1u << 8)) | kCtlUsbSuspend; // keeps IR
  hcCommandStatus = 0;
  hcInterruptStatus = 0;
  hcInterruptEnable = 0;
  hcHCCA = 0;
  hcPeriodCurrentED = 0;
  hcControlHeadED = 0;
  hcControlCurrentED = 0;
  hcBulkHeadED = 0;
  hcBulkCurrentED = 0;
  hcDoneHead = 0;
  hcFmInterval = 0x2EDF;
  hcFmNumber = 0;
  hcPeriodicStart = 0;
  hcLSThreshold = 0x0628;
  doneCounter = 7;
  UpdateInterruptLine();
  UpdateClock();
}

// MMIO

void Xe::PCIDev::OHCI::Read(u64 barOffset, u8 *data, u64 size) {
  const u32 offset = static_cast<u32>(barOffset);
  u32 value = 0;
  {
    std::lock_guard lock(ports->Mutex());
    value = ReadRegister(offset & ~3u);
  }
  OHCI_DEBUGP("{} Read({:#x}) == {:#x}", instance, offset, value);
  value = byteswap_le<u32>(value) >> ((offset & 3) * 8);
  memcpy(data, &value, std::min<u64>(size, 4));
}

void Xe::PCIDev::OHCI::Write(u64 barOffset, const u8 *data, u64 size) {
  const u32 offset = static_cast<u32>(barOffset);
  if (size != 4 || (offset & 3)) {
    LOG_WARNING(OHCI, "{} unsupported {}-byte write at {:#x}", instance, size, offset);
    return;
  }
  u32 value = 0;
  memcpy(&value, data, 4);
  value = byteswap_le<u32>(value);
  OHCI_DEBUGP("{} Write({:#x}, {:#x})", instance, offset, value);
  std::lock_guard lock(ports->Mutex());
  WriteRegister(offset, value);
}

u32 Xe::PCIDev::OHCI::ReadRegister(u32 offset) {
  switch (offset) {
    case kRegRevision: return 0x10;
    case kRegControl: return hcControl;
    case kRegCommandStatus: return hcCommandStatus;
    case kRegInterruptStatus: return hcInterruptStatus;
    case kRegInterruptEnable:
    case kRegInterruptDisable: return hcInterruptEnable;
    case kRegHCCA: return hcHCCA;
    case kRegPeriodCurrentED: return hcPeriodCurrentED;
    case kRegControlHeadED: return hcControlHeadED;
    case kRegControlCurrentED: return hcControlCurrentED;
    case kRegBulkHeadED: return hcBulkHeadED;
    case kRegBulkCurrentED: return hcBulkCurrentED;
    case kRegDoneHead: return hcDoneHead;
    case kRegFmInterval: return hcFmInterval;
    // FRT follows FIT. The frame is always half done.
    case kRegFmRemaining: return (hcFmInterval & 0x80000000u) | ((hcFmInterval & 0x3FFF) / 2);
    case kRegFmNumber: return hcFmNumber;
    case kRegPeriodicStart: return hcPeriodicStart;
    case kRegLSThreshold: return hcLSThreshold;
    case kRegRhDescriptorA: return hcRhDescriptorA;
    case kRegRhDescriptorB: return hcRhDescriptorB;
    case kRegRhStatus: return hcRhStatus;
    default:
      if (offset >= kRegRhPortStatus && offset < kRegRhPortStatus + numPorts * 4) {
        return hcRhPortStatus[(offset - kRegRhPortStatus) / 4];
      }
      LOG_WARNING(OHCI, "{} read of unknown register {:#x}", instance, offset);
      return 0;
  }
}

void Xe::PCIDev::OHCI::WriteRegister(u32 offset, u32 value) {
  switch (offset) {
    case kRegControl: WriteControl(value); break;
    case kRegCommandStatus:
      if (value & kCmdHCR) {
        SoftReset();
        break;
      }
      hcCommandStatus |= value & (kCmdCLF | kCmdBLF | kCmdOCR);
      break;
    case kRegInterruptStatus: {
      const bool cleared = (hcInterruptStatus & value) != 0;
      hcInterruptStatus &= ~value;
      // The line is forwarded as edges. The kernel's ISR drops MIE, so whatever arrives meanwhile raises a new edge
      // when its DPC sets MIE again. Linux (ohci_irq) only acknowledges the bits it read: a WDH or SF that came in
      // while it ran keeps the line up, no edge follows and the controller goes silent (khubd hangs in usb_kill_urb
      // waiting for SF). Still asserted after an acknowledgement means new work, so send the interrupt again.
      UpdateInterruptLine(cleared);
      break;
    }
    case kRegInterruptEnable:
      hcInterruptEnable |= value & (kIntSources | kIntMIE);
      UpdateInterruptLine();
      break;
    case kRegInterruptDisable:
      hcInterruptEnable &= ~value;
      UpdateInterruptLine();
      break;
    case kRegHCCA: hcHCCA = value & ~0xFFu; break;
    case kRegControlHeadED: hcControlHeadED = value & ~0xFu; break;
    case kRegControlCurrentED: hcControlCurrentED = value & ~0xFu; break;
    case kRegBulkHeadED: hcBulkHeadED = value & ~0xFu; break;
    case kRegBulkCurrentED: hcBulkCurrentED = value & ~0xFu; break;
    // Read-only per spec. The kernel's driver clears it after draining it.
    case kRegDoneHead: hcDoneHead = value & ~0xFu; break;
    case kRegFmInterval: hcFmInterval = value & 0xFFFF3FFFu; break;
    case kRegPeriodicStart: hcPeriodicStart = value & 0x3FFF; break;
    case kRegLSThreshold: hcLSThreshold = value & 0xFFF; break;
    case kRegRhDescriptorA: hcRhDescriptorA = (hcRhDescriptorA & ~kRhaWritable) | (value & kRhaWritable); break;
    case kRegRhDescriptorB: hcRhDescriptorB = value; break;
    case kRegRhStatus: WriteRhStatus(value); break;
    default:
      if (offset >= kRegRhPortStatus && offset < kRegRhPortStatus + numPorts * 4) {
        WritePortStatus((offset - kRegRhPortStatus) / 4, value);
        break;
      }
      LOG_WARNING(OHCI, "{} write of {:#x} to read-only/unknown register {:#x}", instance, value, offset);
      break;
  }
}

void Xe::PCIDev::OHCI::WriteControl(u32 value) {
  const u32 oldState = hcControl & kCtlHCFS;
  hcControl = value & 0x7FF;
  const u32 newState = hcControl & kCtlHCFS;
  if (oldState != newState) {
    LOG_INFO(OHCI, "{} functional state {} -> {}", instance, oldState >> 6, newState >> 6);
    if (newState == (2u << 6)) {
      // SOF is generated from the first frame after entering UsbOperational.
      doneCounter = 7;
    }
  }
  UpdateClock();
}

void Xe::PCIDev::OHCI::WriteRhStatus(u32 value) {
  // LPS/LPSC (clear/set global power) have no effect: no power switching.
  if (value & kRhsOCIC) { hcRhStatus &= ~kRhsOCIC; }
  if (value & kRhsDRWE) { hcRhStatus |= kRhsDRWE; }
  if (value & kRhsCRWE) { hcRhStatus &= ~kRhsDRWE; }
}

void Xe::PCIDev::OHCI::WritePortStatus(u32 port, u32 value) {
  u32 &status = hcRhPortStatus[port];
  status &= ~(value & kPortChanges);

  const bool connected = status & kPortCCS;
  // "Set" writes on a disconnected port only raise ConnectStatusChange, telling the driver it's empty.
  if (value & (kPortPES | kPortPSS | kPortPRS)) {
    if (!connected) {
      SetPortChange(port, kPortCSC);
      return;
    }
  }
  if (value & kPortCCS) { status &= ~(kPortPES | kPortPSS); } // ClearPortEnable
  if (value & kPortPES) { status |= kPortPES; }               // SetPortEnable
  if (value & kPortPSS) { status |= kPortPSS; }               // SetPortSuspend
  if ((value & kPortPOCI) && (status & kPortPSS)) {           // ClearSuspendStatus: resume
    status &= ~kPortPSS;
    SetPortChange(port, kPortPSSC);
  }
  if ((value & kPortPRS) && !(status & kPortPRS)) { // SetPortReset
    status |= kPortPRS;
    status &= ~kPortPSS;
    portResetFrames[port] = kPortResetFrames;
    if (USB::Device *device = ports->OhciDevice(port)) { device->BusReset(); }
    UpdateClock();
  }
}

void Xe::PCIDev::OHCI::SetPortChange(u32 port, u32 changeBits) {
  hcRhPortStatus[port] |= changeBits;
  SetInterruptStatus(kIntRHSC);
}

void Xe::PCIDev::OHCI::OnPortChanged(u32 port) {
  if (port >= numPorts) { return; }
  USB::Device *device = ports->OhciDevice(port);
  u32 &status = hcRhPortStatus[port];
  if (device && !(status & kPortCCS)) {
    status |= kPortCCS;
    status = device->GetSpeed() == USB::Speed::Low ? status | kPortLSDA : status & ~kPortLSDA;
    LOG_INFO(OHCI, "{} port {}: {} connected", instance, port, ports->GetDevice(port)->GetName());
    SetPortChange(port, kPortCSC);
  } else if (!device && (status & kPortCCS)) {
    status &= ~(kPortCCS | kPortPES | kPortPSS | kPortPRS | kPortLSDA);
    portResetFrames[port] = 0;
    LOG_INFO(OHCI, "{} port {}: disconnected", instance, port);
    SetPortChange(port, kPortCSC);
  }
}

// Interrupts

void Xe::PCIDev::OHCI::SetInterruptStatus(u32 bits) {
  hcInterruptStatus |= bits;
  UpdateInterruptLine();
}

// The ISR drops the line by clearing MIE.
void Xe::PCIDev::OHCI::UpdateInterruptLine(bool acknowledged) {
  interruptLine.Set((hcInterruptEnable & kIntMIE) && (hcInterruptStatus & hcInterruptEnable & kIntSources),
                    acknowledged);
}

// Frames

void Xe::PCIDev::OHCI::UpdateClock() {
  bool resetting = false;
  for (u32 port = 0; port < numPorts; ++port) { resetting |= portResetFrames[port] != 0; }
  clock.SetActive(IsOperational() || resetting);
}

void Xe::PCIDev::OHCI::OnFrames(u32 frames) {
  std::lock_guard lock(ports->Mutex());
  for (u32 i = 0; i < frames; ++i) {
    AdvancePortResets();
    if (IsOperational()) { RunFrame(); }
  }
  UpdateClock();
}

void Xe::PCIDev::OHCI::AdvancePortResets() {
  for (u32 port = 0; port < numPorts; ++port) {
    if (!portResetFrames[port] || --portResetFrames[port]) { continue; }
    u32 &status = hcRhPortStatus[port];
    status &= ~kPortPRS;
    if (status & kPortCCS) { status |= kPortPES; }
    SetPortChange(port, kPortPRSC);
  }
}

void Xe::PCIDev::OHCI::RunFrame() {
  // Done queue: written back at a frame boundary once its delay ran out and the driver consumed the last one.
  if (hcDoneHead && doneCounter == 0 && !(hcInterruptStatus & kIntWDH)) {
    const bool otherPending = (hcInterruptStatus & hcInterruptEnable & kIntSources & ~kIntWDH) != 0;
    dma.Write32(hcHCCA + kHccaDoneHead, hcDoneHead | (otherPending ? 1 : 0));
    hcDoneHead = 0;
    doneCounter = 7;
    SetInterruptStatus(kIntWDH);
  } else if (doneCounter != 7 && doneCounter != 0) {
    --doneCounter;
  }

  // SOF: HcFmNumber, HccaFrameNumber (with HccaPad1 = 0), SF, FNO when bit 15 flips.
  hcFmNumber = (hcFmNumber + 1) & 0xFFFF;
  if (hcHCCA) { dma.Write32(hcHCCA + kHccaFrameNumber, hcFmNumber); }
  u32 bits = kIntSF;
  if ((hcFmNumber & 0x7FFF) == 0) { bits |= kIntFNO; }
  SetInterruptStatus(bits);

  if (!hcHCCA) { return; }

  if (hcControl & kCtlPLE) {
    ProcessEdList(dma.Read32(hcHCCA + 4 * (hcFmNumber & 31)) & ~0xFu, true);
    hcPeriodCurrentED = 0;
  }
  // ControlListFilled / BulkListFilled gate the lists. The controller sets them again when it found a TD.
  if ((hcControl & kCtlCLE) && (hcCommandStatus & kCmdCLF)) {
    hcCommandStatus &= ~kCmdCLF;
    if (ProcessEdList(hcControlHeadED, false)) { hcCommandStatus |= kCmdCLF; }
    hcControlCurrentED = 0;
  }
  if ((hcControl & kCtlBLE) && (hcCommandStatus & kCmdBLF)) {
    hcCommandStatus &= ~kCmdBLF;
    if (ProcessEdList(hcBulkHeadED, false)) { hcCommandStatus |= kCmdBLF; }
    hcBulkCurrentED = 0;
  }
}

bool Xe::PCIDev::OHCI::ProcessEdList(u32 head, bool periodic) {
  bool found = false;
  u32 edAddress = head;
  for (u32 count = 0; edAddress && count < kMaxEdsPerList; ++count) {
    u32 next = 0;
    if (!dma.ReadDwords(edAddress + 12, &next, 1)) {
      LOG_ERROR(OHCI, "{} ED {:#x} outside RAM", instance, edAddress);
      SetInterruptStatus(1u << 4); // UnrecoverableError
      break;
    }
    found |= ProcessEd(edAddress, periodic);
    edAddress = next & ~0xFu;
  }
  return found;
}

bool Xe::PCIDev::OHCI::ProcessEd(u32 edAddress, bool periodic) {
  u32 ed[4];
  if (!dma.ReadDwords(edAddress, ed, 4) || (ed[0] & kEdSkip)) { return false; }
  const bool iso = ed[0] & kEdIso;
  if (iso && (!periodic || !(hcControl & kCtlIE))) { return false; }

  bool found = false;
  for (u32 count = 0; count < kMaxTdsPerEd; ++count) {
    if ((ed[2] & kEdHalted) || (ed[2] & ~0xFu) == (ed[1] & ~0xFu)) { break; }
    found = true;
    const TdResult result = iso ? ProcessIsoTd(edAddress, ed) : ProcessGeneralTd(edAddress, ed);
    // Interrupt endpoints get one TD per visit (their polling interval is the tree position).
    if (result == TdResult::Pending || (periodic && !iso)) { break; }
  }
  return found;
}

Xe::PCIDev::USB::Device *Xe::PCIDev::OHCI::FindDevice(u8 address) const {
  for (u32 port = 0; port < numPorts; ++port) {
    if ((hcRhPortStatus[port] & (kPortPES | kPortPSS)) != kPortPES) { continue; }
    USB::Device *device = ports->OhciDevice(port);
    if (device && device->GetAddress() == address) { return device; }
  }
  return nullptr;
}

void Xe::PCIDev::OHCI::QueueDone(u32 tdAddress, u32 &tdNext, u32 delayInterrupt) {
  tdNext = hcDoneHead;
  hcDoneHead = tdAddress;
  if (delayInterrupt < doneCounter) { doneCounter = static_cast<u8>(delayInterrupt); }
}

// General TD: dword 0 = R (18), DP (19-20), DI (21-23), T (24-25), EC (26-27), CC (28-31), CBP, NextTD, BE.
Xe::PCIDev::OHCI::TdResult Xe::PCIDev::OHCI::ProcessGeneralTd(u32 edAddress, u32 *ed) {
  const u32 tdAddress = ed[2] & ~0xFu;
  u32 td[4];
  if (!dma.ReadDwords(tdAddress, td, 4)) {
    LOG_ERROR(OHCI, "{} TD {:#x} outside RAM", instance, tdAddress);
    SetInterruptStatus(1u << 4);
    ed[2] |= kEdHalted;
    dma.Write32(edAddress + 8, ed[2]);
    return TdResult::Retired;
  }

  const u8 functionAddress = ed[0] & 0x7F;
  const u8 endpoint = (ed[0] >> 7) & 0xF;
  const u32 edDirection = (ed[0] >> 11) & 3;
  const u32 direction = (edDirection == 1 || edDirection == 2) ? edDirection : (td[0] >> 19) & 3;
  const u32 maxPacket = std::clamp<u32>((ed[0] >> 16) & 0x7FF, 1, kMaxPacket);
  const bool rounding = td[0] & (1u << 18);
  u32 toggle = (td[0] & (1u << 25)) ? (td[0] >> 24) & 1 : (ed[2] & kEdCarry) ? 1 : 0;
  const SplitBuffer buffer = SplitBuffer::Make(td[1], td[3]);

  USB::Pid pid = USB::Pid::Setup;
  if (direction == 1) {
    pid = USB::Pid::Out;
  } else if (direction == 2) {
    pid = USB::Pid::In;
  } else if (direction == 3) {
    LOG_WARNING(OHCI, "{} TD {:#x} with reserved direction", instance, tdAddress);
  }

  u32 cc = kCcNoError;
  u32 pos = 0;
  bool shortPacket = false;
  USB::Device *device = FindDevice(functionAddress);
  if (!device || direction == 3) {
    cc = kCcDeviceNotResponding;
  } else {
    // A device that takes whole transfers gets everything the TD has left in one transaction.
    const bool whole = endpoint != 0 && pid != USB::Pid::Setup && device->WholeTransfers();
    thread_local std::vector<u8> packet(kMaxTdBuffer);
    do {
      const u32 remaining = buffer.length - pos;
      const u32 chunk = pid == USB::Pid::Setup
                          ? std::min<u32>(buffer.length, 8)
                          : std::min(whole ? remaining : std::min(remaining, maxPacket), kMaxTdBuffer);
      if (pid != USB::Pid::In && chunk && !buffer.Copy(dma, pos, packet.data(), chunk, false)) {
        cc = kCcDeviceNotResponding;
        break;
      }
      const USB::TransactionResult result = device->Transact(pid, endpoint, packet.data(), chunk);
      if (result.handshake == USB::Handshake::Nak) {
        // Stays queued. Progress so far is kept in CBP and the TD's toggle.
        if (pos) {
          td[1] = buffer.AddressAt(pos);
          td[0] = (td[0] & ~(3u << 24)) | ((2u | toggle) << 24);
          dma.WriteDwords(tdAddress, td, 2);
        }
        return TdResult::Pending;
      }
      if (result.handshake == USB::Handshake::Stall) {
        cc = kCcStall;
        break;
      }
      if (result.handshake == USB::Handshake::Timeout) {
        cc = kCcDeviceNotResponding;
        break;
      }
      const u32 moved = std::min(result.actual, chunk);
      if (pid == USB::Pid::In && moved) { buffer.Copy(dma, pos, packet.data(), moved, true); }
      pos += pid == USB::Pid::In ? moved : chunk;
      const bool endedShort = pid == USB::Pid::In && moved < chunk;
      toggle ^= whole ? PacketsFor(pid == USB::Pid::In ? moved : chunk, maxPacket, endedShort) & 1 : 1;
      if (pid == USB::Pid::In && moved < chunk) {
        shortPacket = true;
        break;
      }
    } while (pos < buffer.length);
  }
  if (cc == kCcNoError && shortPacket && !rounding) { cc = kCcDataUnderrun; }

  // Retire: CC, EC = 0, T = toggle from the TD, CBP = next byte or 0 when complete, HeadP = NextTD with the carry.
  const u32 nextTd = td[2] & ~0xFu;
  td[0] = (td[0] & 0x00FFFFFFu) | (cc << 28) | ((2u | toggle) << 24);
  td[1] = pos >= buffer.length ? 0 : buffer.AddressAt(pos);
  QueueDone(tdAddress, td[2], cc == kCcNoError ? (td[0] >> 21) & 7 : 0);
  dma.WriteDwords(tdAddress, td, 3);

  ed[2] = nextTd | (toggle ? kEdCarry : 0) | (cc != kCcNoError ? kEdHalted : 0);
  dma.Write32(edAddress + 8, ed[2]);
  if (cc != kCcNoError) {
    OHCI_DEBUGP("{} TD {:#x} (addr {} ep {}) completed with CC {}", instance, tdAddress, functionAddress, endpoint, cc);
  }
  return TdResult::Retired;
}

// Isochronous TD: dword 0 = SF (0-15), DI (21-23), FC (24-26), CC (28-31), BP0, NextTD, BE, then 8 16-bit offsets,
// rewritten as packet status words (CC 12-15, size 0-10) as the packets run.
Xe::PCIDev::OHCI::TdResult Xe::PCIDev::OHCI::ProcessIsoTd(u32 edAddress, u32 *ed) {
  const u32 tdAddress = ed[2] & ~0x1Fu;
  u32 td[8];
  if (!dma.ReadDwords(tdAddress, td, 8)) {
    SetInterruptStatus(1u << 4);
    ed[2] |= kEdHalted;
    dma.Write32(edAddress + 8, ed[2]);
    return TdResult::Retired;
  }
  const s16 relative = static_cast<s16>(hcFmNumber - (td[0] & 0xFFFF));
  const u32 frameCount = (td[0] >> 24) & 7;
  if (relative < 0) { return TdResult::Pending; }

  auto psw = [&](u32 index) -> u32 { return (td[4 + index / 2] >> ((index & 1) * 16)) & 0xFFFF; };
  auto setPsw = [&](u32 index, u32 value) {
    const u32 shift = (index & 1) * 16;
    td[4 + index / 2] = (td[4 + index / 2] & ~(0xFFFFu << shift)) | ((value & 0xFFFF) << shift);
  };

  u32 tdCc = kCcNoError;
  if (static_cast<u32>(relative) > frameCount) {
    // Too late for all of its packets.
    tdCc = kCcDataOverrun;
  } else {
    const u32 index = static_cast<u32>(relative);
    const u32 bp0 = td[1] & ~0xFFFu;
    const u32 bePage = td[3] & ~0xFFFu;
    auto offsetAddress = [&](u32 offset) { return ((offset & 0x1000) ? bePage : bp0) + (offset & 0xFFF); };
    const u32 start = offsetAddress(psw(index));
    const u32 end = index < frameCount ? offsetAddress(psw(index + 1)) - 1 : td[3];
    const SplitBuffer buffer = end + 1 == start ? SplitBuffer{} : SplitBuffer::Make(start, end);
    const u32 length = std::min(buffer.length, kMaxPacket);

    const u32 edDirection = (ed[0] >> 11) & 3;
    const u8 endpoint = (ed[0] >> 7) & 0xF;
    USB::Device *device = FindDevice(ed[0] & 0x7F);
    u32 cc = kCcNoError;
    u32 size = 0;
    if (!device || (edDirection != 1 && edDirection != 2)) {
      cc = kCcDeviceNotResponding;
    } else {
      std::array<u8, kMaxPacket> packet{};
      const bool in = edDirection == 2;
      if (!in && length) { buffer.Copy(dma, 0, packet.data(), length, false); }
      const USB::TransactionResult result =
        device->Transact(in ? USB::Pid::In : USB::Pid::Out, endpoint, packet.data(), length);
      if (result.handshake == USB::Handshake::Stall) {
        cc = kCcStall;
      } else if (result.handshake == USB::Handshake::Timeout) {
        cc = kCcDeviceNotResponding;
      } else if (in) {
        // A NAK on an isochronous IN is an empty packet.
        size = result.handshake == USB::Handshake::Ack ? std::min(result.actual, length) : 0;
        if (size) { buffer.Copy(dma, 0, packet.data(), size, true); }
        if (size < length) { cc = kCcDataUnderrun; }
      }
    }
    setPsw(index, (cc << 12) | (size & 0x7FF));
    if (index < frameCount) {
      dma.WriteDwords(tdAddress + 16, td + 4, 4);
      return TdResult::Pending;
    }
  }

  const u32 nextTd = td[2] & ~0x1Fu;
  td[0] = (td[0] & 0x0FFFFFFFu) | (tdCc << 28);
  QueueDone(tdAddress, td[2], (td[0] >> 21) & 7);
  dma.WriteDwords(tdAddress, td, 8);
  ed[2] = nextTd | (ed[2] & kEdCarry);
  dma.Write32(edAddress + 8, ed[2]);
  return TdResult::Retired;
}
