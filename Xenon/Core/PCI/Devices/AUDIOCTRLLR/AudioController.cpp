/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "AudioController.h"

Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR(const std::string &deviceName, u64 size) :
  PCIDevice(deviceName, size) {
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x580C1414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02880006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x04010001;
  pciConfigSpace.configSpaceHeader.regB.hexData = 0x75011039;
  pciConfigSpace.configSpaceHeader.regF.hexData = 0x0B340100;
  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x40; // BAR0
}

void Xe::PCIDev::AUDIOCTRLR::Read(u64 readAddress, u8 *data, u64 size) {
  LOG_DEBUG(AudioController, "Device Read at address {:#x}, size {:#d}", readAddress, size);
}

void Xe::PCIDev::AUDIOCTRLR::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::AUDIOCTRLR::Write(u64 writeAddress, const u8 *data, u64 size) {
  u64 dataIn = 0;
  memcpy(&dataIn, data, size);
  LOG_DEBUG(AudioController, "Device Write at address {:#x}, data {:#x}, size {:#d}", writeAddress, dataIn, size);
}

void Xe::PCIDev::AUDIOCTRLR::MemSet(u64 writeAddress, s32 data, u64 size)
{}

void Xe::PCIDev::AUDIOCTRLR::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  // Check if we're being scanned
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
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
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
      tmp = 0; // Register not implemented
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
}
