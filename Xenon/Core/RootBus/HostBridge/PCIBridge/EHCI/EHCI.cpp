// Copyright 2025 Xenon Emulator Project

#include "Base/Assert.h"

#include "EHCI.h"

Xe::PCIDev::EHCI::EHCI(const std::string &deviceName, u64 size, s32 instance, u32 ports) :
  PCIDevice(deviceName, size),
  instance(instance), ports(ports) {

  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = instance == 0 ? 0x58051414 : 0x58071414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02900106;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x0C032001;

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x1000; // BAR0

  // Set Capability Registers
  capLength = 0x20;
  hcsParams = (ports & 0xF) | (1 << 16); // NPCC = 0, N_Ports = ports, PPC = 1
  hccParams = 0x6; // Assume 64-bit address capability and EECP = 0
  hcspPortRoute = 0;
}

void Xe::PCIDev::EHCI::Read(u64 readAddress, u8 *data, u64 size) {
  u16 offset = readAddress & 0xFFF;

  ASSERT(size == 4);

  u32 value = 0;
  memcpy(&value, data, size);
  
  switch (offset) {
  // Capability Registers
  case 0x00: // CAPLENGTH (8-bit) + HCIVERSION (16-bit)
    value = (0x0100 << 16) | 0x20; // HCIVERSION = 1.0, CAPLENGTH = 0x20
    break;
  case 0x04:
    value = hcsParams;
    break;
  case 0x08:
    value = hccParams;
    break;
  case 0x0C:
    value = hcspPortRoute;
    break;

  // Operational Registers
  case 0x20:
    value = usbCmd;
    break;
  case 0x24:
    value = 0x1000; // usbSts, return this for now.
    break;
  case 0x28:
    value = usbIntr;
    break;
  case 0x2C:
    value = frameIndex;
    break;
  case 0x30:
    value = ctrlDsSegment;
    break;
  case 0x34:
    value = periodicListBase;
    break;
  case 0x38:
    value = asyncListAddr;
    break;
  case 0x40:
    value = configFlag;
    break;
  default:
    if (offset >= 0x44 && offset < 0x44 + 8 * 4) {
      u32 portIndex = (offset - 0x44) / 4;
      if (portIndex < (hcsParams & 0xF)) {
        value = portSC[portIndex];
      }
    }
    break;
  }
  value = byteswap_le<u32>(value);
  LOG_DEBUG(EHCI, "{} Read({:#x}, data) == {:#x}", instance, offset, value);

  memcpy(data, &value, size);
}
void Xe::PCIDev::EHCI::Write(u64 writeAddress, const u8 *data, u64 size) {
  u16 offset = writeAddress & 0xFFF;
  ASSERT(size == 4);

  u32 value;
  memcpy(&value, data, size);
  value = byteswap_le<u32>(value);

  switch (offset) {
  case 0x20:
    usbCmd = value;
    // Reset
    if (usbCmd & (1 << 1)) {
      usbCmd &= ~(1 << 1);
    }
    LOG_DEBUG(OHCI, "{} USBCMD = {:#x}", instance, value);
    break;
  case 0x24:
    usbSts &= ~value; // Writing 1 clears bits
    LOG_DEBUG(OHCI, "{} USBSTS = {:#x}", instance, value);
    break;
  case 0x28:
    usbIntr = value;
    LOG_DEBUG(OHCI, "{} USBINTR = {:#x}", instance, value);
    break;
  case 0x2C:
    frameIndex = value;
    LOG_DEBUG(OHCI, "{} FRINDEX = {:#x}", instance, value);
    break;
  case 0x30:
    ctrlDsSegment = value;
    LOG_DEBUG(OHCI, "{} CTRLDSSEGMENT = {:#x}", instance, value);
    break;
  case 0x34:
    periodicListBase = value;
    LOG_DEBUG(OHCI, "{} PERIODICLISTBASE = {:#x}", instance, value);
    break;
  case 0x38:
    asyncListAddr = value;
    LOG_DEBUG(OHCI, "{} ASYNCLISTADDR = {:#x}", instance, value);
    break;
  case 0x40:
    configFlag = value;
    LOG_DEBUG(OHCI, "{} CONFIGFLAG = {:#x}", instance, value);
    break;
  default:
    if (offset >= 0x44 && offset < 0x44 + 8 * 4) {
      u32 portIndex = (offset - 0x44) / 4;
      if (portIndex < (hcsParams & 0xF)) {
        LOG_DEBUG(OHCI, "{} PORTSC[{}] = {:#x}", instance, portIndex, value);
        portSC[portIndex] = value;
      }
    } else {
      LOG_WARNING(OHCI, "{} Write({:#x}, {:#x}, {})", instance, offset, value, size);
    }
    break;
  }
}

void Xe::PCIDev::EHCI::MemSet(u64 writeAddress, s32 data, u64 size)
{}

void Xe::PCIDev::EHCI::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::EHCI::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
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
