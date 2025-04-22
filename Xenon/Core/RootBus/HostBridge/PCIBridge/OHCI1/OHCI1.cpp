// Copyright 2025 Xenon Emulator Project

#include "OHCI1.h"

Xe::PCIDev::OHCI1::OHCI1::OHCI1(const std::string &deviceName, u64 size) : OHCI(deviceName, size, 1, 5) {
  // TODO(bitsh1ft3r): Implement PCIe Capabilities.
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x58061414;
}
