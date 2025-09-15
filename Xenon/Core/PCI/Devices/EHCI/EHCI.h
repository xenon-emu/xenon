/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/PCIDevice.h"

#define EHCI_DEV_SIZE 0x1000

namespace Xe {
namespace PCIDev {

class EHCI : public PCIDevice {
public:
  EHCI(const std::string &deviceName, u64 size, s32 instance, u32 ports);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8 *data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8 *data, u64 size) override;

private:
  // Internal data
  s32 instance;
  u32 ports;
  // Capability Registers
  u32 capLength; // 0x00 - CAPLENGTH (low byte) + HCIVERSION (upper word)
  u32 hcsParams; // 0x04 - HCSPARAMS
  u32 hccParams; // 0x08 - HCCPARAMS
  u32 hcspPortRoute; // 0x0C - HCSP-PORTROUTE
  // Operational Registers
  u32 usbCmd; // 0x20 - USBCMD
  u32 usbSts; // 0x24 - USBSTS
  u32 usbIntr; // 0x28 - USBINTR
  u32 frameIndex; // 0x2C - FRINDEX
  u32 ctrlDsSegment; // 0x30 - CTRLDSSEGMENT
  u32 periodicListBase; // 0x34 - PERIODICLISTBASE
  u32 asyncListAddr; // 0x38 - ASYNCLISTADDR
  u32 configFlag; // 0x40 - CONFIGFLAG
  u32 portSC[9]; // 0x44-... - PORTSC
};

} // namespace PCIDev
} // namespace Xe
