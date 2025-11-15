/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "ODD.h"

#include "Base/Config.h"
#include "Base/Logging/Log.h"

// Enables ODD Debug output
#define ODD_DEBUG

// Describes the ATA transfer modes available to the SET_TRNASFER_MODE subcommand.
enum class ATA_TRANSFER_MODE {
  PIO = 0x00,
  PIO_NO_IORDY = 0x01,
  PIO_FLOW_CONTROL_MODE3 = 0x08,
  PIO_FLOW_CONTROL_MODE4 = 0x09,
  MULTIWORD_DMA_MODE0 = 0x20,
  MULTIWORD_DMA_MODE1 = 0x21,
  MULTIWORD_DMA_MODE2 = 0x22,
  MULTIWORD_DMA_MODE3 = 0x23,
  ULTRA_DMA_MODE0 = 0x40,
  ULTRA_DMA_MODE1 = 0x41,
  ULTRA_DMA_MODE2 = 0x42,
  ULTRA_DMA_MODE3 = 0x43,
  ULTRA_DMA_MODE4 = 0x44,
  ULTRA_DMA_MODE5 = 0x45,
  ULTRA_DMA_MODE6 = 0x46,
};

Xe::PCIDev::ODD::ODD(const char* deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram) 
  : PCIDevice(deviceName, size) {
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

  // Set our PCI device sizes
  pciDevSizes[0] = 0x20; // BAR0
  pciDevSizes[1] = 0x10; // BAR1

  // Assign our PCI bridge and RAM pointers
  parentBus = parentPCIBridge;
  ramPtr = ram;

  // Initialize our input and output buffers
  atapiState.dataInBuffer.init(ATAPI_CDROM_SECTOR_SIZE, true);
  atapiState.dataInBuffer.reset();
  atapiState.dataOutBuffer.init(ATAPI_CDROM_SECTOR_SIZE, true);
  atapiState.dataOutBuffer.reset();

  // Set our Inquiry Data
  constexpr char vendorIdentification[] = "PLDS   16D2S";
  memcpy(&atapiState.atapiInquiryData.vendorIdentification,
    vendorIdentification, sizeof(vendorIdentification));

  atapiState.mountedODDImage = std::make_unique<STRIP_UNIQUE(atapiState.mountedODDImage)>(Config::filepaths.oddImage);

  if (atapiState.mountedODDImage.get()->isHandleValid()) {
    if (fs::exists(Config::filepaths.oddImage)) {
      try {
        std::error_code fsError;
        u64 fileSize = fs::file_size(Config::filepaths.oddImage, fsError);
        if (fileSize != -1 && fileSize) {
          // File is valid
          atapiState.imageAttached = true;
        }
        else {
          fileSize = 0;
          if (fsError) {
            LOG_ERROR(ODD, "Filesystem error: {} ({})", fsError.message(), fsError.value());
          }
        }
      }
      catch (const std::exception &ex) {
        LOG_ERROR(ODD, "Exception trying to check if image is valid. {}", ex.what());
        atapiState.imageAttached = false;
      }
    }
  }

  if (!atapiState.imageAttached) {
    LOG_INFO(ODD, "No ODD image found - disabling device.");
  }

  oddThreadRunning = atapiState.imageAttached;

  // Set the SCR's at offset 0xC0 (SiS-like)
  // SStatus
  data = atapiState.imageAttached ? 0x00000113 : 0;
  atapiState.regs.SStatus = data;
  memcpy(&pciConfigSpace.data[0xC0], &data, 4); // SSTATUS_DET_COM_ESTABLISHED.
                                                // SSTATUS_SPD_GEN1_COM_SPEED.
                                                // SSTATUS_IPM_INTERFACE_ACTIVE_STATE
  // SError
  data = 0x001F0201;
  atapiState.regs.SError = data;
  memcpy(&pciConfigSpace.data[0xC4], &data, 4);
  // SControl
  data = 0x00000300;
  atapiState.regs.SControl = data;
  memcpy(&pciConfigSpace.data[0xC8], &data, 4); // SCONTROL_IPM_ALL_PM_DISABLED
  // SActive
  data = 0x00000040;
  atapiState.regs.SActive = data;
  memcpy(&pciConfigSpace.data[0xCC], &data, 4);

  // Device ready to receive commands.
  atapiState.regs.status = ATA_STATUS_DRDY;

  // Enter ODD Worker Thread
  oddWorkerThread = std::thread(&Xe::PCIDev::ODD::oddThreadLoop, this);
}

// PCI Read
void Xe::PCIDev::ODD::Read(u64 readAddress, u8 *data, u64 size) {
  // PCI BAR0 is the Primary Command Block Base Address
  u8 atapiCommandReg =
      static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the Primary Control Block Base Address
  u8 atapiControlReg =
      static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR1);

#ifdef ODD_DEBUG
  LOG_DEBUG(ODD, "[Read]: Reg {}, address {:#x}", getATAPIRegisterName(readAddress & 0xFF), readAddress);
#endif // ODD_DEBUG

  // Command Registers
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
                         pciConfigSpace.configSpaceHeader.BAR0)) {

    switch (atapiCommandReg) {
    case ATA_REG_DATA:
      if (!atapiState.dataOutBuffer.empty()) {
        size = std::fmin(size, atapiState.dataOutBuffer.count());
        memcpy(&atapiState.regs.data, atapiState.dataOutBuffer.get(), size);
        atapiState.dataOutBuffer.resize(size);
        atapiState.regs.status &= 0xFFFFFFF7; // Clear DRQ.
        // Check for a completed read.
        if (atapiState.dataOutBuffer.count() == 0) {
          atapiState.dataOutBuffer.reset(); // Reset pointer.
        }
      }
      memcpy(data, &atapiState.regs.data, size);
      break;
    case ATAPI_REG_ERROR:
      memcpy(data, &atapiState.regs.error, size);
      // Clear the error status on the status register
      atapiState.regs.status &= ~ATA_STATUS_ERR_CHK;
      return;
    case ATAPI_REG_INT_REAS:
      memcpy(data, &atapiState.regs.interruptReason, size);
      return;
    case ATAPI_REG_LBA_LOW:
      memcpy(data, &atapiState.regs.lbaLow, size);
      return;
    case ATAPI_REG_BYTE_COUNT_LOW:
      memcpy(data, &atapiState.regs.byteCountLow, size);
      return;
    case ATAPI_REG_BYTE_COUNT_HIGH:
      memcpy(data, &atapiState.regs.byteCountHigh, size);
      return;
    case ATAPI_REG_DEVICE:
      memcpy(data, &atapiState.regs.deviceSelect, size);
      return;
    case ATAPI_REG_STATUS:
      // TODO(bitsh1ft3r): Reading to the status reg should cancel any pending interrupts
      memcpy(data, &atapiState.regs.status, size);
      return;
    case ATAPI_REG_ALTERNATE_STATUS:
      // Reading to the alternate status register returns the contents of the Status register,
      // but it does not clean pending interrupts. Also wastes 100ns
      std::this_thread::sleep_for(100ns);
      memcpy(data, &atapiState.regs.status, size);
      return;
    case ATA_REG_SSTATUS:
      memcpy(data, &atapiState.regs.SStatus, size);
      return;
    case ATA_REG_SERROR:
      memcpy(data, &atapiState.regs.SError, size);
      return;
    case ATA_REG_SCONTROL:
      memcpy(data, &atapiState.regs.SControl, size);
      return;
    case ATA_REG_SACTIVE:
      memcpy(data, &atapiState.regs.SActive, size);
      return;
    default:
      LOG_ERROR(ODD, "Unknown Command Register Block register being read, command code = 0x{:X}", atapiCommandReg);
      break;
    }
  } else {  // Control (DMA) registers
    switch (atapiControlReg) {
    case ATAPI_DMA_REG_COMMAND:
      memcpy(data, &atapiState.regs.dmaCommand, size);
      break;
    case ATAPI_DMA_REG_STATUS:
      memcpy(data, &atapiState.regs.dmaStatus, size);
      break;
    case ATAPI_DMA_REG_TABLE_OFFSET:
      memcpy(data, &atapiState.regs.dmaTableOffset, size);
      break;
    default:
      LOG_ERROR(ODD, "Unknown Control Register Block register being read, command code = 0x{:X}", atapiControlReg);
      break;
    }
  }
}
// PCI Write
void Xe::PCIDev::ODD::Write(u64 writeAddress, const u8 *data, u64 size) {
  // PCI BAR0 is the Primary Command Block Base Address
  u8 atapiCommandReg =
      static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the Primary Control Block Base Address
  u8 atapiControlReg =
      static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);

  u32 inData = 0;
  memcpy(&inData, data, size);

#ifdef ODD_DEBUG
  LOG_DEBUG(ODD, "[Write]: Reg {}, address {:#x}, data {:#x}", getATAPIRegisterName(writeAddress & 0xFF),
    writeAddress, inData);
#endif // ODD_DEBUG

  // Command Registers
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
                         pciConfigSpace.configSpaceHeader.BAR0)) {

    switch (atapiCommandReg) {
    case ATAPI_REG_DATA: {
      // Reset the DRQ status
      atapiState.regs.status &= ~ATA_STATUS_DRQ;

      memcpy(&atapiState.regs.data, data, size);

      // Push the data onto our buffer
      size = std::fmin(size, atapiState.dataInBuffer.count());
      memcpy(atapiState.dataInBuffer.get(), data, size);
      atapiState.dataInBuffer.resize(size);

      // Check if we're executing a SCSI command input and we have a full
      // command
      if (atapiState.dataInBuffer.size() >= XE_ATAPI_CDB_SIZE &&
          atapiState.regs.command == ATA_COMMAND_PACKET) {
        // Process SCSI Command
        processSCSICommand();
        // Reset our buffer ptr.
        atapiState.dataInBuffer.reset();
        // Request an Interrupt.
        atapiIssueInterrupt();
      }
      return;
    } break;
    case ATAPI_REG_FEATURES:
      memcpy(&atapiState.regs.features, data, size);
      return;
    case ATAPI_REG_SECTOR_COUNT:
      memcpy(&atapiState.regs.sectorCount, data, size);
      return;
    case ATAPI_REG_LBA_LOW:
      memcpy(&atapiState.regs.lbaLow, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_LOW:
      memcpy(&atapiState.regs.byteCountLow, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_HIGH:
      memcpy(&atapiState.regs.byteCountHigh, data, size);
      return;
    case ATAPI_REG_DEVICE:
      memcpy(&atapiState.regs.deviceSelect, data, size);
      return;
    case ATAPI_REG_COMMAND:
      memcpy(&atapiState.regs.command, data, size);
      // Reset the status & error register
      atapiState.regs.status &= ~ATA_STATUS_ERR_CHK;
      atapiState.regs.error &= ~ATA_ERROR_ABRT;

#ifdef ODD_DEBUG
      LOG_DEBUG(ODD, "ATAPI Command received: {}", getATACommandName(atapiState.regs.command));
#endif

      switch (atapiState.regs.command) {
      case ATA_COMMAND_PACKET: {
        atapiState.regs.status |= ATA_STATUS_DRQ;
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
      case ATA_COMMAND_SET_FEATURES:
        switch (atapiState.regs.features) {
        case ATA_SF_SUBCOMMAND_SET_TRANSFER_MODE: {
          ATA_TRANSFER_MODE mode = static_cast<ATA_TRANSFER_MODE>(atapiState.regs.sectorCount);
          switch (mode) {
          case ATA_TRANSFER_MODE::PIO:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO");
            break;
          case ATA_TRANSFER_MODE::PIO_NO_IORDY:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_NO_IORDY");
            break;
          case ATA_TRANSFER_MODE::PIO_FLOW_CONTROL_MODE3:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_FLOW_CONTROL_MODE3");
            break;
          case ATA_TRANSFER_MODE::PIO_FLOW_CONTROL_MODE4:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_FLOW_CONTROL_MODE4");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE0:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE0");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE1:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE1");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE2:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE2");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE3:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE3");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE0:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE0");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE1:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE1");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE2:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE2");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE3:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE3");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE4:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE4");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE5:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE5");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE6:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE6");
            break;
          default:
            LOG_DEBUG(ODD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to {:#x}", atapiState.regs.sectorCount);
            break;
          }
          atapiState.regs.ataTransferMode = inData;
          }
          // Request interrupt - For some weird reason (it's 3am, dont honestly care rn)
          // This doesnt work with the issue interrupt behavior TODO.
          parentBus->RouteInterrupt(PRIO_SATA_ODD);
        }
        break;
      default: {
        LOG_ERROR(ODD, "Unknown command, command code = 0x{:X}", atapiState.regs.command);
      }  break;
      }
      return;
    case ATAPI_REG_DEVICE_CONTROL: {
      memcpy(&atapiState.regs.deviceControl, data, size);
      return;
    } break;
    case ATA_REG_SSTATUS:
      memcpy(&atapiState.regs.SStatus, data, size);
      // Write also on PCI config space data
      memcpy(&pciConfigSpace.data[0xC0], &data, 4);
      return;
    case ATA_REG_SERROR:
      memcpy(&atapiState.regs.SError, data, size);
      // Write also on PCI config space data.
      memcpy(&pciConfigSpace.data[0xC4], &data, 4);
      return;
    case ATA_REG_SCONTROL:
      memcpy(&atapiState.regs.SControl, data, size);
      // Write also on PCI config space data.
      memcpy(&pciConfigSpace.data[0xC8], &data, 4);
#ifdef ODD_DEBUG
      if (atapiState.regs.SControl & 1)
        LOG_DEBUG(ODD, "[SCONTROL]: Resetting SATA link!");
#endif // ODD_DEBUG
      return;
    case ATA_REG_SACTIVE:
      memcpy(&atapiState.regs.SActive, data, size);
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
    case ATAPI_DMA_REG_COMMAND:
      memcpy(&atapiState.regs.dmaCommand, data, size);
      if (atapiState.regs.dmaCommand & XE_ATAPI_DMA_ACTIVE) {
        atapiState.regs.dmaStatus = XE_ATA_DMA_ACTIVE; // Signal DMA active status.
      }
      break;
    case ATAPI_DMA_REG_STATUS:
      memcpy(&atapiState.regs.dmaStatus, data, size);
      break;
    case ATAPI_DMA_REG_TABLE_OFFSET:
      memcpy(&atapiState.regs.dmaTableOffset, data, size);
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
      atapiState.regs.status &= ~ATA_STATUS_DRQ;

      memset(&atapiState.regs.data, data, size);

      // Push the data onto our buffer
      size = std::fmin(size, atapiState.dataInBuffer.count());
      memset(atapiState.dataInBuffer.get(), data, size);
      atapiState.dataInBuffer.resize(size);

      // Check if we're executing a SCSI command input and we have a full
      // command
      if (atapiState.dataInBuffer.size() >= XE_ATAPI_CDB_SIZE &&
          atapiState.regs.command == ATA_COMMAND_PACKET) {
        // Process a SCSI command
        processSCSICommand();
        // Reset our buffer pointer
        atapiState.dataInBuffer.reset();
        // Request an interrupt
        parentBus->RouteInterrupt(PRIO_SATA_ODD);
      }
      return;
    } break;
    case ATAPI_REG_FEATURES:
      memset(&atapiState.regs.features, data, size);
      return;
    case ATAPI_REG_SECTOR_COUNT:
      memset(&atapiState.regs.sectorCount, data, size);
      return;
    case ATAPI_REG_LBA_LOW:
      memset(&atapiState.regs.lbaLow, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_LOW:
      memset(&atapiState.regs.byteCountLow, data, size);
      return;
    case ATAPI_REG_BYTE_COUNT_HIGH:
      memset(&atapiState.regs.byteCountHigh, data, size);
      return;
    case ATAPI_REG_DEVICE:
      memset(&atapiState.regs.deviceSelect, data, size);
      return;
    case ATAPI_REG_COMMAND:
      memset(&atapiState.regs.command, data, size);

      // Reset the status register
      atapiState.regs.status &= ~ATA_STATUS_ERR_CHK;

      // Reset the error register
      atapiState.regs.error &= ~ATA_ERROR_ABRT;

      switch (atapiState.regs.command) {
      case ATA_COMMAND_PACKET: {
        atapiState.regs.status |= ATA_STATUS_DRQ;
        return;
      } break;
      case ATA_COMMAND_IDENTIFY_PACKET_DEVICE: {
        memset(atapiState.dataOutBuffer.get(), data, size);
        // Set the transfer size:
        // bytecount = LBA High << 8 | LBA Mid
        constexpr size_t dataSize = sizeof(XE_ATAPI_IDENTIFY_DATA);
        atapiState.regs.lbaLow = 1;
        atapiState.regs.byteCountLow = dataSize & 0xFF;
        atapiState.regs.byteCountHigh = (dataSize >> 8) & 0xFF;
        // Set the drive status
        atapiState.regs.status = ATA_STATUS_DRDY | ATA_STATUS_DRQ | ATA_STATUS_DF;

        // Request an interrupt
        parentBus->RouteInterrupt(PRIO_SATA_CDROM);
        return;
      } break;
      case ATA_COMMAND_IDENTIFY_DEVICE: {
        atapiIdentifyCommand();
        return;
      } break;
      default:
        LOG_ERROR(ODD, "Unknown command, command code = 0x{:X}", atapiState.regs.command);
        break;
      }
      return;
    case ATAPI_REG_DEVICE_CONTROL:
      memset(&atapiState.regs.deviceControl, data, size);
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
      memset(&atapiState.regs.dmaCommand, data, size);

      if (atapiState.regs.dmaCommand & XE_ATAPI_DMA_ACTIVE) {
        // Start our DMA operation
        doDMA();
        // Change our DMA status after completion
        atapiState.regs.dmaStatus &= ~XE_ATAPI_DMA_ACTIVE;
      }
      break;
    case ATAPI_DMA_REG_STATUS:
      memset(&atapiState.regs.dmaStatus, data, size);
      break;
    case ATAPI_DMA_REG_TABLE_OFFSET:
      memset(&atapiState.regs.dmaTableOffset, data, size);
      break;
    default:
      LOG_ERROR(ODD, "Unknown Control Register Block register being written, command code = 0x{:X}", atapiControlReg);
      break;
    }
  }
}

// Config read.
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
// Config write.
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

//
// ATA Commands
//

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
  atapiState.regs.status |= ATA_STATUS_ERR_CHK;

  atapiState.regs.error |= ATA_ERROR_ABRT;
  atapiState.regs.interruptReason = 0x1;
  atapiState.regs.lbaLow = 0x1;
  atapiState.regs.byteCountLow = 0x14;
  atapiState.regs.byteCountHigh = 0xEB;

  // Set interrupt reason
  atapiState.regs.interruptReason = IDE_INTERRUPT_REASON_IO;

  // An interrupt must also be requested
  atapiIssueInterrupt();
}

void Xe::PCIDev::ODD::atapiIdentifyPacketDeviceCommand() {
  if (!atapiState.dataOutBuffer.init(sizeof(XE_ATAPI_IDENTIFY_DATA), true)) {
    LOG_ERROR(ODD, "Failed to initialize data buffer for atapiIdentifyPacketDeviceCommand");
  }

  XE_ATAPI_IDENTIFY_DATA *identifyData;

  // Reset the pointer
  atapiState.dataOutBuffer.reset();
  identifyData = (XE_ATAPI_IDENTIFY_DATA *)atapiState.dataOutBuffer.get();

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
  constexpr size_t dataSize = sizeof(XE_ATAPI_IDENTIFY_DATA);

  atapiState.regs.lbaLow = 1;
  atapiState.regs.byteCountLow = dataSize & 0xFF;
  atapiState.regs.byteCountHigh = (dataSize >> 8) & 0xFF;

  // Set the drive status
  atapiState.regs.status = ATA_STATUS_DRDY | ATA_STATUS_DRQ | ATA_STATUS_DF;

  // Request interrupt
  atapiIssueInterrupt();
}

//
// Utilities
//

static const std::unordered_map<u32, const std::string> ataCommandNameMap = {
  { 0x08, "DEVICE_RESET" },
  { 0x20, "READ_SECTORS" },
  { 0x25, "READ_DMA_EXT" },
  { 0x27, "READ_NATIVE_MAX_ADDRESS_EXT" },
  { 0x30, "WRITE_SECTORS" },
  { 0x35, "WRITE_DMA_EXT" },
  { 0x40, "READ_VERIFY_SECTORS" },
  { 0x42, "READ_VERIFY_SECTORS_EXT" },
  { 0x60, "READ_FPDMA_QUEUED" },
  { 0x91, "SET_DEVICE_PARAMETERS" },
  { 0xA0, "PACKET" },
  { 0xA1, "IDENTIFY_PACKET_DEVICE" },
  { 0xC4, "READ_MULTIPLE" },
  { 0xC5, "WRITE_MULTIPLE" },
  { 0xC6, "SET_MULTIPLE_MODE" },
  { 0xC8, "READ_DMA" },
  { 0xCA, "WRITE_DMA" },
  { 0xE0, "STANDBY_IMMEDIATE" },
  { 0xE7, "FLUSH_CACHE" },
  { 0xEC, "IDENTIFY_DEVICE" },
  { 0xEF, "SET_FEATURES" },
  { 0xF1, "SECURITY_SET_PASSWORD" },
  { 0xF2, "SECURITY_UNLOCK" },
  { 0xF6, "SECURITY_DISABLE_PASSWORD" }
};

// Returns the command name as an std::string.
std::string Xe::PCIDev::ODD::getATACommandName(u32 commandID) {
  auto it = ataCommandNameMap.find(commandID);
  if (it != ataCommandNameMap.end()) {
    return it->second;
  }
  else {
    LOG_ERROR(ODD, "Unknown Command: {:#x}", commandID);
    return "Unknown Command";
  }
}

static const std::unordered_map<u32, const std::string> atapiRegisterNameMap = {
  { 0x00, "Data" },
  { 0x01, "Error (Read)/Features (Write)" },
  { 0x02, "Interrupt Reason (Read)/ Sector Count (Write)" },
  { 0x03, "Lba Low" },
  { 0x04, "Byte Count Low" },
  { 0x05, "Byte Count High" },
  { 0x06, "Device Select" },
  { 0x07, "Status (Read)/ Command (Write)" },
  { 0x0A, "Alternative Status (Read)/ Device Control (Write)" },
  { 0x10, "SStatus" },
  { 0x14, "SError" },
  { 0x18, "SControl" },
  { 0x1C, "SActive" },
  { 0x20, "DMA Command" },
  { 0x22, "DMA Status" },
  { 0x24, "DMA Table Offset" }
};

// Returns the register name as an std::string.
std::string Xe::PCIDev::ODD::getATAPIRegisterName(u32 regID) {
  auto it = atapiRegisterNameMap.find(regID);
  if (it != atapiRegisterNameMap.end()) {
    return it->second;
  }
  else {
    LOG_ERROR(ODD, "Unknown Register: {:#x}", regID);
    return "Unknown register";
  }
}

// Worker thread for DMA.
void Xe::PCIDev::ODD::oddThreadLoop() {
  // Check if we should be running
  if (!oddThreadRunning)
    return;
  LOG_INFO(ODD, "Entered ODD worker thread.");
  while (oddThreadRunning) {
    // Check if we should exit early
    oddThreadRunning = XeRunning;
    if (!oddThreadRunning)
      break;
    // Check for the DMA active command.
    if (atapiState.regs.dmaCommand & XE_ATA_DMA_ACTIVE) {
      // Start our DMA operation
      doDMA();
      // Change our DMA status after completion.
      atapiState.regs.dmaCommand &= ~1; // Clear active status.
      atapiState.regs.dmaStatus = XE_ATA_DMA_INTR; // Signal Interrupt.
    }
    // Sleep for some time.
    std::this_thread::sleep_for(5ms);
  }

  LOG_INFO(ODD, "Exiting ODD worker thread.");
}

// Performs the DMA operation until it reaches the end of the PRDT.
void Xe::PCIDev::ODD::doDMA() {
  for (;;) {
    // Read the first entry of the table in memory
    u8 *DMAPointer = ramPtr->GetPointerToAddress(atapiState.regs.dmaTableOffset +
      atapiState.dmaState.currentTableOffset);
    // Each entry is 64 bit long
    memcpy(&atapiState.dmaState, DMAPointer, 8);

    // Store current position in the table
    atapiState.dmaState.currentTableOffset += 8;

    // If this bit in the Command register is set we're facing a read operation
    bool readOperation = atapiState.regs.dmaCommand & XE_ATAPI_DMA_WR;
    // This bit specifies that we're facing the last entry in the PRD Table
    bool lastEntry = atapiState.dmaState.currentPRD.control & 0x8000;
    // The byte count to read/write
    u16 size = atapiState.dmaState.currentPRD.sizeInBytes;
    // The address in memory to be written to/read from
    u32 bufferAddress = atapiState.dmaState.currentPRD.physAddress;
    // Buffer Pointer in main memory
    u8 *bufferInMemory = ramPtr->GetPointerToAddress(bufferAddress);

    if (readOperation) {
      // Reading from us
      size = std::fmin(static_cast<u32>(size), atapiState.dataOutBuffer.count());

      // Buffer overrun? Apparently there can be entries in the PRDT which have a zero byte count.
      if (size == 0) {
        LOG_WARNING(ODD, "[DMA Worker Read] Entry read size is zero.");
      }

      memcpy(bufferInMemory, atapiState.dataOutBuffer.get(), size);
      atapiState.dataOutBuffer.resize(size);
    }
    else {
      // Writing to us
      size = std::fmin(static_cast<u32>(size), atapiState.dataInBuffer.count());
      // Buffer overrun? Apparently there can be entries in the PRDT which have a zero byte count.
      if (size == 0) {
        LOG_WARNING(ODD, "[DMA Worker Write] Entry write size is zero.");
      }
      memcpy(atapiState.dataInBuffer.get(), bufferInMemory, size);
      atapiState.dataInBuffer.resize(size);
    }
    if (lastEntry) {
      // Reset the current position
      atapiState.dmaState.currentTableOffset = 0;
      // After completion we must raise an interrupt
      atapiIssueInterrupt();
      return;
    }
  }
}

// Issues an interrupt to the XCPU.
void Xe::PCIDev::ODD::atapiIssueInterrupt() {
  if ((atapiState.regs.deviceControl & ATA_DEVICE_CONTROL_NIEN) == 0) {
#ifdef ODD_DEBUG
    LOG_DEBUG(ODD, "Issuing interrupt.");
#endif // ODD_DEBUG
    parentBus->RouteInterrupt(PRIO_SATA_HDD);
  }
}

// Processes SCSI commands.
void Xe::PCIDev::ODD::processSCSICommand() {
  atapiState.dataInBuffer.reset();
  memcpy(&atapiState.scsiCBD.AsByte, atapiState.dataInBuffer.get(), 16);

  // Read/Sector Data for R/W operations
  u64 readOffset = 0;
  memcpy(&readOffset, &atapiState.scsiCBD.CDB12.LogicalBlock, 4);
  u32 sectorCount = 0;
  memcpy(&sectorCount, &atapiState.scsiCBD.CDB12.TransferLength, 4);
  readOffset = byteswap_be<u32>(static_cast<u32>(readOffset));
  sectorCount = byteswap_be<u32>(static_cast<u32>(sectorCount));
  char sense[15] = {};
  sense[0] = 0x70;
  switch (atapiState.scsiCBD.CDB12.OperationCode) {
  case SCSIOP_TEST_UNIT_READY:

    break;
  case SCSIOP_REQUEST_SENSE:
    LOG_DEBUG(ODD, "atapi_request_sense");

    // Copy our data struct
    memcpy(atapiState.dataOutBuffer.get(), &sense,
      sizeof(sense));
    // Set the Status register to data request
    atapiState.regs.status |= ATA_STATUS_DRQ;
    break;
  case SCSIOP_INQUIRY:
    // Copy our data struct
    memcpy(atapiState.dataOutBuffer.get(), &atapiState.atapiInquiryData,
      sizeof(XE_ATAPI_INQUIRY_DATA));
    // Set the Status register to data request
    atapiState.regs.status |= ATA_STATUS_DRQ;
    atapiState.regs.SActive = 0x40; // SActive to 0x40, SATA driver in xboxkrnl checks this.
    break;
  case SCSIOP_READ:
    readOffset *= ATAPI_CDROM_SECTOR_SIZE;
    sectorCount *= ATAPI_CDROM_SECTOR_SIZE;

    atapiState.dataOutBuffer.init(sectorCount, false);
    atapiState.dataOutBuffer.reset();
    atapiState.mountedODDImage->Read(readOffset, atapiState.dataOutBuffer.get(),
      sectorCount);
    break;
  default:
    LOG_ERROR(ODD, "Unknown SCSI Command requested: 0x{:X}", atapiState.scsiCBD.CDB12.OperationCode);
  }

  atapiState.regs.interruptReason = IDE_INTERRUPT_REASON_IO;
}