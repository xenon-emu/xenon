// Copyright 2025 Xenon Emulator Project

#include "OHCI.h"

#include "Base/Logging/Log.h"

Xe::PCIDev::OHCI::OHCI(const std::string &deviceName, u64 size, int instance, unsigned int ports) : PCIDevice(deviceName, size), instance(instance), ports(ports) {
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02800156;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x0C03100F;
  pciConfigSpace.configSpaceHeader.reg3.hexData = 0x00800000;

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x1000; // BAR0

  HcControl = 0;
  HcInterruptStatus = 0;
  HcRhDescriptorA = (1<<24) | ports;
}

void Xe::PCIDev::OHCI::Read(u64 readAddress, u8 *data, u64 size) {
  u64 offset = readAddress & 0xfff;
  assert(size == 4);

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
  case 0x1c:
    ret = HcPeriodCurrentED;
    break;
  case 0x34:
    ret = HcFmInterval;
    break;
  case 0x48:
    ret = HcRhDescriptorA;
    break;
  case 0x4c:
    ret = HcRhDescriptorB;
    break;
  default:
    LOG_WARNING(OHCI, "{} Read({:#x}, data)", instance, offset);
  }
  LOG_DEBUG(OHCI, "{} Read({:#x}, data) == {:#x}", instance, offset, ret);
  u32 *data32 = (u32*)data;
  *data32 = byteswap_le<u32>(ret);
  //*data32 = ret;
}

void Xe::PCIDev::OHCI::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::OHCI::Write(u64 writeAddress, const u8 *data, u64 size) {
  u64 offset = writeAddress & 0xfff;
  assert(size == 4);

  u32 value = byteswap_le<u32>( *(u32*)data );
  //u32 value = *(u32*)data;
  switch (offset) {
  case 0x0:
    HcRevision = value;
    LOG_DEBUG(OHCI, "{} HcRevision = {:#x}, {:#x}", instance, value, writeAddress);
    break;
  case 0x4:
    HcControl = value;
    LOG_DEBUG(OHCI, "{} HcControl = {:#x}", instance, value);
    break;
  case 0x8:
    HcCommandStatus = value;
    LOG_DEBUG(OHCI, "{} HcCommandStatus = {:#x}", instance, value);
    break;
  case 0x10:
    HcInterruptEnable |= value;
    break;
  case 0x14:
    HcInterruptEnable &= ~value;
    break;
  case 0x18:
    HcHCCA = value & ~0xff;
    LOG_DEBUG(OHCI, "{} HcHCCA = {:#x}", instance, value);
    break;
  case 0x20:
    HcControlHeadED = value;
    LOG_DEBUG(OHCI, "{} HcControlHeadED = {:#x}", instance, value);
    break;
  case 0x28:
    HcBulkHeadED = value;
    LOG_DEBUG(OHCI, "{} HcBulkHeadED = {:#x}", instance, value);
    break;
  case 0x34:
    HcFmInterval = value;
    LOG_DEBUG(OHCI, "{} HcFmInterval = {:#x}", instance, value);
    break;
  case 0x40:
    HcPeriodicStart = value;
    LOG_DEBUG(OHCI, "{} HcPeriodicStart = {:#x}", instance, value);
    break;
  case 0x50:
    HcRhStatus = value;
    LOG_DEBUG(OHCI, "{} HcRhStatus = {:#x}", instance, value);
    break;
  default:
    LOG_WARNING(OHCI, "{} Write({:#x}, {:#x}, {})", instance, offset, value, size);
  }

  if (HcCommandStatus & 1) { // HCR
    HcCommandStatus &= ~1;
    // TODO, reset everything else
  }
}

void Xe::PCIDev::OHCI::MemSet(u64 writeAddress, s32 data, u64 size)
{}

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
