/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/PCI/PCIBar.h"
#include "Core/PCI/PCIe.h"

#include <cstring>
#include <string>
#include <utility>

struct PCIDeviceInfo {
  std::string deviceName{};
  u64 size = 0;
};

// A function on the southbridge's PCI bus (bus 1). The PCI-PCI bridge decodes its BARs and calls
// BarRead/BarWrite/BarMemSet with the offset into the BAR. Config accesses arrive one dword lane at a time (never
// crossing a dword), with the register offset in the low byte of the address. After a write the bridge re-latches
// the BARs to the sizes in pciDevSizes, so a device needs no BAR sizing of its own. Config reads and writes default
// to the stored bytes with the type 0 header's read-only registers masked off, and a device only overrides them
// for registers it gives a meaning of its own.
class PCIDevice {
public:
  PCIDevice(std::string deviceName, u64 size) {
    deviceInfo.deviceName = std::move(deviceName);
    deviceInfo.size = size;
    // Everything is writable except the type 0 header's read-only registers: IDs, status, class and revision, header
    // type and BIST, cardbus CIS pointer, subsystem IDs, expansion ROM (not implemented), capabilities pointer and
    // reserved bytes, interrupt pin, min grant and max latency.
    memset(pciConfigWriteMask, 0xFF, sizeof(pciConfigWriteMask));
    const auto readOnly = [this](u32 offset, u32 count) { memset(&pciConfigWriteMask[offset], 0, count); };
    readOnly(0x00, 4);
    readOnly(0x06, 2);
    readOnly(0x08, 4);
    readOnly(0x0E, 2);
    readOnly(0x28, 12);
    readOnly(0x34, 8);
    readOnly(0x3D, 3);
  }
  virtual ~PCIDevice() = default;

  // Memory access decoded by BAR bar, offset bytes into it. A device with one BAR overrides Read/Write. One whose
  // BARs map different things (the SFCX register file and flash window, the SATA command and bus master blocks)
  // overrides BarRead/BarWrite and tells them apart by bar.
  virtual void Read(u64 offset, u8 *data, u64 size) {}
  virtual void Write(u64 offset, const u8 *data, u64 size) {}
  virtual void BarRead(u32 bar, u64 offset, u8 *data, u64 size) { Read(offset, data, size); }
  virtual void BarWrite(u32 bar, u64 offset, const u8 *data, u64 size) { Write(offset, data, size); }
  // A fill is a run of ordinary writes.
  virtual void BarMemSet(u32 bar, u64 offset, s32 data, u64 size) {
    Xe::PCI::FillInChunks(data, size,
                          [&](u64 done, const u8 *bytes, u64 count) { BarWrite(bar, offset + done, bytes, count); });
  }

  virtual void ConfigRead(u64 readAddress, u8 *data, u64 size) {
    for (u64 i = 0; i != size; ++i) { data[i] = pciConfigSpace.data[(readAddress + i) & 0xFF]; }
  }
  // BARs are stored as written, the bridge latches them to their sizes afterwards.
  virtual void ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
    for (u64 i = 0; i != size; ++i) {
      const u32 at = static_cast<u32>(writeAddress + i) & 0xFF;
      const u8 mask = pciConfigWriteMask[at];
      pciConfigSpace.data[at] = static_cast<u8>((pciConfigSpace.data[at] & ~mask) | (data[i] & mask));
    }
  }

  // Platform reset (the console's reset line, so reboot and power off). Puts the registers back to their power-on
  // values and stops all work: running and queued DMA, frame clocks, worker jobs, pending and held interrupts.
  // Returns once nothing of the device touches guest memory any more, and must not depend on the guest being alive.
  // The bridge restores the config space afterwards (RestorePowerOnConfig).
  virtual void PlatformReset() {}

  const std::string &GetDeviceName() const { return deviceInfo.deviceName; }

  // The config space as the constructor left it. The bridge keeps it when the device is first attached.
  void SavePowerOnConfig() {
    if (powerOnConfigSaved) { return; }
    memcpy(powerOnConfig, pciConfigSpace.data, sizeof(powerOnConfig));
    powerOnConfigSaved = true;
  }
  void RestorePowerOnConfig() {
    if (powerOnConfigSaved) { memcpy(pciConfigSpace.data, powerOnConfig, sizeof(powerOnConfig)); }
  }

  // Size of BAR bar (0 = not implemented). A device that declares no BAR sizes decodes its size at BAR0.
  u32 BarSize(u32 bar) const {
    if (bar >= Xe::PCI::ConfigBarCount) { return 0; }
    bool sized = false;
    for (const u32 barSize : pciDevSizes) { sized |= barSize != 0; }
    if (!sized) { return bar == 0 ? static_cast<u32>(deviceInfo.size) : 0; }
    return pciDevSizes[bar];
  }

  u32 GetBar(u32 bar) const {
    u32 value = 0;
    memcpy(&value, &pciConfigSpace.data[Xe::PCI::ConfigBar0 + 4 * bar], sizeof(value));
    return value;
  }
  void SetBar(u32 bar, u32 value) {
    memcpy(&pciConfigSpace.data[Xe::PCI::ConfigBar0 + 4 * bar], &value, sizeof(value));
  }
  u16 GetCommand() const {
    u16 value = 0;
    memcpy(&value, &pciConfigSpace.data[Xe::PCI::ConfigCommand], sizeof(value));
    return value;
  }
  void SetCommand(u16 value) { memcpy(&pciConfigSpace.data[Xe::PCI::ConfigCommand], &value, sizeof(value)); }

  // Configuration Space.
  PCI_TYPE0_CONFIG_SPACE pciConfigSpace = {};
  // Size of each BAR (0 = not implemented): what it decodes and what a size probe reads back.
  u32 pciDevSizes[6] = {};

private:
  PCIDeviceInfo deviceInfo = {""};
  u8 pciConfigWriteMask[Xe::PCI::ConfigSpaceSize] = {};
  u8 powerOnConfig[0x100] = {};
  bool powerOnConfigSaved = false;
};
