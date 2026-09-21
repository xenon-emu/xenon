/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "ODD.h"

#include "Base/Config.h"
#include "Base/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <plusaes/plusaes.hpp>
#include <random>
#include <vector>

// Enables ODD Debug output
// #define ODD_DEBUG

#ifndef ODD_DEBUG
  #define DEBUGP(x, ...)
#else
  #define DEBUGP(x, ...) LOG_DEBUG(ODD, x, ##__VA_ARGS__);
#endif

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

// Data was pulled off of an PLDS DG-16D5S retail ODD.
const u8 identifyDataBytes[] = {
  0xC0, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x38, 0x44, 0x33, 0x31, 0x42, 0x42, 0x34, 0x32, 0x36, 0x36, 0x32, 0x31, 0x30, 0x30, 0x48, 0x36, 0x20, 0x4A,
  0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x31, 0x32, 0x33, 0x20, 0x20, 0x20, 0x20, 0x4C, 0x50, 0x53,
  0x44, 0x20, 0x20, 0x20, 0x20, 0x47, 0x44, 0x31, 0x2D, 0x44, 0x36, 0x53, 0x35, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x40, 0x00, 0x04, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x78, 0x00, 0x78,
  0x00, 0x78, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x10, 0x02, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const u8 atapiInquiryDataBytes[]
  = {0x05, 0x80, 0x00, 0x32, 0x5B, 0x00, 0x00, 0x00, 0x50, 0x4C, 0x44, 0x53, 0x20, 0x20, 0x20, 0x20, 0x44, 0x47,
     0x2D, 0x31, 0x36, 0x44, 0x35, 0x53, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x31, 0x35, 0x33, 0x32};

//
// Device lifecycle and MMIO
//

Xe::PCIDev::ODD::ODD(const char* deviceName, u64 size, PCIBridge* parentPCIBridge, RAM* ram)
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

  // Reserve the transfer buffers up front. Every command sizes them to its own transfer, so this only avoids an
  // allocation on the first one.
  atapiState.dataInBuffer.Begin(ATAPI_CDROM_SECTOR_SIZE);
  atapiState.dataOutBuffer.Begin(ATAPI_CDROM_SECTOR_SIZE);

  // Set our identify data.
  memcpy(&atapiState.atapiIdentifyData, identifyDataBytes, sizeof(atapiState.atapiIdentifyData));

  // Set our inquiry data.
  memcpy(&atapiState.atapiInquiryData, atapiInquiryDataBytes, sizeof(atapiState.atapiInquiryData));

  atapiState.mountedODDImage = std::make_unique<STRIP_UNIQUE(atapiState.mountedODDImage)>(Config::filepaths.oddImage);

  if (atapiState.mountedODDImage.get()->isHandleValid()) {
    if (fs::exists(Config::filepaths.oddImage)) {
      try {
        std::error_code fsError;
        u64 fileSize = fs::file_size(Config::filepaths.oddImage, fsError);
        if (fileSize != -1 && fileSize) {
          // File is valid
          atapiState.imageAttached = true;
        } else {
          fileSize = 0;
          if (fsError) { LOG_ERROR(ODD, "Filesystem error: {} ({})", fsError.message(), fsError.value()); }
        }
      } catch (const std::exception& ex) {
        LOG_ERROR(ODD, "Exception trying to check if image is valid. {}", ex.what());
        atapiState.imageAttached = false;
      }
    }
  }

  if (!atapiState.imageAttached) { LOG_INFO(ODD, "No ODD image found - disabling device."); }

  oddThreadRunning = atapiState.imageAttached;

  // Set the SCR's at offset 0xC0 (SiS-like)
  // SStatus
  data = atapiState.imageAttached ? 0x00000113 : 0;
  atapiState.regs.SStatus = data;
  memcpy(&pciConfigSpace.data[0xC0], &data, 4); // SSTATUS_DET_COM_ESTABLISHED.
                                                // SSTATUS_SPD_GEN1_COM_SPEED.
                                                // SSTATUS_IPM_INTERFACE_ACTIVE_STATE
  // SError - Initialize with no errors (bits are set by hardware when errors occur)
  data = 0x00000000;
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

  // Locate Game partition if the image is an XGD image and get our DVD key.
  locateGamePartition();
  loadDvdKey();

  // A disc that is already mounted at construction is presented as inserted and spun up (Ready). Only a runtime
  // InsertDisc() drives the spin-up sequence. With no image the drive reports an empty tray, which the kernel maps to
  // STATUS_NO_MEDIA_IN_DEVICE.
  atapiState.mediaState = atapiState.imageAttached ? MediaState::Ready : MediaState::NoMedia;

  // Load the per-disc security data (DVDX2 authentication page + control data) used by the disc authentication path, if
  // it sits next to the image.
  loadDiscSecurityData();

  // Enter ODD Worker Thread
  oddWorkerThread = std::thread(&Xe::PCIDev::ODD::oddThreadLoop, this);
}

// MMIO Read
void Xe::PCIDev::ODD::Read(u64 readAddress, u8* data, u64 size) {
  bool shouldInterrupt = false;

  // PCI BAR0 is the Primary Command Block Base Address
  u8 atapiCommandReg = static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the Primary Control Block Base Address
  u8 atapiControlReg = static_cast<u8>(readAddress - pciConfigSpace.configSpaceHeader.BAR1);

  DEBUGP("[Read]: Reg {}, address {:#x}", getATAPIRegisterName(readAddress & 0xFF), readAddress);

  {
    std::lock_guard lock(oddMutex);

    // Command Registers
    if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 - pciConfigSpace.configSpaceHeader.BAR0)) {

      switch (atapiCommandReg) {
        case ATA_REG_DATA: {
          // PIO data-in. The register is a single dword, so a read never takes
          // more than that out of the transfer buffer.
          const u32 chunk = static_cast<u32>(std::min<u64>(size, sizeof(atapiState.regs.data)));
          if (!atapiState.dataOutBuffer.Empty()) {
            // Zero first: a short tail then reads back as zero padding rather than as whatever the register happened to
            // hold last.
            atapiState.regs.data = 0;
            atapiState.dataOutBuffer.Take(&atapiState.regs.data, chunk);
            // Only clear DRQ once the entire transfer has been consumed.
            if (atapiState.dataOutBuffer.Empty()) {
              atapiState.regs.status &= ~ATA_STATUS_DRQ; // Clear DRQ.
              atapiState.regs.status |= ATA_STATUS_DRDY; // Device ready.
              // Signal transfer completion.
              atapiState.regs.interruptReason |= ATA_INTERRUPT_REASON_IO | ATA_INTERRUPT_REASON_CD;
              shouldInterrupt = true;
            }
          }
          memcpy(data, &atapiState.regs.data, chunk);
        } break;
        case ATAPI_REG_ERROR:
          memcpy(data, &atapiState.regs.error, size);
          // Clear the error status on the status register
          atapiState.regs.status &= ~ATA_STATUS_ERR_CHK;
          // Reset the error register
          atapiState.regs.error = 0;
          return;
        case ATAPI_REG_INT_REAS: memcpy(data, &atapiState.regs.interruptReason, size); return;
        case ATAPI_REG_LBA_LOW: memcpy(data, &atapiState.regs.lbaLow, size); return;
        case ATAPI_REG_BYTE_COUNT_LOW: memcpy(data, &atapiState.regs.byteCountLow, size); return;
        case ATAPI_REG_BYTE_COUNT_HIGH: memcpy(data, &atapiState.regs.byteCountHigh, size); return;
        case ATAPI_REG_DEVICE: memcpy(data, &atapiState.regs.deviceSelect, size); return;
        case ATAPI_REG_STATUS: memcpy(data, &atapiState.regs.status, size); break;
        case ATAPI_REG_ALTERNATE_STATUS: memcpy(data, &atapiState.regs.status, size); break;
        case ATA_REG_SSTATUS: memcpy(data, &atapiState.regs.SStatus, size); break;
        case ATA_REG_SERROR: memcpy(data, &atapiState.regs.SError, size); break;
        case ATA_REG_SCONTROL: memcpy(data, &atapiState.regs.SControl, size); break;
        case ATA_REG_SACTIVE: memcpy(data, &atapiState.regs.SActive, size); break;
        default:
          LOG_ERROR(ODD, "Unknown Command Register Block register being read, command code = 0x{:X}", atapiCommandReg);
          break;
      }
    } else { // Control (DMA) registers
      switch (atapiControlReg) {
        case ATAPI_DMA_REG_COMMAND: memcpy(data, &atapiState.regs.dmaCommand, size); break;
        case ATAPI_DMA_REG_STATUS: memcpy(data, &atapiState.regs.dmaStatus, size); break;
        case ATAPI_DMA_REG_TABLE_OFFSET: memcpy(data, &atapiState.regs.dmaTableOffset, size); break;
        default:
          LOG_ERROR(ODD, "Unknown Control Register Block register being read, command code = 0x{:X}", atapiControlReg);
          break;
      }
    }
  }

  // Route interrupts outside of lock to avoid lock contention.
  if (shouldInterrupt) { atapiIssueInterrupt(); }
  // Cancel interrupt must also be outside lock.
  if (atapiCommandReg == ATAPI_REG_STATUS) { parentBus->CancelInterrupt(PRIO_SATA_ODD); }
}

// MMIO Write
void Xe::PCIDev::ODD::Write(u64 writeAddress, const u8* data, u64 size) {
  bool shouldInterrupt = false;

  // PCI BAR0 is the Primary Command Block Base Address
  u8 atapiCommandReg = static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the Primary Control Block Base Address
  u8 atapiControlReg = static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);

  u32 inData = 0;
  memcpy(&inData, data, size);

  std::lock_guard lock(oddMutex);

  DEBUGP("[Write]: Reg {}, address {:#x}, data {:#x}, byte count {:#d}", getATAPIRegisterName(writeAddress & 0xFF),
         writeAddress, inData, size);

  // Command Registers
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 - pciConfigSpace.configSpaceHeader.BAR0)) {

    switch (atapiCommandReg) {
      case ATAPI_REG_DATA: {
        // PIO data-out. This register carries two different things: the 12-byte CDB that follows a PACKET command, and
        // the parameter list of a command that takes one (MODE SELECT(10)). The input buffer was sized for whichever is
        // in flight, so appending is all that is needed here.
        atapiState.regs.status &= ~ATA_STATUS_DRQ;

        memcpy(&atapiState.regs.data, data, std::min<u64>(size, sizeof(atapiState.regs.data)));
        atapiState.dataInBuffer.Put(data, static_cast<u32>(size));

        if (ap20.selectPending) {
          // Parameter list of a MODE SELECT(10) arriving over PIO rather than DMA. Once it is complete it is dispatched
          // exactly as the DMA path dispatches it.
          if (atapiState.dataInBuffer.Empty()) { consumeModeSelectData(); }
        } else if (packetCdbPending && atapiState.dataInBuffer.Empty()) {
          // A whole CDB: hand it to the worker thread, once.
          packetCdbPending = false;
          atapiState.scsiCommandPending.store(true, std::memory_order_release);
        }
        return;
      } break;
      case ATAPI_REG_FEATURES: memcpy(&atapiState.regs.features, data, size); return;
      case ATAPI_REG_SECTOR_COUNT: memcpy(&atapiState.regs.sectorCount, data, size); return;
      case ATAPI_REG_LBA_LOW: memcpy(&atapiState.regs.lbaLow, data, size); return;
      case ATAPI_REG_BYTE_COUNT_LOW: memcpy(&atapiState.regs.byteCountLow, data, size); return;
      case ATAPI_REG_BYTE_COUNT_HIGH: memcpy(&atapiState.regs.byteCountHigh, data, size); return;
      case ATAPI_REG_DEVICE: memcpy(&atapiState.regs.deviceSelect, data, size); return;
      case ATAPI_REG_COMMAND:
        memcpy(&atapiState.regs.command, data, size);
        // Reset the status & error register
        atapiState.regs.status &= ~ATA_STATUS_ERR_CHK;
        atapiState.regs.error &= ~ATA_ERROR_ABRT;

        DEBUGP("ATAPI Command received: {}", getATACommandName(atapiState.regs.command));

        switch (atapiState.regs.command) {
          case ATA_COMMAND_PACKET: {
            atapiState.regs.status |= ATA_STATUS_DRQ;
            // Size the input buffer for exactly one CDB. Sizing it here rather than leaving whatever the previous
            // command left behind is what keeps a short parameter list from truncating the next CDB.
            ap20.selectPending = false;
            packetCdbPending = true;
            atapiState.dataInBuffer.Begin(XE_ATAPI_CDB_SIZE);
            return;
          } break;
          case ATA_COMMAND_IDENTIFY_PACKET_DEVICE: {
            atapiIdentifyPacketDeviceCommand();
            shouldInterrupt = true;
          } break;
          case ATA_COMMAND_IDENTIFY_DEVICE: {
            atapiIdentifyCommand();
            shouldInterrupt = true;
          } break;
          case ATA_COMMAND_STANDBY_IMMEDIATE:
          case ATA_COMMAND_IDLE_IMMEDIATE:
            atapiState.regs.error = 0;
            atapiState.regs.status = ATA_STATUS_DRDY;
            atapiState.regs.interruptReason &= ~7;
            atapiState.regs.interruptReason |= ATA_INTERRUPT_REASON_CD | ATA_INTERRUPT_REASON_IO;
            sscState.initialized = true;
            shouldInterrupt = true;
            break;
          case ATA_COMMAND_SET_FEATURES:
            switch (atapiState.regs.features) {
              case ATA_SF_SUBCOMMAND_SET_TRANSFER_MODE: {
                ATA_TRANSFER_MODE mode = static_cast<ATA_TRANSFER_MODE>(atapiState.regs.sectorCount);
                switch (mode) {
                  case ATA_TRANSFER_MODE::PIO: DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO"); break;
                  case ATA_TRANSFER_MODE::PIO_NO_IORDY:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_NO_IORDY");
                    break;
                  case ATA_TRANSFER_MODE::PIO_FLOW_CONTROL_MODE3:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_FLOW_CONTROL_MODE3");
                    break;
                  case ATA_TRANSFER_MODE::PIO_FLOW_CONTROL_MODE4:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to PIO_FLOW_CONTROL_MODE4");
                    break;
                  case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE0:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE0");
                    break;
                  case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE1:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE1");
                    break;
                  case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE2:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE2");
                    break;
                  case ATA_TRANSFER_MODE::MULTIWORD_DMA_MODE3:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to MULTIWORD_DMA_MODE3");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE0:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE0");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE1:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE1");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE2:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE2");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE3:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE3");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE4:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE4");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE5:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE5");
                    break;
                  case ATA_TRANSFER_MODE::ULTRA_DMA_MODE6:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to ULTRA_DMA_MODE6");
                    break;
                  default:
                    DEBUGP("[CMD](SET_TRANSFER_MODE): Setting transfer mode to {:#x}", atapiState.regs.sectorCount);
                    break;
                }
                atapiState.regs.ataTransferMode = inData;
              }
                // Request interrupt (will be routed outside lock)
                shouldInterrupt = true;
            }
            break;
          default: {
            LOG_ERROR(ODD, "Unknown command, command code = 0x{:X}", atapiState.regs.command);
          } break;
        }
        break;
      case ATAPI_REG_DEVICE_CONTROL: {
        memcpy(&atapiState.regs.deviceControl, data, size);
        return;
      } break;
      case ATA_REG_SSTATUS:
        memcpy(&atapiState.regs.SStatus, data, size);
        // Write also on PCI config space data
        memcpy(&pciConfigSpace.data[0xC0], data, 4);
        return;
      case ATA_REG_SERROR:
        memcpy(&atapiState.regs.SError, data, size);
        // Write also on PCI config space data.
        memcpy(&pciConfigSpace.data[0xC4], data, 4);
        return;
      case ATA_REG_SCONTROL:
        memcpy(&atapiState.regs.SControl, data, size);
        // Write also on PCI config space data.
        memcpy(&pciConfigSpace.data[0xC8], data, 4);
        if (atapiState.regs.SControl & 1) DEBUGP("[SCONTROL]: Resetting SATA link!");
        return;
      case ATA_REG_SACTIVE: memcpy(&atapiState.regs.SActive, data, size); return;
      default: {
        u64 tmp = 0;
        memcpy(&tmp, data, size);
        LOG_ERROR(ODD,
                  "Unknown Command Register Block register being written, command reg = 0x{:X}"
                  ", write address = 0x{:X}, data = 0x{:X}",
                  atapiCommandReg, writeAddress, tmp);
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
      case ATAPI_DMA_REG_STATUS: memcpy(&atapiState.regs.dmaStatus, data, size); break;
      case ATAPI_DMA_REG_TABLE_OFFSET: memcpy(&atapiState.regs.dmaTableOffset, data, size); break;
      default:
        LOG_ERROR(ODD, "Unknown Control Register Block register being written, command code = 0x{:X}", atapiControlReg);
        break;
    }
  }

  // Route interrupts outside of lock to avoid lock contention.
  if (shouldInterrupt) { atapiIssueInterrupt(); }
}

// MMIO Memset
void Xe::PCIDev::ODD::MemSet(u64 writeAddress, s32 data, u64 size) {
  std::lock_guard lock(oddMutex);
  // PCI BAR0 is the primary command block base address
  u8 atapiCommandReg = static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR0);

  // PCI BAR1 is the primary command block base address
  u8 atapiControlReg = static_cast<u8>(writeAddress - pciConfigSpace.configSpaceHeader.BAR1);

  // Who are we writing to?
  if (atapiCommandReg < (pciConfigSpace.configSpaceHeader.BAR1 - pciConfigSpace.configSpaceHeader.BAR0)) {
    // Command Registers
    switch (atapiCommandReg) {
      case ATAPI_REG_DATA: {
        // Same path as Write(), with a repeated byte instead of a buffer.
        atapiState.regs.status &= ~ATA_STATUS_DRQ;

        memset(&atapiState.regs.data, data, std::min<u64>(size, sizeof(atapiState.regs.data)));
        atapiState.dataInBuffer.PutValue(data, static_cast<u32>(size));

        if (ap20.selectPending) {
          if (atapiState.dataInBuffer.Empty()) { consumeModeSelectData(); }
        } else if (packetCdbPending && atapiState.dataInBuffer.Empty()) {
          packetCdbPending = false;
          atapiState.scsiCommandPending.store(true, std::memory_order_release);
        }
        return;
      } break;
      case ATAPI_REG_FEATURES: memset(&atapiState.regs.features, data, size); return;
      case ATAPI_REG_SECTOR_COUNT: memset(&atapiState.regs.sectorCount, data, size); return;
      case ATAPI_REG_LBA_LOW: memset(&atapiState.regs.lbaLow, data, size); return;
      case ATAPI_REG_BYTE_COUNT_LOW: memset(&atapiState.regs.byteCountLow, data, size); return;
      case ATAPI_REG_BYTE_COUNT_HIGH: memset(&atapiState.regs.byteCountHigh, data, size); return;
      case ATAPI_REG_DEVICE: memset(&atapiState.regs.deviceSelect, data, size); return;
      case ATAPI_REG_COMMAND:
        memset(&atapiState.regs.command, data, size);

        // Reset the status register
        atapiState.regs.status &= ~ATA_STATUS_ERR_CHK;

        // Reset the error register
        atapiState.regs.error &= ~ATA_ERROR_ABRT;

        switch (atapiState.regs.command) {
          case ATA_COMMAND_PACKET: {
            atapiState.regs.status |= ATA_STATUS_DRQ;
            ap20.selectPending = false;
            packetCdbPending = true;
            atapiState.dataInBuffer.Begin(XE_ATAPI_CDB_SIZE);
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
          case ATA_COMMAND_STANDBY_IMMEDIATE:
          case ATA_COMMAND_IDLE_IMMEDIATE:
            atapiState.regs.error = 0;
            atapiState.regs.status = ATA_STATUS_DRDY;
            atapiState.regs.interruptReason &= ~7;
            atapiState.regs.interruptReason |= ATA_INTERRUPT_REASON_CD | ATA_INTERRUPT_REASON_IO;
            sscState.initialized = true;
            return;
          default: LOG_ERROR(ODD, "Unknown command, command code = 0x{:X}", atapiState.regs.command); break;
        }
        return;
      case ATAPI_REG_DEVICE_CONTROL: memset(&atapiState.regs.deviceControl, data, size); return;
      default:
        u64 tmp = 0;
        memset(&tmp, data, size);
        LOG_ERROR(ODD,
                  "Unknown Command Register Block register being written, command reg = 0x{:X}"
                  ", write address = 0x{:X}, data = 0x{:X}",
                  atapiCommandReg, writeAddress, tmp);
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
      case ATAPI_DMA_REG_STATUS: memset(&atapiState.regs.dmaStatus, data, size); break;
      case ATAPI_DMA_REG_TABLE_OFFSET: memset(&atapiState.regs.dmaTableOffset, data, size); break;
      default:
        LOG_ERROR(ODD, "Unknown Control Register Block register being written, command code = 0x{:X}", atapiControlReg);
        break;
    }
  }
}

// PCI Config read.
void Xe::PCIDev::ODD::ConfigRead(u64 readAddress, u8* data, u64 size) {
  const u8 readReg = static_cast<u8>(readAddress);
  if (readReg >= XE_SIS_SCR_BASE && readReg <= 0xFF) {
    // Read the SATA status and control registers
    switch ((readReg - XE_SIS_SCR_BASE) / 4) {
      case SCR_STATUS_REG: LOG_WARNING(ODD, "SCR ConfigRead to SCR_STATUS_REG."); break;
      case SCR_ERROR_REG: LOG_WARNING(ODD, "SCR ConfigRead to SCR_ERROR_REG."); break;
      case SCR_CONTROL_REG: LOG_WARNING(ODD, "SCR ConfigRead to SCR_CONTROL_REG."); break;
      case SCR_ACTIVE_REG: LOG_WARNING(ODD, "SCR ConfigRead to SCR_ACTIVE_REG."); break;
      case SCR_NOTIFICATION_REG: LOG_WARNING(ODD, "SCR ConfigRead to SCR_NOTIFICATION_REG."); break;
      default: LOG_ERROR(ODD, "SCR ConfigRead to reg 0x{:X}", readReg * 4); break;
    }
  }
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
  DEBUGP("ConfigRead to reg 0x{:X}", readReg * 4);
}

// PCI Config write.
void Xe::PCIDev::ODD::ConfigWrite(u64 writeAddress, const u8* data, u64 size) {
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
          if (x >= pciDevSizes[regOffset]) { break; }
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
      tmp = 0;                                   // Register not implemented
    }
  }

  u8 writeReg = static_cast<u8>(writeAddress);
  if (writeReg >= XE_SIS_SCR_BASE && writeReg <= 0xFF) {
    // Write to the SATA status and control registers
    switch ((writeReg - XE_SIS_SCR_BASE) / 4) {
      case SCR_STATUS_REG: LOG_WARNING(ODD, "SCR ConfigWrite to SCR_STATUS_REG, data 0x{:X}", tmp); break;
      case SCR_ERROR_REG: LOG_WARNING(ODD, "SCR ConfigWrite to SCR_ERROR_REG, data 0x{:X}", tmp); break;
      case SCR_CONTROL_REG: LOG_WARNING(ODD, "SCR ConfigWrite to SCR_CONTROL_REG, data 0x{:X}", tmp); break;
      case SCR_ACTIVE_REG: LOG_WARNING(ODD, "SCR ConfigWrite to SCR_ACTIVE_REG, data 0x{:X}", tmp); break;
      case SCR_NOTIFICATION_REG: LOG_WARNING(ODD, "SCR ConfigRead to SCR_NOTIFICATION_REG, data 0x{:X}", tmp); break;
      default: LOG_ERROR(ODD, "SCR ConfigWrite to reg 0x{:X}, data 0x{:X}", writeReg * 4, tmp); break;
    }
  }
  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
  DEBUGP("ConfigWrite to reg 0x{:X}, data 0x{:X}", writeReg * 4, tmp);
}

//
// Worker thread, DMA and interrupts
//

// Worker thread for DMA.
void Xe::PCIDev::ODD::oddThreadLoop() {
  // Check if we should be running.
  if (!oddThreadRunning.load()) return;
  LOG_INFO(ODD, "Entered ODD worker thread.");
  while (oddThreadRunning.load()) {
    // Check if we should exit early.
    oddThreadRunning.store(XeRunning);
    if (!oddThreadRunning.load()) break;

    bool shouldInterruptDMA = false;
    bool shouldInterruptSCSI = false;
    bool seekDelay = false;

    // DMA and SCSI processing under lock.
    {
      std::lock_guard lock(oddMutex);

      // Check for the DMA active command, and only start the DMA engine if there's not any pending SCSI command for
      // processing.
      if (atapiState.regs.dmaCommand & XE_ATA_DMA_ACTIVE
          && !atapiState.scsiCommandPending.load(std::memory_order_acquire)) {

        // A command that ended in CHECK CONDITION has no data phase, and its ERR/CHK bit has to survive.
        const bool commandFailed = (atapiState.regs.status & ATA_STATUS_ERR_CHK) != 0;

        if (commandFailed) {
          DEBUGP("DMA requested for a command that ended in CHECK CONDITION - completing with the error intact "
                 "and no data transfer.");
          // Nothing arrived, so there is no parameter list to consume.
          ap20.selectPending = false;
        } else {
          DEBUGP("Started DMA Operation. Direction : {}",
                 (atapiState.regs.dmaCommand & XE_ATAPI_DMA_WR ? "Out" : "In"));
          doDMA();
          // A MODE SELECT(10) parameter list has just landed in the input buffer. This is a no-op for every other
          // command. It has to run before the completion registers are written below, because acting on the parameter
          // list can fail the command.
          consumeModeSelectData();
        }

        // Re read the verdict, consuming the parameter list may have failed a command whose data phase itself went
        // through without incident.
        const bool completedWithError = (atapiState.regs.status & ATA_STATUS_ERR_CHK) != 0;

        // Change our DMA status after completion.
        atapiState.regs.dmaCommand &= ~XE_ATA_DMA_ACTIVE; // Clear active status.
        atapiState.regs.dmaStatus = XE_ATA_DMA_INTR | (completedWithError ? XE_ATA_DMA_ERR : 0);
        atapiState.regs.SActive = 0x40;
        // On success clear BSY and report ready, on failure leave DRDY|ERR_CHK and the error register exactly as the
        // command left them.
        if (!completedWithError) { atapiState.regs.status = ATA_STATUS_DRDY; }
        atapiState.regs.interruptReason = ATA_INTERRUPT_REASON_IO | ATA_INTERRUPT_REASON_CD;

        // After completion we must raise an interrupt.
        shouldInterruptDMA = true;
      }

      // Check for pending SCSI commands.
      if (atapiState.scsiCommandPending.load(std::memory_order_acquire)) {
        processSCSICommand();

        // Check if we need to issue an interrupt.
        // If DMA is active, the interrupt will be issued by the DMA engine.
        // If DMA is not active, we need to issue the interrupt ourselves.
        // Note: check for bit 0 of features register (DMA bit)
        if (!(atapiState.regs.features & 1)) { shouldInterruptSCSI = true; }

        atapiState.scsiCommandPending.store(false, std::memory_order_release);
      }
      // Taken out under the lock, acted on outside it.
      seekDelay = challengeNeedsSeekDelay;
      challengeNeedsSeekDelay = false;
    } // End lock scope

    // A disc challenge has to take as long as a real drive's seek, because the kernel times this command and the
    // hypervisor counts how many were slow enough. The wait goes here, outside the lock and before the interrupt, so
    // the guest can still reach the registers and so it lands inside the window SataCdRomSendAP20Select is measuring.
    if (seekDelay) {
      DEBUGP("Disc challenge: waiting {} ms before completing the MODE SELECT(10).", CHALLENGE_SEEK::DELAY_MS);
      std::this_thread::sleep_for(std::chrono::milliseconds(CHALLENGE_SEEK::DELAY_MS));
    }

    // Route interrupts outside of lock to avoid lock contention.
    if (shouldInterruptDMA || shouldInterruptSCSI) { atapiIssueInterrupt(); }

    // Sleep for some time.
    std::this_thread::sleep_for(50ns);
  }

  LOG_INFO(ODD, "Exiting ODD worker thread.");
}

// Performs the DMA operation until it reaches the end of the PRDT.
void Xe::PCIDev::ODD::doDMA() {
  for (;;) {
    // Read the first entry of the table in memory
    u8* DMAPointer
      = ramPtr->GetPointerToAddress(atapiState.regs.dmaTableOffset + atapiState.dmaState.currentTableOffset);
    // Each entry is 64 bit long
    memcpy(&atapiState.dmaState, DMAPointer, 8);

    // Store current position in the table
    atapiState.dmaState.currentTableOffset += 8;

    // If this bit in the Command register is set we're facing a read operation
    bool readOperation = atapiState.regs.dmaCommand & XE_ATAPI_DMA_WR;
    // This bit specifies that we're facing the last entry in the PRD Table
    bool lastEntry = atapiState.dmaState.currentPRD.control & 0x8000;
    // The byte count to read/write
    u32 size = atapiState.dmaState.currentPRD.sizeInBytes;
    // The address in memory to be written to/read from
    u32 bufferAddress = atapiState.dmaState.currentPRD.physAddress;
    // Buffer Pointer in main memory
    u8* bufferInMemory = ramPtr->GetPointerToAddress(bufferAddress);
    // ATA DMA Spec states then the host will write a size of 0 to request 64K of data.
    if (size == 0) { size = 65536; }

    DEBUGP("DMA {} Operation: {} bytes at {:#x}, curTableOffset = {:#x}, lastEntry = {}", readOperation ? "OUT" : "IN",
           size, bufferAddress, atapiState.dmaState.currentTableOffset, lastEntry ? "True" : "False");

    if (readOperation) {
      // Device to host: hand over what is left of the data-in transfer.
      size = atapiState.dataOutBuffer.Take(bufferInMemory, size);
    } else {
      // Host to device: append to the data-out transfer.
      size = atapiState.dataInBuffer.Put(bufferInMemory, size);
    }
    if (lastEntry) {
      // Reset the current position
      atapiState.dmaState.currentTableOffset = 0;
      return;
    }
  }
}

// Issues an interrupt to the XCPU.
void Xe::PCIDev::ODD::atapiIssueInterrupt() {
  if ((atapiState.regs.deviceControl & ATA_DEVICE_CONTROL_NIEN) == 0) {
    DEBUGP("Issuing interrupt.");
    parentBus->RouteInterrupt(PRIO_SATA_ODD);
  }
}

// Processes SCSI commands.
void Xe::PCIDev::ODD::processSCSICommand() {
  // The CDB was written into the input buffer during the packet phase. XE_CDB is 16 bytes wide but the Xenon ODD CDB is
  // 12, so clear first and copy only what actually arrived.
  memset(&atapiState.scsiCBD, 0, sizeof(atapiState.scsiCBD));
  atapiState.dataInBuffer.Rewind();
  atapiState.dataInBuffer.Take(atapiState.scsiCBD.AsByte, XE_ATAPI_CDB_SIZE);

  const u8 commandID = atapiState.scsiCBD.AsByte[0];

  DEBUGP("SCSI Command received: {}", getSCSICommandName(commandID));

  switch (commandID) {
    case SCSIOP_TEST_UNIT_READY: scsiTestUnitReadyCommand(); break;
    case SCSIOP_REQUEST_SENSE: scsiRequestSenseCommand(); break;
    case SCSIOP_INQUIRY: scsiInquiryCommand(); break;
    case SCSIOP_START_STOP: scsiStartStopUnitCommand(); break;
    case SCSIOP_TOGGLE_LOCK: scsiPreventAllowRemovalCommand(); break;
    case SCSIOP_READ_CAPACITY: scsiReadCapacityCommand(); break;
    case SCSIOP_READ10: scsiRead10Command(); break;
    case SCSIOP_READ_TOC: scsiReadTocCommand(); break;
    case SCSIOP_GET_CONFIG: scsiGetConfigurationCommand(); break;
    case SCSIOP_EVENT_INFO: scsiGetEventStatusNotificationCommand(); break;
    case SCSIOP_MODE_SENSE6: scsiModeSense6Command(); break;
    case SCSIOP_MODE_SELECT10: scsiModeSelect10Command(); break;
    case SCSIOP_MODE_SENSE10: scsiModeSense10Command(); break;
    case SCSIOP_READ_DVD_S: scsiReadDvdStructureCommand(); break;
    case SCSIOP_SET_CD_SPEED: scsiSetCdSpeedCommand(); break;
    default:
      LOG_WARNING(ODD, "Unsupported SCSI command: 0x{:X}", commandID);
      failWithSense(0x05, 0x20, 0x00); // ILLEGAL REQUEST / invalid command opcode.
      break;
  }
}

//
// Command completion helpers
//

// Does a basic setup of registers for an ATAPI command that has no outputs/errors.
void Xe::PCIDev::ODD::atapiNopCommand() {
  atapiState.regs.error = 0;
  atapiState.regs.status = ATA_STATUS_DRDY;
  atapiState.regs.interruptReason &= ~7;
  atapiState.regs.interruptReason |= ATA_INTERRUPT_REASON_CD | ATA_INTERRUPT_REASON_IO;
}

// Register tail every data in command shares.
void Xe::PCIDev::ODD::completeDataIn(u32 transferSize) {
  atapiState.regs.error = 0;
  // Interrupt reason: data (C/D = 0) travelling to the host (I/O = 1).
  atapiState.regs.interruptReason |= ATA_INTERRUPT_REASON_IO;
  atapiState.regs.interruptReason &= ~ATA_INTERRUPT_REASON_CD;
  // The initiator sizes the transfer from the byte count registers.
  atapiState.regs.byteCountLow = transferSize & 0xFF;
  atapiState.regs.byteCountHigh = (transferSize >> 8) & 0xFF;

  if (atapiState.regs.features & IDE_FEATURE_DMA) {
    atapiState.regs.status = ATA_STATUS_BSY | ATA_STATUS_DRDY; // BSY set, DRQ cleared for DMA.
  } else {
    atapiState.regs.status = ATA_STATUS_DRDY | ATA_STATUS_DRQ;
  }
}

// Serve a fixed response as the data in phase of the current command.
void Xe::PCIDev::ODD::scsiDataIn(const void* response, u32 length, u32 allocLen) {
  // SCSI: never hand back more than the initiator asked for.
  const u32 transferSize = std::min(length, allocLen);
  if (!atapiState.dataOutBuffer.Fill(response, transferSize)) {
    LOG_ERROR(ODD, "Failed to allocate a {} byte data-in buffer.", transferSize);
    failWithSense(0x04, 0x44, 0x00); // HARDWARE ERROR / internal target failure.
    return;
  }
  completeDataIn(transferSize);
}

// Queue the sense the next REQUEST SENSE will report. For a non-zero key this is a CHECK CONDITION: the ERR/CHK bit is
// set on the current command and the initiator is expected to follow up with REQUEST SENSE.
void Xe::PCIDev::ODD::setPendingSense(u8 key, u8 asc, u8 ascq) { atapiState.pendingSense = {key, asc, ascq}; }

void Xe::PCIDev::ODD::failWithSense(u8 key, u8 asc, u8 ascq) {
  setPendingSense(key, asc, ascq);
  atapiState.regs.error = ATA_ERROR_ABRT;
  atapiState.regs.status = ATA_STATUS_DRDY | ATA_STATUS_ERR_CHK;
  atapiState.regs.interruptReason = ATA_INTERRUPT_REASON_IO | ATA_INTERRUPT_REASON_CD;
}

//
// ATA commands
//

void Xe::PCIDev::ODD::atapiIdentifyCommand() {
  // Used by software to decide whether the device is an ATA or ATAPI device.

  // Set the drive status.
  atapiState.regs.status = ATA_STATUS_ERR_CHK | ATA_STATUS_DRDY;

  atapiState.regs.error = ATA_ERROR_ABRT;
  atapiState.regs.interruptReason = 0x1;
  atapiState.regs.lbaLow = 0x1;
  atapiState.regs.byteCountLow = 0x14;
  atapiState.regs.byteCountHigh = 0xEB;

  // Set interrupt reason (OR with existing ATAPI signature).
  atapiState.regs.interruptReason |= ATA_INTERRUPT_REASON_IO;
}

void Xe::PCIDev::ODD::atapiIdentifyPacketDeviceCommand() {
  if (!atapiState.dataOutBuffer.Fill(&atapiState.atapiIdentifyData, sizeof(XE_ATAPI_IDENTIFY_DATA))) {
    LOG_ERROR(ODD, "Failed to initialize data buffer for atapiIdentifyPacketDeviceCommand");
    return;
  }

  // Set the transfer size:
  // bytecount = LBA High << 8 | LBA Mid
  constexpr size_t dataSize = sizeof(XE_ATAPI_IDENTIFY_DATA);

  atapiState.regs.lbaLow = 1;
  atapiState.regs.byteCountLow = dataSize & 0xFF;
  atapiState.regs.byteCountHigh = (dataSize >> 8) & 0xFF;

  // Set the drive status.
  atapiState.regs.status = ATA_STATUS_DRDY | ATA_STATUS_DRQ;
}

//
// Media presence and the tray
//

// Insert a disc at runtime: signal a media change (unit attention) and enter the spinup sequence, mirroring a real tray
// close.
void Xe::PCIDev::ODD::InsertDisc() {
  std::lock_guard lock(oddMutex);
  if (!atapiState.imageAttached) {
    LOG_WARNING(ODD, "InsertDisc requested but no image is attached.");
    return;
  }
  atapiState.mediaState = MediaState::BecomingReady;
  atapiState.spinUpRepliesRemaining = 4; // A handful of 'not ready' polls.
  // Unit attention: not-ready-to-ready change, medium may have changed.
  setPendingSense(0x06, 0x28, 0x00);
  LOG_INFO(ODD, "Disc inserted - drive spinning up.");
}

void Xe::PCIDev::ODD::EjectDisc() {
  std::lock_guard lock(oddMutex);
  atapiState.mediaState = MediaState::NoMedia;
  atapiState.spinUpRepliesRemaining = 0;
  // Unit attention: not-ready-to-ready change so the next command notices.
  setPendingSense(0x06, 0x28, 0x00);
  LOG_INFO(ODD, "Disc ejected.");
}

bool Xe::PCIDev::ODD::IsDiscPresent() {
  std::lock_guard lock(oddMutex);
  return atapiState.mediaState != MediaState::NoMedia;
}

// TEST UNIT READY (0x00). Real drives use this as the poll that gates every media access. The kernel's
// SataCdRomFinishRequestSense retries on "becoming ready" (sense 2/04) and gives up with STATUS_NO_MEDIA on "medium not
// present" (sense 2/3A), so those are exactly the senses reported here.
void Xe::PCIDev::ODD::scsiTestUnitReadyCommand() {
  switch (atapiState.mediaState) {
    case MediaState::Ready:
      atapiNopCommand(); // GOOD.
      break;
    case MediaState::BecomingReady:
      // Report 'not ready, becoming ready' a bounded number of times, then spin up.
      if (atapiState.spinUpRepliesRemaining > 0) {
        --atapiState.spinUpRepliesRemaining;
        failWithSense(0x02, 0x04, 0x01); // NOT READY / LUN becoming ready.
      } else {
        atapiState.mediaState = MediaState::Ready;
        atapiNopCommand();
      }
      break;
    case MediaState::NoMedia:
    default:
      failWithSense(0x02, 0x3A, 0x00); // NOT READY / medium not present.
      break;
  }
}

//
// Mount time: image layout, security data and keys
//

// Find the game partition in the mounted image, every guest LBA is relative to it.
void Xe::PCIDev::ODD::locateGamePartition() {
  using GP = GAME_PARTITION;

  atapiState.gamePartitionOffset = 0;
  // Return if no image is mounted.
  if (!atapiState.mountedODDImage) { return; }

  // Image size.
  const u64 size = atapiState.mountedODDImage->Size();

  for (const u64 candidate : GP::CANDIDATES) {
    const u64 at = candidate + GP::MAGIC_OFFSET;
    if (at + GP::MAGIC_LENGTH > size) { continue; }

    u8 magic[GP::MAGIC_LENGTH] = {};
    if (!atapiState.mountedODDImage->Read(at, magic, sizeof(magic))) { continue; }
    if (memcmp(magic, GP::MAGIC, GP::MAGIC_LENGTH) != 0) { continue; }

    atapiState.gamePartitionOffset = candidate;
    LOG_INFO(ODD, "Game partition found at {:#x} ({} bytes of image beyond it).", candidate, size - candidate);
    return;
  }

  LOG_WARNING(ODD,
              "No game partition marker found in the mounted image, none of the usual offsets has "
              "\"MICROSOFT*XBOX*MEDIA\" at +{:#x}.",
              GP::MAGIC_OFFSET);
}

// Load the per-disc security data:
//   <image>.authpage / <image>.ss.authpage
//       MODE SENSE(10) page 0x3E reply, 42 bytes, or 34 for a bare page.
//   <image>.ss / <image>.ss.bin / ss.bin beside the image
//       READ DVD STRUCTURE reply, either the 1640-byte reply or a raw
//       2048-byte security sector, which is converted below. Prefer the raw
//       sector: RAW_SS_RECORDS needs the part past the 1636-byte cut.
void Xe::PCIDev::ODD::loadDiscSecurityData() {
  const std::string& imagePath = Config::filepaths.oddImage;

  // Read a whole sidecar file, trying each candidate name in turn.
  auto readSidecar = [&](std::initializer_list<std::string> candidates, std::string& chosenPath) {
    std::vector<u8> bytes;
    for (const std::string& path : candidates) {
      std::error_code ec;
      if (!fs::exists(path, ec) || ec) { continue; }
      const u64 sz = fs::file_size(path, ec);
      if (ec || sz == 0 || sz > 1_MiB) { continue; }
      std::ifstream f(path, std::ios::binary);
      if (!f.is_open()) { continue; }
      bytes.resize(static_cast<size_t>(sz));
      f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(sz));
      if (f.gcount() != static_cast<std::streamsize>(sz)) {
        bytes.clear();
        continue;
      }
      chosenPath = path;
      return bytes;
    }
    return bytes;
  };

  // Directory the image lives in, for a bare "ss.bin" alongside it.
  std::string imageDir;
  {
    std::error_code ec;
    fs::path parent = fs::path(imagePath).parent_path();
    imageDir = parent.empty() ? std::string(".") : parent.string();
  }

  //
  // Authentication page (MODE SENSE(10) page 0x3E).
  //

  atapiState.security.authPagePresent = false;
  std::string authPath;
  std::vector<u8> authBytes
    = readSidecar({imagePath + ".authpage", imagePath + ".ss.authpage", imageDir + "/authpage.bin"}, authPath);
  if (!authBytes.empty()) {
    constexpr size_t AUTH_PAGE_SIZE = sizeof(atapiState.security.authPage); // 42
    constexpr size_t AUTH_HEADER_SIZE = 8;
    if (authBytes.size() == AUTH_PAGE_SIZE) {
      memcpy(atapiState.security.authPage, authBytes.data(), AUTH_PAGE_SIZE);
      atapiState.security.authPagePresent = true;
      LOG_INFO(ODD, "Loaded disc authentication page: {} ({} bytes).", authPath, authBytes.size());
    } else if (authBytes.size() == AUTH_PAGE_SIZE - AUTH_HEADER_SIZE) {
      // A bare page with no mode parameter header: synthesise the header.
      memset(atapiState.security.authPage, 0, AUTH_PAGE_SIZE);
      atapiState.security.authPage[1] = static_cast<u8>(AUTH_PAGE_SIZE - 2); // Mode data length.
      memcpy(atapiState.security.authPage + AUTH_HEADER_SIZE, authBytes.data(), authBytes.size());
      atapiState.security.authPagePresent = true;
      LOG_INFO(ODD, "Loaded disc authentication page: {} ({} bytes, mode parameter header added).", authPath,
               authBytes.size());
    } else {
      LOG_WARNING(ODD, "Disc authentication page {} is {} bytes, expected {} (full reply) or {} (bare page).", authPath,
                  authBytes.size(), AUTH_PAGE_SIZE, AUTH_PAGE_SIZE - AUTH_HEADER_SIZE);
    }
  }

  // DVDX2 control data (READ DVD STRUCTURE 0xAD):

  atapiState.security.controlDataPresent = false;
  std::string ssPath;
  std::vector<u8> ssBytes = readSidecar({imagePath + ".ss", imagePath + ".ss.bin", imageDir + "/ss.bin"}, ssPath);
  if (!ssBytes.empty()) {
    constexpr size_t CONTROL_DATA_SIZE = sizeof(atapiState.security.controlData); // 1640
    constexpr size_t CONTROL_HEADER_SIZE = 4;
    constexpr size_t CONTROL_BODY_SIZE = CONTROL_DATA_SIZE - CONTROL_HEADER_SIZE; // 1636
    constexpr size_t RAW_SECURITY_SECTOR_SIZE = 2048;

    if (ssBytes.size() == CONTROL_DATA_SIZE) {
      memcpy(atapiState.security.controlData, ssBytes.data(), CONTROL_DATA_SIZE);
      atapiState.security.controlDataPresent = true;
      LOG_INFO(ODD, "Loaded disc control data: {} ({} bytes, used verbatim).", ssPath, ssBytes.size());
    } else if (ssBytes.size() >= RAW_SECURITY_SECTOR_SIZE) {
      // Raw security sector: wrap the first 1636 bytes in the reply header.
      memset(atapiState.security.controlData, 0, CONTROL_DATA_SIZE);
      // The length field carries the structure length, not the reply length: 0x664 for the 1636 bytes that follow the
      // 4-byte header.
      const u16 dataLength = static_cast<u16>(CONTROL_BODY_SIZE);
      atapiState.security.controlData[0] = static_cast<u8>((dataLength >> 8) & 0xFF);
      atapiState.security.controlData[1] = static_cast<u8>(dataLength & 0xFF);
      memcpy(atapiState.security.controlData + CONTROL_HEADER_SIZE, ssBytes.data(), CONTROL_BODY_SIZE);
      atapiState.security.controlDataPresent = true;
      // Keep the whole sector.
      memcpy(atapiState.security.rawSector, ssBytes.data(), sizeof(atapiState.security.rawSector));
      atapiState.security.rawSectorPresent = true;
      LOG_INFO(ODD,
               "Loaded disc control data: {} ({} byte security sector; first {} bytes wrapped in the "
               "READ DVD STRUCTURE header to make the {} byte reply).",
               ssPath, ssBytes.size(), CONTROL_BODY_SIZE, CONTROL_DATA_SIZE);
    } else {
      LOG_WARNING(ODD, "Disc control data {} is {} bytes; expected {} (full reply) or {} (raw security sector).",
                  ssPath, ssBytes.size(), CONTROL_DATA_SIZE, RAW_SECURITY_SECTOR_SIZE);
    }
  }

  // A real drive always has an authentication page; it builds one when it detects media. Do the same when no captured
  // page sits next to the image, otherwise MODE SENSE(10) page 0x3E fails.
  if (!atapiState.security.authPagePresent && atapiState.security.controlDataPresent) { synthesizeAuthPage(); }
}

// Build the authentication page when none was captured beside the image. Three fields have to be right for the guest to
// accept it at all: +0x0B must read 1, +0x0D must agree with control data byte 4 , and +0x09 / +0x01 are the page and
// mode data lengths. Taking +0x0D from the disc's own layer descriptor makes the pair consistent by construction.
void Xe::PCIDev::ODD::synthesizeAuthPage() {
  u8* page = atapiState.security.authPage;
  memset(page, 0, sizeof(atapiState.security.authPage));

  // Mode parameter header: mode data length counts everything after itself.
  page[1] = static_cast<u8>(AUTH_PAGE::SIZE - 2); // 40

  page[AUTH_PAGE::PAGE_CODE] = AUTH_PAGE::PAGE_CODE_AUTH;
  // HvxDvdAuthVerifyAuthPage compares this byte for byte before it decrypts anything.
  page[AUTH_PAGE::PAGE_LENGTH] = AUTH_PAGE::PAGE_LENGTH_VALUE; // 32
  page[AUTH_PAGE::VALID_FLAG] = AUTH_PAGE::VALID_FLAG_SET;
  // Control data byte 4 is layer descriptor byte 0: book type << 4 | version.
  page[AUTH_PAGE::DISC_TYPE] = atapiState.security.controlData[4];
  // Only consulted on the non-XGD2 branch, where 2 and 3 select between the two anti-piracy revisions.
  page[AUTH_PAGE::AP_LEVEL] = 2;

  // The initialisation vector the drive will use for the control data.
  std::random_device rd;
  std::uniform_int_distribution<u32> byte(0, 0xFF);
  for (u32 i = 0; i != 16; ++i) { page[AUTH_PAGE::SESSION_IV + i] = static_cast<u8>(byte(rd)); }

  atapiState.security.authPagePresent = true;
  atapiState.security.authPageSynthesized = true;

  LOG_WARNING(ODD,
              "No captured authentication page found - built one from the control data "
              "(disc type {:#04x}, valid flag 1, AP level 2). This is structurally correct but carries no "
              "per-disc secrets, so the hypervisor may still reject it.",
              page[AUTH_PAGE::DISC_TYPE]);
}

// Reads the console's DVD key: 32 hex digits with any surrounding whitespace, or a raw 16-byte file.
void Xe::PCIDev::ODD::loadDvdKey() {
  dvdKeyValid = false;
  memset(dvdKey, 0, sizeof(dvdKey));

  const std::string& path = Config::filepaths.dvdKeyPath;

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(ODD, "Could not open the DVD key file '{}'. AP2.0 drive authentication will fail.", path);
    return;
  }

  const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  // Accept 32 hex digits with any amount of surrounding whitespace...
  std::string hex;
  bool hexOk = true;
  for (const char c : contents) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isspace(uc)) { continue; }
    if (!std::isxdigit(uc) || hex.size() == sizeof(dvdKey) * 2) {
      hexOk = false;
      break;
    }
    hex.push_back(c);
  }

  if (hexOk && hex.size() == sizeof(dvdKey) * 2) {
    for (size_t i = 0; i != sizeof(dvdKey); ++i) {
      dvdKey[i] = static_cast<u8>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    dvdKeyValid = true;
  } else if (contents.size() == sizeof(dvdKey)) {
    // ...or a raw 16-byte binary key file.
    memcpy(dvdKey, contents.data(), sizeof(dvdKey));
    dvdKeyValid = true;
  }

  if (!dvdKeyValid) {
    LOG_ERROR(ODD,
              "The DVD key file '{}' is neither 32 hex digits nor 16 raw bytes ({} bytes read). "
              "AP2.0 drive authentication will fail.",
              path, contents.size());
    return;
  }

  // HvxDvdAuthBuildNVPage encrypts the challenge with the key at offset 0x100 of
  // the console's keyvault, so this has to be that console's key. A valid-looking
  // key from a different console produces a perfectly well-formed reply that the
  // hypervisor still rejects, which is indistinguishable from a protocol bug
  // unless the key in use is on the record.
  LOG_INFO(ODD,
           "DVD key loaded from '{}': {:02X}{:02X}....{:02X}{:02X}. It must belong to the keyvault of the "
           "NAND image being booted.",
           path, dvdKey[0], dvdKey[1], dvdKey[14], dvdKey[15]);
}

//
// SCSI commands
//

void Xe::PCIDev::ODD::scsiReadCapacityCommand() {
  u8 capacityBuffer[8] = {};
  // The capacity the guest is told about is the game partition's, because that is what its LBAs address.
  const u64 imageCapacity
    = (atapiState.mountedODDImage->Size() - atapiState.gamePartitionOffset) / ATAPI_CDROM_SECTOR_SIZE;
  // LBA of the last block of this image.
  capacityBuffer[0] = static_cast<u8>(imageCapacity >> 24);
  capacityBuffer[1] = static_cast<u8>(imageCapacity >> 16);
  capacityBuffer[2] = static_cast<u8>(imageCapacity >> 8);
  capacityBuffer[3] = static_cast<u8>(imageCapacity);
  // Block size.
  capacityBuffer[4] = static_cast<u8>(ATAPI_CDROM_SECTOR_SIZE >> 24);
  capacityBuffer[5] = static_cast<u8>(ATAPI_CDROM_SECTOR_SIZE >> 16);
  capacityBuffer[6] = static_cast<u8>(ATAPI_CDROM_SECTOR_SIZE >> 8);
  capacityBuffer[7] = static_cast<u8>(ATAPI_CDROM_SECTOR_SIZE);

  // READ CAPACITY has no allocation length field; the reply is always 8 bytes.
  scsiDataIn(capacityBuffer, sizeof(capacityBuffer), sizeof(capacityBuffer));
}

void Xe::PCIDev::ODD::scsiInquiryCommand() {
  // Allocation length is CDB byte 4.
  const u32 allocLen = atapiState.scsiCBD.AsByte[4];
  // SataCdRomInitializeContinue reads the vendor ID out of bytes 8..15 and the product revision out of bytes 32..35 to
  // pick an HCDF runtime patch, so the full 36 bytes matter even though only the first few are inspected earlier.
  scsiDataIn(&atapiState.atapiInquiryData, sizeof(XE_ATAPI_INQUIRY_DATA), allocLen);
}

void Xe::PCIDev::ODD::scsiRead10Command() {
  const u32 blockCount = (static_cast<u32>(atapiState.scsiCBD.AsByte[7]) << 8) | atapiState.scsiCBD.AsByte[8];
  const u32 blockAddress = (static_cast<u32>(atapiState.scsiCBD.AsByte[2]) << 24)
                           | (static_cast<u32>(atapiState.scsiCBD.AsByte[3]) << 16)
                           | (static_cast<u32>(atapiState.scsiCBD.AsByte[4]) << 8) | atapiState.scsiCBD.AsByte[5];

  const u32 transferSize = blockCount * ATAPI_CDROM_SECTOR_SIZE;
  // The guest counts sectors from the start of the game partition, not from the start of the file.
  const u64 readOffset = atapiState.gamePartitionOffset + static_cast<u64>(blockAddress) * ATAPI_CDROM_SECTOR_SIZE;

  DEBUGP("Read10: LBA {:#x}, {} block(s), offset {:#x}, size {:#x}", blockAddress, blockCount, readOffset,
         transferSize);

  if (transferSize == 0) {
    atapiNopCommand();
    return;
  }

  if (!atapiState.dataOutBuffer.Begin(transferSize, true)) {
    LOG_ERROR(ODD, "Failed to allocate a {} byte read buffer.", transferSize);
    failWithSense(0x04, 0x44, 0x00); // HARDWARE ERROR / internal target failure.
    return;
  }

  if (!atapiState.mountedODDImage->Read(readOffset, atapiState.dataOutBuffer.Data(), transferSize)) {
    LOG_WARNING(ODD, "Read10 of {:#x} bytes at offset {:#x} failed or ran past the end of the image.", transferSize,
                readOffset);
  }

  completeDataIn(transferSize);
}

void Xe::PCIDev::ODD::scsiReadTocCommand() {
  const u16 allocLen = (static_cast<u16>(atapiState.scsiCBD.AsByte[7]) << 8) | atapiState.scsiCBD.AsByte[8];

  // Minimal TOC: one data track (1) + lead-out (0xAA).
  constexpr u8 firstTrack = 1;
  constexpr u8 lastTrack = 1;
  constexpr u8 numDescriptors = (lastTrack - firstTrack + 1) + 1;

  const u32 leadOutAddress
    = static_cast<u32>((atapiState.mountedODDImage->Size() - atapiState.gamePartitionOffset) / ATAPI_CDROM_SECTOR_SIZE);

  u8 response[4 + numDescriptors * 8] = {};
  // TOC data length counts everything after the length field itself.
  const u16 tocLen = static_cast<u16>(sizeof(response) - 2);

  // Header: 2 bytes BE length, 1 byte first track, 1 byte last track.
  response[0] = static_cast<u8>((tocLen >> 8) & 0xFF);
  response[1] = static_cast<u8>(tocLen & 0xFF);
  response[2] = firstTrack;
  response[3] = lastTrack;

  size_t off = 4;
  for (u8 t = firstTrack; t <= lastTrack; ++t) {
    response[off + 0] = 0x14; // ADR=1 (upper nibble), CONTROL=4 (lower nibble) -> data track.
    response[off + 1] = t;    // Track number.
    // Track start LBA, MSB..LSB - a single-track image starts at 0.
    off += 8;
  }

  // Lead-out descriptor.
  response[off + 0] = 0x14; // ADR=1, CONTROL=4.
  response[off + 1] = 0xAA; // Lead-out track number.
  response[off + 4] = static_cast<u8>((leadOutAddress >> 24) & 0xFF);
  response[off + 5] = static_cast<u8>((leadOutAddress >> 16) & 0xFF);
  response[off + 6] = static_cast<u8>((leadOutAddress >> 8) & 0xFF);
  response[off + 7] = static_cast<u8>((leadOutAddress) & 0xFF);

  scsiDataIn(response, sizeof(response), allocLen);
}

// GET CONFIGURATION (0x46). The kernel/media detection uses this to learn the current profile. Report DVD-ROM (0x0010)
// so a mounted image is recognised as a DVD, with no media, report profile 0. Minimal Feature Header only.
void Xe::PCIDev::ODD::scsiGetConfigurationCommand() {
  const u16 allocLen = (static_cast<u16>(atapiState.scsiCBD.AsByte[7]) << 8) | atapiState.scsiCBD.AsByte[8];

  // Feature Header: 4-byte data length + 2 reserved + 2-byte current profile.
  u8 response[8] = {};
  const u16 currentProfile = (atapiState.mediaState == MediaState::Ready) ? 0x0010 : 0x0000; // DVD-ROM.
  // Data length counts everything after this field (here just the 4 header bytes).
  response[3] = 0x04;
  response[6] = (currentProfile >> 8) & 0xFF;
  response[7] = currentProfile & 0xFF;

  scsiDataIn(response, sizeof(response), allocLen);
}

// Handles REQUEST_SENSE (0x03), 18-byte fixed-format sense data.
void Xe::PCIDev::ODD::scsiRequestSenseCommand() {
  const u8 allocLen = atapiState.scsiCBD.AsByte[4];
  u8 response[18] = {};

  response[0] = 0x70;                               // Response code: current error, fixed format.
  response[2] = atapiState.pendingSense.key & 0x0F; // Sense key.
  response[7] = 0x0A;                               // Additional sense length = 10 (total 18 - 7 - 1).
  response[12] = atapiState.pendingSense.asc;       // Additional Sense Code.
  response[13] = atapiState.pendingSense.ascq;      // Additional Sense Code Qualifier.

  // REQUEST SENSE clears the contingent-allegiance / unit-attention condition, once the initiator has read the sense,
  // the drive reverts to NO SENSE.
  atapiState.pendingSense = {};

  scsiDataIn(response, sizeof(response), allocLen);
}

// Handles the GET EVENT STATUS NOTIFICATION command (0x4A).
void Xe::PCIDev::ODD::scsiGetEventStatusNotificationCommand() {
  const u8 notificationClassRequest = atapiState.scsiCBD.AsByte[4];
  const u16 allocLen = (static_cast<u16>(atapiState.scsiCBD.AsByte[7]) << 8) | atapiState.scsiCBD.AsByte[8];

  // Response buffer: 4-byte event header + 4-byte media event descriptor.
  u8 response[8] = {};
  u16 dataLen = 0;

  // We primarily support the media status class (bit 4).
  if (notificationClassRequest & 0x10) {
    // Header (4 bytes) + descriptor (4 bytes); the length field counts the rest.
    dataLen = 6;

    response[0] = (dataLen >> 8) & 0xFF;
    response[1] = dataLen & 0xFF;

    // Byte 2: bit 7 = NEA (0 = event available), bits 6..4 = notification class.
    response[2] = 0x40; // Media class (4).
    response[3] = 0x10; // Supported classes: media (bit 4).

    // Event descriptor byte 0: event code (0 = no change, 1 = eject request,
    // 2 = new media, 3 = media removal).
    response[4] = 0x00;
    // Byte 1: bit 1 = media present, bit 0 = door open.
    response[5] = (atapiState.mediaState != MediaState::NoMedia) ? 0x02 : 0x00;
  } else {
    // No event or unsupported class requested: empty header with NEA = 1.
    dataLen = 2;
    response[0] = (dataLen >> 8) & 0xFF;
    response[1] = dataLen & 0xFF;
    response[2] = 0x80; // NEA = 1.
    response[3] = 0x10; // Supported classes: media.
  }

  scsiDataIn(response, static_cast<u32>(dataLen) + 2, allocLen);
}

// START STOP UNIT (0x1B). The kernel spins the drive up and down around media changes, report success.
void Xe::PCIDev::ODD::scsiStartStopUnitCommand() {
  const bool start = (atapiState.scsiCBD.AsByte[4] & 0x01) != 0;
  const bool loej = (atapiState.scsiCBD.AsByte[4] & 0x02) != 0;
  DEBUGP("START_STOP_UNIT: start = {}, load/eject = {}", start, loej);
  atapiNopCommand();
}

// PREVENT ALLOW MEDIUM REMOVAL (0x1E). The tray is virtual, so locking it is a no-op.
void Xe::PCIDev::ODD::scsiPreventAllowRemovalCommand() {
  DEBUGP("PREVENT_ALLOW_MEDIUM_REMOVAL: prevent = {}", (atapiState.scsiCBD.AsByte[4] & 0x01) != 0);
  atapiNopCommand();
}

// SET CD SPEED (0xBB). Recorded for MODE SENSE(10) page 0x20 to report back.
void Xe::PCIDev::ODD::scsiSetCdSpeedCommand() {
  DEBUGP("SET_CD_SPEED: read {:#x}, write {:#x}",
         (static_cast<u16>(atapiState.scsiCBD.AsByte[2]) << 8) | atapiState.scsiCBD.AsByte[3],
         (static_cast<u16>(atapiState.scsiCBD.AsByte[4]) << 8) | atapiState.scsiCBD.AsByte[5]);
  atapiNopCommand();
}

//
// MODE SELECT(10) / MODE SENSE(10)
//

// MODE SELECT(10) (0x55)
void Xe::PCIDev::ODD::scsiModeSelect10Command() {
  // CDB bytes 7..8 are the parameter list length. For the AP2.0 page the kernel derives it in SataCdRomSendAP20Select
  // as modeDataLength + 2, and the hypervisor set modeDataLength to pageLength + 8, so this is pageLength + 10 - 58
  // bytes for the 0x3B page, 74 for the 0x28 one.
  const u16 paramLen = (static_cast<u16>(atapiState.scsiCBD.AsByte[7]) << 8) | atapiState.scsiCBD.AsByte[8];

  DEBUGP("MODE_SELECT10: parameter list length {:#d}", paramLen);

  if (paramLen == 0) {
    atapiNopCommand();
    return;
  }

  // Size the input buffer: this is a data out phase, so the parameter list arrives into the input buffer and the output
  // buffer is not involved.
  if (!atapiState.dataInBuffer.Begin(paramLen)) {
    LOG_ERROR(ODD, "Failed to allocate a {} byte parameter buffer for MODE SELECT(10).", paramLen);
    failWithSense(0x04, 0x44, 0x00); // HARDWARE ERROR / internal target failure.
    return;
  }

  ap20.selectPending = true;

  atapiState.regs.error = 0;
  // Interrupt reason: data (C/D = 0) travelling to the device (I/O = 0).
  atapiState.regs.interruptReason &= ~(ATA_INTERRUPT_REASON_IO | ATA_INTERRUPT_REASON_CD);
  atapiState.regs.byteCountLow = paramLen & 0xFF;
  atapiState.regs.byteCountHigh = (paramLen >> 8) & 0xFF;

  if (atapiState.regs.features & IDE_FEATURE_DMA) {
    atapiState.regs.status = ATA_STATUS_BSY | ATA_STATUS_DRDY;
  } else {
    atapiState.regs.status = ATA_STATUS_DRDY | ATA_STATUS_DRQ;
  }
}

// Takes the parameter data of a completed MODE SELECT(10) out of the input buffer. Called from the DMA completion path
// and, for a PIO data out phase,from the data register write that finishes the transfer.
void Xe::PCIDev::ODD::consumeModeSelectData() {
  if (!ap20.selectPending) { return; }
  ap20.selectPending = false;

  // How much actually arrived, not how much was asked for.
  const u32 received = std::min(atapiState.dataInBuffer.Transferred(), AP20_AUTH_STATE::MAX_PAGE_SIZE);

  memset(ap20.page, 0, sizeof(ap20.page));
  atapiState.dataInBuffer.Rewind();
  atapiState.dataInBuffer.Take(ap20.page, received);
  ap20.pageSize = received;

  if (received <= AP20_AUTH_STATE::PAGE_LENGTH_OFFSET) {
    LOG_ERROR(ODD, "MODE SELECT(10) parameter list was {} bytes - too short to contain a mode page.", received);
    return;
  }

  const u8 pageCode = ap20.page[AP20_AUTH_STATE::PAGE_CODE_OFFSET] & 0x3F;
  const u8 pageLength = ap20.page[AP20_AUTH_STATE::PAGE_LENGTH_OFFSET];

  // Page 0x20 is the SSC spindle speed page, not an auth page.
  if (pageCode == 0x20) {
    const u8 speedIdx = ap20.page[AP20_AUTH_STATE::PAGE_BODY_OFFSET];
    if (speedIdx >= 1 && speedIdx <= 4) {
      sscState.currentSpeed = speedIdx;
      DEBUGP("MODE_SELECT10: SSC speed set to {}x", speedIdx);
    } else {
      LOG_WARNING(ODD, "MODE_SELECT10 page 0x20 requested speed index {}, which is out of the 1-4 range.", speedIdx);
    }
    return;
  }

  // The firmware challenge/response rides the same MODE SELECT(10) / MODE SENSE(10) transport as AP2.0 and its pages
  // are the same shape, so it is only the page code that tells them apart.
  if (AP20_AUTH_STATE::IsFwcrPageCode(pageCode)) {
    DEBUGP("Firmware challenge page received: page code {:#x}, page length {:#x}, {} bytes. NOT IMPLEMENTED.", pageCode,
           pageLength, received);
    return;
  }

  // Page 0x3E is the DVDX2 authentication page: the disc challenge/response loop drives it through MODE SELECT(10) then
  // MODE SENSE(10), the same way AP2.0 drive authentication uses its own page code.
  if (pageCode == AUTH_PAGE::PAGE_CODE_AUTH) {
    applyAuthPageSelect(ap20.page, received);
    return;
  }

  if (!AP20_AUTH_STATE::IsAuthPageCode(pageCode)) {
    LOG_WARNING(ODD, "MODE SELECT(10) carried an unrecognised vendor page code {:#x} ({} bytes), ignored.", pageCode,
                received);
    return;
  }

  // The hypervisor's AP2.0 page. Both variants carry the ciphertext at [10..41] and the IV at [42..57], so the page
  // only has to be long enough to hold them.
  ap20.pageCode = pageCode;
  ap20.pageValid = received >= AP20_AUTH_STATE::REPLY_SIZE && pageLength >= AP20_AUTH_STATE::REPLY_PAGE_LENGTH;

  if (!ap20.pageValid) {
    LOG_ERROR(ODD,
              "AP2.0 authentication page is malformed: page code {:#x}, page length {}, {} bytes received "
              "(need at least {} bytes with a page length of {:#x}).",
              pageCode, pageLength, received, AP20_AUTH_STATE::REPLY_SIZE, AP20_AUTH_STATE::REPLY_PAGE_LENGTH);
    return;
  }

  DEBUGP("AP2.0 authentication page received: page code {:#x}, page length {:#x}, {} bytes.", pageCode, pageLength,
         received);
}

// MODE SENSE(10) (0x5A). Dispatches on the CDB page code.
void Xe::PCIDev::ODD::scsiModeSense10Command() {
  const u8 pageCode = atapiState.scsiCBD.AsByte[2] & 0x3F;
  const u16 allocLen = (static_cast<u16>(atapiState.scsiCBD.AsByte[7]) << 8) | atapiState.scsiCBD.AsByte[8];

  // The firmware challenge is checked before anything else: it uses the same transport as AP2.0 and only the page code
  // separates them.
  if (AP20_AUTH_STATE::IsFwcrPageCode(pageCode)) {
    scsiModeSense10FwcrCommand(pageCode, allocLen);
    return;
  }

  // The AP2.0 reply comes back under whatever page code the hypervisor chose for the MODE SELECT(10) it just sent: 0x3B
  // on most consoles, 0x28 on the ones where HvxDvdAuthBuildNVPage takes its other branch.
  if (ap20.pageValid && pageCode == ap20.pageCode) {
    performAP20Auth(allocLen);
    return;
  }

  switch (pageCode) {
    case 0x20: scsiModeSense10Page20Command(allocLen); return;
    case 0x2A: scsiModeSense10Page2ACommand(allocLen); return;
    case 0x3E: scsiModeSense10Page3ECommand(allocLen); return;
    default: break;
  }

  LOG_WARNING(ODD, "Unsupported MODE_SENSE10 page code {:#x}", pageCode);
  failWithSense(0x05, 0x24, 0x00); // ILLEGAL REQUEST / invalid field in CDB.
}

// Handles MODE_SENSE6 (0x1A), 4-byte header, no block descriptor.
void Xe::PCIDev::ODD::scsiModeSense6Command() {
  const u8 pageCode = atapiState.scsiCBD.AsByte[2] & 0x3F;
  const u8 allocLen = atapiState.scsiCBD.AsByte[4];

  // Minimal 4-byte header response for unsupported pages.
  u8 response[4] = {};
  response[0] = 3; // Mode data length = total - 1.
  response[1] = 0; // Medium type.
  response[2] = 0; // Device specific.
  response[3] = 0; // Block descriptor length.

  if (pageCode != 0) { LOG_WARNING(ODD, "MODE_SENSE6 page {:#x} not fully implemented", pageCode); }

  scsiDataIn(response, sizeof(response), allocLen);
}

// Handles MODE_SENSE10 page 0x20 - SSC spindle speed page.
// 8-byte header + 12-byte page (total 20 bytes).
// response[10] = currentSpeed index (1-4); value 0 or > 4 traps the kernel.
void Xe::PCIDev::ODD::scsiModeSense10Page20Command(u16 allocLen) {
  u8 response[20] = {};
  // Mode parameter header (8 bytes).
  response[0] = 0x00;
  response[1] = static_cast<u8>(sizeof(response) - 2); // Mode data length = 18.
  // Bytes 2-7: medium type, device specific, block descriptor length (all 0).

  // Page 0x20: SSC speed page (10-byte body).
  response[8] = 0x20;                   // Page code.
  response[9] = 0x0A;                   // Page length = 10.
  response[10] = sscState.currentSpeed; // Speed index 1-4.
  // Bytes 11-19: reserved (0).

  scsiDataIn(response, sizeof(response), allocLen);
}

// MODE SENSE(10) page 0x2A - CD/DVD capabilities and mechanical status.
// 8-byte mode parameter header + a 20-byte page.
void Xe::PCIDev::ODD::scsiModeSense10Page2ACommand(u16 allocLen) {
  u8 response[28] = {};

  // Mode parameter header: data length counts everything after the field.
  response[0] = 0x00;
  response[1] = static_cast<u8>(sizeof(response) - 2);
  // Bytes 2..7: medium type, device specific, block descriptor length (all 0).

  response[8] = 0x2A;  // Page code.
  response[9] = 0x12;  // Page length (18).
  response[10] = 0x1F; // Read caps: DVD-ROM, DVD-R, DVD-RAM, CD-R, CD-RW.
  response[11] = 0x00; // Read-only drive: no write capabilities.

  scsiDataIn(response, sizeof(response), allocLen);
}

//
// AP2.0 drive authentication
//

// MODE SENSE(10) for the AP2.0 page:
// HvxDvdAuthBuildNVPage sends, HvxDvdAuthVerifyNVPage checks:
//
//   select[10..41] = AES-CBC-Enc(DVD key, IV, sessionKey || challenge)
//   select[42..57] = IV
//
//   reply[8]       must equal the select's page code
//   reply[9]       must equal 0x30, for both hypervisor variants
//   reply[10..41]  decrypted with AES-CBC(sessionKey, the HV's own IV) must be 16 zero bytes followed by the challenge.
void Xe::PCIDev::ODD::performAP20Auth(u16 allocLen) {
  if (!ap20.pageValid) {
    LOG_ERROR(ODD, "AP2.0 authentication requested before a valid page was received.");
    failWithSense(0x05, 0x24, 0x00); // ILLEGAL REQUEST / invalid field in CDB.
    return;
  }

  if (!dvdKeyValid) {
    LOG_ERROR(ODD, "AP2.0 authentication requested but no usable DVD key is loaded.");
    failWithSense(0x05, 0x24, 0x00);
    return;
  }

  // The IV the hypervisor generated, and the two encrypted blocks that precede it.
  u8 aesCBCIv[16] = {};
  memcpy(aesCBCIv, &ap20.page[42], sizeof(aesCBCIv));

  // Decrypt the session key and challenge with this console's DVD key.
  u8 decryptedPageData[32] = {};
  plusaes::Error err
    = plusaes::decrypt_cbc(&ap20.page[AP20_AUTH_STATE::PAGE_BODY_OFFSET], sizeof(decryptedPageData), dvdKey,
                           sizeof(dvdKey), &aesCBCIv, decryptedPageData, sizeof(decryptedPageData), nullptr);
  if (err != plusaes::kErrorOk) {
    LOG_ERROR(ODD, "AP2.0 authentication: AES-CBC decrypt of the challenge failed (plusaes error {}).",
              static_cast<int>(err));
    failWithSense(0x04, 0x44, 0x00); // HARDWARE ERROR / internal target failure.
    return;
  }

  const u8* sessionKey = &decryptedPageData[0];     // First plaintext block.
  const u8* challengeData = &decryptedPageData[16]; // Second plaintext block.

  // Keep the session key. Authentication is not the end of its life as the control data served later is encrypted under
  // it.
  memcpy(ap20SessionKey, sessionKey, sizeof(ap20SessionKey));
  ap20SessionKeyValid = true;

  // The response the hypervisor expects: a zero block, then the challenge back.
  u8 responseData[32] = {};
  memcpy(&responseData[16], challengeData, 16);

  // Build the 58-byte reply page.
  u8 outPage[AP20_AUTH_STATE::REPLY_SIZE] = {};
  // Mode data length = total - 2 = page length + 8, which is the 0x38 the kernel preset in its own buffer before
  // issuing the MODE SENSE(10).
  outPage[1] = AP20_AUTH_STATE::REPLY_PAGE_LENGTH + 8;
  // Echo the page code the hypervisor picked; it is compared byte for byte.
  outPage[AP20_AUTH_STATE::PAGE_CODE_OFFSET] = ap20.pageCode;
  outPage[AP20_AUTH_STATE::PAGE_LENGTH_OFFSET] = AP20_AUTH_STATE::REPLY_PAGE_LENGTH;
  // Echo the IV, as a real drive does.
  memcpy(&outPage[42], aesCBCIv, sizeof(aesCBCIv));

  err = plusaes::encrypt_cbc(responseData, sizeof(responseData), sessionKey, 16, &aesCBCIv,
                             &outPage[AP20_AUTH_STATE::PAGE_BODY_OFFSET], sizeof(responseData), false);
  if (err != plusaes::kErrorOk) {
    LOG_ERROR(ODD, "AP2.0 authentication: AES-CBC encrypt of the response failed (plusaes error {}).",
              static_cast<int>(err));
    failWithSense(0x04, 0x44, 0x00);
    return;
  }

  DEBUGP("AP2.0 authentication: answered page code {:#x}, {} of {} bytes requested.", ap20.pageCode,
         std::min<u32>(sizeof(outPage), allocLen), allocLen);

  scsiDataIn(outPage, sizeof(outPage), allocLen);
}

// MODE SENSE(10) for a firmware challenge/response page.

// The answer is computed from the real drive firmware's secret, this is the check that catches a flashed drive.
void Xe::PCIDev::ODD::scsiModeSense10FwcrCommand(u8 pageCode, u16 allocLen) {
  LOG_WARNING(ODD, "Firmware challenge/response requested (MODE SENSE(10) page {:#x}, {} bytes). NOT IMPLEMENTED!",
              pageCode, allocLen);
  failWithSense(0x05, 0x24, 0x00); // ILLEGAL REQUEST / invalid field in CDB.
}

//
// XGD2 disc authentication
//

// Encrypt or decrypt AUTH_PAGE::BODY_CIPHER, the one AES-CBC block of the authentication page that does not travel in
// the clear.

// The hypervisor does the same on its side: HvxDvdAuthGetAuthPage encrypts before the kernel sends,
// HvxDvdAuthVerifyAuthPagedecrypts on the way back.

// The IV is the drive's own, published in the page, the key is the session key recovered during AP2.0 drive
// authentication.
bool Xe::PCIDev::ODD::cryptAuthPageBody(u8* page, bool encrypt) {
  if (!ap20SessionKeyValid) {
    LOG_WARNING(ODD,
                "Authentication page {} requested before AP2.0 drive authentication established a session "
                "key, the page body cannot be read or written.",
                encrypt ? "encryption" : "decryption");
    return false;
  }

  // plusaes updates the IV as it chains, so it gets a copy of the page's.
  u8 iv[16] = {};
  memcpy(iv, &page[AUTH_PAGE::SESSION_IV], sizeof(iv));

  u8 out[AUTH_PAGE::BODY_CIPHER_LENGTH] = {};
  const plusaes::Error err = encrypt ? plusaes::encrypt_cbc(&page[AUTH_PAGE::BODY_CIPHER], sizeof(out), ap20SessionKey,
                                                            sizeof(ap20SessionKey), &iv, out, sizeof(out), false)
                                     : plusaes::decrypt_cbc(&page[AUTH_PAGE::BODY_CIPHER], sizeof(out), ap20SessionKey,
                                                            sizeof(ap20SessionKey), &iv, out, sizeof(out), nullptr);
  if (err != plusaes::kErrorOk) {
    LOG_ERROR(ODD, "AES-CBC {} of the authentication page body failed (plusaes error {}).",
              encrypt ? "encrypt" : "decrypt", static_cast<int>(err));
    return false;
  }

  memcpy(&page[AUTH_PAGE::BODY_CIPHER], out, sizeof(out));
  return true;
}

// Apply a MODE SELECT(10) page 0x3E to the drive's authentication page.

// The page does not arrive readable: HvxDvdAuthGetAuthPage encrypts page[0x0A..0x19] before the kernel ever sees it, so
// the index and challenge word have to be decrypted before anything can be matched against them. The drive keeps its
// own page in plaintext and re-applies the layer when it answers.
void Xe::PCIDev::ODD::applyAuthPageSelect(const u8* page, u32 length) {
  if (length < AUTH_PAGE::SIZE) {
    LOG_WARNING(ODD, "MODE SELECT(10) page 0x3E carried {} bytes, expected {} - ignored.", length, AUTH_PAGE::SIZE);
    return;
  }

  if (!atapiState.security.authPagePresent) {
    LOG_WARNING(ODD, "MODE SELECT(10) page 0x3E arrived but the drive has no authentication page to update.");
    return;
  }

  // Work on a copy, the caller's buffer is the recorded MODE SELECT data and the decrypt is in place.
  u8 plain[AUTH_PAGE::SIZE];
  memcpy(plain, page, sizeof(plain));
  if (!cryptAuthPageBody(plain, /*encrypt=*/false)) {
    LOG_ERROR(ODD, "MODE SELECT(10) page 0x3E could not be decrypted, so the challenge in it cannot be read.");
    return;
  }

  u8* live = atapiState.security.authPage;

  // However this turns out, the command has to take as long as a real drive would. The hypervisor times it and counts
  // the slow ones.
  challengeNeedsSeekDelay = true;

  const u8 challengeIndex = plain[AUTH_PAGE::CHALLENGE_INDEX];
  const u8* challengeData = &plain[AUTH_PAGE::CHALLENGE_DATA];

  // Carried across on a successful challenge.
  live[AUTH_PAGE::FINAL_FLAG] = plain[AUTH_PAGE::FINAL_FLAG];
  live[AUTH_PAGE::AUTH_FLAG] = plain[AUTH_PAGE::AUTH_FLAG];
  memset(&live[AUTH_PAGE::RESPONSE_DATA], 0, 4);

  if (answerDiscChallenge(plain)) {
    // A drive that answered a challenge is authenticated. The host clears this flag on the first challenge of a run and
    // sets it afterwards.
    live[AUTH_PAGE::AUTH_FLAG] = plain[AUTH_PAGE::AUTH_FLAG] | 1;
    DEBUGP("Disc challenge index {} (challenge {:02X}{:02X}{:02X}{:02X}) answered.", challengeIndex, challengeData[0],
           challengeData[1], challengeData[2], challengeData[3]);
    return;
  }

  // Neither table had it.
  LOG_WARNING(ODD,
              "Disc challenge index {} (challenge {:02X}{:02X}{:02X}{:02X}) has no answer in either table, disc "
              "authentication will fail.",
              challengeIndex, challengeData[0], challengeData[1], challengeData[2], challengeData[3]);
}

// Answer a disc challenge from the disc's own plaintext table.
bool Xe::PCIDev::ODD::answerDiscChallenge(const u8* selectPage) {
  using TABLE = DISC_CHALLENGE_TABLE;

  if (!atapiState.security.controlDataPresent) {
    LOG_WARNING(ODD, "Disc challenge: no control data is loaded, so the disc's response table is not available.");
    return false;
  }

  const u8* table = &atapiState.security.controlData[TABLE::BASE];

  // The plaintext table holds as many records as the encrypted copy holds entries, and that count sits in the clear
  // beside it.
  const u8 declared = atapiState.security.controlData[TABLE::COUNT];
  const u32 records = (declared != 0 && declared <= TABLE::MAX_COUNT) ? declared : TABLE::MAX_RECORDS;

  // The disc stores the challenge in the same byte order the page carries it, so this is a plain 4-byte compare.
  const u8* wanted = &selectPage[AUTH_PAGE::CHALLENGE_DATA];

  for (u32 i = 0; i != records; ++i) {
    const u8* record = table + i * TABLE::RECORD_SIZE;
    const u8* challenge = &record[TABLE::CHALLENGE];

    // Unused records are zero filled, a zero challenge is never asked for.
    if ((challenge[0] | challenge[1] | challenge[2] | challenge[3]) == 0) { continue; }

    if (memcmp(challenge, wanted, 4) != 0) { continue; }

    const u8* stored = &record[TABLE::RESPONSE];
    u8* out = &atapiState.security.authPage[AUTH_PAGE::RESPONSE_DATA];

    // A record whose top two response bytes are zero is an angular one: the disc stores only the angle, as a
    // little-endian halfword, and the drive supplies the other half.
    const bool angular = stored[2] == 0 && stored[3] == 0;

    out[0] = stored[0];
    out[1] = stored[1];
    if (angular) {
      // HvxDvdAuthVerifyAuthPage reads this field little endian for an angular entry and requires its high half-word to
      // equal the high halfword of the table's response, which is the first two bytes of the challenge it just sent.
      out[2] = wanted[1];
      out[3] = wanted[0];
    } else {
      out[2] = stored[2];
      out[3] = stored[3];
    }

    DEBUGP("Disc challenge answered from the disc's own table: record {} at control data {:#x}, {} answer "
           "{:02X}{:02X}{:02X}{:02X}{}.",
           i, TABLE::BASE + i * TABLE::RECORD_SIZE, angular ? "angular" : "exact", out[0], out[1], out[2], out[3],
           angular ? fmt::format(" ({} degrees)", static_cast<u32>(stored[0]) | (static_cast<u32>(stored[1]) << 8))
                   : std::string());

    return true;
  }

  // Not every entry is answered from this table. Types 1 and 0xE0 keep their answers in the raw security-sector records
  // instead, past the end of what READ DVD STRUCTURE returns.
  DEBUGP("Disc challenge: challenge word {:02X}{:02X}{:02X}{:02X} is not in the table at control data {:#x} - "
         "trying the raw security-sector records.",
         wanted[0], wanted[1], wanted[2], wanted[3], TABLE::BASE);

  if (answerFromRawSecuritySector(selectPage)) { return true; }

  LOG_WARNING(ODD,
              "Disc challenge: challenge word {:02X}{:02X}{:02X}{:02X} could not be answered from either table. "
              "Either the security data does not belong to this disc, or it was captured in a format that does "
              "not carry them.",
              wanted[0], wanted[1], wanted[2], wanted[3]);
  return false;
}

// Answer a challenge from the raw security-sector records, for the two entry types the table does not carry. Matched on
// the challenge ID. The answer is the low 16 bits of each of the record's two 24-bit fields.
bool Xe::PCIDev::ODD::answerFromRawSecuritySector(const u8* selectPage) {
  using RAW = RAW_SS_RECORDS;

  if (!atapiState.security.rawSectorPresent) {
    LOG_WARNING(ODD,
                "Disc challenge: no raw security sector is loaded, so the records at {:#x} that answer the "
                "type 1 and type 0xE0 entries are not available. Supply the full 2048-byte sector rather "
                "than a 1640-byte control-data capture.",
                RAW::BASE);
    return false;
  }

  const u8 wantedId = selectPage[AUTH_PAGE::CHALLENGE_INDEX];

  for (u32 i = 0; i != RAW::COUNT; ++i) {
    const u8* record = &atapiState.security.rawSector[RAW::BASE + i * RAW::RECORD_SIZE];
    if (record[RAW::ID] != wantedId) { continue; }
    if (!RAW::CarriesResponse(record[RAW::TYPE])) {
      LOG_WARNING(ODD,
                  "Disc challenge: raw record {} (id {:#04x}) is response type {:#04x}, whose fields are sector "
                  "bounds rather than an answer.",
                  i, record[RAW::ID], record[RAW::TYPE]);
      return false;
    }

    // The answer is the low half of each 24-bit field.
    const u8 hi[2] = {record[RAW::FIELD1 + 1], record[RAW::FIELD1 + 2]};
    const u8 lo[2] = {record[RAW::FIELD2 + 1], record[RAW::FIELD2 + 2]};

    u8* out = &atapiState.security.authPage[AUTH_PAGE::RESPONSE_DATA];
    const bool angular = RAW::IsAngular(record[RAW::TYPE]);
    if (angular) {
      // Read with lwbrx, so the same 32-bit value little endian.
      out[0] = lo[1];
      out[1] = lo[0];
      out[2] = hi[1];
      out[3] = hi[0];
    } else {
      // Read with lwz, so big endian, which is the order the fields are in.
      out[0] = hi[0];
      out[1] = hi[1];
      out[2] = lo[0];
      out[3] = lo[1];
    }

    DEBUGP("Disc challenge answered from the raw security sector: record {} at {:#x}, id {:#04x}, response type "
           "{:#04x} ({}), answer {:02X}{:02X}{:02X}{:02X}.",
           i, RAW::BASE + i * RAW::RECORD_SIZE, record[RAW::ID], record[RAW::TYPE], angular ? "angular" : "exact",
           out[0], out[1], out[2], out[3]);

    return true;
  }

  LOG_WARNING(ODD, "Disc challenge: no raw security-sector record carries challenge id {:#04x}.", wantedId);
  return false;
}

// MODE SENSE(10) page 0x3E: both the first step of disc authentication and the read-back half of every challenge round.

// The drive keeps its page in plaintext and applies the transport layer here.
void Xe::PCIDev::ODD::scsiModeSense10Page3ECommand(u16 allocLen) {
  if (!atapiState.security.authPagePresent) {
    // No security data: abort the way a drive that cannot read the auth area would.
    LOG_WARNING(ODD, "MODE SENSE(10) page 0x3E requested but no disc authentication page is loaded - "
                     "disc authentication cannot pass.");
    failWithSense(0x05, 0x24, 0x00); // ILLEGAL REQUEST / invalid field in CDB.
    return;
  }

  // Serve a copy, the stored page stays plaintext so the next challenge round, and a re-authentication under a fresh
  // session key, still work off it.
  u8 wire[AUTH_PAGE::SIZE];
  memcpy(wire, atapiState.security.authPage, sizeof(wire));

  DEBUGP("MODE SENSE(10) page 0x3E: serving {} bytes, disc type [0x0D] = {:#04x}, AP level [0x0E] = {}, response "
         "[0x14] = {:02X}{:02X}{:02X}{:02X}.",
         std::min<u32>(sizeof(wire), allocLen), wire[AUTH_PAGE::DISC_TYPE], wire[AUTH_PAGE::AP_LEVEL],
         wire[AUTH_PAGE::RESPONSE_DATA], wire[AUTH_PAGE::RESPONSE_DATA + 1], wire[AUTH_PAGE::RESPONSE_DATA + 2],
         wire[AUTH_PAGE::RESPONSE_DATA + 3]);

  if (!cryptAuthPageBody(wire, true)) {
    LOG_ERROR(ODD, "MODE SENSE(10) page 0x3E cannot be encrypted, so the hypervisor will reject it. AP2.0 drive "
                   "authentication has to succeed before disc authentication is attempted.");
  }

  scsiDataIn(wire, sizeof(wire), allocLen);
}

// Put the control data into the form the drive actually transmits.

// A drive does not serve the structure raw. HvxDvdAuthRecordXControl AES-CBC-decrypts controlData[0x15 .. 0x15+1616]
// under the AP2.0 session key, with the authentication page's bytes 26..41 as the IV, before it verifies anything.
bool Xe::PCIDev::ODD::encryptControlData(u8* controlData, u32 length) {
  // Immediately after the 4-byte header and the 17-byte layer descriptor.
  constexpr u32 CIPHER_OFFSET = 0x15;
  constexpr u32 CIPHER_LENGTH = 1616; // 101 AES blocks.

  if (!ap20SessionKeyValid) {
    LOG_WARNING(ODD, "Control data requested before AP2.0 drive authentication established a session key - "
                     "serving it unencrypted, which the hypervisor will reject.");
    return false;
  }

  if (!atapiState.security.authPagePresent) {
    LOG_WARNING(ODD, "Control data requested with no authentication page to take the IV from - serving it "
                     "unencrypted.");
    return false;
  }

  if (length < CIPHER_OFFSET + CIPHER_LENGTH) {
    LOG_ERROR(ODD, "Control data is {} bytes, too short to hold the {} byte encrypted region at {:#x}.", length,
              CIPHER_LENGTH, CIPHER_OFFSET);
    return false;
  }

  u8 iv[16] = {};
  memcpy(iv, &atapiState.security.authPage[AUTH_PAGE::SESSION_IV], sizeof(iv));

  std::unique_ptr<u8[]> cipher = std::make_unique<u8[]>(CIPHER_LENGTH);
  const plusaes::Error err = plusaes::encrypt_cbc(&controlData[CIPHER_OFFSET], CIPHER_LENGTH, ap20SessionKey,
                                                  sizeof(ap20SessionKey), &iv, cipher.get(), CIPHER_LENGTH, false);
  if (err != plusaes::kErrorOk) {
    LOG_ERROR(ODD, "AES-CBC encrypt of the control data failed (plusaes error {}).", static_cast<int>(err));
    return false;
  }

  memcpy(&controlData[CIPHER_OFFSET], cipher.get(), CIPHER_LENGTH);

  DEBUGP("Control data encrypted under the AP2.0 session key ({:02X}{:02X}..{:02X}{:02X}), IV "
         "{:02X}{:02X}..{:02X}{:02X}, {} bytes from {:#x}.",
         ap20SessionKey[0], ap20SessionKey[1], ap20SessionKey[14], ap20SessionKey[15], iv[0], iv[1], iv[14], iv[15],
         CIPHER_LENGTH, CIPHER_OFFSET);
  return true;
}

// READ DVD STRUCTURE (0xAD) - DVDX2 control data.
void Xe::PCIDev::ODD::scsiReadDvdStructureCommand() {
  if (!atapiState.security.controlDataPresent) {
    LOG_WARNING(ODD, "READ DVD STRUCTURE requested but no disc control data is loaded - "
                     "disc authentication cannot pass.");
    failWithSense(0x05, 0x24, 0x00); // ILLEGAL REQUEST / invalid field in CDB.
    return;
  }

  // READ DVD STRUCTURE is a 12-byte CDB: the allocation length is bytes 8..9.
  const u16 allocLen = (static_cast<u16>(atapiState.scsiCBD.AsByte[8]) << 8) | atapiState.scsiCBD.AsByte[9];

  DEBUGP("READ DVD STRUCTURE: serving {} bytes, PFI byte 0 [4] = {:#04x}, challenge table version [0x304] = {}, "
         "challenge count [0x305] = {}.",
         std::min<u32>(sizeof(atapiState.security.controlData), allocLen), atapiState.security.controlData[4],
         atapiState.security.controlData[0x304], atapiState.security.controlData[0x305]);

  // Serve a copy, the stored structure stays plaintext so a later re-authentication can encrypt it again under a fresh
  // session key.
  u8 wire[sizeof(atapiState.security.controlData)];
  memcpy(wire, atapiState.security.controlData, sizeof(wire));
  encryptControlData(wire, sizeof(wire));

  scsiDataIn(wire, sizeof(wire), allocLen);
}

//
// Debug name tables
//

static const std::unordered_map<u8, const std::string> ataCommandNameMap = {{0x08, "DEVICE_RESET"},
                                                                            {0x20, "READ_SECTORS"},
                                                                            {0x25, "READ_DMA_EXT"},
                                                                            {0x27, "READ_NATIVE_MAX_ADDRESS_EXT"},
                                                                            {0x30, "WRITE_SECTORS"},
                                                                            {0x35, "WRITE_DMA_EXT"},
                                                                            {0x40, "READ_VERIFY_SECTORS"},
                                                                            {0x42, "READ_VERIFY_SECTORS_EXT"},
                                                                            {0x60, "READ_FPDMA_QUEUED"},
                                                                            {0x91, "SET_DEVICE_PARAMETERS"},
                                                                            {0xA0, "PACKET"},
                                                                            {0xA1, "IDENTIFY_PACKET_DEVICE"},
                                                                            {0xC4, "READ_MULTIPLE"},
                                                                            {0xC5, "WRITE_MULTIPLE"},
                                                                            {0xC6, "SET_MULTIPLE_MODE"},
                                                                            {0xC8, "READ_DMA"},
                                                                            {0xCA, "WRITE_DMA"},
                                                                            {0xE0, "STANDBY_IMMEDIATE"},
                                                                            {0xE1, "IDLE_IMMEDIATE"},
                                                                            {0xE7, "FLUSH_CACHE"},
                                                                            {0xEC, "IDENTIFY_DEVICE"},
                                                                            {0xEF, "SET_FEATURES"},
                                                                            {0xF1, "SECURITY_SET_PASSWORD"},
                                                                            {0xF2, "SECURITY_UNLOCK"},
                                                                            {0xF6, "SECURITY_DISABLE_PASSWORD"}};

// Returns the command name as an std::string.
std::string Xe::PCIDev::ODD::getATACommandName(u32 commandID) {
  auto it = ataCommandNameMap.find(commandID);
  if (it != ataCommandNameMap.end()) {
    return it->second;
  } else {
    LOG_ERROR(ODD, "Unknown Command: {:#x}", commandID);
    return "Unknown Command";
  }
}

static const std::unordered_map<u8, const std::string> scsiCommandNameMap = {
  // 6 Byte 'Standard' CDB
  {0x00, "TEST_UNIT_READY"},
  {0x03, "REQUEST_SENSE"},
  {0x04, "FORMAT_UNIT"},
  {0x12, "INQUIRY"},
  {0x15, "MODE_SELECT6"},
  {0x1A, "MODE_SENSE6"},
  {0x1B, "START_STOP"},
  {0x1E, "TOGGLE_LOCK"},
  // 10 Byte CDB
  {0x23, "READ_FMT_CAP"},
  {0x25, "READ_CAPACITY"},
  {0x28, "READ10"},
  {0x2B, "SEEK10"},
  {0x2C, "ERASE10"},
  {0x2A, "WRITE10"},
  {0x2E, "VER_WRITE10"},
  {0x2F, "VERIFY10"},
  {0x35, "SYNC_CACHE"},
  {0x3B, "WRITE_BUF"},
  {0x3C, "READ_BUF"},
  {0x42, "READ_SUBCH"},
  {0x43, "READ_TOC"},
  {0x44, "READ_HEADER"},
  {0x45, "PLAY_AUDIO10"},
  {0x46, "GET_CONFIG"},
  {0x47, "PLAY_AUDIOMSF"},
  {0x4A, "EVENT_INFO"},
  {0x4B, "TOGGLE_PAUSE"},
  {0x4E, "STOP"},
  {0x51, "READ_INFO"},
  {0x52, "READ_TRK_INFO"},
  {0x53, "RES_TRACK"},
  {0x54, "SEND_OPC"},
  {0x55, "MODE_SELECT10"},
  {0x58, "REPAIR_TRACK"},
  {0x5A, "MODE_SENSE10"},
  {0x5B, "CLOSE_TRACK"},
  {0x5C, "READ_BUF_CAP"},
  // 12 Byte CDB
  {0xA1, "BLANK"},
  {0xA3, "SEND_KEY"},
  {0xA4, "REPORT_KEY"},
  {0xA5, "PLAY_AUDIO12"},
  {0xA6, "LOAD_CD"},
  {0xA7, "SET_RD_AHEAD"},
  {0xA8, "READ12"},
  {0xAA, "WRITE12"},
  {0xAC, "GET_PERF"},
  {0xAD, "READ_DVD_S"},
  {0xB6, "SET_STREAM"},
  {0xB9, "READ_CD_MSF"},
  {0xBA, "SCAN"},
  {0xBB, "SET_CD_SPEED"},
  {0xBC, "PLAY_CD"},
  {0xBD, "MECH_STATUS"},
  {0xBE, "READ_CD"},
  {0xBF, "SEND_DVD_S"}};

// Returns the command name as an std::string.
std::string Xe::PCIDev::ODD::getSCSICommandName(u32 commandID) {
  auto it = scsiCommandNameMap.find(commandID);
  if (it != scsiCommandNameMap.end()) {
    return it->second;
  } else {
    LOG_ERROR(ODD, "Unknown Command: {:#x}", commandID);
    return "Unknown Command";
  }
}

static const std::unordered_map<u8, const std::string> atapiRegisterNameMap
  = {{0x00, "Data"},
     {0x01, "Error (Read)/Features (Write)"},
     {0x02, "Interrupt Reason (Read)/ Sector Count (Write)"},
     {0x03, "Lba Low"},
     {0x04, "Byte Count Low"},
     {0x05, "Byte Count High"},
     {0x06, "Device Select"},
     {0x07, "Status (Read)/ Command (Write)"},
     {0x0A, "Alternative Status (Read)/ Device Control (Write)"},
     {0x10, "SStatus"},
     {0x14, "SError"},
     {0x18, "SControl"},
     {0x1C, "SActive"},
     {0x20, "DMA Command"},
     {0x22, "DMA Status"},
     {0x24, "DMA Table Offset"}};

// Returns the register name as an std::string.
std::string Xe::PCIDev::ODD::getATAPIRegisterName(u32 regID) {
  auto it = atapiRegisterNameMap.find(regID);
  if (it != atapiRegisterNameMap.end()) {
    return it->second;
  } else {
    LOG_ERROR(ODD, "Unknown Register: {:#x}", regID);
    return "Unknown register";
  }
}
