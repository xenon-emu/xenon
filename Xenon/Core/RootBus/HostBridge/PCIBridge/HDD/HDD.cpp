// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "HDD.h"

#include "Base/Logging/Log.h"

#define HDD_DEBUG

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
  data = 0x00000113;
  ataState.regs.SStatus = data;
  memcpy(&pciConfigSpace.data[0xC0], &data, 4); // SSTATUS_DET_COM_ESTABLISHED.
                                                // SSTATUS_SPD_GEN1_COM_SPEED.
                                                // SSTATUS_IPM_INTERFACE_ACTIVE_STATE.
  // SError.
  data = 0x001D0003;
  ataState.regs.SError = data;
  memcpy(&pciConfigSpace.data[0xC4], &data, 4);
  // SControl.
  data = 0x00000300;
  ataState.regs.SControl = data;
  memcpy(&pciConfigSpace.data[0xC8], &data, 4); // SCONTROL_IPM_ALL_PM_DISABLED.
  // SActive
  data = 0x00000040;
  ataState.regs.SActive = data;
  memcpy(&pciConfigSpace.data[0xCC], &data, 4);

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x20; // BAR0
  pciDevSizes[1] = 0x10; // BAR1

  // Assign our PCI Bridge pointer
  parentBus = parentPCIBridge;

  // Device ready to receive commands.
  ataState.regs.status = ATA_STATUS_DRDY;

  // Mount our HDD image according to config.
  ataState.mountedHDDImage = std::make_unique<STRIP_UNIQUE(ataState.mountedHDDImage)>(Config::filepaths.hddImage);
}

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

    switch (ataCommandReg)
    {
    case ATA_REG_DATA:
      memcpy(data, &ataState.regs.data, size);
      break;
    case ATA_REG_ERROR:
      memcpy(data, &ataState.regs.error, size);
      break;
    case ATA_REG_SECTORCOUNT:
      memcpy(data, &ataState.regs.sectorCount, size);
      break;
    case ATA_REG_LBA_LOW:
      memcpy(data, &ataState.regs.lbaLow, size);
      break;
    case ATA_REG_LBA_MED:
      memcpy(data, &ataState.regs.lbaMiddle, size);
      break;
    case ATA_REG_LBA_HI:
      memcpy(data, &ataState.regs.lbaHigh, size);
      break;
    case ATA_REG_DEV_SEL:
      memcpy(data, &ataState.regs.deviceSelect, size);
      break;
    case ATA_REG_STATUS:
      memcpy(data, &ataState.regs.status, size);
      break;
    case ATA_REG_ALT_STATUS:
      memcpy(data, &ataState.regs.altStatus, size);
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
    switch (ataControlReg)
    {
    default:
      LOG_ERROR(HDD, "Unknown control register {:#x} being read. Byte count = {:#d}", ataControlReg, size);
      break;
    }
  }
}

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

    switch (regOffset)
    {
    case ATA_REG_DATA:
      memcpy(&ataState.regs.data, data, size);
      break;
    case ATA_REG_FEATURES:
      memcpy(&ataState.regs.features, data, size);
      break;
    case ATA_REG_SECTORCOUNT:
      memcpy(&ataState.regs.sectorCount, data, size);
      break;
    case ATA_REG_LBA_LOW:
      memcpy(&ataState.regs.lbaLow, data, size);
      break;
    case ATA_REG_LBA_MED:
      memcpy(&ataState.regs.lbaMiddle, data, size);
      break;
    case ATA_REG_LBA_HI:
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

      switch (ataState.regs.command)
      {
      case ATA_COMMAND_SET_FEATURES:
        switch (ataState.regs.features)
        {
        case ATA_SF_SUBCOMMAND_SET_TRANSFER_MODE:
        {
          ATA_TRANSFER_MODE mode = static_cast<ATA_TRANSFER_MODE>(ataState.regs.sectorCount);
          switch (mode)
          {
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

          ataState.regs.ataTransferMode = inData;
        }
        break;
        default:
          LOG_ERROR(HDD, "[CMD]: Set features {:#x} subcommand unknown.", ataState.regs.features);
          break;
        }
        // Request interrupt.
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
      // Write also on PCI config space data.
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

      if (ataState.regs.SControl & 1)
        LOG_DEBUG(HDD, "[SCONTROL]: Resetting SATA link!");
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
  
    switch (regOffset)
    {
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

// Issues an interrupt to the XCPU.
void Xe::PCIDev::HDD::ataIssueInterrupt() {
  parentBus->RouteInterrupt(PRIO_SATA_HDD);
}

void Xe::PCIDev::HDD::ataCopyIdentifyDeviceData() {
  
}


//
// Utilities
//

static const std::unordered_map<u32, const std::string> ataCommandNameMap = {
    { 0x08, "DEVICE_RESET" },
    { 0x20, "READ_SECTORS" },
    { 0x25, "READ_DMA_EXT" },
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
  }
  else {
    LOG_ERROR(Xenos, "Unknown Command: {:#x}", commandID);
    return "Unknown Command";
  }
}
