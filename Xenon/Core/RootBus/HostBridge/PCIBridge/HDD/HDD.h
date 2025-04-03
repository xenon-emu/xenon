// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

#define HDD_DEV_SIZE 0x30

//
//	HDD inside the  Xbox 360 is manged via an ATA interface.
//

// ATA Status register flags
#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_IDX 0x02
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_BSY 0x80

// ATA Device Control register flags
#define ATA_DEVICE_CONTROL_NIEN 0x02
#define ATA_DEVICE_CONTROL_SRST 0x04

// ATA Commands
#define ATA_COMMAND_DEVICE_RESET 0x08
#define ATA_COMMAND_READ_SECTORS 0x20
#define ATA_COMMAND_READ_DMA_EXT 0x25
#define ATA_COMMAND_WRITE_SECTORS 0x30
#define ATA_COMMAND_WRITE_DMA_EXT 0x35
#define ATA_COMMAND_VERIFY 0x40
#define ATA_COMMAND_VERIFY_EXT 0x42
#define ATA_COMMAND_SET_DEVICE_PARAMETERS 0x91
#define ATA_COMMAND_PACKET 0xA0
#define ATA_COMMAND_IDENTIFY_PACKET_DEVICE 0xA1
#define ATA_COMMAND_READ_MULTIPLE 0xC4
#define ATA_COMMAND_WRITE_MULTIPLE 0xC5
#define ATA_COMMAND_SET_MULTIPLE_MODE 0xC6
#define ATA_COMMAND_READ_DMA 0xC8
#define ATA_COMMAND_WRITE_DMA 0xCA
#define ATA_COMMAND_STANDBY_IMMEDIATE 0xE0
#define ATA_COMMAND_FLUSH_CACHE 0xE7
#define ATA_COMMAND_IDENTIFY_DEVICE 0xEC
#define ATA_COMMAND_SET_FEATURES 0xEF
#define ATA_COMMAND_SECURITY_SET_PASSWORD 0xF1
#define ATA_COMMAND_SECURITY_UNLOCK 0xF2
#define ATA_COMMAND_SECURITY_DISABLE_PASSWORD 0xF6

// ATA Registers Offsets
#define ATA_REG_DATA 0x0
#define ATA_REG_ERROR 0x4
#define ATA_REG_SECTORCOUNT 0x8
#define ATA_REG_LBA_LOW 0xC
#define ATA_REG_LBA_MED 0x10
#define ATA_REG_LBA_HI 0x14
#define ATA_REG_DEV_SEL 0x18
#define ATA_REG_CMD_STATUS 0x1C
#define ATA_REG_DEV_CTRL 0x20

// ATA Identify Data
// This contains all info and parameters of a specific device.

struct ATA_IDENTIFY_DATA {
  u16 generalConfiguration;
  u16 numberOfCylinders;
  u16 reserved1;
  u16 numberOfHeads;
  u16 reserved2[2];
  u16 numberOfSectorsPerTrack;
  u16 reserved3[3];
  u8 serialNumber[20];
  u16 reserved4[3];
  u8 firmwareRevision[8];
  u8 modelNumber[40];
  u16 maximumBlockTransfer : 8;
  u16 reserved5 : 8;
  u16 reserved6;
  u16 capabilities;
  u16 reserved7;
  u16 reserved8 : 8;
  u16 pioCycleTimingMode : 8;
  u16 reserved9;
  u16 translationFieldsValid : 3;
  u16 reserved10 : 13;
  u16 numberOfCurrentCylinders;
  u16 numberOfCurrentHeads;
  u16 currentSectorsPerTrack;
  u32 currentSectorCapacity;
  u16 currentMultiSectorSetting;
  u32 userAddressableSectors;
  u16 singleWordDMASupport : 8;
  u16 singleWordDMAActive : 8;
  u16 multiWordDMASupport : 8;
  u16 multiWordDMAActive : 8;
  u16 advancedPIOModes : 8;
  u16 reserved11 : 8;
  u16 minimumMWXferCycleTime;
  u16 recommendedMWXferCycleTime;
  u16 minimumPIOCycleTime;
  u16 minimumPIOCycleTimeIORDY;
  u16 reserved12[11];
  u16 majorRevision;
  u16 minorRevision;
  union {
    struct {
      u16 sMARTFeatureSupport : 1;
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
    u16 dataHex;
  } support1;
  union {
    struct {
      u16 downloadMicrocodeCommandSupport : 1;
      u16 dmaQueuedCommandSupport : 1;
      u16 cFAFeatureSupport : 1;
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
    u16 dataHex;
  } support2;
  union {
    u16 dataHex;
  } support3;
  union {
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
    u16 dataHex;
  } enabled1;
  union {
    struct {
      u16 downloadMicrocodeCommandEnabled : 1;
      u16 dmaQueuedCommandEnabled : 1;
      u16 CFAFeatureEnabled : 1;
      u16 advancedPowerManagementFeatureEnabled : 1;
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
    u16 dataHex;
  } enabled2;
  union {
    u16 dataHex;
  } enabled3;
  u16 ultraDMASupport : 8;
  u16 ultraDMAActive : 8;
  u16 reserved19[11];
  u32 userAddressableSectors48Bit[2];
  u16 reserved20[23];
  u16 mediaStatusNotification : 2;
  u16 reserved21 : 6;
  u16 deviceWriteProtect : 1;
  u16 reserved22 : 7;
  u16 securitySupported : 1;
  u16 securityEnabled : 1;
  u16 securityLocked : 1;
  u16 securityFrozen : 1;
  u16 securityCountExpired : 1;
  u16 securityEraseSupported : 1;
  u16 reserved23 : 2;
  u16 securityLevel : 1;
  u16 reserved24 : 7;
  u16 reserved25[127];
};

// ATA Register State
struct ATA_REG_STATE {
  // Command
  u32 data;         // Address 0x00
  u32 error;        // Address 0x04
  u32 sectorCount;  // Address 0x08
  u32 lbaLow;       // Address 0x0C
  u32 lbaMiddle;    // Address 0x10
  u32 lbaHigh;      // Address 0x14
  u32 deviceSelect; // Address 0x18
  union {           // Address 0x1C
    u32 command;
    u32 status;
  };
  // Control
  union { // Address 0x20
    u32 deviceControl;
    u32 altStatus;
  };
};

// ATA Device State
struct ATA_DEV_STATE {
  ATA_REG_STATE ataReadState = {0};
  ATA_REG_STATE ataWriteState = {0};
  ATA_IDENTIFY_DATA ataIdentifyData = {0};
  std::vector<u8> readBuffer;
  std::vector<u8> writeBufferl;
};

class HDD : public PCIDevice {
public:                            
  HDD(const char *deviceName, u64 size,
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
  ATA_DEV_STATE ataDeviceState = {};

  void ataCopyIdentifyDeviceData();
};
