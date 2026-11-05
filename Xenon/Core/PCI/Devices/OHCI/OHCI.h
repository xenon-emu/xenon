/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Devices/USB/USBFrameClock.h"
#include "Core/PCI/Devices/USB/USBPort.h"
#include "Core/PCI/DmaEngine.h"
#include "Core/PCI/InterruptLine.h"
#include "Core/PCI/PCIDevice.h"

#include <memory>

class PCIBridge;
class RAM;

// OHCI 1.0a USB 1.1 host controller, the companion of an EHCI.
//
// The kernel's USB driver restarts it with HCR, HCFS=Reset, FmInterval (FIT toggled, FSMPS 0x2778, FI 0x2EDF),
// PeriodicStart 0x2A2F, HCCA, HcControl = CBSR 1:1 | PLE | IE | CLE | BLE | Operational, RhDescriptorA = NPS | NOCP,
// RhStatus = LPSC | CRWE and InterruptEnable = MIE | WDH | FNO | RHSC, with the control and bulk heads pointing at
// dummy EDs.
//
// Interrupts need MIE in HcInterruptEnable. A pure FNO is acked in the ISR (frame high bits from HccaFrameNumber),
// anything else disables MIE and queues the DPC. The DPC takes HccaDoneHead (WDH), acks HcInterruptStatus, handles SF
// (paused endpoints, then disables SF) and RHSC, and re-enables MIE. The line is level: it follows (status & enable)
// with MIE set. Endpoint removal sets ED.sKip, enables SF and frees the ED on the next SF, so an ED is never touched
// after the SOF that follows its sKip.
//
// Root hub: PRS (timer 100 ms), PRSC reported with PES and LSDA, CSC with CCS, all changes cleared with 0x1F0000. Done
// queue entries are general TDs (32 bytes) or isochronous TDs, and the kernel tells them apart by bit 16 of dword 0
// (the ED's F bit it copies there).

namespace Xe::PCIDev {

  class OHCI : public PCIDevice, public USB::PortListener {
  public:
    // Size of the register window (BAR0).
    static constexpr u32 DevSize = 0x1000;

    OHCI(const std::string &deviceName, u64 size, s32 instance, PCIBridge *parentPCIBridge, RAM *ram,
         std::shared_ptr<USB::PortGroup> ports);
    ~OHCI();
    void Read(u64 offset, u8 *data, u64 size) override;
    void Write(u64 offset, const u8 *data, u64 size) override;
    void PlatformReset() override;

    // Port group lock held.
    void OnPortChanged(u32 port) override;

    std::shared_ptr<USB::PortGroup> GetPorts() const { return ports; }
    // Stops frame processing.
    void StopFrames() { clock.Stop(); }

  private:
    static constexpr u32 kMaxPorts = 15;

    // Result of running one TD.
    enum class TdResult {
      Retired,
      Pending
    };

    void HardReset();
    void SoftReset();

    u32 ReadRegister(u32 offset);
    void WriteRegister(u32 offset, u32 value);
    void WriteControl(u32 value);
    void WriteRhStatus(u32 value);
    void WritePortStatus(u32 port, u32 value);
    void SetPortChange(u32 port, u32 changeBits);

    void SetInterruptStatus(u32 bits);
    void UpdateInterruptLine(bool acknowledged = false);
    void UpdateClock();
    bool IsOperational() const { return ((hcControl >> 6) & 3) == 2; }

    // Frame clock callback.
    void OnFrames(u32 frames);
    void RunFrame();
    void AdvancePortResets();
    // Walks an ED list and says whether any ED had TDs queued.
    bool ProcessEdList(u32 head, bool periodic);
    bool ProcessEd(u32 edAddress, bool periodic);
    TdResult ProcessGeneralTd(u32 edAddress, u32 *ed);
    TdResult ProcessIsoTd(u32 edAddress, u32 *ed);
    // Retires a TD to the done queue: tdNext (the TD's NextTD dword) is replaced by the old done head.
    void QueueDone(u32 tdAddress, u32 &tdNext, u32 delayInterrupt);
    USB::Device *FindDevice(u8 address) const;

    s32 instance;
    Xe::PCI::InterruptLine interruptLine;
    std::shared_ptr<USB::PortGroup> ports;
    Xe::PCI::DmaEngine dma;
    USB::FrameClock clock;
    u32 numPorts = 0;

    // Registers
    u32 hcControl = 0;                  // 0x04
    u32 hcCommandStatus = 0;            // 0x08
    u32 hcInterruptStatus = 0;          // 0x0C
    u32 hcInterruptEnable = 0;          // 0x10 / 0x14
    u32 hcHCCA = 0;                     // 0x18
    u32 hcPeriodCurrentED = 0;          // 0x1C
    u32 hcControlHeadED = 0;            // 0x20
    u32 hcControlCurrentED = 0;         // 0x24
    u32 hcBulkHeadED = 0;               // 0x28
    u32 hcBulkCurrentED = 0;            // 0x2C
    u32 hcDoneHead = 0;                 // 0x30
    u32 hcFmInterval = 0;               // 0x34
    u32 hcFmNumber = 0;                 // 0x3C
    u32 hcPeriodicStart = 0;            // 0x40
    u32 hcLSThreshold = 0;              // 0x44
    u32 hcRhDescriptorA = 0;            // 0x48
    u32 hcRhDescriptorB = 0;            // 0x4C
    u32 hcRhStatus = 0;                 // 0x50
    u32 hcRhPortStatus[kMaxPorts] = {}; // 0x54 + 4n

    // Frames left until a port reset completes (0 = none in progress).
    u8 portResetFrames[kMaxPorts] = {};
    // Done queue interrupt counter (7 = no interrupt pending).
    u8 doneCounter = 7;
  };

} // namespace Xe::PCIDev
