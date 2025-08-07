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

#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIBridge/SATA.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"
#include "Core/XCPU/Interpreter/PPCInternal.h"

#define ODD_DEV_SIZE 0x30

namespace Xe {
namespace PCIDev {

// Read Capacity Data - returned in Big Endian format
struct READ_CAPACITY_DATA {
  u32 logicalBlockAddress;
  u32 bytesPerBlock;
};

//
// Data Buffers
//

class DataBuffer {
public:
  bool empty(void) { return _pointer >= _size; }
  u32 count(void) { return _size - _pointer; }
  u32 size(void) { return _pointer; }
  u8 *get(void) { return _data.get() + _pointer; }
  void resize(u32 v) { _pointer += v; }
  void reset(void) { _pointer = 0; }
  bool init(u32 maxLength, bool clear) {
    if (_data && (maxLength > _size)) {
      _data.reset();
      _size = 0;
      reset();
    }
    if (!_data) {
      _data = std::make_unique<STRIP_UNIQUE_ARR(_data)>(maxLength);
    }
    if (_data) {
      _size = std::max(_size, maxLength);
      _pointer = _size; // Empty()
      if (clear)
        memset(_data.get(), 0, maxLength);
      return true;
    }
    return false;
  }
private:
  std::unique_ptr<u8[]> _data;
  u32 _size;
  u32 _pointer;
};

//
// Read Only Storage
//
#ifdef _WIN32
class Storage {
public:
  Storage(const std::string Filename) {
    hFile = CreateFileA(Filename.data(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  }

  ~Storage(void) {
    if (hFile != INVALID_HANDLE_VALUE)
      CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;
  }

  u32 Size() {
    u32 cb;
    cb = GetFileSize(hFile, nullptr);
    return (cb == INVALID_FILE_SIZE) ? 0 : cb;
  }

  bool Read(u64 Offset, u8 *Destination, u32 cu8s) {
    DWORD cbRead;
    OVERLAPPED Over;

    memset(&Over, 0, sizeof Over);
    Over.Offset = LODW(Offset);
    Over.OffsetHigh = HIDW(Offset);
    return (ReadFile(hFile, Destination, cu8s, &cbRead, &Over) &&
            (cbRead == cu8s));
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

//
// SCSI Inquiry Data Structure
//

union inquiryDataByte0 {
  u8 dataHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u8 peripheralDevType : 5;
    u8 peripheralQualifier : 3;
  };
#else
  struct {
    u8 peripheralQualifier : 3;
    u8 peripheralDevType : 5;
  };
#endif
};
union inquiryDataByte1 {
  u8 dataHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u8 res : 7;
    u8 rmb : 1;
  };
#else
  struct {
    u8 rmb : 1;
    u8 res : 7;
  };
#endif
};
union inquiryDataByte3 {
  u8 dataHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u8 responseDataFormat : 4;
    u8 hisup : 1;
    u8 normaca : 1;
    u8 obs : 2;
  };
#endif
};
union inquiryDataByte5 {
  u8 dataHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u8 protect : 1;
    u8 res : 2;
    u8 threePc : 1;
    u8 tpgs : 2;
    u8 acc : 1;
    u8 sccs : 1;
  };
#else
  struct {
    u8 sccs : 1;
    u8 acc : 1;
    u8 tpgs : 2;
    u8 threePc : 1;
    u8 res : 2;
    u8 protect : 1;
  };
#endif
};
union inquiryDataByte6 {
  u8 dataHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u8 re0 : 1;
    u8 obs0 : 3;
    u8 multip : 1;
    u8 vs : 1;
    u8 encserv : 1;
    u8 ob1 : 1;
  };
#else
  struct {
    u8 ob1 : 1;
    u8 encserv : 1;
    u8 vs : 1;
    u8 multip : 1;
    u8 obs0 : 3;
    u8 re0 : 1;
  };
#endif
};
union inquiryDataByte7 {
  u8 dataHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u8 vs : 1;
    u8 cmdque : 1;
    u8 obs0 : 2;
    u8 res : 2;
    u8 obs1 : 2;
  };
#else
  struct {
    u8 obs1 : 2;
    u8 res : 2;
    u8 obs0 : 2;
    u8 cmdque : 1;
    u8 vs : 1;
  };
#endif
};

// XeLL only reads the first 36 bytes.
struct XE_ATAPI_INQUIRY_DATA {
  inquiryDataByte0 byte0;
  inquiryDataByte1 byte1;
  u8 version;
  inquiryDataByte3 byte3;
  u8 additionalLenght;
  inquiryDataByte5 byte5;
  inquiryDataByte6 byte6;
  inquiryDataByte7 byte7;
  u8 vendorIdentification[8];
  u8 productIdentification[16];
  u8 productRevisionLevel[4];
};

/*
* This structure is returned by the IDENTIFY_DEVICE and IDENTIFY_PACKET_DEVICE commands
*/
#pragma pack(push, 1)
struct XE_ATAPI_IDENTIFY_DATA {
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
// SCSI Command Descriptor Block
//

union XE_CDB {
  //
  // Generic 6-Byte CDB
  //

#ifdef __LITTLE_ENDIAN__
  struct _CDB6GENERIC {
    u8 OperationCode;
    u8 Immediate : 1;
    u8 CommandUniqueBits : 4;
    u8 LogicalUnitNumber : 3;
    u8 CommandUniqueBytes[3];
    u8 Link : 1;
    u8 Flag : 1;
    u8 Reserved : 4;
    u8 VendorUnique : 2;
  } CDB6GENERIC, *PCDB6GENERIC;
#else
  struct _CDB6GENERIC {
    u8 VendorUnique : 2;
    u8 Reserved : 4;
    u8 Flag : 1;
    u8 Link : 1;
    u8 CommandUniqueBytes[3];
    u8 LogicalUnitNumber : 3;
    u8 CommandUniqueBits : 4;
    u8 Immediate : 1;
    u8 OperationCode;
  } CDB6GENERIC, *PCDB6GENERIC;
#endif
  //
  // Standard 10-byte CDB
  //

#ifdef __LITTLE_ENDIAN__
  struct _CDB10 {
    u8 OperationCode;
    u8 RelativeAddress : 1;
    u8 Reserved1 : 2;
    u8 ForceUnitAccess : 1;
    u8 DisablePageOut : 1;
    u8 LogicalUnitNumber : 3;
    union {
      struct {
        u8 LogicalBlockByte0;
        u8 LogicalBlockByte1;
        u8 LogicalBlockByte2;
        u8 LogicalBlockByte3;
      };
      u32 LogicalBlock;
    };
    u8 Reserved2;
    union {
      struct {
        u8 TransferBlocksMsb;
        u8 TransferBlocksLsb;
      };
      u16 TransferBlocks;
    };
    u8 Control;
  } CDB10, *PCDB10;
#else
  struct _CDB10 {
    u8 Control;
    union {
      struct {
        u8 TransferBlocksMsb;
        u8 TransferBlocksLsb;
      };
      u16 TransferBlocks;
    };
    u8 Reserved2;
    union {
      struct {
        u8 LogicalBlockByte0;
        u8 LogicalBlockByte1;
        u8 LogicalBlockByte2;
        u8 LogicalBlockByte3;
      };
      u32 LogicalBlock;
    };
    u8 LogicalUnitNumber : 3;
    u8 DisablePageOut : 1;
    u8 ForceUnitAccess : 1;
    u8 Reserved1 : 2;
    u8 RelativeAddress : 1;
    u8 OperationCode;
  } CDB10, *PCDB10;
#endif

  //
  // Standard 12-byte CDB
  //

#ifdef __LITTLE_ENDIAN__
  struct _CDB12 {
    u8 OperationCode;
    u8 RelativeAddress : 1;
    u8 Reserved1 : 2;
    u8 ForceUnitAccess : 1;
    u8 DisablePageOut : 1;
    u8 LogicalUnitNumber : 3;
    u8 LogicalBlock[4];   // [0]=MSB, [3]=LSB
    u8 TransferLength[4]; // [0]=MSB, [3]=LSB
    u8 Reserved2;
    u8 Control;
  } CDB12, *PCDB12;
#else
  struct _CDB12 {
    u8 Control;
    u8 Reserved2;
    u8 TransferLength[4]; // [0]=MSB, [3]=LSB
    u8 LogicalBlock[4];   // [0]=MSB, [3]=LSB
    u8 LogicalUnitNumber : 3;
    u8 DisablePageOut : 1;
    u8 ForceUnitAccess : 1;
    u8 Reserved1 : 2;
    u8 RelativeAddress : 1;
    u8 OperationCode;
  } CDB12, *PCDB12;
#endif

  u32 AsUlong[4];
  u8 AsByte[16];
};

//
// Direct Memory Accesss PRD
//

// DMA Physical Region Descriptor
struct XE_ATAPI_DMA_PRD {
  u32 physAddress; // physical memory address of a data buffer
  u16 sizeInBytes;
  u16 control;
};

struct XE_ATAPI_DMA_STATE {
  XE_ATAPI_DMA_PRD currentPRD = {0};
  u32 currentTableOffset = 0;
};

//
// ATAPI Register State Structure
//
struct XE_ATAPI_REGISTERS {
  /* Command Block */

  // Offset 0x0
  u32 dataReg;
  // Offset 0x1
  struct {
    // When Read
    u32 errorReg;
    // When written
    u32 featuresReg;
  };
  // Offset 0x2
  struct {
    // When Read
    u32 interruptReasonReg;
    // When written
    u32 sectorCountReg;
  };
  // Offset 0x3
  u32 lbaLowReg;
  // Offset 0x4
  u32 byteCountLowReg;
  // Offset 0x5
  u32 byteCountHighReg;
  // Offset 0x6
  u32 deviceReg;
  // Offset 0x7
  struct {
    // When Read
    u32 statusReg;
    // When written
    u32 commandReg;
  };
  // Offset 0xA
  struct {
    // When Read
    u32 altStatusReg;
    // When written
    u32 devControlReg;
  };
  // Offset 0x10
  u32 unk_10;
  // Offset 0x14
  u32 unk_14;
  // Offset 0x18
  u32 signatureReg;
  // Offset 0x1C
  u32 unk_1C;

  /* Control Block */
  // Offset 0x0
  u32 dmaCmdReg;
  // Offset 0x2
  u32 dmaStatusReg;
  // Offset 0x4
  u32 dmaTableOffsetReg;
};

//
// ATAPI Device State Structure
//
struct XE_ATAPI_DEV_STATE {
  // Register Set
  XE_ATAPI_REGISTERS atapiRegs = {0};
  // Data Read Buffer
  DataBuffer dataReadBuffer;
  // Data Write Buffer
  DataBuffer dataWriteBuffer;
  // ATAPI Inquiry Data
  XE_ATAPI_INQUIRY_DATA atapiInquiryData = {};
  // ATA Identify structure.
  XE_ATAPI_IDENTIFY_DATA atapiIdentifyData = {0};
  // SCSI Command Descriptor Block
  XE_CDB scsiCBD = {};
  // Direct Memroy Access Processing
  XE_ATAPI_DMA_STATE dmaState = {};
  // Mounted ISO Image
  std::unique_ptr<Storage> mountedCDImage{};
};

class ODD : public PCIDevice {
public:
  ODD(const char* deviceName, u64 size,
    PCIBridge *parentPCIBridge, RAM *ram);

  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

private:
  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus;

  // RAM pointer. Used for DMA.
  RAM *mainMemory;

  // ATAPI Device State.
  XE_ATAPI_DEV_STATE atapiState = {};

  // SCSI Command Processing
  void processSCSICommand();

  void doDMA();

  // Misc
  void atapiReset();
  void atapiIdentifyPacketDeviceCommand();
  void atapiIdentifyCommand();
};

} // namespace PCIDev
} // namespace Xe
