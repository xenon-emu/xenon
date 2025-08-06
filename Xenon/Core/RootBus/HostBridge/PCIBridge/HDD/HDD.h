// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "Core/XCPU/Interpreter/PPCInternal.h"
#include "Core/RootBus/HostBridge/PCIBridge/SATA.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"

#define HDD_DEV_SIZE 0x30

namespace Xe {
namespace PCIDev {

//
// Read/Write Storage.
//
#ifdef _WIN32
  class ReadWriteStorage {
  public:
    ReadWriteStorage(const std::string Filename) {
      hFile = CreateFileA(Filename.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    ~ReadWriteStorage(void) {
      if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
      hFile = INVALID_HANDLE_VALUE;
    }

    u32 Size() {
      u32 cb;
      cb = GetFileSize(hFile, nullptr);
      return (cb == INVALID_FILE_SIZE) ? 0 : cb;
    }

    bool Read(u64 Offset, u8* Destination, u32 cu8s) {
      DWORD cbRead;
      OVERLAPPED Over;

      memset(&Over, 0, sizeof Over);
      Over.Offset = LODW(Offset);
      Over.OffsetHigh = HIDW(Offset);
      return (ReadFile(hFile, Destination, cu8s, &cbRead, &Over) &&
        (cbRead == cu8s));
    }

    bool Write(u64 Offset, u8* Source, u32 cu8s) {
      DWORD cbWritten;
      OVERLAPPED Over;

      memset(&Over, 0, sizeof Over);
      Over.Offset = LODW(Offset);
      Over.OffsetHigh = HIDW(Offset);
      return (WriteFile(hFile, Source, cu8s, &cbWritten, &Over) &&
        (cbWritten == cu8s));
    }

  private:
    HANDLE hFile;
  };
#else
  class Storage {
  public:
    Storage(const std::string Filename) {
      fd = open(Filename.c_str(), O_RDONLY);
    }
    ~Storage() {
      if (fd != -1)
        close(fd);
      fd = -1;
    }

    u32 Size() {
      struct stat st;
      if (fstat(fd, &st) != 0)
        return 0;
      return static_cast<u32>(st.st_size);
    }

    bool Read(u64 Offset, u8* Destination, u32 cu8s) {
      if (lseek(fd, Offset, SEEK_SET) == (off_t)-1)
        return false;
      ssize_t bytesRead = read(fd, Destination, cu8s);
      return bytesRead == static_cast<ssize_t>(cu8s);
    }

  private:
    int fd;
  };
#endif

/*
* This structure is returned by the IDENTIFY_DEVICE and IDENTIFY_PACKET_DEVICE commands
*/
#pragma pack(push, 1)
  struct XE_ATA_IDENTIFY_DATA {
    u16 generalConfiguration;            // word 0
    u16 numberOfCylinders;               // word 1
    u16 reserved1;                       // word 2
    u16 numberOfHeads;                   // word 3
    u16 reserved2[2];                    // word 4-5
    u16 NumberOfSectorsPerTrack;         // word 6
    u16 reserved3[3];                    // word 7-9
    u8 serialNumber[20];                 // word 10-19
    u16 reserved4[3];                    // word 20-22
    u8 firmwareRevision[8];              // word 23-26
    u8 modelNumber[40];                  // word 27-46
    u16 maximumBlockTransfer : 8;        // word 47
    u16 reserved5 : 8;
    u16 reserved6;                       // word 48
    u16 capabilities;                    // word 49
    u16 reserved7;                       // word 50
    u16 reserved8 : 8;                   // word 51
    u16 pioCycleTimingMode : 8;
    u16 reserved9;                       // word 52
    u16 translationFieldsValid : 3;      // word 53
    u16 reserved10 : 13;
    u16 numberOfCurrentCylinders;        // word 54
    u16 numberOfCurrentHeads;            // word 55
    u16 currentSectorsPerTrack;          // word 56
    u32 currentSectorCapacity;           // word 57-58
    u16 currentMultiSectorSetting;       // word 59
    u32 userAddressableSectors;          // word 60-61
    u16 singleWordDMASupport : 8;        // word 62
    u16 singleWordDMAActive : 8;
    u16 multiWordDMASupport : 8;         // word 63
    u16 multiWordDMAActive : 8;
    u16 advancedPIOModes : 8;            // word 64
    u16 reserved11 : 8;
    u16 minimumMWXferCycleTime;          // word 65
    u16 recommendedMWXferCycleTime;      // word 66
    u16 minimumPIOCycleTime;             // word 67
    u16 minimumPIOCycleTimeIORDY;        // word 68
    u16 reserved12[11];                  // word 69-79
    u16 majorRevision;                   // word 80
    u16 minorRevision;                   // word 81
    union {                              // word 82
      struct {
        u16 SMARTFeatureSupport : 1;
        u16 securityModeFeatureSupport : 1;
        u16 removableMediaFeatureSupport : 1;
        u16 powerManagementFeatureSupport : 1;
        u16 packetFeatureSupport : 1;
        u16 writeCacheFeatureSupport : 1;
        u16 lookAheadFeatureSupport : 1;
        u16 releaseInterruptSupport : 1;
        u16 serviceInterruptSupport : 1;
        u16 deviceResetCommandSupport : 1;
        u16 hostProtectedAreaFeatureSupport : 1;
        u16 reserved13 : 1;
        u16 writeBufferCommandSupport : 1;
        u16 readBufferCommandSupport : 1;
        u16 nopCommandSupport : 1;
        u16 reserved14 : 1;
      };
      u16 dataAsu16;
    } support1;
    union {                              // word 83
      struct {
        u16 downloadMicrocodeCommandSupport : 1;
        u16 dmaQueuedCommandSupport : 1;
        u16 CFAFeatureSupport : 1;
        u16 advancedPowerManagementFeatureSupport : 1;
        u16 mediaStatusNotificationFeatureSupport : 1;
        u16 powerFromStandbyFeatureSupport : 1;
        u16 setFeaturesAfterPowerUpRequired : 1;
        u16 addressOffsetReservedAreaBoot : 1;
        u16 setMaximumCommandSupport : 1;
        u16 acousticManagementFeatureSupport : 1;
        u16 lba48BitFeatureSupport : 1;
        u16 deviceConfigOverlapFeatureSupport : 1;
        u16 flushCacheCommandSupport : 1;
        u16 flushCacheExtCommandSupport : 1;
        u16 reserved15 : 2;
      };
      u16 dataAsu16;
    } support2;
    union {                              // word 84
      u16 dataAsu16;
    } support3;
    union {                              // word 85
      struct {
        u16 SMARTFeatureEnabled : 1;
        u16 securityModeFeatureEnabled : 1;
        u16 removableMediaFeatureEnabled : 1;
        u16 powerManagementFeatureEnabled : 1;
        u16 packetFeatureEnabled : 1;
        u16 writeCacheFeatureEnabled : 1;
        u16 lookAheadFeatureEnabled : 1;
        u16 releaseInterruptEnabled : 1;
        u16 serviceInterruptEnabled : 1;
        u16 deviceResetCommandEnabled : 1;
        u16 hostProtectedAreaFeatureEnabled : 1;
        u16 reserved16 : 1;
        u16 writeBufferCommandEnabled : 1;
        u16 readBufferCommandEnabled : 1;
        u16 nopCommandEnabled : 1;
        u16 reserved17 : 1;
      };
      u16 dataAsu16;
    } enabled1;
    union {                              // word 86
      struct {
        u16 downloadMicrocodeCommandEnabled : 1;
        u16 dmaQueuedCommandEnabled : 1;
        u16 CFAFeatureEnabled : 1;
        u16 ddvancedPowerManagementFeatureEnabled : 1;
        u16 mediaStatusNotificationFeatureEnabled : 1;
        u16 powerFromStandbyFeatureEnabled : 1;
        u16 setFeaturesAfterPowerUpRequired2 : 1;
        u16 addressOffsetReservedAreaBoot2 : 1;
        u16 setMaximumCommandEnabled : 1;
        u16 acousticManagementFeatureEnabled : 1;
        u16 lba48BitFeatureEnabled : 1;
        u16 deviceConfigOverlapFeatureEnabled : 1;
        u16 flushCacheCommandEnabled : 1;
        u16 flushCacheExtCommandEnabled : 1;
        u16 reserved18 : 2;
      };
      u16 dataAsu16;
    } enabled2;
    union {                              // word 87
      u16 dataAsu16;
    } enabled3;
    u16 ultraDMASupport : 8;             // word 88
    u16 ultraDMAActive : 8;
    u16 reserved19[11];                  // word 89-99
    u32 userAddressableSectors48Bit[2];  // word 100-104
    u16 reserved20[23];                  // word 104-126
    u16 mediaStatusNotification : 2;     // word 127
    u16 reserved21 : 6;
    u16 deviceWriteProtect : 1;
    u16 reserved22 : 7;
    u16 securitySupported : 1;           // word 128
    u16 securityEnabled : 1;
    u16 securityLocked : 1;
    u16 securityFrozen : 1;
    u16 securityCountExpired : 1;
    u16 securityEraseSupported : 1;
    u16 reserved23 : 2;
    u16 securityLevel : 1;
    u16 reserved24 : 7;
    u16 reserved25[127];                 // word 129-255
  };
#pragma pack(pop)

//
// ATA Register State
//
struct ATA_REG_STATE {
  // Command
  u32 data;         // Address 0x00
  struct {          // Address 0x04
    u32 error;      // When Read
    u32 features;   // When Written
  };                
  u32 sectorCount;  // Address 0x08
  u32 lbaLow;       // Address 0x0C
  u32 lbaMiddle;    // Address 0x10
  u32 lbaHigh;      // Address 0x14
  u32 deviceSelect; // Address 0x18
  struct {          // Address 0x1C
    u32 status;     // When Read
    u32 command;    // When Written
  };
  // Control
  struct {           // Address 0x20
    u32 altStatus;      // When Read
    u32 deviceControl;  // When Written
  };
};

// ATA Device State
struct ATA_DEV_STATE {
  // Register state.
  ATA_REG_STATE regs = {0};
  // Identify Data for our Hard Drive.
  XE_ATA_IDENTIFY_DATA ataIdentifyData = {0};
  // Mounted HDD Image.
  std::unique_ptr<ReadWriteStorage> mountedHDDImage{};
};

class HDD : public PCIDevice {
public:                            
  HDD(const std::string &deviceName, u64 size,
    PCIBridge *parentPCIBridge);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus;

  // Device State
  ATA_DEV_STATE ataState = {};

  void ataCopyIdentifyDeviceData();
};

} // namespace PCIDev
} // namespace Xe
