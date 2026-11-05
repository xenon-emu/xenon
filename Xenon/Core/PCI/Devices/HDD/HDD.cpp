/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "HDD.h"

#include "Base/Logging/Log.h"

#include <algorithm>
#include <bit>
#include <utility>

// Every command the drive executes, with its task file.
// #define HDD_DEBUG

#ifdef HDD_DEBUG
  #define HDD_DEBUGP(x, ...) LOG_DEBUG(HDD, "[HDD] " x, ##__VA_ARGS__)
#else
  #define HDD_DEBUGP(x, ...)
#endif

// IDENTIFY DEVICE response of a Hitachi HTS545025B9SA00 (250 GB retail drive). The capacity and geometry words are
// replaced with what the mounted image can back.
static const u8 kIdentifyDataBytes[] = {
  0x5a, 0x04, 0xff, 0x3f, 0x37, 0xc8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x31, 0x31, 0x32, 0x30, 0x38, 0x32, 0x42, 0x50, 0x32, 0x4e, 0x38, 0x33, 0x53, 0x4e, 0x33, 0x44, 0x42, 0x4b,
  0x55, 0x54, 0x03, 0x00, 0x50, 0x38, 0x04, 0x00, 0x42, 0x50, 0x4f, 0x32, 0x36, 0x43, 0x47, 0x34, 0x69, 0x48, 0x61,
  0x74, 0x68, 0x63, 0x20, 0x69, 0x54, 0x48, 0x35, 0x53, 0x35, 0x34, 0x32, 0x30, 0x42, 0x35, 0x53, 0x39, 0x30, 0x41,
  0x20, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10,
  0x80, 0x00, 0x40, 0x00, 0x0f, 0x00, 0x40, 0x00, 0x02, 0x00, 0x02, 0x07, 0x00, 0xff, 0x3f, 0x10, 0x00, 0x3f, 0x00,
  0x10, 0xfc, 0xfb, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x78, 0x00, 0x78,
  0x00, 0x78, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00,
  0x02, 0x17, 0x00, 0x00, 0x5e, 0x00, 0x40, 0x00, 0xfc, 0x01, 0x28, 0x00, 0x6b, 0x74, 0x69, 0x7f, 0x63, 0x61, 0x69,
  0x74, 0x49, 0xbc, 0x63, 0x61, 0x7f, 0x10, 0x29, 0x00, 0x2a, 0x00, 0x80, 0x40, 0xfe, 0xff, 0x00, 0x00, 0xfe, 0x80,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x59, 0x1c, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x88, 0x00, 0x50, 0xa6, 0xcc, 0xcf, 0x6c, 0xdc, 0xb5, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x40, 0x1c, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x0b, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
  0x4d, 0x32, 0x00, 0x00, 0x00, 0x00, 0x81, 0x72, 0x45, 0x45, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x15, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x10, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xc7, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa5, 0xc2};
static_assert(sizeof(kIdentifyDataBytes) == 512);

// Status register.
constexpr u8 kStatusBsy = 0x80;
constexpr u8 kStatusDrdy = 0x40;
constexpr u8 kStatusDsc = 0x10;
constexpr u8 kStatusDrq = 0x08;
constexpr u8 kStatusErr = 0x01;
// Status of a ready, idle drive.
constexpr u8 kStatusReady = kStatusDrdy | kStatusDsc;
// Status with no PHY (SControl DET 4): a floating bus.
constexpr u8 kStatusNoDevice = 0x7F;

// Error register.
constexpr u8 kErrorUnc = 0x40;
constexpr u8 kErrorIdnf = 0x10;
constexpr u8 kErrorAbrt = 0x04;
// Diagnostic code after a reset or EXECUTE DEVICE DIAGNOSTIC: device 0 passed.
constexpr u8 kErrorDiagnosticPassed = 0x01;

// Device Control register.
constexpr u8 kDevCtlNien = 0x02;
constexpr u8 kDevCtlSrst = 0x04;
constexpr u8 kDevCtlHob = 0x80;

// Device register: DEV selects device 1, which a SATA port does not have.
constexpr u8 kDeviceDev1 = 0x10;

// Bus master command and status.
constexpr u8 kBmCmdStart = 0x01;
constexpr u8 kBmCmdToMemory = 0x08;
constexpr u8 kBmStatusActive = 0x01;
constexpr u8 kBmStatusError = 0x02;
constexpr u8 kBmStatusInterrupt = 0x04;
constexpr u8 kBmStatusDriveCapable = 0x60;

// SError after power-on.
constexpr u32 kSErrorPowerOn = 0x001D0003;

constexpr u64 kSectorSize = ATA_SECTOR_SIZE;
// Sectors a 28-bit command can reach.
constexpr u64 kLba28Limit = 0x10000000;
// Retail Partition1 starts here.
constexpr u64 kPartition1Start = 0x987580;

// SMART command signature in LBA Mid/High.
constexpr u8 kSmartLbaMid = 0x4F;
constexpr u8 kSmartLbaHigh = 0xC2;

namespace AtaCmd {
  constexpr u8 Nop = 0x00;
  constexpr u8 DeviceReset = 0x08;
  constexpr u8 ReadSectors = 0x20;
  constexpr u8 ReadSectorsNoRetry = 0x21;
  constexpr u8 ReadSectorsExt = 0x24;
  constexpr u8 ReadDmaExt = 0x25;
  constexpr u8 ReadNativeMaxExt = 0x27;
  constexpr u8 ReadMultipleExt = 0x29;
  constexpr u8 ReadLogExt = 0x2F;
  constexpr u8 WriteSectors = 0x30;
  constexpr u8 WriteSectorsNoRetry = 0x31;
  constexpr u8 WriteSectorsExt = 0x34;
  constexpr u8 WriteDmaExt = 0x35;
  constexpr u8 SetMaxExt = 0x37;
  constexpr u8 WriteMultipleExt = 0x39;
  constexpr u8 WriteDmaFuaExt = 0x3D;
  constexpr u8 ReadVerify = 0x40;
  constexpr u8 ReadVerifyNoRetry = 0x41;
  constexpr u8 ReadVerifyExt = 0x42;
  constexpr u8 Seek = 0x70;
  constexpr u8 ExecuteDiagnostic = 0x90;
  constexpr u8 InitializeParameters = 0x91;
  constexpr u8 DownloadMicrocode = 0x92;
  constexpr u8 StandbyImmediateAlt = 0x94;
  constexpr u8 IdleImmediateAlt = 0x95;
  constexpr u8 StandbyAlt = 0x96;
  constexpr u8 IdleAlt = 0x97;
  constexpr u8 CheckPowerModeAlt = 0x98;
  constexpr u8 SleepAlt = 0x99;
  constexpr u8 Packet = 0xA0;
  constexpr u8 IdentifyPacket = 0xA1;
  constexpr u8 Smart = 0xB0;
  constexpr u8 ReadMultiple = 0xC4;
  constexpr u8 WriteMultiple = 0xC5;
  constexpr u8 SetMultiple = 0xC6;
  constexpr u8 ReadDma = 0xC8;
  constexpr u8 ReadDmaNoRetry = 0xC9;
  constexpr u8 WriteDma = 0xCA;
  constexpr u8 WriteDmaNoRetry = 0xCB;
  constexpr u8 WriteMultipleFuaExt = 0xCE;
  constexpr u8 StandbyImmediate = 0xE0;
  constexpr u8 IdleImmediate = 0xE1;
  constexpr u8 Standby = 0xE2;
  constexpr u8 Idle = 0xE3;
  constexpr u8 ReadBuffer = 0xE4;
  constexpr u8 CheckPowerMode = 0xE5;
  constexpr u8 Sleep = 0xE6;
  constexpr u8 FlushCache = 0xE7;
  constexpr u8 WriteBuffer = 0xE8;
  constexpr u8 FlushCacheExt = 0xEA;
  constexpr u8 Identify = 0xEC;
  constexpr u8 SetFeatures = 0xEF;
  constexpr u8 SecuritySetPassword = 0xF1;
  constexpr u8 SecurityUnlock = 0xF2;
  constexpr u8 SecurityErasePrepare = 0xF3;
  constexpr u8 SecurityEraseUnit = 0xF4;
  constexpr u8 SecurityFreezeLock = 0xF5;
  constexpr u8 SecurityDisablePassword = 0xF6;
  constexpr u8 ReadNativeMax = 0xF8;
  constexpr u8 SetMax = 0xF9;
} // namespace AtaCmd

namespace SetFeature {
  constexpr u8 EnableWriteCache = 0x02;
  constexpr u8 SetTransferMode = 0x03;
  constexpr u8 EnableApm = 0x05;
  constexpr u8 EnableSataFeature = 0x10;
  constexpr u8 EnableAam = 0x42;
  constexpr u8 DisableReadLookAhead = 0x55;
  constexpr u8 DisableRevertToDefaults = 0x66;
  constexpr u8 DisableWriteCache = 0x82;
  constexpr u8 DisableApm = 0x85;
  constexpr u8 DisableSataFeature = 0x90;
  constexpr u8 EnableReadLookAhead = 0xAA;
  constexpr u8 DisableAam = 0xC2;
  constexpr u8 EnableRevertToDefaults = 0xCC;
} // namespace SetFeature

namespace SmartFeature {
  constexpr u8 ReadData = 0xD0;
  constexpr u8 ReadThresholds = 0xD1;
  constexpr u8 Autosave = 0xD2;
  constexpr u8 SaveAttributes = 0xD3;
  constexpr u8 OfflineImmediate = 0xD4;
  constexpr u8 ReadLog = 0xD5;
  constexpr u8 Enable = 0xD8;
  constexpr u8 Disable = 0xD9;
  constexpr u8 ReturnStatus = 0xDA;
  constexpr u8 AutoOffline = 0xDB;
} // namespace SmartFeature

// IDENTIFY DEVICE words and bits the drive keeps current.
constexpr u32 kIdMaxMultiple = 47;   // bits 7:0 sectors per READ/WRITE MULTIPLE block
constexpr u32 kIdMultiple = 59;      // bit 8 valid, bits 7:0 current setting
constexpr u32 kIdMwdma = 63;         // bits 2:0 supported, 10:8 selected
constexpr u32 kIdPioModes = 64;      // bit 0 PIO3, bit 1 PIO4
constexpr u32 kIdSataSupported = 78; // SATA features supported
constexpr u32 kIdSataEnabled = 79;   // SATA features enabled
constexpr u32 kIdEnabled1 = 85;      // bit 0 SMART, bit 5 write cache, bit 6 read look-ahead
constexpr u32 kIdEnabled2 = 86;      // bit 3 APM, bit 9 AAM
constexpr u32 kIdUdma = 88;          // bits 6:0 supported, 14:8 selected
constexpr u32 kIdApmLevel = 91;      // bits 7:0 current APM level
constexpr u32 kIdAamLevel = 94;      // bits 7:0 current AAM level
constexpr u32 kIdSecurity = 128;     // bit 3 frozen
constexpr u32 kIdIntegrity = 255;    // bits 7:0 0xA5, 15:8 checksum

// SMART data structures end in a checksum byte that makes the 512 byte sum zero.
static void SealSmartSector(u8 *sector) {
  u8 sum = 0;
  for (u32 i = 0; i < 511; ++i) { sum = static_cast<u8>(sum + sector[i]); }
  sector[511] = static_cast<u8>(0x100 - sum);
}

// SMART attributes this drive reports.
struct SmartAttribute {
  u8 id;
  u16 flags;
  u8 value;
  u8 threshold;
  u64 raw;
};
constexpr SmartAttribute kSmartAttributes[] = {
  {0x01, 0x000B, 100, 62, 0}, // Raw read error rate
  {0x02, 0x0005, 100, 40, 0}, // Throughput performance
  {0x03, 0x0007, 100, 33, 0}, // Spin-up time
  {0x04, 0x0012, 100, 0, 1},  // Start/stop count
  {0x05, 0x0033, 100, 5, 0},  // Reallocated sector count
  {0x07, 0x000B, 100, 67, 0}, // Seek error rate
  {0x09, 0x0012, 100, 0, 0},  // Power-on hours (raw filled in at read time)
  {0x0A, 0x0013, 100, 60, 0}, // Spin retry count
  {0x0C, 0x0032, 100, 0, 1},  // Power cycle count
  {0xC2, 0x0002, 166, 0, 36}, // Temperature, 36 C
  {0xC4, 0x0032, 100, 0, 0},  // Reallocation event count
  {0xC5, 0x0022, 100, 0, 0},  // Current pending sector count
  {0xC6, 0x0008, 100, 0, 0},  // Offline uncorrectable
  {0xC7, 0x000A, 200, 0, 0},  // UDMA CRC error count
};

Xe::PCIDev::HDD::HDD(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram)
    : PCIDevice(deviceName, size)
    , parentBus(parentPCIBridge)
    , dma(ram) {

  // The ATA controller in the southbridge has two BARs. BAR0 holds the task file, Device Control/AltStatus at 0xA and
  // the SATA SCRs at 0x10..0x1F, BAR1 holds the bus master DMA block.

  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.id.hexData = 0x58031414;
  pciConfigSpace.configSpaceHeader.commandStatus.hexData = 0x02300006;
  pciConfigSpace.configSpaceHeader.classRev.hexData = 0x01060000;
  pciConfigSpace.configSpaceHeader.capabilities.hexData = 0x00000058; // Capabilites Ptr.
  pciConfigSpace.configSpaceHeader.interrupt.hexData = 0x00000100;    // Int line, pin.

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

  // Mount our HDD image according to config.
  image = std::make_unique<Base::ReadWriteStorage>(Config::filepaths.hddImage);
  if (image->isHandleValid()) {
    imageSizeInBytes = image->Size();
    imageAttached = imageSizeInBytes >= kSectorSize;
  }

  if (imageAttached) {
    sectorCount = imageSizeInBytes / kSectorSize;
    LOG_INFO(HDD, "Mounted image: {:#x} bytes, {} sectors.", imageSizeInBytes, sectorCount);
    if (imageSizeInBytes % kSectorSize) {
      LOG_WARNING(HDD, "Image size is not a multiple of 512 bytes, the last {} bytes are not reachable.",
                  imageSizeInBytes % kSectorSize);
    }
    // The kernel lays Partition1 out from sector 0x987580 onwards, so an image smaller than that leaves the guest with
    // no usable data partition.
    if (sectorCount <= kPartition1Start) {
      LOG_WARNING(HDD, "Image is smaller than the 0x987580 sector start of Partition1, the guest will see no "
                       "usable data partition.");
    }
  } else {
    LOG_INFO(HDD, "No HDD image found, disabling device.");
    image.reset();
  }

  BuildIdentify();

  // Power-on state.
  scr.Reset(imageAttached, kSErrorPowerOn);
  if (imageAttached) {
    SetSignature();
    tf.error = kErrorDiagnosticPassed;
    tf.status = kStatusReady;
  } else {
    // No drive: SStatus reports nothing attached, so the kernel skips the channel. XeLL probes the task file instead
    // and expects DRDY and an LBA Low register that ignores writes.
    tf.status = kStatusDrdy;
  }
  scr.Mirror(pciConfigSpace);

  if (imageAttached) {
    hddWorkerThread.Start("HDD", [this] { WorkerLoop(); });
  }
}

Xe::PCIDev::HDD::~HDD() {
  {
    std::lock_guard lock(ataMutex);
    workerStop = true;
  }
  workCV.notify_all();
  hddWorkerThread.Join();
  if (image && imageDirty) { image->Flush(); }
}

// MMIO

// BAR0 is the command block, BAR1 the bus master block.
void Xe::PCIDev::HDD::BarRead(u32 bar, u64 barOffset, u8 *data, u64 size) {
  memset(data, 0, size);
  std::lock_guard lock(ataMutex);
  const u32 reg = static_cast<u32>(barOffset);
  if (bar == 0) {
    if (reg == ATA_REG_DATA) {
      ReadDataPort(data, size);
    } else if (reg >= ATA_REG_SSTATUS) {
      scr.Read(reg - ATA_REG_SSTATUS, data, size);
    } else {
      for (u64 i = 0; i < size && reg + i < ATA_REG_SSTATUS; ++i) {
        data[i] = ReadTaskFileRegister(static_cast<u32>(reg + i));
      }
    }
  } else {
    const u8 busMaster[8] = {dmaCommand,
                             0,
                             dmaStatus,
                             0,
                             static_cast<u8>(dmaPrdTable),
                             static_cast<u8>(dmaPrdTable >> 8),
                             static_cast<u8>(dmaPrdTable >> 16),
                             static_cast<u8>(dmaPrdTable >> 24)};
    for (u64 i = 0; i < size && reg + i < sizeof(busMaster); ++i) { data[i] = busMaster[reg + i]; }
  }
}

void Xe::PCIDev::HDD::BarWrite(u32 bar, u64 barOffset, const u8 *data, u64 size) {
  std::lock_guard lock(ataMutex);
  const u32 reg = static_cast<u32>(barOffset);
  if (bar == 0) {
    if (reg == ATA_REG_DATA) {
      WriteDataPort(data, size);
    } else if (reg >= ATA_REG_SSTATUS) {
      WriteScr(reg - ATA_REG_SSTATUS, data, size);
    } else {
      for (u64 i = 0; i < size && reg + i < ATA_REG_SSTATUS; ++i) {
        WriteTaskFileRegister(static_cast<u32>(reg + i), data[i]);
      }
    }
  } else {
    for (u64 i = 0; i < size && reg + i < 8; ++i) {
      const u32 offset = static_cast<u32>(reg + i);
      const u8 value = data[i];
      switch (offset) {
        case ATA_REG_DMA_COMMAND: WriteDmaCommand(value); break;
        case ATA_REG_DMA_STATUS:
          // ERROR and INTERRUPT are write-1-to-clear, the drive DMA capable bits are plain storage and ACTIVE is
          // read-only (the kernel writes 0x06 before every transfer and 0x04 to ack).
          dmaStatus =
            static_cast<u8>((dmaStatus & ~(value & (kBmStatusError | kBmStatusInterrupt)) & ~kBmStatusDriveCapable)
                            | (value & kBmStatusDriveCapable));
          break;
        case ATA_REG_DMA_TABLE_OFFSET:
        case ATA_REG_DMA_TABLE_OFFSET + 1:
        case ATA_REG_DMA_TABLE_OFFSET + 2:
        case ATA_REG_DMA_TABLE_OFFSET + 3: {
          const u32 shift = (offset - ATA_REG_DMA_TABLE_OFFSET) * 8;
          dmaPrdTable = (dmaPrdTable & ~(0xFFu << shift)) | (static_cast<u32>(value) << shift);
          // The table is dword aligned.
          dmaPrdTable &= ~3u;
        } break;
        default: break;
      }
    }
  }
}

void Xe::PCIDev::HDD::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  std::lock_guard lock(ataMutex);
  PCIDevice::ConfigRead(readAddress, data, size);
}

void Xe::PCIDev::HDD::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  std::lock_guard lock(ataMutex);
  const u8 offset = static_cast<u8>(writeAddress);

  // The SATA SCRs are mirrored at 0xC0 and behave the same as through BAR0.
  if (offset >= XE_SIS_SCR_BASE && offset < XE_SIS_SCR_BASE + 0x10) {
    WriteScr(offset - XE_SIS_SCR_BASE, data, size);
    return;
  }

  PCIDevice::ConfigWrite(writeAddress, data, size);
}

// Registers

u8 Xe::PCIDev::HDD::ReadTaskFileRegister(u32 reg) {
  const bool hob = tf.deviceControl & kDevCtlHob;
  switch (reg) {
    case ATA_REG_ERROR: return tf.error;
    case ATA_REG_SECTORCOUNT: return hob ? tf.sectorCountPrev : tf.sectorCount;
    case ATA_REG_LBA_LOW: return hob ? tf.lbaLowPrev : tf.lbaLow;
    case ATA_REG_LBA_MED: return hob ? tf.lbaMidPrev : tf.lbaMid;
    case ATA_REG_LBA_HI: return hob ? tf.lbaHighPrev : tf.lbaHigh;
    case ATA_REG_DEV_SEL: return tf.device;
    case ATA_REG_STATUS:
      // A SATA port has no device 1.
      if (tf.device & kDeviceDev1) { return 0; }
      // Reading Status acknowledges INTRQ (the kernel's interrupt handler reads it before anything else).
      AcknowledgeInterrupt();
      return tf.status;
    case ATA_REG_ALT_STATUS: return (tf.device & kDeviceDev1) ? 0 : tf.status;
    default: return 0;
  }
}

void Xe::PCIDev::HDD::WriteTaskFileRegister(u32 reg, u8 value) {
  if (reg == ATA_REG_DEV_CTRL) {
    WriteDeviceControl(value);
    return;
  }
  if (reg > ATA_REG_CMD) { return; }

  // Writing any command block register clears HOB.
  tf.deviceControl &= ~kDevCtlHob;

  // The drive owns the task file while BSY is set. Writes are ignored.
  if (tf.status & kStatusBsy) {
    if (reg == ATA_REG_CMD) {
      LOG_WARNING(HDD, "{} written while the drive is busy with {}; ignored.", CommandName(value),
                  CommandName(command));
    }
    return;
  }

  switch (reg) {
    case ATA_REG_FEATURES:
      tf.featuresPrev = tf.features;
      tf.features = value;
      break;
    case ATA_REG_SECTORCOUNT:
      tf.sectorCountPrev = tf.sectorCount;
      tf.sectorCount = value;
      break;
    case ATA_REG_LBA_LOW:
      // Without a drive the register keeps its value, which is how XeLL detects an empty port.
      if (!imageAttached) { break; }
      tf.lbaLowPrev = tf.lbaLow;
      tf.lbaLow = value;
      break;
    case ATA_REG_LBA_MED:
      tf.lbaMidPrev = tf.lbaMid;
      tf.lbaMid = value;
      break;
    case ATA_REG_LBA_HI:
      tf.lbaHighPrev = tf.lbaHigh;
      tf.lbaHigh = value;
      break;
    case ATA_REG_DEV_SEL: tf.device = value; break;
    case ATA_REG_CMD: ExecuteCommand(value); break;
    default: break;
  }
}

void Xe::PCIDev::HDD::WriteDeviceControl(u8 value) {
  const u8 old = tf.deviceControl;
  tf.deviceControl = value;

  if (!(old & kDevCtlSrst) && (value & kDevCtlSrst)) {
    // SRST asserted: the drive drops the current command and stays busy until SRST clears.
    AbandonCommand();
    if (imageAttached) { tf.status = kStatusBsy; }
  } else if ((old & kDevCtlSrst) && !(value & kDevCtlSrst)) {
    DeviceReset();
  }

  // INTRQ follows nIEN.
  if (intrqPending) {
    if ((old & kDevCtlNien) && !(value & kDevCtlNien)) {
      dmaStatus |= kBmStatusInterrupt;
      parentBus->RouteInterrupt(PRIO_SATA_HDD);
    } else if (!(old & kDevCtlNien) && (value & kDevCtlNien)) {
      parentBus->CancelInterrupt(PRIO_SATA_HDD);
    }
  }
}

void Xe::PCIDev::HDD::WriteScr(u32 offset, const u8 *data, u64 size) {
  switch (scr.Write(offset, data, size)) {
    case Xe::PCI::SataScr::Link::ComResetAsserted:
      // The kernel holds DET=1 for 100 us.
      AbandonCommand();
      tf.status = kStatusBsy;
      break;
    case Xe::PCI::SataScr::Link::Offline:
      AbandonCommand();
      tf.status = kStatusNoDevice;
      break;
    case Xe::PCI::SataScr::Link::ComResetReleased:
      // The link is back up and the drive sends its signature. The kernel polls for !BSY && DRDY, no interrupt is
      // involved.
      DeviceReset();
      break;
    case Xe::PCI::SataScr::Link::None: break;
  }
  scr.Mirror(pciConfigSpace);
}

void Xe::PCIDev::HDD::WriteDmaCommand(u8 value) {
  const bool wasRunning = dmaCommand & kBmCmdStart;
  dmaCommand = value & (kBmCmdStart | kBmCmdToMemory);
  if (!wasRunning && (value & kBmCmdStart)) {
    dmaStatus |= kBmStatusActive;
    StartDmaIfReady();
  } else if (wasRunning && !(value & kBmCmdStart)) {
    // Stopping the engine ends its part. A transfer that hasn't been picked up yet is dropped, and the drive keeps
    // waiting for its data like a real one would until it is reset.
    dmaStatus &= ~kBmStatusActive;
    if (job == JobKind::Dma) {
      job = JobKind::None;
      dmaQueued = false;
    }
  }
}

void Xe::PCIDev::HDD::ReadDataPort(u8 *data, u64 size) {
  if (phase != Phase::PioIn || !(tf.status & kStatusDrq)) { return; }
  const u64 n = std::min<u64>(size, pioBlock.size() - pioPos);
  memcpy(data, pioBlock.data() + pioPos, n);
  pioPos += static_cast<u32>(n);
  if (pioPos < pioBlock.size()) { return; }

  // Block done. The next one is announced with DRQ and an interrupt, the end of the command just clears DRQ (the
  // kernel completes after reading the last block).
  if (pioTarget == PioTarget::Disk && xfer.sectors != 0) {
    if (LoadNextPioInBlock()) {
      tf.status = kStatusReady | kStatusDrq;
      RaiseInterrupt();
    }
  } else if (pioTarget == PioTarget::Buffer && !pioPending.empty()) {
    const size_t next = std::min<size_t>(kSectorSize, pioPending.size());
    pioBlock.assign(pioPending.begin(), pioPending.begin() + next);
    pioPending.erase(pioPending.begin(), pioPending.begin() + next);
    pioPos = 0;
    tf.status = kStatusReady | kStatusDrq;
    RaiseInterrupt();
  } else {
    phase = Phase::Idle;
    tf.status = kStatusReady;
  }
}

void Xe::PCIDev::HDD::WriteDataPort(const u8 *data, u64 size) {
  if (phase != Phase::PioOut || !(tf.status & kStatusDrq)) { return; }
  const u64 n = std::min<u64>(size, pioBlock.size() - pioPos);
  memcpy(pioBlock.data() + pioPos, data, n);
  pioPos += static_cast<u32>(n);
  if (pioPos >= pioBlock.size()) { FinishPioOutBlock(); }
}

// Commands

void Xe::PCIDev::HDD::ExecuteCommand(u8 cmd) {
  // A SATA port has no device 1.
  if (tf.device & kDeviceDev1) { return; }

  // A new command replaces a data phase still in progress, and writing Command clears INTRQ.
  if (phase != Phase::Idle) {
    LOG_WARNING(HDD, "{} issued while {} was still in its data phase.", CommandName(cmd), CommandName(command));
    AbandonCommand();
  }
  AcknowledgeInterrupt();

  command = cmd;
  tf.error = 0;
  tf.status = kStatusReady;

  HDD_DEBUGP("Command {} ({:#04x}) features {:#x} count {:#x} lba {:#x}/{:#x} device {:#x}", CommandName(cmd), cmd,
             tf.features, Count48(), Lba28(), Lba48(), tf.device);

  if (!imageAttached) {
    // XeLL only needs IDENTIFY to complete without a drive. It gets zeros.
    if (cmd == AtaCmd::Identify) {
      BeginPioIn(std::vector<u8>(kSectorSize, 0));
    } else {
      Abort();
    }
    return;
  }

  // Everything but the power commands spins the drive up.
  switch (cmd) {
    case AtaCmd::CheckPowerMode:
    case AtaCmd::CheckPowerModeAlt:
    case AtaCmd::StandbyImmediate:
    case AtaCmd::StandbyImmediateAlt:
    case AtaCmd::Standby:
    case AtaCmd::StandbyAlt:
    case AtaCmd::Sleep:
    case AtaCmd::SleepAlt: break;
    default: standby = false; break;
  }

  // RECALIBRATE (0x10..0x1F): nothing to do.
  if ((cmd & 0xF0) == 0x10) {
    CompleteOk();
    return;
  }

  switch (cmd) {
    // PIO reads and writes.
    case AtaCmd::ReadSectors:
    case AtaCmd::ReadSectorsNoRetry: BeginDiskPio(true, false, false); break;
    case AtaCmd::ReadSectorsExt: BeginDiskPio(true, true, false); break;
    case AtaCmd::ReadMultiple: BeginDiskPio(true, false, true); break;
    case AtaCmd::ReadMultipleExt: BeginDiskPio(true, true, true); break;
    case AtaCmd::WriteSectors:
    case AtaCmd::WriteSectorsNoRetry: BeginDiskPio(false, false, false); break;
    case AtaCmd::WriteSectorsExt: BeginDiskPio(false, true, false); break;
    case AtaCmd::WriteMultiple: BeginDiskPio(false, false, true); break;
    case AtaCmd::WriteMultipleExt: BeginDiskPio(false, true, true); break;
    case AtaCmd::WriteMultipleFuaExt: BeginDiskPio(false, true, true, true); break;

    // DMA reads and writes (the kernel uses 0xC8/0xCA below 0x10000000 sectors, else 0x25/0x35).
    case AtaCmd::ReadDma:
    case AtaCmd::ReadDmaNoRetry: BeginDma(true, false, false); break;
    case AtaCmd::ReadDmaExt: BeginDma(true, true, false); break;
    case AtaCmd::WriteDma:
    case AtaCmd::WriteDmaNoRetry: BeginDma(false, false, false); break;
    case AtaCmd::WriteDmaExt: BeginDma(false, true, false); break;
    case AtaCmd::WriteDmaFuaExt: BeginDma(false, true, true); break;

    // READ VERIFY and SEEK: the media is always readable.
    case AtaCmd::ReadVerify:
    case AtaCmd::ReadVerifyNoRetry:
    case AtaCmd::ReadVerifyExt:
    case AtaCmd::Seek: {
      const bool lba48 = cmd == AtaCmd::ReadVerifyExt;
      const u64 lba = lba48 ? Lba48() : Lba28();
      const u32 count = cmd == AtaCmd::Seek ? 1 : (lba48 ? Count48() : Count28());
      if (RangeValid(lba, count, lba48)) {
        CompleteOk();
      } else {
        CompleteError(kErrorIdnf);
      }
    } break;

    // The native max address is the last LBA, not the sector count.
    case AtaCmd::ReadNativeMax: {
      const u64 max = std::min(sectorCount, kLba28Limit) - 1;
      tf.lbaLow = static_cast<u8>(max);
      tf.lbaMid = static_cast<u8>(max >> 8);
      tf.lbaHigh = static_cast<u8>(max >> 16);
      tf.device = static_cast<u8>((tf.device & 0xF0) | ((max >> 24) & 0x0F));
      CompleteOk();
    } break;
    case AtaCmd::ReadNativeMaxExt: {
      const u64 max = sectorCount - 1;
      tf.lbaLow = static_cast<u8>(max);
      tf.lbaMid = static_cast<u8>(max >> 8);
      tf.lbaHigh = static_cast<u8>(max >> 16);
      tf.lbaLowPrev = static_cast<u8>(max >> 24);
      tf.lbaMidPrev = static_cast<u8>(max >> 32);
      tf.lbaHighPrev = static_cast<u8>(max >> 40);
      CompleteOk();
    } break;

    case AtaCmd::Identify: BeginPioIn(IdentifyBytes()); break;
    case AtaCmd::SetFeatures: ExecuteSetFeatures(); break;
    case AtaCmd::Smart: ExecuteSmart(); break;
    case AtaCmd::ReadLogExt: ExecuteReadLog(true); break;

    case AtaCmd::SetMultiple: {
      // 0 turns multiple mode off, otherwise a power of two up to IDENTIFY word 47.
      const u8 count = tf.sectorCount;
      const u8 max = static_cast<u8>(identify[kIdMaxMultiple]);
      if (count != 0 && (count > max || !std::has_single_bit(count))) {
        Abort();
        break;
      }
      multipleCount = count;
      identify[kIdMultiple] = static_cast<u16>(count ? (0x0100 | count) : 0);
      UpdateIdentifyChecksum();
      CompleteOk();
    } break;

    case AtaCmd::ReadBuffer: BeginPioIn(std::vector<u8>(sectorBuffer.begin(), sectorBuffer.end())); break;
    case AtaCmd::WriteBuffer: BeginPioOut(static_cast<u32>(sectorBuffer.size())); break;

    // Power management.
    case AtaCmd::CheckPowerMode:
    case AtaCmd::CheckPowerModeAlt:
      tf.sectorCount = standby ? 0x00 : 0xFF;
      CompleteOk();
      break;
    case AtaCmd::IdleImmediate:
    case AtaCmd::IdleImmediateAlt:
    case AtaCmd::Idle:
    case AtaCmd::IdleAlt: CompleteOk(); break;
    case AtaCmd::StandbyImmediate:
    case AtaCmd::StandbyImmediateAlt:
    case AtaCmd::Standby:
    case AtaCmd::StandbyAlt:
    case AtaCmd::Sleep:
    case AtaCmd::SleepAlt:
      // The drive writes its cache back before spinning down (the kernel issues FLUSH CACHE and then STANDBY IMMEDIATE
      // on power down).
      standby = true;
      QueueFlush();
      break;

    // FLUSH CACHE: the kernel's flush and power-down paths only ever use 0xE7, 0xEA is the 48-bit form other guests
    // use.
    case AtaCmd::FlushCache:
    case AtaCmd::FlushCacheExt: QueueFlush(); break;

    case AtaCmd::ExecuteDiagnostic:
      SetSignature();
      tf.error = kErrorDiagnosticPassed;
      CompleteOk();
      break;
    case AtaCmd::InitializeParameters: CompleteOk(); break;

    case AtaCmd::SecurityFreezeLock:
      identify[kIdSecurity] |= 0x0008;
      UpdateIdentifyChecksum();
      CompleteOk();
      break;

    // Not supported by this drive: NOP (the kernel's performance pipe toggle only waits for its interrupt), packet
    // commands (not an ATAPI device), firmware download, HPA changes and passwords.
    case AtaCmd::Nop:
    case AtaCmd::DeviceReset:
    case AtaCmd::Packet:
    case AtaCmd::IdentifyPacket:
    case AtaCmd::DownloadMicrocode:
    case AtaCmd::SetMax:
    case AtaCmd::SetMaxExt:
    case AtaCmd::SecuritySetPassword:
    case AtaCmd::SecurityUnlock:
    case AtaCmd::SecurityErasePrepare:
    case AtaCmd::SecurityEraseUnit:
    case AtaCmd::SecurityDisablePassword: Abort(); break;

    default:
      if (!warnedCommands.test(cmd)) {
        warnedCommands.set(cmd);
        LOG_WARNING(HDD, "Unsupported command {} ({:#04x}) aborted.", CommandName(cmd), cmd);
      }
      Abort();
      break;
  }
}

void Xe::PCIDev::HDD::ExecuteSetFeatures() {
  const u8 count = tf.sectorCount;
  switch (tf.features) {
    case SetFeature::EnableWriteCache: identify[kIdEnabled1] |= 1u << 5; break;
    case SetFeature::DisableWriteCache: identify[kIdEnabled1] &= ~(1u << 5); break;
    case SetFeature::EnableReadLookAhead: identify[kIdEnabled1] |= 1u << 6; break;
    case SetFeature::DisableReadLookAhead: identify[kIdEnabled1] &= ~(1u << 6); break;
    case SetFeature::SetTransferMode: {
      // The kernel sends 0x44 (UDMA4) while it initialises the disk, and ERR here is fatal for it ("hard disk not DMA
      // configured").
      const u8 n = count & 0x07;
      bool supported = false;
      switch (count & 0xF8) {
        case 0x00: supported = n <= 1; break;                                              // PIO default
        case 0x08: supported = n <= 2 || (identify[kIdPioModes] & (1u << (n - 3))); break; // PIO n
        case 0x20:                                                                         // Multiword DMA n
          supported = identify[kIdMwdma] & (1u << n);
          if (supported) {
            identify[kIdMwdma] = static_cast<u16>((identify[kIdMwdma] & 0x00FF) | (0x0100u << n));
            identify[kIdUdma] &= 0x00FF;
          }
          break;
        case 0x40: // Ultra DMA n
          supported = identify[kIdUdma] & (1u << n);
          if (supported) {
            identify[kIdUdma] = static_cast<u16>((identify[kIdUdma] & 0x00FF) | (0x0100u << n));
            identify[kIdMwdma] &= 0x00FF;
          }
          break;
        default: break;
      }
      if (!supported) {
        LOG_WARNING(HDD, "SET FEATURES: transfer mode {:#04x} is not supported.", count);
        Abort();
        return;
      }
    } break;
    case SetFeature::EnableApm:
      // The kernel sets the level with 0x05 and disables APM with 0x85.
      if (count == 0x00 || count == 0xFF) {
        Abort();
        return;
      }
      identify[kIdEnabled2] |= 1u << 3;
      identify[kIdApmLevel] = static_cast<u16>((identify[kIdApmLevel] & 0xFF00) | count);
      break;
    case SetFeature::DisableApm: identify[kIdEnabled2] &= ~(1u << 3); break;
    case SetFeature::EnableAam:
      identify[kIdEnabled2] |= 1u << 9;
      identify[kIdAamLevel] = static_cast<u16>((identify[kIdAamLevel] & 0xFF00) | count);
      break;
    case SetFeature::DisableAam: identify[kIdEnabled2] &= ~(1u << 9); break;
    case SetFeature::EnableRevertToDefaults:
    case SetFeature::DisableRevertToDefaults: break;
    case SetFeature::EnableSataFeature:
    case SetFeature::DisableSataFeature:
      if (count >= 16 || !(identify[kIdSataSupported] & (1u << count))) {
        Abort();
        return;
      }
      if (tf.features == SetFeature::EnableSataFeature) {
        identify[kIdSataEnabled] |= 1u << count;
      } else {
        identify[kIdSataEnabled] &= ~(1u << count);
      }
      break;
    default:
      LOG_WARNING(HDD, "SET FEATURES: subcommand {:#04x} (count {:#04x}) is not supported.", tf.features, count);
      Abort();
      return;
  }
  UpdateIdentifyChecksum();
  CompleteOk();
}

void Xe::PCIDev::HDD::ExecuteSmart() {
  if (tf.lbaMid != kSmartLbaMid || tf.lbaHigh != kSmartLbaHigh) {
    Abort();
    return;
  }
  if (!smartEnabled && tf.features != SmartFeature::Enable) {
    Abort();
    return;
  }
  switch (tf.features) {
    case SmartFeature::ReadData: BeginPioIn(SmartData()); break;
    case SmartFeature::ReadThresholds: BeginPioIn(SmartThresholds()); break;
    case SmartFeature::ReadLog: ExecuteReadLog(false); break;
    case SmartFeature::Autosave:
    case SmartFeature::SaveAttributes:
    case SmartFeature::OfflineImmediate:
    case SmartFeature::AutoOffline: CompleteOk(); break;
    case SmartFeature::Enable:
    case SmartFeature::Disable:
      smartEnabled = tf.features == SmartFeature::Enable;
      identify[kIdEnabled1] = static_cast<u16>((identify[kIdEnabled1] & ~1u) | (smartEnabled ? 1u : 0u));
      UpdateIdentifyChecksum();
      CompleteOk();
      break;
    case SmartFeature::ReturnStatus:
      // No attribute is past its threshold.
      tf.lbaMid = kSmartLbaMid;
      tf.lbaHigh = kSmartLbaHigh;
      CompleteOk();
      break;
    default:
      LOG_WARNING(HDD, "SMART: subcommand {:#04x} is not supported.", tf.features);
      Abort();
      break;
  }
}

void Xe::PCIDev::HDD::ExecuteReadLog(bool extended) {
  const u8 log = tf.lbaLow;
  const u32 count = extended ? ((static_cast<u32>(tf.sectorCountPrev) << 8) | tf.sectorCount) : tf.sectorCount;
  const u32 page = extended ? ((static_cast<u32>(tf.lbaMidPrev) << 8) | tf.lbaMid) : 0;

  std::vector<u8> pages;
  if (log == 0x00) {
    // Log directory, version 1. SMART logging also lists the summary error and self-test logs.
    pages.assign(kSectorSize, 0);
    pages[0] = 0x01;
    if (!extended) {
      pages[2 * 0x01] = 1;
      pages[2 * 0x06] = 1;
    }
  } else if (!extended && (log == 0x01 || log == 0x06)) {
    // Summary error log (version 1) / self-test log (revision 1), both empty.
    pages.assign(kSectorSize, 0);
    pages[0] = 0x01;
    SealSmartSector(pages.data());
  }

  if (pages.empty() || count == 0 || (static_cast<u64>(page) + count) * kSectorSize > pages.size()) {
    Abort();
    return;
  }
  BeginPioIn(std::vector<u8>(pages.begin() + page * kSectorSize, pages.begin() + (page + count) * kSectorSize));
}

void Xe::PCIDev::HDD::BeginDiskPio(bool toHost, bool lba48, bool multiple, bool forceUnitAccess) {
  // READ/WRITE MULTIPLE need SET MULTIPLE MODE first.
  if (multiple && multipleCount == 0) {
    Abort();
    return;
  }
  const u64 lba = lba48 ? Lba48() : Lba28();
  const u32 count = lba48 ? Count48() : Count28();
  if (!RangeValid(lba, count, lba48)) {
    LOG_ERROR(HDD, "{}: LBA {:#x} + {} sectors is outside the {} sector image.", CommandName(command), lba, count,
              sectorCount);
    CompleteError(kErrorIdnf);
    return;
  }

  pioTarget = PioTarget::Disk;
  xfer = {lba, count, toHost, forceUnitAccess};
  drqSectors = multiple ? multipleCount : 1;

  if (toHost) {
    if (!LoadNextPioInBlock()) { return; }
    phase = Phase::PioIn;
    tf.status = kStatusReady | kStatusDrq;
    RaiseInterrupt();
  } else {
    pioBlock.assign(std::min(drqSectors, xfer.sectors) * kSectorSize, 0);
    pioPos = 0;
    phase = Phase::PioOut;
    // The first block of a data-out command is requested without an interrupt.
    tf.status = kStatusReady | kStatusDrq;
  }
}

bool Xe::PCIDev::HDD::LoadNextPioInBlock() {
  const u32 n = std::min(drqSectors, xfer.sectors);
  pioBlock.resize(n * kSectorSize);
  pioPos = 0;
  NoteActivity();
  if (!image->Read(xfer.lba * kSectorSize, pioBlock.data(), static_cast<u32>(pioBlock.size()))) {
    LOG_ERROR(HDD, "{}: image read failed at LBA {:#x} ({} sectors).", CommandName(command), xfer.lba, n);
    CompleteError(kErrorUnc);
    return false;
  }
  xfer.lba += n;
  xfer.sectors -= n;
  return true;
}

void Xe::PCIDev::HDD::FinishPioOutBlock() {
  if (pioTarget == PioTarget::Buffer) {
    std::copy_n(pioBlock.begin(), std::min(pioBlock.size(), sectorBuffer.size()), sectorBuffer.begin());
    CompleteOk();
    return;
  }

  const u32 n = static_cast<u32>(pioBlock.size() / kSectorSize);
  NoteActivity();
  if (!image->Write(xfer.lba * kSectorSize, pioBlock.data(), static_cast<u32>(pioBlock.size()))) {
    LOG_ERROR(HDD, "{}: image write failed at LBA {:#x} ({} sectors).", CommandName(command), xfer.lba, n);
    CompleteError(kErrorAbrt);
    return;
  }
  imageDirty = true;
  xfer.lba += n;
  xfer.sectors -= n;

  if (xfer.sectors != 0) {
    // Ask for the next block.
    pioBlock.assign(std::min(drqSectors, xfer.sectors) * kSectorSize, 0);
    pioPos = 0;
    tf.status = kStatusReady | kStatusDrq;
    RaiseInterrupt();
    return;
  }
  if (xfer.forceUnitAccess && image->Flush()) { imageDirty = false; }
  // Completion interrupt after the last block (the kernel waits for it).
  CompleteOk();
}

void Xe::PCIDev::HDD::BeginPioIn(std::vector<u8> data) {
  // Generated data goes out in 512 byte DRQ blocks, each announced by an interrupt.
  const size_t padded = std::max<size_t>(kSectorSize, (data.size() + kSectorSize - 1) / kSectorSize * kSectorSize);
  data.resize(padded, 0);
  pioTarget = PioTarget::Buffer;
  pioBlock.assign(data.begin(), data.begin() + kSectorSize);
  pioPending.assign(data.begin() + kSectorSize, data.end());
  pioPos = 0;
  phase = Phase::PioIn;
  tf.status = kStatusReady | kStatusDrq;
  RaiseInterrupt();
}

void Xe::PCIDev::HDD::BeginPioOut(u32 bytes) {
  pioTarget = PioTarget::Buffer;
  pioBlock.assign(bytes, 0);
  pioPos = 0;
  phase = Phase::PioOut;
  tf.status = kStatusReady | kStatusDrq;
}

void Xe::PCIDev::HDD::BeginDma(bool toHost, bool lba48, bool forceUnitAccess) {
  const u64 lba = lba48 ? Lba48() : Lba28();
  const u32 count = lba48 ? Count48() : Count28();
  if (!RangeValid(lba, count, lba48)) {
    LOG_ERROR(HDD, "{}: LBA {:#x} + {} sectors is outside the {} sector image.", CommandName(command), lba, count,
              sectorCount);
    // Fails before the data phase. The INTRQ still sets the bus master INTERRUPT bit, which is what the kernel waits
    // for before it looks at ERR.
    CompleteError(kErrorIdnf);
    return;
  }
  xfer = {lba, count, toHost, forceUnitAccess};
  dmaQueued = false;
  phase = Phase::Dma;
  // The drive requests the transfer with DRQ and BSY clear until the bus master moves it. XeLL (libxenon
  // xenon_ata_read_sectors) waits for !BSY && DRDY after the command and only then starts the bus master, so BSY
  // here would deadlock it. The kernel just waits for the interrupt.
  tf.status = kStatusReady | kStatusDrq;
  StartDmaIfReady();
}

void Xe::PCIDev::HDD::StartDmaIfReady() {
  // Runs once both the DMA command and the bus master start have happened (the kernel and Linux write the command
  // first, then DmaCommand = 0x09 for a read or 0x01 for a write).
  if (phase != Phase::Dma || dmaQueued || !(dmaCommand & kBmCmdStart)) { return; }
  const bool engineToMemory = dmaCommand & kBmCmdToMemory;
  if (engineToMemory != xfer.toHost) {
    LOG_WARNING(HDD, "{}: the bus master is set up for the other direction; following the command.",
                CommandName(command));
  }
  dmaJob = {xfer.lba, xfer.sectors, xfer.toHost, xfer.forceUnitAccess, dmaPrdTable};
  dmaQueued = true;
  job = JobKind::Dma;
  workCV.notify_one();
}

void Xe::PCIDev::HDD::QueueFlush() {
  // Nothing written since the last flush.
  if (!imageDirty) {
    CompleteOk();
    return;
  }
  phase = Phase::Busy;
  tf.status = kStatusBsy | kStatusDrdy;
  job = JobKind::Flush;
  workCV.notify_one();
}

// Completion, interrupts and resets

void Xe::PCIDev::HDD::CompleteOk() {
  phase = Phase::Idle;
  pioPending.clear();
  tf.status = kStatusReady;
  RaiseInterrupt();
}

void Xe::PCIDev::HDD::CompleteError(u8 error) {
  phase = Phase::Idle;
  pioPending.clear();
  tf.error = error;
  tf.status = kStatusReady | kStatusErr;
  RaiseInterrupt();
}

void Xe::PCIDev::HDD::Abort() { CompleteError(kErrorAbrt); }

void Xe::PCIDev::HDD::RaiseInterrupt() {
  intrqPending = true;
  // nIEN keeps INTRQ low (the kernel clears it after the first COMRESET).
  if (tf.deviceControl & kDevCtlNien) { return; }
  // The bus master INTERRUPT bit latches every INTRQ edge.
  dmaStatus |= kBmStatusInterrupt;
  parentBus->RouteInterrupt(PRIO_SATA_HDD);
}

void Xe::PCIDev::HDD::AcknowledgeInterrupt() {
  if (!intrqPending) { return; }
  intrqPending = false;
  // Withdraw it if the bridge is still holding it behind its mask.
  parentBus->CancelInterrupt(PRIO_SATA_HDD);
}

void Xe::PCIDev::HDD::SetSignature() {
  // ATA (non-packet) device signature.
  tf.sectorCount = 0x01;
  tf.lbaLow = 0x01;
  tf.lbaMid = 0x00;
  tf.lbaHigh = 0x00;
  tf.device = 0x00;
  tf.sectorCountPrev = 0;
  tf.lbaLowPrev = 0;
  tf.lbaMidPrev = 0;
  tf.lbaHighPrev = 0;
}

void Xe::PCIDev::HDD::DeviceReset() {
  AbandonCommand();
  standby = false;
  if (!imageAttached) {
    tf.status = kStatusDrdy;
    return;
  }
  SetSignature();
  tf.error = kErrorDiagnosticPassed;
  tf.status = kStatusReady;
}

// The console's reset line: back to the state the constructor leaves, with the image kept and the worker idle.
void Xe::PCIDev::HDD::PlatformReset() {
  std::unique_lock lock(ataMutex);
  AbandonCommand();
  // A job that's already running finishes its transfer (its result is discarded by the generation). Wait for it so it
  // can't write guest memory once the reset returns.
  idleCV.wait(lock, [this] { return !workerActive; });
  if (image && imageDirty && image->Flush()) { imageDirty = false; }

  tf = {};
  dmaCommand = 0;
  dmaStatus = 0;
  dmaPrdTable = 0;
  intrqPending = false;
  parentBus->CancelInterrupt(PRIO_SATA_HDD);
  pioTarget = PioTarget::Buffer;
  drqSectors = 1;
  command = 0;
  multipleCount = 0;
  smartEnabled = true;
  standby = false;
  sectorBuffer.fill(0);
  warnedCommands.reset();
  powerOnTime = std::chrono::steady_clock::now();
  BuildIdentify();

  scr.Reset(imageAttached, kSErrorPowerOn);
  if (imageAttached) {
    SetSignature();
    tf.error = kErrorDiagnosticPassed;
    tf.status = kStatusReady;
  } else {
    tf.status = kStatusDrdy;
  }
  scr.Mirror(pciConfigSpace);
}

void Xe::PCIDev::HDD::AbandonCommand() {
  phase = Phase::Idle;
  pioBlock.clear();
  pioPending.clear();
  pioPos = 0;
  xfer = {};
  dmaQueued = false;
  // A job not picked up yet is dropped. One already running finishes, but its result is discarded.
  job = JobKind::None;
  ++generation;
  AcknowledgeInterrupt();
}

// Worker thread

void Xe::PCIDev::HDD::WorkerLoop() {
  LOG_INFO(HDD, "Entered HDD worker thread.");
  std::unique_lock lock(ataMutex);
  while (true) {
    workCV.wait(lock, [this] { return workerStop || job != JobKind::None; });
    if (workerStop) { break; }
    const JobKind kind = std::exchange(job, JobKind::None);
    const DmaJob params = dmaJob;
    const u64 jobGeneration = generation;
    // A flush covers everything written up to here.
    const bool flushNeeded = kind == JobKind::Flush && std::exchange(imageDirty, false);
    workerActive = true;
    lock.unlock();
    if (kind == JobKind::Dma) {
      RunDmaJob(params, jobGeneration);
    } else {
      RunFlushJob(jobGeneration, flushNeeded);
    }
    lock.lock();
    workerActive = false;
    idleCV.notify_all();
  }
  LOG_INFO(HDD, "Exiting HDD worker thread.");
}

void Xe::PCIDev::HDD::RunDmaJob(const DmaJob &params, u64 jobGeneration) {
  // Walk the PRD table and move the data straight between the image and guest memory.
  const u64 startOffset = params.lba * kSectorSize;
  u64 diskOffset = startOffset;
  u64 remaining = static_cast<u64>(params.sectors) * kSectorSize;
  bool tableLonger = false;
  bool badAddress = false;
  bool ioError = false;

  Xe::PCI::PrdWalk walk;
  if (remaining != 0) {
    walk = Xe::PCI::WalkPrdTable(dma, params.prdTable, [&](u32 address, u32 length) {
      const u32 chunk = static_cast<u32>(std::min<u64>(length, remaining));
      // The disk's bytes are the newest in these pages from now on, not what the GPU last wrote there.
      u8 *destination = params.toHost ? dma.BeginWrite(address, chunk) : nullptr;
      const u8 *source = params.toHost ? nullptr : dma.ReadPointer(address, chunk);
      if (!destination && !source) {
        badAddress = true;
        return false;
      }
      NoteActivity();
      const bool ok =
        params.toHost ? image->Read(diskOffset, destination, chunk) : image->Write(diskOffset, source, chunk);
      if (params.toHost) { dma.EndWrite(address, chunk); }
      if (!ok) {
        ioError = true;
        return false;
      }
      diskOffset += chunk;
      remaining -= chunk;
      // This entry was bigger than what the drive had left.
      if (chunk < length) { tableLonger = true; }
      return remaining != 0;
    });
    if (walk.fault != Xe::PCI::PrdFault::None) { badAddress = true; }
  }
  // More entries follow the last one used.
  if (remaining == 0 && !walk.last) { tableLonger = true; }
  const bool tableShort = remaining != 0 && !ioError && !badAddress;

  bool flushed = false;
  if (!params.toHost && params.forceUnitAccess && !ioError) { flushed = image->Flush(); }

  if (badAddress) {
    LOG_ERROR(HDD, "DMA: PRD table at {:#x} points outside guest memory.", params.prdTable);
  } else if (ioError) {
    LOG_ERROR(HDD, "DMA: image {} failed at offset {:#x}.", params.toHost ? "read" : "write", diskOffset);
  } else if (tableShort) {
    LOG_ERROR(HDD, "DMA: PRD table at {:#x} ended {:#x} bytes before the transfer.", params.prdTable, remaining);
  }

  std::lock_guard lock(ataMutex);
  if (!params.toHost && diskOffset != startOffset && !flushed) { imageDirty = true; }
  if (jobGeneration != generation || phase != Phase::Dma) { return; }
  dmaQueued = false;
  // The bus master stays ACTIVE when its table is longer than the transfer (the kernel reports a data overrun for
  // that), and ERROR is flagged for a bad table. A real drive left waiting on a short table would just time out,
  // failing the command is kinder.
  if (!tableLonger) { dmaStatus &= ~kBmStatusActive; }
  if (badAddress || tableShort) { dmaStatus |= kBmStatusError; }
  if (ioError) {
    CompleteError(params.toHost ? kErrorUnc : kErrorAbrt);
  } else if (badAddress || tableShort) {
    CompleteError(kErrorAbrt);
  } else {
    CompleteOk();
  }
}

void Xe::PCIDev::HDD::RunFlushJob(u64 jobGeneration, bool flushNeeded) {
  const bool ok = !flushNeeded || image->Flush();
  if (!ok) { LOG_ERROR(HDD, "{}: flushing the image failed.", CommandName(command)); }

  std::lock_guard lock(ataMutex);
  if (!ok) { imageDirty = true; }
  if (jobGeneration != generation || phase != Phase::Busy) { return; }
  if (ok) {
    CompleteOk();
  } else {
    CompleteError(kErrorAbrt);
  }
}

// Addressing

u64 Xe::PCIDev::HDD::Lba28() const {
  return (static_cast<u64>(tf.device & 0x0F) << 24) | (static_cast<u64>(tf.lbaHigh) << 16)
         | (static_cast<u64>(tf.lbaMid) << 8) | tf.lbaLow;
}

u64 Xe::PCIDev::HDD::Lba48() const {
  return (static_cast<u64>(tf.lbaHighPrev) << 40) | (static_cast<u64>(tf.lbaMidPrev) << 32)
         | (static_cast<u64>(tf.lbaLowPrev) << 24) | (static_cast<u64>(tf.lbaHigh) << 16)
         | (static_cast<u64>(tf.lbaMid) << 8) | tf.lbaLow;
}

u32 Xe::PCIDev::HDD::Count28() const { return tf.sectorCount ? tf.sectorCount : 256; }

u32 Xe::PCIDev::HDD::Count48() const {
  const u32 count = (static_cast<u32>(tf.sectorCountPrev) << 8) | tf.sectorCount;
  return count ? count : 65536;
}

bool Xe::PCIDev::HDD::RangeValid(u64 lba, u64 count, bool lba48) const {
  // The kernel and Linux always set the LBA bit. CHS addressing is not emulated.
  if (lba + count > sectorCount) { return false; }
  return lba48 || lba + count <= kLba28Limit;
}

// IDENTIFY and SMART

void Xe::PCIDev::HDD::BuildIdentify() {
  if (!imageAttached) {
    identify.fill(0);
    return;
  }
  for (size_t i = 0; i < identify.size(); ++i) {
    identify[i] = static_cast<u16>(kIdentifyDataBytes[2 * i] | (kIdentifyDataBytes[2 * i + 1] << 8));
  }

  // The kernel takes the capacity from words 100..103 when bit 10 of words 83 and 86 (48-bit addressing supported
  // and enabled) is set, substituting 0xFFFFFFFF when words 102..103 are non zero. Otherwise it takes words 60..61
  // clamped to 0x0FFFFFFF. Every partition bound follows from it, so it has to be what the image can back.
  const u32 sectors28 = static_cast<u32>(std::min<u64>(sectorCount, kLba28Limit - 1));
  identify[60] = static_cast<u16>(sectors28);
  identify[61] = static_cast<u16>(sectors28 >> 16);
  for (u32 i = 0; i < 4; ++i) { identify[100 + i] = static_cast<u16>(sectorCount >> (16 * i)); }

  // Default CHS geometry (16 heads, 63 sectors per track) for images below the CHS limit.
  constexpr u64 kChsSectorsPerCylinder = 16 * 63;
  constexpr u64 kChsLimit = 16383 * kChsSectorsPerCylinder;
  if (sectorCount < kChsLimit) {
    const u16 cylinders = static_cast<u16>(sectorCount / kChsSectorsPerCylinder);
    const u32 chsSectors = static_cast<u32>(cylinders * kChsSectorsPerCylinder);
    identify[1] = cylinders;
    identify[54] = cylinders;
    identify[57] = static_cast<u16>(chsSectors);
    identify[58] = static_cast<u16>(chsSectors >> 16);
  }

  // Report the identity the image's security sector was signed for, because the kernel authenticates the drive
  // against it. It reads 0x200 bytes at byte 0x2000 and wants serial (+0x00, 20 bytes) and model (+0x1C, 40 bytes)
  // to equal IDENTIFY words 10..19 and 27..46 (firmware at +0x14 vs words 23..26 only prints a warning), each word
  // byte-swapped against the sector, so standard ATA strings with the first character in the high byte. Then the
  // sector count (+0x58, little endian) has to be <= the drive capacity and != 348, the RSA signature at +0x5C must
  // cover +0x00..+0x5B, and the SHA-1 at +0x44 must match the 0xC00-byte logo at byte 0x2200. If any of that fails a
  // retail kernel gets no usable HDD.
  constexpr u64 kSecuritySectorOffset = 0x2000;
  std::array<u8, kSectorSize> sec{};
  if (image && imageSizeInBytes >= kSecuritySectorOffset + kSectorSize
      && image->Read(kSecuritySectorOffset, sec.data(), static_cast<u32>(sec.size()))) {
    // An unwritten image (zeros or erased) has no security sector. Keep the built-in identity then.
    const auto printable = [&](u32 offset, u32 size) {
      bool any = false;
      for (u32 i = 0; i < size; ++i) {
        if (sec[offset + i] < 0x20 || sec[offset + i] > 0x7E) { return false; }
        any |= sec[offset + i] != ' ';
      }
      return any;
    };
    if (printable(0x00, 20) && printable(0x14, 8) && printable(0x1C, 40)) {
      const auto copyString = [&](u32 offset, u32 firstWord, u32 words) {
        for (u32 i = 0; i < words; ++i) {
          identify[firstWord + i] = static_cast<u16>((sec[offset + 2 * i] << 8) | sec[offset + 2 * i + 1]);
        }
      };
      copyString(0x00, 10, 10);
      copyString(0x14, 23, 4);
      copyString(0x1C, 27, 20);
      const u32 signedSectors = static_cast<u32>(sec[0x58] | (sec[0x59] << 8) | (sec[0x5A] << 16) | (sec[0x5B] << 24));
      const auto str = [&](u32 offset, u32 size) {
        return std::string(reinterpret_cast<const char *>(&sec[offset]), size);
      };
      LOG_INFO(HDD, "Security sector identity: model '{}', serial '{}', firmware '{}', {} sectors.", str(0x1C, 40),
               str(0x00, 20), str(0x14, 8), signedSectors);
      if (signedSectors > sectorCount) {
        LOG_WARNING(HDD,
                    "Security sector is signed for {} sectors but the image has {}, the kernel will reject it. "
                    "The image must be at least {:#x} bytes.",
                    signedSectors, sectorCount, static_cast<u64>(signedSectors) * kSectorSize);
      }
    }
  }

  multipleCount = (identify[kIdMultiple] & 0x0100) ? static_cast<u8>(identify[kIdMultiple]) : 0;
  smartEnabled = identify[kIdEnabled1] & 1;
  UpdateIdentifyChecksum();
}

void Xe::PCIDev::HDD::UpdateIdentifyChecksum() {
  // Word 255: signature 0xA5 and a checksum that makes the 512 byte sum zero.
  u8 sum = 0xA5;
  for (u32 i = 0; i < kIdIntegrity; ++i) {
    sum = static_cast<u8>(sum + static_cast<u8>(identify[i]) + static_cast<u8>(identify[i] >> 8));
  }
  identify[kIdIntegrity] = static_cast<u16>(0x00A5 | (static_cast<u8>(0x100 - sum) << 8));
}

std::vector<u8> Xe::PCIDev::HDD::IdentifyBytes() const {
  // Little endian words. The kernel reads them through the data port with 16-bit loads, which leaves the same little
  // endian image in memory.
  std::vector<u8> bytes(identify.size() * 2);
  for (size_t i = 0; i < identify.size(); ++i) {
    bytes[2 * i] = static_cast<u8>(identify[i]);
    bytes[2 * i + 1] = static_cast<u8>(identify[i] >> 8);
  }
  return bytes;
}

std::vector<u8> Xe::PCIDev::HDD::SmartData() const {
  std::vector<u8> data(kSectorSize, 0);
  // Data structure revision.
  data[0] = 0x10;
  const u64 powerOnHours =
    std::chrono::duration_cast<std::chrono::hours>(std::chrono::steady_clock::now() - powerOnTime).count();
  size_t offset = 2;
  for (const SmartAttribute &attribute : kSmartAttributes) {
    const u64 raw = attribute.id == 0x09 ? powerOnHours : attribute.raw;
    data[offset + 0] = attribute.id;
    data[offset + 1] = static_cast<u8>(attribute.flags);
    data[offset + 2] = static_cast<u8>(attribute.flags >> 8);
    data[offset + 3] = attribute.value;
    data[offset + 4] = attribute.value; // worst
    for (u32 i = 0; i < 6; ++i) { data[offset + 5 + i] = static_cast<u8>(raw >> (8 * i)); }
    offset += 12;
  }
  data[362] = 0x00; // Offline data collection never started.
  data[363] = 0x00; // Last self-test completed without error.
  data[364] = 0x45; // 581 s to complete offline data collection.
  data[365] = 0x02;
  data[367] = 0x5B; // Offline data collection capabilities.
  data[368] = 0x03; // SMART capabilities: saves data before power saving, supports autosave.
  data[370] = 0x01; // Error logging supported.
  data[372] = 0x01; // Short self-test: 1 minute.
  data[373] = 0x55; // Extended self-test: 85 minutes.
  SealSmartSector(data.data());
  return data;
}

std::vector<u8> Xe::PCIDev::HDD::SmartThresholds() const {
  std::vector<u8> data(kSectorSize, 0);
  data[0] = 0x10;
  size_t offset = 2;
  for (const SmartAttribute &attribute : kSmartAttributes) {
    data[offset + 0] = attribute.id;
    data[offset + 1] = attribute.threshold;
    offset += 12;
  }
  SealSmartSector(data.data());
  return data;
}

const char *Xe::PCIDev::HDD::CommandName(u8 cmd) {
  switch (cmd) {
    case AtaCmd::Nop: return "NOP";
    case AtaCmd::DeviceReset: return "DEVICE RESET";
    case AtaCmd::ReadSectors:
    case AtaCmd::ReadSectorsNoRetry: return "READ SECTORS";
    case AtaCmd::ReadSectorsExt: return "READ SECTORS EXT";
    case AtaCmd::ReadDmaExt: return "READ DMA EXT";
    case AtaCmd::ReadNativeMaxExt: return "READ NATIVE MAX ADDRESS EXT";
    case AtaCmd::ReadMultipleExt: return "READ MULTIPLE EXT";
    case AtaCmd::ReadLogExt: return "READ LOG EXT";
    case AtaCmd::WriteSectors:
    case AtaCmd::WriteSectorsNoRetry: return "WRITE SECTORS";
    case AtaCmd::WriteSectorsExt: return "WRITE SECTORS EXT";
    case AtaCmd::WriteDmaExt: return "WRITE DMA EXT";
    case AtaCmd::SetMaxExt: return "SET MAX ADDRESS EXT";
    case AtaCmd::WriteMultipleExt: return "WRITE MULTIPLE EXT";
    case AtaCmd::WriteDmaFuaExt: return "WRITE DMA FUA EXT";
    case AtaCmd::ReadVerify:
    case AtaCmd::ReadVerifyNoRetry: return "READ VERIFY SECTORS";
    case AtaCmd::ReadVerifyExt: return "READ VERIFY SECTORS EXT";
    case AtaCmd::Seek: return "SEEK";
    case AtaCmd::ExecuteDiagnostic: return "EXECUTE DEVICE DIAGNOSTIC";
    case AtaCmd::InitializeParameters: return "INITIALIZE DEVICE PARAMETERS";
    case AtaCmd::DownloadMicrocode: return "DOWNLOAD MICROCODE";
    case AtaCmd::Packet: return "PACKET";
    case AtaCmd::IdentifyPacket: return "IDENTIFY PACKET DEVICE";
    case AtaCmd::Smart: return "SMART";
    case AtaCmd::ReadMultiple: return "READ MULTIPLE";
    case AtaCmd::WriteMultiple: return "WRITE MULTIPLE";
    case AtaCmd::SetMultiple: return "SET MULTIPLE MODE";
    case AtaCmd::ReadDma:
    case AtaCmd::ReadDmaNoRetry: return "READ DMA";
    case AtaCmd::WriteDma:
    case AtaCmd::WriteDmaNoRetry: return "WRITE DMA";
    case AtaCmd::WriteMultipleFuaExt: return "WRITE MULTIPLE FUA EXT";
    case AtaCmd::StandbyImmediate:
    case AtaCmd::StandbyImmediateAlt: return "STANDBY IMMEDIATE";
    case AtaCmd::IdleImmediate:
    case AtaCmd::IdleImmediateAlt: return "IDLE IMMEDIATE";
    case AtaCmd::Standby:
    case AtaCmd::StandbyAlt: return "STANDBY";
    case AtaCmd::Idle:
    case AtaCmd::IdleAlt: return "IDLE";
    case AtaCmd::ReadBuffer: return "READ BUFFER";
    case AtaCmd::CheckPowerMode:
    case AtaCmd::CheckPowerModeAlt: return "CHECK POWER MODE";
    case AtaCmd::Sleep:
    case AtaCmd::SleepAlt: return "SLEEP";
    case AtaCmd::FlushCache: return "FLUSH CACHE";
    case AtaCmd::WriteBuffer: return "WRITE BUFFER";
    case AtaCmd::FlushCacheExt: return "FLUSH CACHE EXT";
    case AtaCmd::Identify: return "IDENTIFY DEVICE";
    case AtaCmd::SetFeatures: return "SET FEATURES";
    case AtaCmd::SecuritySetPassword: return "SECURITY SET PASSWORD";
    case AtaCmd::SecurityUnlock: return "SECURITY UNLOCK";
    case AtaCmd::SecurityErasePrepare: return "SECURITY ERASE PREPARE";
    case AtaCmd::SecurityEraseUnit: return "SECURITY ERASE UNIT";
    case AtaCmd::SecurityFreezeLock: return "SECURITY FREEZE LOCK";
    case AtaCmd::SecurityDisablePassword: return "SECURITY DISABLE PASSWORD";
    case AtaCmd::ReadNativeMax: return "READ NATIVE MAX ADDRESS";
    case AtaCmd::SetMax: return "SET MAX ADDRESS";
    default: return (cmd & 0xF0) == 0x10 ? "RECALIBRATE" : "UNKNOWN";
  }
}
