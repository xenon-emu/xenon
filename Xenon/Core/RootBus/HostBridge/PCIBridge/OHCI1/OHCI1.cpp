// Copyright 2025 Xenon Emulator Project

#include "OHCI1.h"

Xe::PCIDev::OHCI1::OHCI1::OHCI1(const char *deviceName, u64 size) : PCIDevice(deviceName, size) {
  // TODO(bitsh1ft3r): Implement PCIe Capabilities.
  // Set PCI Properties.
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x58061414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02800156;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x0C03100F;
  pciConfigSpace.configSpaceHeader.reg3.hexData = 0x00800000;
  // Set our PCI Dev Sizes.
  pciDevSizes[0] = 0x1000; // BAR0
}

void Xe::PCIDev::OHCI1::OHCI1::Read(u64 readAddress, u8 *data, u64 size)
{}

void Xe::PCIDev::OHCI1::OHCI1::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::OHCI1::OHCI1::Write(u64 writeAddress, const u8 *data, u64 size)
{}

void Xe::PCIDev::OHCI1::OHCI1::MemSet(u64 writeAddress, s32 data, u64 size)
{}

void Xe::PCIDev::OHCI1::OHCI1::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  // Check if we're being scanned.
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery.
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          tmp &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address.
      tmp = 0; // Register not implemented.
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
}
