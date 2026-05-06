/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <cstring>
#include <string>

#include "Base/Hash.h"

#include "Core/PCI/PCIe.h"

class PCIDevice {
public:
  PCIDevice(const char *deviceName, u64 deviceSize)
    : name(deviceName), size(deviceSize)
  {
    hash = Base::JoaatStringHash(deviceName, false);
  }

  virtual void Read(u64 address, u8 *data, u64 size)
  {}
  virtual void Write(u64 address, const u8 *data, u64 size)
  {}
  virtual void MemSet(u64 address, s32 data, u64 size)
  {}

  virtual void ConfigRead(u64 address, u8 *data, u64 size)
  {}
  virtual void ConfigWrite(u64 address, const u8 *data, u64 size)
  {}

  std::string GetDeviceName() {
    return name;
  }

  constexpr u64 GetHash() {
    return hash;
  }

  // Checks wether a given address is mapped in the device's BAR's
  bool IsAddressMappedInBAR(u32 address) {
    u32 bar0 = pciConfigSpace.BAR0;
    u32 bar1 = pciConfigSpace.BAR1;
    u32 bar2 = pciConfigSpace.BAR2;
    u32 bar3 = pciConfigSpace.BAR3;
    u32 bar4 = pciConfigSpace.BAR4;
    u32 bar5 = pciConfigSpace.BAR5;

    if ((address >= bar0 && address < (bar0 + size)) ||
        (address >= bar1 && address < (bar1 + size)) ||
        (address >= bar2 && address < (bar2 + size)) ||
        (address >= bar3 && address < (bar3 + size)) ||
        (address >= bar4 && address < (bar4 + size)) ||
        (address >= bar5 && address < (bar5 + size))
      )
    {
      return true;
    }

    return false;
  }

  // Checks if the device is allowed to respond to memory R/W
  bool IsDeviceResponseAllowed() {
    return PCI_CONFIG_HDR_REG1_COMMAND_REG{ pciConfigSpace.reg1.command }.memorySpace == 1;
  }

  // Configuration Space.
  union GENRAL_PCI_DEVICE_CONFIG_SPACE pciConfigSpace = {};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux
  u32 pciDevSizes[6] = {};
private:
  const char *name = "";
  u64 hash = 0, size = 0;
};
