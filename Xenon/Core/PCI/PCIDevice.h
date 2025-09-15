/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <cstring>
#include <string>

#include "Core/PCI/PCIe.h"

struct PCIDeviceInfo {
  std::string deviceName{};
  u64 size = 0;
};

class PCIDevice {
public:
  PCIDevice(std::string deviceName, u64 size) {
    deviceInfo.deviceName = deviceName;
    deviceInfo.size = size;
  }
  virtual void Read(u64 readAddress, u8 *data, u64 size) {}
  virtual void Write(u64 writeAddress, const u8 *data, u64 size) {}
  virtual void MemSet(u64 writeAddress, s32 data, u64 size) {}

  virtual void ConfigRead(u64 readAddress, u8 *data, u64 size) {}
  virtual void ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {}

  std::string GetDeviceName() { return deviceInfo.deviceName; }

  // Checks wether a given address is mapped in the device's BAR's
  bool IsAddressMappedInBAR(u32 address) {
    u32 bar0 = pciConfigSpace.configSpaceHeader.BAR0;
    u32 bar1 = pciConfigSpace.configSpaceHeader.BAR1;
    u32 bar2 = pciConfigSpace.configSpaceHeader.BAR2;
    u32 bar3 = pciConfigSpace.configSpaceHeader.BAR3;
    u32 bar4 = pciConfigSpace.configSpaceHeader.BAR4;
    u32 bar5 = pciConfigSpace.configSpaceHeader.BAR5;

    if ((address >= bar0 && address < (bar0 + deviceInfo.size)) ||
        (address >= bar1 && address < (bar1 + deviceInfo.size)) ||
        (address >= bar2 && address < (bar2 + deviceInfo.size)) ||
        (address >= bar3 && address < (bar3 + deviceInfo.size)) ||
        (address >= bar4 && address < (bar4 + deviceInfo.size)) ||
        (address >= bar5 && address < (bar5 + deviceInfo.size))) {
      return true;
    }

    return false;
  }

  // Checks if the device is allowed to respond to memory R/W
  bool isDeviceResponseAllowed() {
    PCI_CONFIG_HDR_REG1_COMMAND_REG commandReg = {};
    commandReg.hexData = pciConfigSpace.configSpaceHeader.reg1.command;
    if (commandReg.memorySpace == true) {
      return true;
    }
    return false;
  }

  // Configuration Space.
  GENRAL_PCI_DEVICE_CONFIG_SPACE pciConfigSpace = {};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux
  u32 pciDevSizes[6] = {};
private:
  PCIDeviceInfo deviceInfo = { "" };
};
