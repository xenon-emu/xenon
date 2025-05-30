// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "ODD.h"

#include "Base/Config.h"
#include "Base/Logging/Log.h"

void Xe::PCIDev::ODD::atapiReset() {
  // Set status to ready
  atapiState.atapiRegs.statusReg = ATA_STATUS_DRDY;

  // Initialize our input and output buffers
  atapiState.dataWriteBuffer.init(ATAPI_CDROM_SECTOR_SIZE, true);
  atapiState.dataWriteBuffer.reset();
  atapiState.dataReadBuffer.init(ATAPI_CDROM_SECTOR_SIZE, true);
  atapiState.dataReadBuffer.reset();
  // These seem to be used to detect the presence of a disc drive
  atapiState.atapiRegs.unk_10 = 0x1;
  atapiState.atapiRegs.unk_14 = 0x1;
  atapiState.atapiRegs.signatureReg = 0xEB140101;
  atapiState.atapiRegs.unk_1C = 0x1;

  // Set our Inquiry Data
  constexpr char vendorIdentification[] = "PLDS   16D2S";
  memcpy(&atapiState.atapiInquiryData.vendorIdentification,
         vendorIdentification, sizeof(vendorIdentification));

  atapiState.mountedCDImage = std::make_unique<STRIP_UNIQUE(atapiState.mountedCDImage)>(Config::filepaths.oddImage);
}

void Xe::PCIDev::ODD::atapiIdentifyPacketDeviceCommand() {
  // This command is only for ATAPI devices
  LOG_DEBUG(ODD, "ATAPI_IDENTIFY_PACKET_DEVICE_COMMAND");

  if (!atapiState.dataReadBuffer.init(sizeof(XE_ATA_IDENTIFY_DATA), true)) {
    LOG_ERROR(ODD, "Failed to initialize data buffer for atapiIdentifyPacketDeviceCommand");
  }

  XE_ATA_IDENTIFY_DATA* identifyData;

  // Reset the pointer
  atapiState.dataReadBuffer.reset();
  identifyData = (XE_ATA_IDENTIFY_DATA*)atapiState.dataReadBuffer.get();

  // The data is stored in little endian
  constexpr u8 serialNumber[] = { 0x38, 0x44, 0x33, 0x31, 0x42, 0x42, 0x34, 0x32,
    0x36, 0x36, 0x32, 0x31, 0x30, 0x30, 0x48, 0x36, 0x20, 0x4a, 0x20, 0x20 };

  constexpr u8 firmwareRevision[] = { 0x35, 0x31, 0x32, 0x33, 0x20, 0x20, 0x20, 0x20 };

  constexpr u8 modelNumber[] = { 0x4c, 0x50, 0x53, 0x44, 0x20 ,0x20, 0x20, 0x20,
    0x47, 0x44, 0x31, 0x2d, 0x44, 0x36, 0x53, 0x35,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };

  // Set the data
  identifyData->generalConfiguration = 0x85C0;
  memcpy(&identifyData->serialNumber, &serialNumber, sizeof(serialNumber));
  memcpy(&identifyData->firmwareRevision, &firmwareRevision, sizeof(firmwareRevision));
  memcpy(&identifyData->modelNumber, &modelNumber, sizeof(modelNumber));

  identifyData->capabilities = 0x0F00;
  identifyData->reserved7 = 0x40;
  identifyData->reserved8 = 0x00;
  identifyData->reserved9 = 0x0200;
  identifyData->translationFieldsValid = 0x6;
  identifyData->advancedPIOModes = 0x3;
  identifyData->minimumMWXferCycleTime = 0x78;
  identifyData->recommendedMWXferCycleTime = 0x78;
  identifyData->minimumPIOCycleTime = 0x78;
  identifyData->minimumPIOCycleTimeIORDY = 0x78;
  identifyData->majorRevision = 0xf8;
  identifyData->minorRevision = 0x210;
  identifyData->ultraDMASupport = 0x20;
  identifyData->ultraDMAActive = 0x3f;


  // Set the transfer size:
  // bytecount = LBA High << 8 | LBA Mid
  constexpr size_t dataSize = sizeof(XE_ATA_IDENTIFY_DATA);

  atapiState.atapiRegs.lbaLowReg = 1;
  atapiState.atapiRegs.byteCountLowReg = dataSize & 0xFF;
  atapiState.atapiRegs.byteCountHighReg = (dataSize >> 8) & 0xFF;

  // Set the drive status
  atapiState.atapiRegs.statusReg = ATA_STATUS_DRDY | ATA_STATUS_DRQ | ATA_STATUS_DF;

  // Request interrupt
  parentBus->RouteInterrupt(PRIO_SATA_CDROM);
}


void Xe::PCIDev::ODD::atapiIdentifyCommand() {
  // Used by software to decide whether the device is an ATA or ATAPI device
  /*
      ATAPI drives will set the ABRT bit in the Error register and will place
     the signature of ATAPI drives in the Interrupt Reason, LBA Low, Byte Count
     Low, and Byte Count High registers

      ATAPI Reg         | ATAPI Signature
      ------------------------------------
      Interrupt Reason  | 0x1
      LBA Low           | 0x1
      Byte Count Low    | 0x14
      Byte Count High   | 0xEB
  */

  // Set the drive status
  atapiState.atapiRegs.statusReg |= ATA_STATUS_ERR_CHK;

  atapiState.atapiRegs.errorReg |= ATA_ERROR_ABRT;
  atapiState.atapiRegs.interruptReasonReg = 0x1;
  atapiState.atapiRegs.lbaLowReg = 0x1;
  atapiState.atapiRegs.byteCountLowReg = 0x14;
  atapiState.atapiRegs.byteCountHighReg = 0xEB;

  // An interrupt must also be requested
  parentBus->RouteInterrupt(PRIO_SATA_ODD);

  // Set interrupt reason
  atapiState.atapiRegs.interruptReasonReg = IDE_INTERRUPT_REASON_IO;
}

void Xe::PCIDev::ODD::processSCSICommand() {
  atapiState.dataWriteBuffer.reset();
  memcpy(&atapiState.scsiCBD.AsByte, atapiState.dataWriteBuffer.get(), 16);

  // Read/Sector Data for R/W operations
  u64 readOffset = 0;
  memcpy(&readOffset, &atapiState.scsiCBD.CDB12.LogicalBlock, 4);
  u32 sectorCount = 0;
  memcpy(&sectorCount, &atapiState.scsiCBD.CDB12.TransferLength, 4);
  readOffset = byteswap_be<u32>(static_cast<u32>(readOffset));
  sectorCount = byteswap_be<u32>(static_cast<u32>(sectorCount));

  switch (atapiState.scsiCBD.CDB12.OperationCode) {
  case SCSIOP_INQUIRY:
    // Copy our data struct
    memcpy(atapiState.dataReadBuffer.get(), &atapiState.atapiInquiryData,
           sizeof(XE_ATAPI_INQUIRY_DATA));
    // Set the Status register to data request
    atapiState.atapiRegs.statusReg |= ATA_STATUS_DRQ;
    break;
  case SCSIOP_READ:
    readOffset *= ATAPI_CDROM_SECTOR_SIZE;
    sectorCount *= ATAPI_CDROM_SECTOR_SIZE;

    atapiState.dataReadBuffer.init(sectorCount, false);
    atapiState.dataReadBuffer.reset();
    atapiState.mountedCDImage->Read(readOffset, atapiState.dataReadBuffer.get(),
                                    sectorCount);
    break;
  default:
    LOG_ERROR(ODD, "Unknown SCSI Command requested: 0x{:X}", atapiState.scsiCBD.CDB12.OperationCode);
  }

  atapiState.atapiRegs.interruptReasonReg = IDE_INTERRUPT_REASON_IO;
}

void Xe::PCIDev::ODD::doDMA() {
  for (;;) {
    // Read the first entry of the table in memory
    u8* DMAPointer = mainMemory->GetPointerToAddress(atapiState.atapiRegs.dmaTableOffsetReg + atapiState.dmaState.currentTableOffset);
    // Each entry is 64 bit long
    memcpy(&atapiState.dmaState, DMAPointer, 8);

    // Store current position in the table
    atapiState.dmaState.currentTableOffset += 8;

    // If this bit in the Command register is set we're facing a read operation
    bool readOperation = atapiState.atapiRegs.dmaCmdReg & XE_ATAPI_DMA_WR;
    // This bit specifies that we're facing the last entry in the PRD Table
    bool lastEntry = atapiState.dmaState.currentPRD.control & 0x8000;
    // The byte count to read/write
    u16 size = atapiState.dmaState.currentPRD.sizeInBytes;
    // The address in memory to be written to/read from
    u32 bufferAddress = atapiState.dmaState.currentPRD.physAddress;
    // Buffer Pointer in main memory
    u8 *bufferInMemory = mainMemory->GetPointerToAddress(bufferAddress);

    if (readOperation) {
      // Reading from us
      size = std::fmin(static_cast<u32>(size), atapiState.dataReadBuffer.count());

      // Buffer overrun?
      if (size == 0)
        return;
      memcpy(bufferInMemory, atapiState.dataReadBuffer.get(), size);
      atapiState.dataReadBuffer.resize(size);
    } else {
      // Writing to us
      size = std::fmin(static_cast<u32>(size), atapiState.dataWriteBuffer.count());
      // Buffer overrun?
      if (size == 0)
        return;
      memcpy(atapiState.dataWriteBuffer.get(), bufferInMemory, size);
      atapiState.dataWriteBuffer.resize(size);
    }
    if (lastEntry) {
      // Reset the current position
      atapiState.dmaState.currentTableOffset = 0;
      // After completion we must raise an interrupt
      parentBus->RouteInterrupt(PRIO_SATA_ODD);
      return;
    }
  }
}

Xe::PCIDev::ODD::ODD(const char* deviceName, u64 size,
  PCIBridge *parentPCIBridge, RAM *ram) : PCIDevice(deviceName, size) {
  // Note:
  // The ATA/ATAPI Controller in the Xenon Southbridge contain two BAR's:
  // The first is for the Command Block (Regs 0-7) + DevCtrl/AltStatus reg at offset 0xA
  // The second is for the BMDMA (Bus Master DMA) block

  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x58021414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02300006;
  pciConfigSpace.configSpaceHeader.reg2.hexData = 0x01060000;
  pciConfigSpace.configSpaceHeader.regD.hexData = 0x00000058; // Capabilites pointer
  pciConfigSpace.configSpaceHeader.regF.hexData = 0x00000100; // Int line, pin

  u32 data = 0;

  // Capabilities at offset 0x58:
  data = 0x80020001;
  memcpy(&pciConfigSpace.data[0x58], &data, 4);
  data = 0x00112400;
  memcpy(&pciConfigSpace.data[0x60], &data, 4);
  data = 0x7F7F7F7F;
  memcpy(&pciConfigSpace.data[0x70], &data, 4);
  memcpy(&pciConfigSpace.data[0x74], &data, 4); // Field value is the same as above
  data = 0xC07231BE;
  memcpy(&pciConfigSpace.data[0x80], &data, 4);
  data = 0x100C04CC;
  memcpy(&pciConfigSpace.data[0x98], &data, 4);
  data = 0x004108C0;
  memcpy(&pciConfigSpace.data[0x9C], &data, 4);

  // Set the SCR's at offset 0xC0 (SiS-like)
  // SStatus
  data = 0x00000113;
  memcpy(&pciConfigSpace.data[0xC0], &data, 4); // SSTATUS_DET_COM_ESTABLISHED
                                                // SSTATUS_SPD_GEN1_COM_SPEED
                                                // SSTATUS_IPM_INTERFACE_ACTIVE_STATE
  // SError
  data = 0x001F0201;
  memcpy(&pciConfigSpace.data[0xC4], &data, 4);
  // SControl
  data = 0x00000300;
  memcpy(&pciConfigSpace.data[0xC8], &data, 4); // SCONTROL_IPM_ALL_PM_DISABLED

  // Set our PCI device sizes
  pciDevSizes[0] = 0x20; // BAR0
  pciDevSizes[1] = 0x10; // BAR1

  // Assign our PCI bridge and RAM pointers
  parentBus = parentPCIBridge;
  mainMemory = ram;

  // Reset our state
  atapiReset();
}

void Xe::PCIDev::ODD::Read(u64 readAddress, u8 *data, u64 size) {
  // PCI BAR0 is the Primary Command Block Base Address
  u8 atapiCommandReg =
      static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the Primary Control Block Base Address
  u8 atapiControlReg =
      static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR1);

  // Who are we reading from?
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
                         pciConfigSpace.configSpaceHeader.BAR0)) {
    // Command Registers
    switch (atapiCommandReg) {
    case ATAPI_REG_DATA: {
      // Check if we have some data to return
      if (!atapiState.dataReadBuffer.empty()) {
        size = std::fmin(size, atapiState.dataReadBuffer.count());
        memcpy(data, atapiState.dataReadBuffer.get(), size);
        atapiState.dataReadBuffer.resize(size);
        return;
      }
      return;
    } break;
    case ATAPI_REG_ERROR:
      memcpy(data, &atapiState.atapiRegs.errorReg, size);
      // Clear the error status on the status register
      atapiState.atapiRegs.statusReg &= ~ATA_STATUS_ERR_CHK;
      return;
    case ATAPI_REG_INT_REAS:
      memcpy(data, &atapiState.atapiRegs.interruptReasonReg, size);
      return;
    case ATAPI_REG_LBA_LOW:
      memcpy(data, &atapiState.atapiRegs.lbaLowReg, size);
      return;
    case ATAPI_REG_BYTE_COUNT_LOW:
      memcpy(data, &atapiState.atapiRegs.byteCountLowReg, size);
      return;
    case ATAPI_REG_BYTE_COUNT_HIGH:
      memcpy(data, &atapiState.atapiRegs.byteCountHighReg, size);
      return;
    case ATAPI_REG_DEVICE:
      memcpy(data, &atapiState.atapiRegs.deviceReg, size);
      return;
    case ATAPI_REG_STATUS:
      // TODO(bitsh1ft3r): Reading to the status reg should cancel any pending interrupts
      memcpy(data, &atapiState.atapiRegs.statusReg, size);
      return;
    case ATAPI_REG_ALTERNATE_STATUS:
      // Reading to the alternate status register returns the contents of the Status register,
      // but it does not clean pending interrupts. Also wastes 100ns
      std::this_thread::sleep_for(100ns);
      memcpy(data, &atapiState.atapiRegs.statusReg, size);
      return;
    case 0x10:
      memcpy(data, &atapiState.atapiRegs.unk_10, size);
      return;
    case 0x14:
      memcpy(data, &atapiState.atapiRegs.unk_14, size);
      return;
    case ATAPI_REG_SIGNATURE:
      memcpy(data, &atapiState.atapiRegs.signatureReg, size);
      return;
    case 0x1C:
      memcpy(data, &atapiState.atapiRegs.unk_1C, size);
      return;
    default:
      LOG_ERROR(ODD, "Unknown Command Register Block register being read, command code = 0x{:X}", atapiCommandReg);
      break;
    }
  } else {
    // Control Registers
    switch (atapiControlReg) {
    case ATAPI_DMA_REG_COMMAND:
      memcpy(data, &atapiState.atapiRegs.dmaCmdReg, size);
      break;
    case ATAPI_DMA_REG_STATUS:
      memcpy(data, &atapiState.atapiRegs.dmaStatusReg, size);
      break;
    case ATAPI_DMA_REG_TABLE_OFFSET:
      memcpy(data, &atapiState.atapiRegs.dmaTableOffsetReg, size);
      break;
    default:
      LOG_ERROR(ODD, "Unknown Control Register Block register being read, command code = 0x{:X}", atapiControlReg);
      break;
    }
  }
}

void Xe::PCIDev::ODD::Write(u64 writeAddress, const u8 *data, u64 size) {
  // PCI BAR0 is the Primary Command Block Base Address
  u8 atapiCommandReg =
      static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the Primary Control Block Base Address
  u8 atapiControlReg =
      static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);

  // Who are we writing to?
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
                         pciConfigSpace.configSpaceHeader.BAR0)) {
    // Command Registers
    switch (atapiCommandReg) {
    case ATAPI_REG_DATA: {
      // Reset the DRQ status
      atapiState.atapiRegs.statusReg &= ~ATA_STATUS_DRQ;

      memcpy(&atapiState.atapiRegs.dataReg, data, size);

      // Push the data onto our buffer
      size = std::fmin(size, atapiState.dataWriteBuffer.count());
      memcpy(atapiState.dataWriteBuffer.get(), data, size);
      atapiState.dataWriteBuffer.resize(size);

      // Check if we're executing a SCSI command input and we have a full
      // command
      if (atapiState.dataWriteBuffer.size() >= XE_ATAPI_CDB_SIZE &&
          atapiState.atapiRegs.commandReg == ATA_COMMAND_PACKET) {
        // Process SCSI Command
        processSCSICommand();
        // Reset our buffer ptr.
        atapiState.dataWriteBuffer.reset();
        // Request an Interrupt.
        parentBus->RouteInterrupt(PRIO_SATA_ODD);
      }
      return;
    } break;
    case ATAPI_REG_FEATURES:
      memcpy(&atapiState.atapiRegs.featuresReg, data, size);
      return;
    case ATAPI_REG_SECTOR_COUNT:
      memcpy(&atapiState.atapiRegs.sectorCountReg, data, size);
      return;
    case ATAPI_REG_LBA_LOW:
      memcpy(&atapiState.atapiRegs.lbaLowReg, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_LOW:
      memcpy(&atapiState.atapiRegs.byteCountLowReg, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_HIGH:
      memcpy(&atapiState.atapiRegs.byteCountHighReg, data, size);
      return;
    case ATAPI_REG_DEVICE:
      memcpy(&atapiState.atapiRegs.deviceReg, data, size);
      return;
    case ATAPI_REG_COMMAND:
      memcpy(&atapiState.atapiRegs.commandReg, data, size);
      // Reset the status & error register
      atapiState.atapiRegs.statusReg &= ~ATA_STATUS_ERR_CHK;
      atapiState.atapiRegs.errorReg &= ~ATA_ERROR_ABRT;

      switch (atapiState.atapiRegs.commandReg) {
      case ATA_COMMAND_PACKET: {
        atapiState.atapiRegs.statusReg |= ATA_STATUS_DRQ;
        return;
      } break;
      case ATA_COMMAND_IDENTIFY_PACKET_DEVICE: {
        atapiIdentifyPacketDeviceCommand();
        return;
      } break;
      case ATA_COMMAND_IDENTIFY_DEVICE: {
        atapiIdentifyCommand();
        return;
      } break;
      default: {
        LOG_ERROR(ODD, "Unknown command, command code = 0x{:X}", atapiState.atapiRegs.commandReg);
      }  break;
      }
      return;
    case ATAPI_REG_DEVICE_CONTROL: {
      memcpy(&atapiState.atapiRegs.devControlReg, data, size);
      return;
    } break;
    case 0x10:
      memcpy(&atapiState.atapiRegs.unk_10, data, size);
      return;
    case 0x14:
      memcpy(&atapiState.atapiRegs.unk_14, data, size);
      return;
    case ATAPI_REG_SIGNATURE:
      memcpy(&atapiState.atapiRegs.signatureReg, data, size);
      return;
    case 0x1C:
      memcpy(&atapiState.atapiRegs.unk_1C, data, size);
      return;
    default: {
      u64 tmp = 0;
      memcpy(&tmp, data, size);
      LOG_ERROR(ODD, "Unknown Command Register Block register being written, command reg = 0x{:X}"
        ", write address = 0x{:X}, data = 0x{:X}", atapiCommandReg, writeAddress, tmp);
    } break;
    }
  } else {
    // Control registers
    switch (atapiControlReg) {
    case ATAPI_DMA_REG_COMMAND: {
      memcpy(&atapiState.atapiRegs.dmaCmdReg, data, size);

      if (atapiState.atapiRegs.dmaCmdReg & XE_ATAPI_DMA_ACTIVE) {
        // Start our DMA operation
        doDMA();
        // Change our DMA status after completion
        atapiState.atapiRegs.dmaStatusReg &= ~XE_ATAPI_DMA_ACTIVE;
      }
    } break;
    case ATAPI_DMA_REG_STATUS:
      memcpy(&atapiState.atapiRegs.dmaStatusReg, data, size);
      break;
    case ATAPI_DMA_REG_TABLE_OFFSET:
      memcpy(&atapiState.atapiRegs.dmaTableOffsetReg, data, size);
      break;
    default:
      LOG_ERROR(ODD, "Unknown Control Register Block register being written, command code = 0x{:X}", atapiControlReg);
      break;
    }
  }
}

void Xe::PCIDev::ODD::MemSet(u64 writeAddress, s32 data, u64 size) {
  // PCI BAR0 is the primary command block base address
  u8 atapiCommandReg =
      static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the primary command block base address
  u8 atapiControlReg =
      static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);

  // Who are we writing to?
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
                         pciConfigSpace.configSpaceHeader.BAR0)) {
    // Command Registers
    switch (atapiCommandReg) {
    case ATAPI_REG_DATA: {
      // Reset the DRQ status
      atapiState.atapiRegs.statusReg &= ~ATA_STATUS_DRQ;

      memset(&atapiState.atapiRegs.dataReg, data, size);

      // Push the data onto our buffer
      size = std::fmin(size, atapiState.dataWriteBuffer.count());
      memset(atapiState.dataWriteBuffer.get(), data, size);
      atapiState.dataWriteBuffer.resize(size);

      // Check if we're executing a SCSI command input and we have a full
      // command
      if (atapiState.dataWriteBuffer.size() >= XE_ATAPI_CDB_SIZE &&
          atapiState.atapiRegs.commandReg == ATA_COMMAND_PACKET) {
        // Process a SCSI command
        processSCSICommand();
        // Reset our buffer pointer
        atapiState.dataWriteBuffer.reset();
        // Request an interrupt
        parentBus->RouteInterrupt(PRIO_SATA_ODD);
      }
      return;
    } break;
    case ATAPI_REG_FEATURES:
      memset(&atapiState.atapiRegs.featuresReg, data, size);
      return;
    case ATAPI_REG_SECTOR_COUNT:
      memset(&atapiState.atapiRegs.sectorCountReg, data, size);
      return;
    case ATAPI_REG_LBA_LOW:
      memset(&atapiState.atapiRegs.lbaLowReg, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_LOW:
      memset(&atapiState.atapiRegs.byteCountLowReg, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_HIGH:
      memset(&atapiState.atapiRegs.byteCountHighReg, data, size);
      return;
    case ATAPI_REG_DEVICE:
      memset(&atapiState.atapiRegs.deviceReg, data, size);
      return;
    case ATAPI_REG_COMMAND:
      memset(&atapiState.atapiRegs.commandReg, data, size);

      // Reset the status register
      atapiState.atapiRegs.statusReg &= ~ATA_STATUS_ERR_CHK;

      // Reset the error register
      atapiState.atapiRegs.errorReg &= ~ATA_ERROR_ABRT;

      switch (atapiState.atapiRegs.commandReg) {
      case ATA_COMMAND_PACKET: {
        atapiState.atapiRegs.statusReg |= ATA_STATUS_DRQ;
        return;
      } break;
      case ATA_COMMAND_IDENTIFY_PACKET_DEVICE: {
        memset(atapiState.dataReadBuffer.get(), data, size);
        // Set the transfer size:
        // bytecount = LBA High << 8 | LBA Mid
        constexpr size_t dataSize = sizeof(XE_ATA_IDENTIFY_DATA);
        atapiState.atapiRegs.lbaLowReg = 1;
        atapiState.atapiRegs.byteCountLowReg = dataSize & 0xFF;
        atapiState.atapiRegs.byteCountHighReg = (dataSize >> 8) & 0xFF;
        // Set the drive status
        atapiState.atapiRegs.statusReg = ATA_STATUS_DRDY | ATA_STATUS_DRQ | ATA_STATUS_DF;

        // Request an interrupt
        parentBus->RouteInterrupt(PRIO_SATA_CDROM);
        return;
      } break;
      case ATA_COMMAND_IDENTIFY_DEVICE: {
        atapiIdentifyCommand();
        return;
      } break;
      default:
        LOG_ERROR(ODD, "Unknown command, command code = 0x{:X}", atapiState.atapiRegs.commandReg);
        break;
      }
      return;
    case ATAPI_REG_DEVICE_CONTROL:
      memset(&atapiState.atapiRegs.devControlReg, data, size);
      return;
    default:
      u64 tmp = 0;
      memset(&tmp, data, size);
      LOG_ERROR(ODD, "Unknown Command Register Block register being written, command reg = 0x{:X}"
        ", write address = 0x{:X}, data = 0x{:X}", atapiCommandReg, writeAddress, tmp);
      break;
    }
  } else {
    // Control Registers
    switch (atapiControlReg) {
    case ATAPI_DMA_REG_COMMAND:
      memset(&atapiState.atapiRegs.dmaCmdReg, data, size);

      if (atapiState.atapiRegs.dmaCmdReg & XE_ATAPI_DMA_ACTIVE) {
        // Start our DMA operation
        doDMA();
        // Change our DMA status after completion
        atapiState.atapiRegs.dmaStatusReg &= ~XE_ATAPI_DMA_ACTIVE;
      }
      break;
    case ATAPI_DMA_REG_STATUS:
      memset(&atapiState.atapiRegs.dmaStatusReg, data, size);
      break;
    case ATAPI_DMA_REG_TABLE_OFFSET:
      memset(&atapiState.atapiRegs.dmaTableOffsetReg, data, size);
      break;
    default:
      LOG_ERROR(ODD, "Unknown Control Register Block register being written, command code = 0x{:X}", atapiControlReg);
      break;
    }
  }
}

void Xe::PCIDev::ODD::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  const u8 readReg = static_cast<u8>(readAddress);
  if (readReg >= XE_SIS_SCR_BASE && readReg <= 0xFF) {
    // Read the SATA status and control registers
    switch ((readReg - XE_SIS_SCR_BASE) / 4) {
    case SCR_STATUS_REG:
      LOG_WARNING(ODD, "SCR ConfigRead to SCR_STATUS_REG.");
      break;
    case SCR_ERROR_REG:
      LOG_WARNING(ODD, "SCR ConfigRead to SCR_ERROR_REG.");
      break;
    case SCR_CONTROL_REG:
      LOG_WARNING(ODD, "SCR ConfigRead to SCR_CONTROL_REG.");
      break;
      case SCR_ACTIVE_REG:
      LOG_WARNING(ODD, "SCR ConfigRead to SCR_ACTIVE_REG.");
      break;
      case SCR_NOTIFICATION_REG:
      LOG_WARNING(ODD, "SCR ConfigRead to SCR_NOTIFICATION_REG.");
      break;
    default:
      LOG_ERROR(ODD, "SCR ConfigRead to reg 0x{:X}", readReg * 4);
      break;
    }
  }
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
  LOG_DEBUG(ODD, "ConfigRead to reg 0x{:X}", readReg * 4);
}

void Xe::PCIDev::ODD::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
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

  u8 writeReg = static_cast<u8>(writeAddress);
  if (writeReg >= XE_SIS_SCR_BASE && writeReg <= 0xFF) {
    // Write to the SATA status and control registers
    switch ((writeReg - XE_SIS_SCR_BASE) / 4) {
    case SCR_STATUS_REG:
      LOG_WARNING(ODD, "SCR ConfigWrite to SCR_STATUS_REG, data 0x{:X}", tmp);
      break;
    case SCR_ERROR_REG:
      LOG_WARNING(ODD, "SCR ConfigWrite to SCR_ERROR_REG, data 0x{:X}", tmp);
      break;
    case SCR_CONTROL_REG:
      LOG_WARNING(ODD, "SCR ConfigWrite to SCR_CONTROL_REG, data 0x{:X}", tmp);
      break;
    case SCR_ACTIVE_REG:
      LOG_WARNING(ODD, "SCR ConfigWrite to SCR_ACTIVE_REG, data 0x{:X}", tmp);
      break;
    case SCR_NOTIFICATION_REG:
      LOG_WARNING(ODD, "SCR ConfigRead to SCR_NOTIFICATION_REG, data 0x{:X}", tmp);
      break;
    default:
      LOG_ERROR(ODD, "SCR ConfigWrite to reg 0x{:X}, data 0x{:X}", writeReg * 4, tmp);
      break;
    }
  }
  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
  LOG_DEBUG(ODD, "ConfigWrite to reg 0x{:X}, data 0x{:X}", writeReg * 4, tmp);
}
