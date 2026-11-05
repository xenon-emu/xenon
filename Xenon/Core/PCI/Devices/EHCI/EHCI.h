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

// EHCI 1.0 USB 2.0 host controller with an OHCI companion.
//
// The kernel's USB driver finds the operational registers at BAR + CAPLENGTH (read as a byte). Initialization clears
// USBINTR, builds a dummy async head QH (H set, overlay Halted) and a 1024-entry periodic frame list, clears RS and
// polls HCHalted. A restart does HCRESET (polled until clear), PERIODICLISTBASE, ASYNCLISTADDR, USBCMD = RS | ITC 4
// (FLS 1024), CONFIGFLAG = 1, USBINTR = 0x3F. ASE is set when the first async endpoint opens, PSE with the first
// periodic one.
//
// The interrupt is (USBSTS & USBINTR & 0x3F), HSE is fatal. The ISR acks USBSTS, extends the frame counter on FLR from
// FRINDEX bit 13, clears USBINTR and queues the DPC, which restores USBINTR = 0x3F. Completion is
// read from the qTD tokens (Active/Halted/error bits, bytes left) and QH.CurrentqTD, and endpoint removal waits for IAA
// after ringing IAAD.
//
// Root ports: reset = PR with PE clear for 50 ms, then PR cleared and polled until it reads 0. A port that shows a
// low-speed K state before reset, or isn't enabled after it (full speed), is given to the OHCI via PortOwner.

namespace Xe::PCIDev {

  class EHCI : public PCIDevice, public USB::PortListener {
  public:
    // Size of the register window (BAR0).
    static constexpr u32 DevSize = 0x1000;

    EHCI(const std::string &deviceName, u64 size, s32 instance, PCIBridge *parentPCIBridge, RAM *ram,
         std::shared_ptr<USB::PortGroup> ports);
    ~EHCI();
    void Read(u64 offset, u8 *data, u64 size) override;
    void Write(u64 offset, const u8 *data, u64 size) override;
    void PlatformReset() override;

    // Port group lock held.
    void OnPortChanged(u32 port) override;

    std::shared_ptr<USB::PortGroup> GetPorts() const { return ports; }
    // Stops frame processing for good (shutdown: before RAM and the PCI bridge go away).
    void StopFrames() { clock.Stop(); }

  private:
    static constexpr u32 kMaxPorts = 15;

    enum class QtdResult {
      Complete,
      Nak,
      Halted
    };

    void Reset();

    u32 ReadRegister(u32 offset);
    void WriteRegister(u32 offset, u32 value);
    void WriteCommand(u32 value);
    void WriteConfigFlag(u32 value);
    void WritePortStatus(u32 port, u32 value);
    void SetPortChange(u32 port, u32 changeBits);

    u32 GetStatus() const;
    void SetStatus(u32 bits);
    void UpdateInterruptLine(bool acknowledged = false);
    void UpdateClock();
    bool IsRunning() const { return usbCmd & 1; }

    // Frame clock callback (takes the port group lock).
    void OnFrames(u32 frames);
    void RunFrame();
    void ProcessAsyncSchedule();
    void ProcessPeriodicSchedule();
    // Runs a queue head's qTDs. 'packets' limits the transactions (interrupt endpoints: one per S-mask microframe).
    void ProcessQueueHead(u32 qhAddress, u32 packets);
    QtdResult ExecuteOverlay(u32 *qh, u32 &packets);
    void ProcessIsochronousTd(u32 itdAddress, u32 *itd);
    USB::Device *FindDevice(u8 address) const;

    s32 instance;
    Xe::PCI::InterruptLine interruptLine;
    std::shared_ptr<USB::PortGroup> ports;
    Xe::PCI::DmaEngine dma;
    USB::FrameClock clock;
    u32 numPorts = 0;

    // Capability registers
    u32 hcsParams = 0;
    u32 hccParams = 0;
    // Operational registers
    u32 usbCmd = 0;             // 0x00
    u32 usbSts = 0;             // 0x04 (latched bits 0-5, the rest is derived)
    u32 usbIntr = 0;            // 0x08
    u32 frameIndex = 0;         // 0x0C
    u32 ctrlDsSegment = 0;      // 0x10
    u32 periodicListBase = 0;   // 0x14
    u32 asyncListAddr = 0;      // 0x18
    u32 configFlag = 0;         // 0x40
    u32 portSC[kMaxPorts] = {}; // 0x44 + 4n
    // Ports whose connect is reported (ConnectStatusChange, PCD) on the next frame tick (OnPortChanged).
    u32 pendingConnects = 0;
  };

} // namespace Xe::PCIDev
