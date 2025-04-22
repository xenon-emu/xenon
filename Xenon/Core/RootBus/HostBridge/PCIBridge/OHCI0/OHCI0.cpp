// Copyright 2025 Xenon Emulator Project

#include "OHCI0.h"

Xe::PCIDev::OHCI0::OHCI0::OHCI0(const std::string &deviceName, u64 size) : OHCI(deviceName, size, 0, 4) {
  // TODO(bitsh1ft3r): Implement PCIe Capabilities.
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x58041414;
}
