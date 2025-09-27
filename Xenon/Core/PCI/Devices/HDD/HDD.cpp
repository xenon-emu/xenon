/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HDD.h"

#include "Base/Logging/Log.h"

//#define HDD_DEBUG

// Data was pulled off of an Hitachi 250Gb retail HDD.
const u8 identifyDataBytes[] = {
  0x5a, 0x04, 0xff, 0x3f,
  0x37, 0xc8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x31, 0x31, 0x32, 0x30, 0x38, 0x32, 0x42, 0x50,
  0x32, 0x4e, 0x38, 0x33, 0x53, 0x4e, 0x33, 0x44, 0x42, 0x4b, 0x55, 0x54,
  0x03, 0x00, 0x50, 0x38, 0x04, 0x00, 0x42, 0x50, 0x4f, 0x32, 0x36, 0x43,
  0x47, 0x34, 0x69, 0x48, 0x61, 0x74, 0x68, 0x63, 0x20, 0x69, 0x54, 0x48,
  0x35, 0x53, 0x35, 0x34, 0x32, 0x30, 0x42, 0x35, 0x53, 0x39, 0x30, 0x41,
  0x20, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x80, 0x00, 0x40, 0x00, 0x0f,
  0x00, 0x40, 0x00, 0x02, 0x00, 0x02, 0x07, 0x00, 0xff, 0x3f, 0x10, 0x00,
  0x3f, 0x00, 0x10, 0xfc, 0xfb, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0x0f,
  0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x78, 0x00, 0x78, 0x00, 0x78, 0x00,
  0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1f, 0x00, 0x02, 0x17, 0x00, 0x00, 0x5e, 0x00, 0x40, 0x00,
  0xfc, 0x01, 0x28, 0x00, 0x6b, 0x74, 0x69, 0x7f, 0x63, 0x61, 0x69, 0x74,
  0x49, 0xbc, 0x63, 0x61, 0x7f, 0x10, 0x29, 0x00, 0x2a, 0x00, 0x80, 0x40,
  0xfe, 0xff, 0x00, 0x00, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x70, 0x59, 0x1c, 0x1d, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x88, 0x00, 0x50, 0xa6, 0xcc,
  0xcf, 0x6c, 0xdc, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x40, 0x1c, 0x40, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x21, 0x00, 0x0b, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
  0x4d, 0x32, 0x00, 0x00, 0x00, 0x00, 0x81, 0x72, 0x45, 0x45, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x15,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x10, 0x21, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xc7, 0x02,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xa5, 0xc2
};

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

Xe::PCIDev::HDD::HDD(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM* ram) :
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

  // Assign our PCI Bridge pointer
  parentBus = parentPCIBridge;

  // Assign our RAM pointer
  ramPtr = ram;

  u32 data = 0;
  // Capabilities at offset 0x58:
  data = 0x80020001;
  memcpy(&pciConfigSpace.data[0x58], &data, 4);
  data = 0x00112400;
  memcpy(&pciConfigSpace.data[0x60], &data, 4);
  data = 0x7F7F7F7F;
  memcpy(&pciConfigSpace.data[0x70], &data, 4);
  // Field value is the same as above
  memcpy(&pciConfigSpace.data[0x74], &data, 4);
  data = 0xC07231BE;
  memcpy(&pciConfigSpace.data[0x80], &data, 4);
  data = 0x40;
  memcpy(&pciConfigSpace.data[0x90], &data, 4);
  data = 0x100C04CC;
  memcpy(&pciConfigSpace.data[0x98], &data, 4);
  data = 0x004108C0;
  memcpy(&pciConfigSpace.data[0x9C], &data, 4);

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x20; // BAR0
  pciDevSizes[1] = 0x10; // BAR1

  // Fill out our identify data structure.
  memcpy(&ataState.ataIdentifyData, &identifyDataBytes, sizeof(identifyDataBytes));

  // Mount our HDD image according to config.
  ataState.mountedHDDImage = std::make_unique<STRIP_UNIQUE(ataState.mountedHDDImage)>(Config::filepaths.hddImage);

  if (ataState.mountedHDDImage.get()->isHandleValid()) {
    if (fs::exists(Config::filepaths.hddImage)) {
      try {
        std::error_code fsError;
        u64 fileSize = fs::file_size(Config::filepaths.hddImage, fsError);
        if (fileSize != -1 && fileSize) {
          // File is valid
          ataState.imageAttached = true;
        } else {
          fileSize = 0;
          if (fsError) {
            LOG_ERROR(HDD, "Filesystem error: {} ({})", fsError.message(), fsError.value());
          }
        }
      } catch (const std::exception &ex) {
        LOG_ERROR(HDD, "Exception trying to check if image is valid. {}", ex.what());
        ataState.imageAttached = false;
      }
    }
  }

  if (!ataState.imageAttached) {
    LOG_INFO(HDD, "No HDD image found - disabling device.");
  }

  hddThreadRunning = ataState.imageAttached;

  // Set the SCR's at offset 0xC0 (SiS-like).
  // SStatus
  data = ataState.imageAttached ? 0x00000113 : 0;
  ataState.regs.SStatus = data;
  memcpy(&pciConfigSpace.data[0xC0], &data, 4); // SSTATUS_DET_COM_ESTABLISHED.
                                                // SSTATUS_SPD_GEN1_COM_SPEED.
                                                // SSTATUS_IPM_INTERFACE_ACTIVE_STATE.
  // SError
  data = 0x001D0003;
  ataState.regs.SError = data;
  memcpy(&pciConfigSpace.data[0xC4], &data, 4);
  // SControl
  data = 0x00000300;
  ataState.regs.SControl = data;
  memcpy(&pciConfigSpace.data[0xC8], &data, 4); // SCONTROL_IPM_ALL_PM_DISABLED.
  // SActive
  data = 0x00000040;
  ataState.regs.SActive = data;
  memcpy(&pciConfigSpace.data[0xCC], &data, 4);

  // Device ready to receive commands.
  ataState.regs.status = ATA_STATUS_DRDY;

  // Enter HDD Worker Thread
  hddWorkerThread = std::thread(&Xe::PCIDev::HDD::hddThreadLoop, this);
}

Xe::PCIDev::HDD::~HDD() {
  // Terminate thread.
  hddThreadRunning = false;
  if (hddWorkerThread.joinable())
    hddWorkerThread.join();
}

// PCI Read
void Xe::PCIDev::HDD::Read(u64 readAddress, u8 *data, u64 size) {
  // PCI BAR0 is the Primary Command Block Base Address
  u8 ataCommandReg =
    static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the DMA Block Base Address
  u8 ataControlReg =
    static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR1);

#ifdef HDD_DEBUG
  LOG_DEBUG(HDD, "[Read]: Address {:#x}, reg offset {:#x}", readAddress, readAddress & 0xFF);
#endif // HDD_DEBUG

  // Command Registers
  if (ataCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
    pciConfigSpace.configSpaceHeader.BAR0)) {

    switch (ataCommandReg) {
    case ATA_REG_DATA:
      if (!ataState.dataOutBuffer.empty()) {
        size = std::fmin(size, ataState.dataOutBuffer.count());
        memcpy(&ataState.regs.data, ataState.dataOutBuffer.get(), size);
        ataState.dataOutBuffer.resize(size);
        ataState.regs.status &= 0xFFFFFFF7; // Clear DRQ.
        // Check for a completed read.
        if (ataState.dataOutBuffer.count() == 0) {
          ataState.dataOutBuffer.reset(); // Reset pointer.
        }
      }
      memcpy(data, &ataState.regs.data, size);
      break;
    case ATA_REG_ERROR:
      memcpy(data, &ataState.regs.error, size);
      break;
    case ATA_REG_SECTORCOUNT:
      memcpy(data, &ataState.regs.sectorCount, size);
      break;
    case ATA_REG_LBA_LOW:
      if (ataState.regs.deviceControl & ATA_DEVICE_CONTROL_HOB) {
        // Reading is performed from the 'previous content' register.
        memcpy(data, &ataState.regs.prevLBALow, size);
      } else {
        // Reading is performed from the 'last content' register.
        memcpy(data, &ataState.regs.lbaLow, size);
      }
      break;
    case ATA_REG_LBA_MED:
      if (ataState.regs.deviceControl & ATA_DEVICE_CONTROL_HOB) {
        // Reading is performed from the 'previous content' register.
        memcpy(data, &ataState.regs.prevLBAMiddle, size);
      } else {
        // Reading is performed from the 'last content' register.
        memcpy(data, &ataState.regs.lbaMiddle, size);
      }
      break;
    case ATA_REG_LBA_HI:
      if (ataState.regs.deviceControl & ATA_DEVICE_CONTROL_HOB) {
        // Reading is performed from the 'previous content' register.
        memcpy(data, &ataState.regs.prevLBAHigh, size);
      } else {
        // Reading is performed from the 'last content' register.
        memcpy(data, &ataState.regs.lbaHigh, size);
      }
      break;
    case ATA_REG_DEV_SEL:
      memcpy(data, &ataState.regs.deviceSelect, size);
      break;
    case ATA_REG_STATUS:
      memcpy(data, &ataState.regs.status, size);
      break;
    case ATA_REG_ALT_STATUS:
      // Reading to the alternate status register returns the contents of the Status register,
      // but it does not clean pending interrupts. Also wastes 100ns
      std::this_thread::sleep_for(100ns);
      memcpy(data, &ataState.regs.status, size);
      break;
    case ATA_REG_SSTATUS:
      memcpy(data, &ataState.regs.SStatus, size);
      break;
    case ATA_REG_SERROR:
      memcpy(data, &ataState.regs.SError, size);
      break;
    case ATA_REG_SCONTROL:
      memcpy(data, &ataState.regs.SControl, size);
      break;
    case ATA_REG_SACTIVE:
      memcpy(data, &ataState.regs.SActive, size);
      break;
    default:
      LOG_ERROR(HDD, "Unknown command register {:#x} being read. Byte count = {:#d}", ataCommandReg, size);
      break;
    }
  } else {  // Control (DMA) registers
    switch (ataControlReg) {
    case ATA_REG_DMA_COMMAND:
      memcpy(data, &ataState.regs.dmaCommand, size);
      break;
    case ATA_REG_DMA_STATUS:
      memcpy(data, &ataState.regs.dmaStatus, size);
      break;
    case ATA_REG_DMA_TABLE_OFFSET:
      memcpy(data, &ataState.regs.dmaTableOffset, size);
      break;
    default:
      LOG_ERROR(HDD, "Unknown control register {:#x} being read. Byte count = {:#d}", ataControlReg, size);
      break;
    }
  }
}
// PCI Write
void Xe::PCIDev::HDD::Write(u64 writeAddress, const u8 *data, u64 size) {

  // PCI BAR0 is the Primary Command Block Base Address
  u8 ataCommandReg =
    static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the DMA Block Base Address
  u8 ataControlReg =
    static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);

  u32 inData = 0;
  memcpy(&inData, data, size);

#ifdef HDD_DEBUG
  LOG_DEBUG(HDD, "[Write]: Address {:#x}, reg offset {:#x}, data {:#x}", writeAddress, writeAddress & 0xFF, inData);
#endif // HDD_DEBUG

  // Command Registers
  if (ataCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 -
    pciConfigSpace.configSpaceHeader.BAR0)) {
    const u8 regOffset = static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

    switch (regOffset) {
    case ATA_REG_DATA:
      memcpy(&ataState.regs.data, data, size);
      break;
    case ATA_REG_FEATURES:
      memcpy(&ataState.regs.features, data, size);
      break;
    case ATA_REG_SECTORCOUNT:
      ataState.regs.prevSectorCount = ataState.regs.sectorCount;
      memcpy(&ataState.regs.sectorCount, data, size);
      break;
    case ATA_REG_LBA_LOW:
      ataState.regs.prevLBALow = ataState.regs.lbaLow;
      memcpy(&ataState.regs.lbaLow, data, size);
      // XeLL checks this to see if it has a drive.
      if (!ataState.imageAttached) {
        ataState.regs.lbaLow = ataState.regs.prevLBALow;
      }
      break;
    case ATA_REG_LBA_MED:
      ataState.regs.prevLBAMiddle = ataState.regs.lbaMiddle;
      memcpy(&ataState.regs.lbaMiddle, data, size);
      break;
    case ATA_REG_LBA_HI:
      ataState.regs.prevLBAHigh = ataState.regs.lbaHigh;
      memcpy(&ataState.regs.lbaHigh, data, size);
      break;
    case ATA_REG_DEV_SEL:
      memcpy(&ataState.regs.deviceSelect, data, size);
      break;
    case ATA_REG_CMD:
      memcpy(&ataState.regs.command, data, size);

#ifdef HDD_DEBUG
      LOG_DEBUG(HDD, "[CMD]: Received Command {}", getATACommandName(ataState.regs.command));
#endif // HDD_DEBUG

      switch (ataState.regs.command) {
      case ATA_COMMAND_READ_DMA: {
#ifdef HDD_DEBUG
        u64 offset = (ataState.regs.lbaHigh << 16) |
          (ataState.regs.lbaMiddle << 8) |
          (ataState.regs.lbaLow);

        u32 sectorCount = ataState.regs.sectorCount;
        LOG_DEBUG(HDD, "[CMD]: [READ DMA] LBA28: {:#x}, sector count {:#x}",
          offset, sectorCount);
#endif // HDD_DEBUG
      }
        ataReadDMACommand();
        break;
      case ATA_COMMAND_READ_DMA_EXT: {
#ifdef HDD_DEBUG
        u64 offset = (static_cast<u64>(ataState.regs.prevLBAHigh) << 40) |
          (static_cast<u64>(ataState.regs.prevLBAMiddle) << 32) |
          (static_cast<u64>(ataState.regs.prevLBALow) << 24) |
          (static_cast<u64>(ataState.regs.lbaHigh) << 16) |
          (static_cast<u64>(ataState.regs.lbaMiddle) << 8) |
          (static_cast<u64>(ataState.regs.lbaLow));

          u32 sectorCount = (ataState.regs.prevSectorCount << 8) | ataState.regs.sectorCount;
          LOG_DEBUG(HDD, "[CMD]: [READ DMA EXT] LBA48: {:#x}, sector count {:#x}",
            offset, sectorCount);
#endif // HDD_DEBUG
        ataReadDMAExtCommand();
        break;
      }
      case ATA_COMMAND_READ_NATIVE_MAX_ADDRESS_EXT:
        ataReadNativeMaxAddressExtCommand();
        // Request interrupt
        ataIssueInterrupt();
        break;
      case ATA_COMMAND_WRITE_DMA:
        ataWriteDMACommand();
        break;
      case ATA_COMMAND_IDENTIFY_DEVICE:
        ataIdentifyDeviceCommand();
        // Request interrupt
        ataIssueInterrupt();
        break;
      case ATA_COMMAND_SET_FEATURES:
        switch (ataState.regs.features) {
        case ATA_SF_SUBCOMMAND_SET_TRANSFER_MODE: {
#ifdef HDD_DEBUG
          ATA_TRANSFER_MODE mode = static_cast<ATA_TRANSFER_MODE>(ataState.regs.sectorCount);
          switch (mode) {
          case ATA_TRANSFER_MODE::PIO:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO");
            break;
          case ATA_TRANSFER_MODE::PIO_NO_IORDY:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_NO_IORDY");
            break;
          case ATA_TRANSFER_MODE::PIO_FLOW_CONTROL_MODE3:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_FLOW_CONTROL_MODE3");
            break;
          case ATA_TRANSFER_MODE::PIO_FLOW_CONTROL_MODE4:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_FLOW_CONTROL_MODE4");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE0:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE0");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE1:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE1");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE2:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE2");
            break;
          case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE3:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE3");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE0:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE0");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE1:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE1");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE2:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE2");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE3:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE3");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE4:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE4");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE5:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE5");
            break;
          case ATA_TRANSFER_MODE::ULTRA_DMA_MODE6:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE6");
            break;
          default:
            LOG_DEBUG(HDD, "[CMD](SET_TRANSFER_MODE): Setting transfer mode to {:#x}", ataState.regs.sectorCount);
            break;
          }
#endif // HDD_DEBUG
          ataState.regs.ataTransferMode = inData;
        }
        break;
        default:
          LOG_ERROR(HDD, "[CMD]: Set features {:#x} subcommand unknown.", ataState.regs.features);
          break;
        }
        // Request interrupt
        ataIssueInterrupt();
        break;
      default:
        LOG_ERROR(HDD, "Unhandled command received {}", getATACommandName(ataState.regs.command));
        break;
      }
      break;
    case ATA_REG_DEV_CTRL:
      memcpy(&ataState.regs.deviceControl, data, size);
      break;
    case ATA_REG_SSTATUS:
      memcpy(&ataState.regs.SStatus, data, size);
      // Write also on PCI config space data
      memcpy(&pciConfigSpace.data[0xC0], &data, 4);
      break;
    case ATA_REG_SERROR:
      memcpy(&ataState.regs.SError, data, size);
      // Write also on PCI config space data.
      memcpy(&pciConfigSpace.data[0xC4], &data, 4);
      break;
    case ATA_REG_SCONTROL:
      memcpy(&ataState.regs.SControl, data, size);
      // Write also on PCI config space data.
      memcpy(&pciConfigSpace.data[0xC8], &data, 4);
#ifdef HDD_DEBUG
      if (ataState.regs.SControl & 1)
        LOG_DEBUG(HDD, "[SCONTROL]: Resetting SATA link!");
#endif // HDD_DEBUG
      break;
    case ATA_REG_SACTIVE:
      memcpy(&ataState.regs.SActive, data, size);
      break;
    default:
      LOG_ERROR(HDD, "Unknown register {:#x} being written. Data {:#x}", regOffset, inData);
      break;
    }
  } else {
    // Control (DMA) registers
    const u8 regOffset = static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);
  
    switch (regOffset) {
    case ATA_REG_DMA_COMMAND:
      memcpy(&ataState.regs.dmaCommand, data, size);
      if (ataState.regs.dmaCommand & XE_ATAPI_DMA_ACTIVE) {
        ataState.regs.dmaStatus = XE_ATA_DMA_ACTIVE; // Signal DMA active status.
      }
      break;
    case ATA_REG_DMA_STATUS:
      memcpy(&ataState.regs.dmaStatus, data, size);
      break;
    case ATA_REG_DMA_TABLE_OFFSET:
      memcpy(&ataState.regs.dmaTableOffset, data, size);
      break;
    default:
      LOG_ERROR(HDD, "Unknown control register {:#x} being written. Byte count = {:#d}", regOffset, size);
      break;
    }
  }
}

void Xe::PCIDev::HDD::MemSet(u64 writeAddress, s32 data, u64 size) {
  const u32 regOffset = (writeAddress & 0xFF) * 4;
  LOG_ERROR(HDD, "Unknown register! Attempted to MEMSET {:#x}", regOffset);
}

// Config read.
void Xe::PCIDev::HDD::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}
// Config write.
void Xe::PCIDev::HDD::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  // Check if we're being scanned
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
        u64 x = 2;
        for (s32 idx = 2; idx < 31; idx++) {
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

//
// ATA Commands
//

void Xe::PCIDev::HDD::ataIdentifyDeviceCommand() {
  if (!ataState.dataOutBuffer.init(sizeof(ataState.ataIdentifyData), true)) {
    LOG_ERROR(HDD, "Failed to initialize data buffer for IDENTIFY_DEVICE command.");
  }

  ataState.dataOutBuffer.reset();

  if (ataState.imageAttached) {
    memcpy(ataState.dataOutBuffer.get(), &ataState.ataIdentifyData, sizeof(ataState.ataIdentifyData));
  } else {
    // Respond with zeros, XeLL just gets... stuck otherwise.
    memset(ataState.dataOutBuffer.get(), 0, sizeof(ataState.ataIdentifyData));
  }

  // Device ready, data request.
  ataState.regs.status |= ATA_STATUS_DRDY | ATA_STATUS_DRQ;
  ataState.regs.SActive = 0x40; // SActive to 0x40, SATA driver in xboxkrnl checks this.
}

// ATA READ DMA (LBA 28 Bit)
void Xe::PCIDev::HDD::ataReadDMACommand() {
  u64 offset = (static_cast<u64>(ataState.regs.lbaHigh) << 16) |
    (static_cast<u64>(ataState.regs.lbaMiddle) << 8) |
    (static_cast<u64>(ataState.regs.lbaLow));

  u32 sectorCount = ataState.regs.sectorCount;
  // If sector count = 0 then 256 logical sectors shall be transfered.
  if (sectorCount == 0) { sectorCount = 256; }

  // Image offset.
  offset = offset * ATA_SECTOR_SIZE;
  // Read count in bytes.
  sectorCount = sectorCount * ATA_SECTOR_SIZE;

  ataState.dataOutBuffer.init(sectorCount, false);
  ataState.dataOutBuffer.reset();
  ataState.mountedHDDImage->Read(offset, ataState.dataOutBuffer.get(), sectorCount);
}

// ATA READ NATIVE MAX ADDRESS EXT (LBA 48 Bit)
void Xe::PCIDev::HDD::ataReadNativeMaxAddressExtCommand() {
  // This command returns the native maximum LBA address of the disk drive.
  u64 lbaMaxAddress = ataState.ataIdentifyData.userAddressableSectors48Bit[0] |
    static_cast<u64>(ataState.ataIdentifyData.userAddressableSectors48Bit[1]) << 32;

  ataState.regs.lbaLow = lbaMaxAddress & 0xFF;
  ataState.regs.prevLBALow = (lbaMaxAddress >> 24) & 0xFF;
  ataState.regs.lbaMiddle = (lbaMaxAddress >> 8) & 0xFF;
  ataState.regs.prevLBAMiddle = (lbaMaxAddress >> 32) & 0xFF;
  ataState.regs.lbaHigh = (lbaMaxAddress >> 16) & 0xFF;
  ataState.regs.prevLBAHigh = (lbaMaxAddress >> 40) & 0xFF;

  ataState.regs.status = ATA_STATUS_DRDY;
}

// ATA READ DMA EXT (LBA 48 Bit)
void Xe::PCIDev::HDD::ataReadDMAExtCommand() {
  u64 offset = (static_cast<u64>(ataState.regs.prevLBAHigh) << 40) |
    (static_cast<u64>(ataState.regs.prevLBAMiddle) << 32) |
    (static_cast<u64>(ataState.regs.prevLBALow) << 24) |
    (static_cast<u64>(ataState.regs.lbaHigh) << 16) |
    (static_cast<u64>(ataState.regs.lbaMiddle) << 8) |
    (static_cast<u64>(ataState.regs.lbaLow));

  u32 sectorCount = (ataState.regs.prevSectorCount << 8) | ataState.regs.sectorCount;
  // If sector count = 0 then 65 536 logical sectors shall be transfered.
  if (sectorCount == 0) {
    sectorCount = 65536;
  }

  // Image offset.
  offset = offset * ATA_SECTOR_SIZE;
  // Read count in bytes.
  sectorCount = sectorCount * ATA_SECTOR_SIZE;

  ataState.dataOutBuffer.init(sectorCount, false);
  ataState.dataOutBuffer.reset();
  ataState.mountedHDDImage->Read(offset, ataState.dataOutBuffer.get(), sectorCount);
}

// ATA WRITE DMA (LBA 28 Bit)
void Xe::PCIDev::HDD::ataWriteDMACommand() {
  u64 offset = (ataState.regs.lbaHigh << 16) |
    (ataState.regs.lbaMiddle << 8) |
    (ataState.regs.lbaLow);

  u32 sectorCount = ataState.regs.sectorCount;
  // If sector count = 0 then 256 logical sectors shall be transfered.
  if (sectorCount == 0) {
    sectorCount = 256;
  }

  // Image offset.
  offset = offset * ATA_SECTOR_SIZE;
  // Read count in bytes.
  sectorCount = sectorCount * ATA_SECTOR_SIZE;

  ataState.mountedHDDImage->Write(offset, ataState.dataInBuffer.get(), sectorCount);
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
const std::string Xe::PCIDev::HDD::getATACommandName(u32 commandID) {
  auto it = ataCommandNameMap.find(commandID);
  if (it != ataCommandNameMap.end()) {
    return it->second;
  } else {
    LOG_ERROR(HDD, "Unknown Command: {:#x}", commandID);
    return "Unknown Command";
  }
}

// Worker thread for DMA.
void Xe::PCIDev::HDD::hddThreadLoop() {
  // Check if we should be running
  if (!hddThreadRunning)
    return;
  LOG_INFO(HDD, "Entered HDD worker thread.");
  while (hddThreadRunning) {
    // Check if we should exit early
    hddThreadRunning = XeRunning;
    if (!hddThreadRunning)
      break;
    // Check for the DMA active command.
    if (ataState.regs.dmaCommand & XE_ATA_DMA_ACTIVE) {
      // Start our DMA operation
      doDMA();
      // Change our DMA status after completion.
      ataState.regs.dmaCommand &= ~1; // Clear active status.
      ataState.regs.dmaStatus = XE_ATA_DMA_INTR; // Signal Interrupt.
    }
    // Sleep for some time.
    std::this_thread::sleep_for(5ms);
  }

  LOG_INFO(HDD, "Exiting HDD worker thread.");
}

// Performs the DMA operation until it reaches the end of the PRDT.
void Xe::PCIDev::HDD::doDMA() {
  for (;;) {
    // Read the first entry of the table in memory
    u8 *DMAPointer = ramPtr->GetPointerToAddress(ataState.regs.dmaTableOffset + ataState.dmaState.currentTableOffset);
    // Each entry is 64 bit long
    memcpy(&ataState.dmaState, DMAPointer, 8);

    // Store current position in the table
    ataState.dmaState.currentTableOffset += 8;

    // If this bit in the Command register is set we're facing a read operation
    bool readOperation = ataState.regs.dmaCommand & XE_ATAPI_DMA_WR;
    // This bit specifies that we're facing the last entry in the PRD Table
    bool lastEntry = ataState.dmaState.currentPRD.control & 0x8000;
    // The byte count to read/write
    u16 size = ataState.dmaState.currentPRD.sizeInBytes;
    // The address in memory to be written to/read from
    u32 bufferAddress = ataState.dmaState.currentPRD.physAddress;
    // Buffer Pointer in main memory
    u8 *bufferInMemory = ramPtr->GetPointerToAddress(bufferAddress);

    if (readOperation) {
      // Reading from us
      size = std::fmin(static_cast<u32>(size), ataState.dataOutBuffer.count());

      // Buffer overrun? Apparently there can be entries in the PRDT which have a zero byte count.
      if (size == 0) {
        LOG_WARNING(HDD, "[DMA Worker Read] Entry read size is zero.");
      }

      memcpy(bufferInMemory, ataState.dataOutBuffer.get(), size);
      ataState.dataOutBuffer.resize(size);
    } else {
      // Writing to us
      size = std::fmin(static_cast<u32>(size), ataState.dataInBuffer.count());
      // Buffer overrun? Apparently there can be entries in the PRDT which have a zero byte count.
      if (size == 0) {
        LOG_WARNING(HDD, "[DMA Worker Write] Entry write size is zero.");
      }
      memcpy(ataState.dataInBuffer.get(), bufferInMemory, size);
      ataState.dataInBuffer.resize(size);
    }
    if (lastEntry) {
      // Reset the current position
      ataState.dmaState.currentTableOffset = 0;
      // After completion we must raise an interrupt
      ataIssueInterrupt();
      return;
    }
  }
}

// Issues an interrupt to the XCPU.
void Xe::PCIDev::HDD::ataIssueInterrupt() {
  if ((ataState.regs.deviceControl & ATA_DEVICE_CONTROL_NIEN) == 0) {
    parentBus->RouteInterrupt(PRIO_SATA_HDD);
  }
}
