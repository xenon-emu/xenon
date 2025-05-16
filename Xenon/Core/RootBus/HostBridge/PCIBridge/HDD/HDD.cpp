// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "HDD.h"

#include "Base/Logging/Log.h"

Xe::PCIDev::HDD::HDD(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge) :
  PCIDevice(deviceName, size) {
  // Note:
   // The ATA/ATAPI Controller in the Xenon Southbridge contain two BAR's:
   // The first is for the Command Block (Regs 0-7) + DevCtrl/AltStatus reg at offset 0xA.
   // The second is for the BMDMA (Bus Master DMA) block.

   // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x58031414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02300006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x01060000;
  pciConfigSpace.configSpaceHeader.regD.hexData = 0x00000058; // Capabilites Ptr.
  pciConfigSpace.configSpaceHeader.regF.hexData = 0x00000100; // Int line, pin.

  u32 data = 0;

  // Capabilities at offset 0x58:
  data = 0x80020001;
  memcpy(&pciConfigSpace.data[0x58], &data, 4);
  data = 0x00112400;
  memcpy(&pciConfigSpace.data[0x60], &data, 4);
  data = 0x7F7F7F7F;
  memcpy(&pciConfigSpace.data[0x70], &data, 4);
  memcpy(&pciConfigSpace.data[0x74], &data, 4); // Field value is the same as above.
  data = 0xC07231BE;
  memcpy(&pciConfigSpace.data[0x80], &data, 4);
  data = 0x40;
  memcpy(&pciConfigSpace.data[0x90], &data, 4);
  data = 0x100C04CC;
  memcpy(&pciConfigSpace.data[0x98], &data, 4);
  data = 0x004108C0;
  memcpy(&pciConfigSpace.data[0x9C], &data, 4);

  // Set the SCR's at offset 0xC0 (SiS-like).
  // SStatus.
  data = 0x00000000;
  memcpy(&pciConfigSpace.data[0xC0], &data, 4); // SSTATUS_DET_NO_DEVICE_DETECTED.
                                                // SSTATUS_SPD_NO_SPEED.
                                                // SSTATUS_IPM_NO_DEVICE.
  // SError.
  data = 0x001F0201;
  memcpy(&pciConfigSpace.data[0xC4], &data, 4);
  // SControl.
  data = 0x00000300;
  memcpy(&pciConfigSpace.data[0xC8], &data, 4); // SCONTROL_IPM_ALL_PM_DISABLED.

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x20; // BAR0
  pciDevSizes[1] = 0x10; // BAR1

  // Assign our PCI Bridge pointer
  parentBus = parentPCIBridge;

  // Device ready to receive commands.
  ataDeviceState.ataReadState.status = ATA_STATUS_DRDY;
}

void Xe::PCIDev::HDD::Read(u64 readAddress, u8 *data, u64 size) {
  const u32 regOffset = (readAddress & 0xFF) * 4;

  if (regOffset < sizeof(ATA_REG_STATE)) {
    switch (regOffset) {
    case ATA_REG_DATA:
      if (!ataDeviceState.readBuffer.empty()) {
        // First clear the DATA Request.
        if (ataDeviceState.ataReadState.status & ATA_STATUS_DRQ) {
          ataDeviceState.ataReadState.status &= ~ATA_STATUS_DRQ;
        }
        // We have some data to send out.
        // Make sure we send the right amount of data.
        size = (size < ataDeviceState.readBuffer.capacity()
                         ? size
                         : ataDeviceState.readBuffer.capacity());
        // Do the actual copy of data.
        memcpy(data, ataDeviceState.readBuffer.data(), size);
        // Remove the readed bytes of the conatiner.
        ataDeviceState.readBuffer.erase(ataDeviceState.readBuffer.begin(),
                                        ataDeviceState.readBuffer.begin() +
                                            size);
        return;
      }
      break;
    default:
      break;
    }

    memcpy(data, (u8*)&ataDeviceState.ataReadState + regOffset, size);
  } else {
    LOG_ERROR(HDD, "Unknown register! Attempted to read 0x{:X}", regOffset);
    memset(data, 0, size);
  }
}

void Xe::PCIDev::HDD::Write(u64 writeAddress, const u8 *data, u64 size) {
  const u32 regOffset = (writeAddress & 0xFF) * 4;

  if (regOffset < sizeof(ATA_REG_STATE)) {
    switch (regOffset) {
    case ATA_REG_CMD_STATUS:
      u64 value = 0;
      memcpy(&value, data, size);
      switch (value) {
      case ATA_COMMAND_DEVICE_RESET:
        break;
      case ATA_COMMAND_READ_SECTORS:
        break;
      case ATA_COMMAND_READ_DMA_EXT:
        break;
      case ATA_COMMAND_WRITE_SECTORS:
        break;
      case ATA_COMMAND_WRITE_DMA_EXT:
        break;
      case ATA_COMMAND_VERIFY:
        break;
      case ATA_COMMAND_VERIFY_EXT:
        break;
      case ATA_COMMAND_SET_DEVICE_PARAMETERS:
        break;
      case ATA_COMMAND_PACKET:
        break;
      case ATA_COMMAND_IDENTIFY_PACKET_DEVICE:
        break;
      case ATA_COMMAND_READ_MULTIPLE:
        break;
      case ATA_COMMAND_WRITE_MULTIPLE:
        break;
      case ATA_COMMAND_SET_MULTIPLE_MODE:
        break;
      case ATA_COMMAND_READ_DMA:
        break;
      case ATA_COMMAND_WRITE_DMA:
        break;
      case ATA_COMMAND_STANDBY_IMMEDIATE:
        break;
      case ATA_COMMAND_FLUSH_CACHE:
        break;
      case ATA_COMMAND_IDENTIFY_DEVICE:
        // Copy the device indetification data to our read buffer.
        ataCopyIdentifyDeviceData();
        // Set data ready flag.
        ataDeviceState.ataReadState.status |= ATA_STATUS_DRQ;
        // Raise an interrupt.
        parentBus->RouteInterrupt(PRIO_SATA_HDD);
        break;
      case ATA_COMMAND_SET_FEATURES:
        break;
      case ATA_COMMAND_SECURITY_SET_PASSWORD:
        break;
      case ATA_COMMAND_SECURITY_UNLOCK:
        break;
      case ATA_COMMAND_SECURITY_DISABLE_PASSWORD:
        break;
      default:
        break;
      }
    }

    memcpy(reinterpret_cast<u8*>(&ataDeviceState.ataWriteState + regOffset), data, size);
  } else {
    LOG_ERROR(HDD, "Unknown register being accessed: (Write) 0x{:X}", regOffset);
  }
}

void Xe::PCIDev::HDD::MemSet(u64 writeAddress, s32 data, u64 size) {
  const u32 regOffset = (writeAddress & 0xFF) * 4;

  if (regOffset < sizeof(ATA_REG_STATE)) {
    switch (regOffset) {
    case ATA_REG_CMD_STATUS:
      u64 value = 0;
      memset(&value, data, size);
      switch (value) {
      case ATA_COMMAND_DEVICE_RESET:
        break;
      case ATA_COMMAND_READ_SECTORS:
        break;
      case ATA_COMMAND_READ_DMA_EXT:
        break;
      case ATA_COMMAND_WRITE_SECTORS:
        break;
      case ATA_COMMAND_WRITE_DMA_EXT:
        break;
      case ATA_COMMAND_VERIFY:
        break;
      case ATA_COMMAND_VERIFY_EXT:
        break;
      case ATA_COMMAND_SET_DEVICE_PARAMETERS:
        break;
      case ATA_COMMAND_PACKET:
        break;
      case ATA_COMMAND_IDENTIFY_PACKET_DEVICE:
        break;
      case ATA_COMMAND_READ_MULTIPLE:
        break;
      case ATA_COMMAND_WRITE_MULTIPLE:
        break;
      case ATA_COMMAND_SET_MULTIPLE_MODE:
        break;
      case ATA_COMMAND_READ_DMA:
        break;
      case ATA_COMMAND_WRITE_DMA:
        break;
      case ATA_COMMAND_STANDBY_IMMEDIATE:
        break;
      case ATA_COMMAND_FLUSH_CACHE:
        break;
      case ATA_COMMAND_IDENTIFY_DEVICE:
        // Copy the device indetification data to our read buffer.
        if (!ataDeviceState.readBuffer.empty())
          LOG_ERROR(HDD, "Read buffer not empty!");
        ataDeviceState.readBuffer.resize(sizeof(ATA_IDENTIFY_DATA));
        memset(ataDeviceState.readBuffer.data(), data, sizeof(ATA_IDENTIFY_DATA));
        // Set data ready flag.
        ataDeviceState.ataReadState.status |= ATA_STATUS_DRQ;
        // Raise an interrupt.
        parentBus->RouteInterrupt(PRIO_SATA_HDD);
        break;
      case ATA_COMMAND_SET_FEATURES:
        break;
      case ATA_COMMAND_SECURITY_SET_PASSWORD:
        break;
      case ATA_COMMAND_SECURITY_UNLOCK:
        break;
      case ATA_COMMAND_SECURITY_DISABLE_PASSWORD:
        break;
      default:
        break;
      }
    }

    memset(reinterpret_cast<u8*>(&ataDeviceState.ataWriteState + regOffset), data, size);
  } else {
    LOG_ERROR(HDD, "Unknown register! Attempted to write 0x{:X}", regOffset);
  }
}

void Xe::PCIDev::HDD::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::HDD::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
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

void Xe::PCIDev::HDD::ataCopyIdentifyDeviceData() {
  if (!ataDeviceState.readBuffer.empty())
    LOG_ERROR(HDD, "Read buffer not empty!");

  ataDeviceState.readBuffer.resize(sizeof(ATA_IDENTIFY_DATA));
  memcpy(ataDeviceState.readBuffer.data(), &ataDeviceState.ataIdentifyData, sizeof(ATA_IDENTIFY_DATA));
}
