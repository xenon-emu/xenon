// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "OHCI.h"

#include "Base/Assert.h"
#include "Base/Logging/Log.h"

Xe::PCIDev::OHCI::OHCI(const std::string &deviceName, u64 size, s32 instance, u32 ports) :
  PCIDevice(deviceName, size),
  instance(instance), ports(ports)
{
  pciConfigSpace.configSpaceHeader.reg0.hexData = instance == 0 ? 0x58041414 : 0x58061414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02800156;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x0C03100F;
  pciConfigSpace.configSpaceHeader.reg3.hexData = 0x00800000;

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x1000; // BAR0

  HcControl = 0;
  HcInterruptStatus = 0;
  HcRhDescriptorA = (1 << 24) | ports;
}

void Xe::PCIDev::OHCI::Read(u64 readAddress, u8 *data, u64 size) {
  u64 offset = readAddress & 0xFFF;
  ASSERT(size == 4);

  u32 ret = 0;

  switch (offset) {
  case 0x0:
    ret = HcRevision;
    break;
  case 0x04:
    ret = HcControl;
    break;
  case 0x8:
    ret = HcCommandStatus;
    break;
  case 0xc:
    ret = HcInterruptStatus;
    break;
  case 0x10:
  case 0x14:
    ret = HcInterruptEnable;
    break;
  case 0x18:
    ret = HcHCCA;
    break;
  case 0x1C:
    ret = HcPeriodCurrentED;
    break;
  case 0x20:
    ret = HcControlHeadED;
    break;
  case 0x28:
    ret = HcBulkHeadED;
    break;
  case 0x34:
    ret = HcFmInterval;
    break;
  case 0x48:
    ret = HcRhDescriptorA;
    break;
  case 0x4C:
    ret = HcRhDescriptorB;
    break;
  default:
    if (offset >= 0x54 && offset < 0x54 + sizeof(HcRhPortStatus)) {
      u32 portIndex = (offset - 0x54) / 4;
      ret = HcRhPortStatus[portIndex];
    }
    break;
  }
  ret = byteswap_le<u32>(ret);
  LOG_DEBUG(OHCI, "{} Read(0x{:X}, data) == 0x{:X}", instance, offset, ret);
  memcpy(data, &ret, size);
}

void Xe::PCIDev::OHCI::Write(u64 writeAddress, const u8 *data, u64 size) {
  u64 offset = writeAddress & 0xFFF;
  ASSERT(size == 4);

  u32 value = 0;
  memcpy(&value, data, size);
  value = byteswap_le<u32>(value);

  switch (offset) {
  case 0x0:
    HcRevision = value;
    LOG_DEBUG(OHCI, "{} HcRevision = 0x{:X}, 0x{:X}", instance, value, writeAddress);
    break;
  case 0x4:
    HcControl = value;
    LOG_DEBUG(OHCI, "{} HcControl = 0x{:X}", instance, value);
    break;
  case 0x8:
    HcCommandStatus = value;
    LOG_DEBUG(OHCI, "{} HcCommandStatus = 0x{:X}", instance, value);
    break;
  case 0x10:
    HcInterruptEnable |= value;
    break;
  case 0x14:
    HcInterruptEnable &= ~value;
    break;
  case 0x18:
    HcHCCA = value & ~0xFF;
    LOG_DEBUG(OHCI, "{} HcHCCA = 0x{:X}", instance, value);
    break;
  case 0x20:
    HcControlHeadED = value;
    LOG_DEBUG(OHCI, "{} HcControlHeadED = 0x{:X}", instance, value);
    break;
  case 0x28:
    HcBulkHeadED = value;
    LOG_DEBUG(OHCI, "{} HcBulkHeadED = 0x{:X}", instance, value);
    break;
  case 0x34:
    HcFmInterval = value;
    LOG_DEBUG(OHCI, "{} HcFmInterval = 0x{:X}", instance, value);
    break;
  case 0x40:
    HcPeriodicStart = value;
    LOG_DEBUG(OHCI, "{} HcPeriodicStart = 0x{:X}", instance, value);
    break;
  case 0x50:
    HcRhStatus = value;
    LOG_DEBUG(OHCI, "{} HcRhStatus = 0x{:X}", instance, value);
    break;
  default:
    if (offset >= 0x54 && offset < 0x54 + sizeof(HcRhPortStatus)) {
      u32 portIndex = (offset - 0x54) / 4;
      LOG_DEBUG(OHCI, "{} HcRhPortStatus[{}] = 0x{:X}", instance, portIndex, value);
      HcRhPortStatus[portIndex] = value;
    } else {
      LOG_WARNING(OHCI, "{} Write(0x{:X}, 0x{:X}, {})", instance, offset, value, size);
    }
    break;
  }

  if (HcCommandStatus & 1) {
    // HCR
    HcCommandStatus &= ~1;
    // TODO, reset everything else
  }
}

void Xe::PCIDev::OHCI::MemSet(u64 writeAddress, s32 data, u64 size)
{}


void Xe::PCIDev::OHCI::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::OHCI::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
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
