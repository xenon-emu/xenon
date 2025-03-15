// Copyright 2025 Xenon Emulator Project

#include "AudioController.h"

Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR::AUDIOCTRLR(const char *deviceName, u64 size) :
  PCIDevice(deviceName, size) {
  // Set PCI Properties.
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580C1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02880006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x04010001;
  // Set our PCI Dev Sizes.
  pciDevSizes[0] = 0x40; // BAR0
}

void Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR::Read(u64 readAddress, u8 *data, u8 byteCount)
{}

void Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR::ConfigRead(u64 readAddress, u8 *data, u8 byteCount) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], byteCount);
}

void Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR::Write(u64 writeAddress, u8 *data, u8 byteCount)
{}

void Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR::ConfigWrite(u64 writeAddress, u8 *data, u8 byteCount) {
  // Check if we're being scanned.
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (*reinterpret_cast<u64*>(data) == 0xFFFFFFFF) { // PCI BAR Size discovery.
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          *reinterpret_cast<u64*>(data) &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        *reinterpret_cast<u64*>(data) &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address.
      *reinterpret_cast<u64*>(data) = 0; // Register not implemented.
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], data, byteCount);
}
