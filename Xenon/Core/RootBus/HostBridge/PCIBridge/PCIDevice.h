// Copyright 2025 Xenon Emulator Project

#pragma once

#include <cstring>

#include "Core/RootBus/HostBridge/PCIe.h"

struct PCIDeviceInfo {
  const char *deviceName;
  u64 size;
};

class PCIDevice {
public:
  PCIDevice(const char *deviceName, u64 size) {
    deviceInfo.deviceName = deviceName;
    deviceInfo.size = size;
  }
  virtual void Read(u64 readAddress, u64 *data, u8 byteCount) {}
  virtual void Write(u64 writeAddress, u64 data, u8 byteCount) {}

  virtual void ConfigRead(u64 readAddress, u64 *data, u8 byteCount) {}
  virtual void ConfigWrite(u64 writeAddress, u64 data, u8 byteCount) {}

  const char *GetDeviceName() { return deviceInfo.deviceName; }

  // Checks wether a given address is mapped in the device's BAR's
  bool isAddressMappedInBAR(u32 address) {
    u32 bar0 = pciConfigSpace.configSpaceHeader.BAR0;
    u32 bar1 = pciConfigSpace.configSpaceHeader.BAR1;
    u32 bar2 = pciConfigSpace.configSpaceHeader.BAR2;
    u32 bar3 = pciConfigSpace.configSpaceHeader.BAR3;
    u32 bar4 = pciConfigSpace.configSpaceHeader.BAR4;
    u32 bar5 = pciConfigSpace.configSpaceHeader.BAR5;

    if ((address >= bar0 && address <= bar0 + deviceInfo.size) ||
        (address >= bar1 && address <= bar1 + deviceInfo.size) ||
        (address >= bar2 && address <= bar2 + deviceInfo.size) ||
        (address >= bar3 && address <= bar3 + deviceInfo.size) ||
        (address >= bar4 && address <= bar4 + deviceInfo.size) ||
        (address >= bar5 && address <= bar5 + deviceInfo.size)) {
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
  // PCI Device Size, using when determining PCI device size of each BAR in Linux.
  u32 pciDevSizes[6] = {};
private:
  PCIDeviceInfo deviceInfo = {nullptr};
};
