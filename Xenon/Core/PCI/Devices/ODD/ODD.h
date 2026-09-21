/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#ifdef _WIN32
  #include <Windows.h>
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/SATA.h"
#include "Core/RAM/RAM.h"
#include "Core/XCPU/PPU/PPCInternal.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#define ODD_DEV_SIZE 0x30

namespace Xe {
  namespace PCIDev {

    //
    // Data Buffers
    //

    class ODDDataBuffer {
    public:
      // Begin a transfer of `length` bytes, growing the allocation if needed.
      // The cursor is left at the start. Returns false only on allocation failure, in which case the buffer is left
      // empty rather than stale.
      bool Begin(u32 length, bool clear = true) {
        if (length > bufCapacity) {
          bufPtr = bufLength = bufCapacity = 0;
          dataPtr = std::make_unique<STRIP_UNIQUE_ARR(dataPtr)>(length);
          if (!dataPtr) { return false; }
          bufCapacity = length;
        }
        bufLength = length;
        bufPtr = 0;
        if (clear && dataPtr) { memset(dataPtr.get(), 0, length); }
        return true;
      }

      // Begin a transfer that serves `length` bytes copied from `src`.
      bool Fill(const void* src, u32 length) {
        if (!Begin(length, false)) { return false; }
        if (length) { memcpy(dataPtr.get(), src, length); }
        return true;
      }

      // Bytes still to transfer.
      u32 Remaining() const { return bufLength - bufPtr; }
      // Bytes transferred so far.
      u32 Transferred() const { return bufPtr; }
      // Size of the current transfer.
      u32 Length() const { return bufLength; }
      // True when the whole transfer has been consumed.
      bool Empty() const { return bufPtr >= bufLength; }

      // Start of the buffer, independent of the cursor.
      u8* Data() const { return dataPtr.get(); }
      // The buffer at the cursor.
      u8* Cursor() const { return dataPtr ? dataPtr.get() + bufPtr : nullptr; }

      // Move up to `length` bytes out of the buffer at the cursor and advance
      // it. Returns how many bytes actually moved.
      u32 Take(void* dst, u32 length) {
        length = std::min(length, Remaining());
        if (length) {
          memcpy(dst, Cursor(), length);
          bufPtr += length;
        }
        return length;
      }

      // Move up to `length` bytes into the buffer at the cursor and advance it.
      // Returns how many bytes actually moved.
      u32 Put(const void* src, u32 length) {
        length = std::min(length, Remaining());
        if (length) {
          memcpy(Cursor(), src, length);
          bufPtr += length;
        }
        return length;
      }

      // As Put(), but writes a repeated byte (MemSet).
      u32 PutValue(s32 value, u32 length) {
        length = std::min(length, Remaining());
        if (length) {
          memset(Cursor(), value, length);
          bufPtr += length;
        }
        return length;
      }

      // Put the cursor back at the start without touching the contents.
      void Rewind() { bufPtr = 0; }

    private:
      std::unique_ptr<u8[]> dataPtr;
      u32 bufCapacity = 0; // Allocated bytes.
      u32 bufLength = 0;   // Bytes in the current transfer.
      u32 bufPtr = 0;      // Cursor into the current transfer.
    };

    //
    // Read Only Storage.
    //
#ifdef _WIN32
    class ReadOnlyStorage {
    public:
      ReadOnlyStorage(const std::string Filename) {
        hFile = CreateFileA(Filename.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      }

      ~ReadOnlyStorage(void) {
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
      }

      u64 Size() {
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) return 0;
        return static_cast<u64>(fileSize.QuadPart);
      }

      bool Read(u64 Offset, u8* Destination, u32 cu8s) {
        DWORD cbRead;
        OVERLAPPED Over;

        memset(&Over, 0, sizeof Over);
        Over.Offset = LODW(Offset);
        Over.OffsetHigh = HIDW(Offset);
        return (ReadFile(hFile, Destination, cu8s, &cbRead, &Over) && (cbRead == cu8s));
      }

      bool isHandleValid() { return (hFile != INVALID_HANDLE_VALUE); }

    private:
      HANDLE hFile;
    };
#else
    class ReadOnlyStorage {
    public:
      ReadOnlyStorage(const std::string Filename) { fd = open(Filename.c_str(), O_RDWR); }
      ~ReadOnlyStorage() {
        if (fd != -1) close(fd);
        fd = -1;
      }

      u64 Size() {
        struct stat st;
        if (fstat(fd, &st) != 0) return 0;
        return static_cast<u64>(st.st_size);
      }

      bool Read(u64 Offset, u8* Destination, u32 cu8s) {
        if (lseek(fd, Offset, SEEK_SET) == (off_t)-1) return false;
        ssize_t bytesRead = read(fd, Destination, cu8s);
        return bytesRead == static_cast<ssize_t>(cu8s);
      }

      bool isHandleValid() { return (fd != -1); }

    private:
      int fd;
    };
#endif // ifdef _WIN32

    //
    // SCSI Inquiry Data Structure
    //

    union inquiryDataByte0 {
      u8 dataHex;
#ifdef __LITTLE_ENDIAN__
      struct {
        u8 peripheralDevType   : 5;
        u8 peripheralQualifier : 3;
      };
#else
      struct {
        u8 peripheralQualifier : 3;
        u8 peripheralDevType   : 5;
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
        u8 hisup              : 1;
        u8 normaca            : 1;
        u8 obs                : 2;
      };
#endif
    };
    union inquiryDataByte5 {
      u8 dataHex;
#ifdef __LITTLE_ENDIAN__
      struct {
        u8 protect : 1;
        u8 res     : 2;
        u8 threePc : 1;
        u8 tpgs    : 2;
        u8 acc     : 1;
        u8 sccs    : 1;
      };
#else
      struct {
        u8 sccs    : 1;
        u8 acc     : 1;
        u8 tpgs    : 2;
        u8 threePc : 1;
        u8 res     : 2;
        u8 protect : 1;
      };
#endif
    };
    union inquiryDataByte6 {
      u8 dataHex;
#ifdef __LITTLE_ENDIAN__
      struct {
        u8 re0     : 1;
        u8 obs0    : 3;
        u8 multip  : 1;
        u8 vs      : 1;
        u8 encserv : 1;
        u8 ob1     : 1;
      };
#else
      struct {
        u8 ob1     : 1;
        u8 encserv : 1;
        u8 vs      : 1;
        u8 multip  : 1;
        u8 obs0    : 3;
        u8 re0     : 1;
      };
#endif
    };
    union inquiryDataByte7 {
      u8 dataHex;
#ifdef __LITTLE_ENDIAN__
      struct {
        u8 vs     : 1;
        u8 cmdque : 1;
        u8 obs0   : 2;
        u8 res    : 2;
        u8 obs1   : 2;
      };
#else
      struct {
        u8 obs1   : 2;
        u8 res    : 2;
        u8 obs0   : 2;
        u8 cmdque : 1;
        u8 vs     : 1;
      };
#endif
    };

    // XeLL only reads the first 36 bytes.
    struct XE_ATAPI_INQUIRY_DATA {
      inquiryDataByte0 byte0;
      inquiryDataByte1 byte1;
      u8 version;
      inquiryDataByte3 byte3;
      u8 additionalLength;
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
      u16 generalConfiguration;     // word 0
      u16 numberOfCylinders;        // word 1
      u16 reserved1;                // word 2
      u16 numberOfHeads;            // word 3
      u16 reserved2[2];             // word 4-5
      u16 NumberOfSectorsPerTrack;  // word 6
      u16 reserved3[3];             // word 7-9
      u8 serialNumber[20];          // word 10-19
      u16 reserved4[3];             // word 20-22
      u8 firmwareRevision[8];       // word 23-26
      u8 modelNumber[40];           // word 27-46
      u16 maximumBlockTransfer : 8; // word 47
      u16 reserved5            : 8;
      u16 reserved6;              // word 48
      u16 capabilities;           // word 49
      u16 reserved7;              // word 50
      u16 reserved8          : 8; // word 51
      u16 pioCycleTimingMode : 8;
      u16 reserved9;                  // word 52
      u16 translationFieldsValid : 3; // word 53
      u16 reserved10             : 13;
      u16 numberOfCurrentCylinders;  // word 54
      u16 numberOfCurrentHeads;      // word 55
      u16 currentSectorsPerTrack;    // word 56
      u32 currentSectorCapacity;     // word 57-58
      u16 currentMultiSectorSetting; // word 59
      u32 userAddressableSectors;    // word 60-61
      u16 singleWordDMASupport : 8;  // word 62
      u16 singleWordDMAActive  : 8;
      u16 multiWordDMASupport  : 8; // word 63
      u16 multiWordDMAActive   : 8;
      u16 advancedPIOModes     : 8; // word 64
      u16 reserved11           : 8;
      u16 minimumMWXferCycleTime;     // word 65
      u16 recommendedMWXferCycleTime; // word 66
      u16 minimumPIOCycleTime;        // word 67
      u16 minimumPIOCycleTimeIORDY;   // word 68
      u16 reserved12[11];             // word 69-79
      u16 majorRevision;              // word 80
      u16 minorRevision;              // word 81
      union {                         // word 82
        struct {
          u16 SMARTFeatureSupport             : 1;
          u16 securityModeFeatureSupport      : 1;
          u16 removableMediaFeatureSupport    : 1;
          u16 powerManagementFeatureSupport   : 1;
          u16 packetFeatureSupport            : 1;
          u16 writeCacheFeatureSupport        : 1;
          u16 lookAheadFeatureSupport         : 1;
          u16 releaseInterruptSupport         : 1;
          u16 serviceInterruptSupport         : 1;
          u16 deviceResetCommandSupport       : 1;
          u16 hostProtectedAreaFeatureSupport : 1;
          u16 reserved13                      : 1;
          u16 writeBufferCommandSupport       : 1;
          u16 readBufferCommandSupport        : 1;
          u16 nopCommandSupport               : 1;
          u16 reserved14                      : 1;
        };
        u16 dataAsu16;
      } support1;
      union { // word 83
        struct {
          u16 downloadMicrocodeCommandSupport       : 1;
          u16 dmaQueuedCommandSupport               : 1;
          u16 CFAFeatureSupport                     : 1;
          u16 advancedPowerManagementFeatureSupport : 1;
          u16 mediaStatusNotificationFeatureSupport : 1;
          u16 powerFromStandbyFeatureSupport        : 1;
          u16 setFeaturesAfterPowerUpRequired       : 1;
          u16 addressOffsetReservedAreaBoot         : 1;
          u16 setMaximumCommandSupport              : 1;
          u16 acousticManagementFeatureSupport      : 1;
          u16 lba48BitFeatureSupport                : 1;
          u16 deviceConfigOverlapFeatureSupport     : 1;
          u16 flushCacheCommandSupport              : 1;
          u16 flushCacheExtCommandSupport           : 1;
          u16 reserved15                            : 2;
        };
        u16 dataAsu16;
      } support2;
      union { // word 84
        u16 dataAsu16;
      } support3;
      union { // word 85
        struct {
          u16 SMARTFeatureEnabled             : 1;
          u16 securityModeFeatureEnabled      : 1;
          u16 removableMediaFeatureEnabled    : 1;
          u16 powerManagementFeatureEnabled   : 1;
          u16 packetFeatureEnabled            : 1;
          u16 writeCacheFeatureEnabled        : 1;
          u16 lookAheadFeatureEnabled         : 1;
          u16 releaseInterruptEnabled         : 1;
          u16 serviceInterruptEnabled         : 1;
          u16 deviceResetCommandEnabled       : 1;
          u16 hostProtectedAreaFeatureEnabled : 1;
          u16 reserved16                      : 1;
          u16 writeBufferCommandEnabled       : 1;
          u16 readBufferCommandEnabled        : 1;
          u16 nopCommandEnabled               : 1;
          u16 reserved17                      : 1;
        };
        u16 dataAsu16;
      } enabled1;
      union { // word 86
        struct {
          u16 downloadMicrocodeCommandEnabled       : 1;
          u16 dmaQueuedCommandEnabled               : 1;
          u16 CFAFeatureEnabled                     : 1;
          u16 ddvancedPowerManagementFeatureEnabled : 1;
          u16 mediaStatusNotificationFeatureEnabled : 1;
          u16 powerFromStandbyFeatureEnabled        : 1;
          u16 setFeaturesAfterPowerUpRequired2      : 1;
          u16 addressOffsetReservedAreaBoot2        : 1;
          u16 setMaximumCommandEnabled              : 1;
          u16 acousticManagementFeatureEnabled      : 1;
          u16 lba48BitFeatureEnabled                : 1;
          u16 deviceConfigOverlapFeatureEnabled     : 1;
          u16 flushCacheCommandEnabled              : 1;
          u16 flushCacheExtCommandEnabled           : 1;
          u16 reserved18                            : 2;
        };
        u16 dataAsu16;
      } enabled2;
      union { // word 87
        u16 dataAsu16;
      } enabled3;
      u16 ultraDMASupport : 8; // word 88
      u16 ultraDMAActive  : 8;
      u16 reserved19[11];                 // word 89-99
      u32 userAddressableSectors48Bit[2]; // word 100-104
      u16 reserved20[23];                 // word 104-126
      u16 mediaStatusNotification : 2;    // word 127
      u16 reserved21              : 6;
      u16 deviceWriteProtect      : 1;
      u16 reserved22              : 7;
      u16 securitySupported       : 1; // word 128
      u16 securityEnabled         : 1;
      u16 securityLocked          : 1;
      u16 securityFrozen          : 1;
      u16 securityCountExpired    : 1;
      u16 securityEraseSupported  : 1;
      u16 reserved23              : 2;
      u16 securityLevel           : 1;
      u16 reserved24              : 7;
      u16 reserved25[127]; // word 129-255
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
        u8 Immediate         : 1;
        u8 CommandUniqueBits : 4;
        u8 LogicalUnitNumber : 3;
        u8 CommandUniqueBytes[3];
        u8 Link         : 1;
        u8 Flag         : 1;
        u8 Reserved     : 4;
        u8 VendorUnique : 2;
      } CDB6GENERIC, *PCDB6GENERIC;
#else
      struct _CDB6GENERIC {
        u8 VendorUnique : 2;
        u8 Reserved     : 4;
        u8 Flag         : 1;
        u8 Link         : 1;
        u8 CommandUniqueBytes[3];
        u8 LogicalUnitNumber : 3;
        u8 CommandUniqueBits : 4;
        u8 Immediate         : 1;
        u8 OperationCode;
      } CDB6GENERIC, *PCDB6GENERIC;
#endif
      //
      // Standard 10-byte CDB
      //

#ifdef __LITTLE_ENDIAN__
      struct _CDB10 {
        u8 OperationCode;
        u8 RelativeAddress   : 1;
        u8 Reserved1         : 2;
        u8 ForceUnitAccess   : 1;
        u8 DisablePageOut    : 1;
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
        u8 DisablePageOut    : 1;
        u8 ForceUnitAccess   : 1;
        u8 Reserved1         : 2;
        u8 RelativeAddress   : 1;
        u8 OperationCode;
      } CDB10, *PCDB10;
#endif

      //
      // Standard 12-byte CDB
      //

#ifdef __LITTLE_ENDIAN__
      struct _CDB12 {
        u8 OperationCode;
        u8 RelativeAddress   : 1;
        u8 Reserved1         : 2;
        u8 ForceUnitAccess   : 1;
        u8 DisablePageOut    : 1;
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
        u8 DisablePageOut    : 1;
        u8 ForceUnitAccess   : 1;
        u8 Reserved1         : 2;
        u8 RelativeAddress   : 1;
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
    // ATAPI Register State
    //
    struct ATAPI_REG_STATE {
      // Command Block
      u32 data;       // Address 0x00
      struct {        // Address 0x01
        u32 error;    // When Read
        u32 features; // When Written
      };
      struct {               // Address 0x02
        u32 interruptReason; // When Read
        u32 sectorCount;     // When Written
      };
      u8 lbaLow;        // Address 0x03
      u8 byteCountLow;  // Address 0x04
      u8 byteCountHigh; // Address 0x05
      u32 deviceSelect; // Address 0x06
      struct {          // Address 0x07
        u32 status;     // When Read
        u32 command;    // When Written
      };
      // Control Block
      struct {             // Address 0xA
        u32 altStatus;     // When Read
        u32 deviceControl; // When Written
      };
      u32 SStatus;  // Address 0x10 (4 bytes)
      u32 SError;   // Address 0x14 (4 bytes)
      u32 SControl; // Address 0x18 (4 bytes)
      u32 SActive;  // Address 0x1C (4 bytes)

      // Transfer mode, set by the set features command using the subcommand 0x3.
      u32 ataTransferMode;

      // DMA registers
      u32 dmaCommand;
      u32 dmaStatus;
      u32 dmaTableOffset;
    };

    // SSC (Spindle Speed Control) State
    struct SSC_STATE {
      u8 currentSpeed = 3; // speed index 1-4 (1x-4x DVD); kernel traps on 0 or > 4.
      bool initialized = false;
    };

    //
    // AP2.0 drive authentication state.
    //

    // The hypervisor builds a vendor mode page, the kernel sends it with MODE SELECT(10) and reads the reply back with
    // MODE SENSE(10). The page code is not a constant: HvxDvdAuthBuildNVPage (HV syscall 37) emits 0x3B, or 0x28 where
    // the 0x200 console flag is set. Both variants lay their fields out identically, so only the total length differs,
    // and the code that arrived is echoed back rather than hardcoded.

    // Page Structure:
    //   [0..1]    mode data length = page length + 8
    //   [8]       page code          [9]  page length
    //   [10..25]  AES-CBC(DVD key, IV) of the session key
    //   [26..41]  same chain, of the challenge
    //   [42..57]  IV
    //   [58..73]  MAC over [10..41], 0x28 variant only; the drive ignores it

    struct AP20_AUTH_STATE {
      static constexpr u32 MAX_PAGE_SIZE = 74; // Largest of the two variants.
      static constexpr u32 PAGE_CODE_OFFSET = 8;
      static constexpr u32 PAGE_LENGTH_OFFSET = 9;
      static constexpr u32 PAGE_BODY_OFFSET = 10;
      // The reply is 58 bytes for both variants: HvxDvdAuthVerifyNVPage checks reply[9] == 0x30 unconditionally.
      static constexpr u8 REPLY_PAGE_LENGTH = 0x30;
      static constexpr u32 REPLY_SIZE = PAGE_BODY_OFFSET + REPLY_PAGE_LENGTH; // 58

      // Returns true if the page code is Auth related.
      static bool IsAuthPageCode(u8 pageCode) { return pageCode == 0x3B || pageCode == 0x28; }

      // The firmware challenge/response shares the AP2.0 transport and is told apart only by its page code, so it must
      // not reach the AP2.0 handler. SataCdRomFwcrExecute picks one per operation.
      static bool IsFwcrPageCode(u8 pageCode) {
        return pageCode == 0x21 || pageCode == 0x23 || pageCode == 0x24 || pageCode == 0x29;
      }

      u8 pageCode = 0;             // Only meaningful once pageValid.
      u32 pageSize = 0;            // Bytes actually received.
      u8 page[MAX_PAGE_SIZE] = {}; // The MODE SELECT(10) parameter data.
      bool selectPending = false;  // Between the CDB and the end of data-out.
      bool pageValid = false;
    };

    //
    // Media presence / ready state machine.
    //

    // A real drive does not present a disc as instantly readable: on insertion it reports unit attention (sense 6 / ASC
    // 0x28), then "becoming ready" (sense 2 / ASC 0x04) while it spins up, and only then GOOD. With no disc it reports
    // "medium not present" (sense 2 / ASC 0x3A).
    // SataCdRomFinishRequestSense branches on exactly these pairs: ASC 0x04 retries on a 100 ms timer, ASC 0x3A maps to
    // STATUS_NO_MEDIA_IN_DEVICE.
    enum class MediaState : u32 {
      NoMedia = 0,   // Tray empty / no image: report "medium not present".
      BecomingReady, // Disc present, spinning up: report "not ready".
      Ready          // Disc present and readable.
    };

    // Fixed-format (18 byte) SCSI sense the drive will hand back on the next REQUEST SENSE. Key/ASC/ASCQ follow SPC.
    // The kernel only inspects the sense key (byte 2 low nibble) and the ASC (byte 12).
    struct SCSI_SENSE {
      u8 key = 0;  // Sense key (0 = NO SENSE / GOOD).
      u8 asc = 0;  // Additional Sense Code.
      u8 ascq = 0; // Additional Sense Code Qualifier.
    };

    //
    // Authentication mode page (MODE SENSE/SELECT(10) page 0x3E).
    //

    // Offsets are into the whole 42-byte reply: 8-byte mode parameter header
    // then the page.

    // Page Strucutre:
    //   +0x0B  must read 1 or the disc is refused.
    //   +0x0D  disc book type and version, compared against control data [4]
    //   +0x0E  anti-piracy revision, non-XGD2 branch only.
    //   +0x0F..+0x17  the challenge exchange.
    struct AUTH_PAGE {
      static constexpr u32 SIZE = 42;
      static constexpr u32 PAGE_CODE = 0x08; // 0x3E in bits 0-5; bit 6 is a hypervisor flag.
      static constexpr u32 PAGE_LENGTH = 0x09;
      static constexpr u32 FINAL_FLAG = 0x0A;
      static constexpr u32 VALID_FLAG = 0x0B;
      static constexpr u32 AUTH_FLAG = 0x0C;
      static constexpr u32 DISC_TYPE = 0x0D;
      static constexpr u32 AP_LEVEL = 0x0E;
      static constexpr u32 CHALLENGE_INDEX = 0x0F;
      static constexpr u32 CHALLENGE_DATA = 0x10; // u32, BIG endian.
      static constexpr u32 RESPONSE_DATA = 0x14;  // u32, LITTLE endian.
      // The IV the drive chose, published in the clear. The hypervisor keeps the page body from +0x08 and reads this
      // back as its state +0x12.
      static constexpr u32 SESSION_IV = 0x1A; // 26, 16 bytes

      // +0x0A..+0x19 does NOT travel in the clear, the flags, challenge index, challenge word and response are one
      // AES-CBC block under the AP2.0 session key, with SESSION_IV as the IV. HvxDvdAuthGetAuthPage encrypts it before
      // the kernel sends the page. HvxDvdAuthVerifyAuthPage and HvxDvdAuthRecordAuthenticationPage decrypt it.
      static constexpr u32 BODY_CIPHER = 0x0A;
      static constexpr u32 BODY_CIPHER_LENGTH = 16; // Exactly one AES block.

      static constexpr u8 PAGE_CODE_AUTH = 0x3E;
      static constexpr u8 VALID_FLAG_SET = 0x01;
      // What HvxDvdAuthVerifyAuthPage requires of the page it gets back.
      static constexpr u8 PAGE_LENGTH_VALUE = 0x20; // 32, checked byte for byte.
    };

    //
    // The disc's own plaintext challenge/response table.
    //

    // The control data carries the challenge table twice. At +0x308 sit 21 encrypted 12-byte entries for the
    // hypervisor, under a key it only derives after RSA-verifying the disc signature. At +0x204 sit the same pairs in
    // the clear, 9 bytes each, for the drive.
    // Entry Structure:
    //   [0..3] challenge, in the same byte order the page carries it
    //   [4..7] response, in wire order - copy verbatim, never reorder
    //   [8]    zero in every record seen
    //

    // HvxDvdAuthVerifyAuthPage reads the answer two ways, by entry type:
    //   type & 0x20 == 0  (1, 0x14, 0x15)    lwz, big endian, exact match
    //   type & 0x20 != 0  (0x24, 0x25, 0xE0) lwbrx, little endian. The low halfword is an angle in degrees and must
    //                                        land within the entry's tolerance, wrapping at 0x168 = 360.

    // The disc does not carry the type, so the two are told apart structurally:
    // Response[2..3] == 0 means an angular record holding only the angle, and the drive supplies the high halfword
    // from the challenge's first two bytes (little endian, so [2] = challenge[1], [3] = challenge[0]). Otherwise all
    // four bytes are the answer.

    // On a retail XGD2 disc that splits the eight records into four tokens and four angles reading 0, 91, 180 and 270
    // degrees, measurement points around the platter.

    struct DISC_CHALLENGE_TABLE {
      // Offsets are into the 1640-byte control data, which is the 2048-byte security sector shifted by 4 (structure[x]
      // = sector[x-4]), so this is sector +0x200.
      static constexpr u32 BASE = 0x204;
      static constexpr u32 RECORD_SIZE = 9;
      static constexpr u32 CHALLENGE = 0; // 4 bytes, same order as the page.
      static constexpr u32 RESPONSE = 4;  // u32, wire order, copy verbatim.
      // The encrypted copy of the table starts at +0x304, which bounds this one.
      static constexpr u32 END = 0x304;
      static constexpr u32 MAX_RECORDS = (END - BASE) / RECORD_SIZE; // 28
      // Both copies of the table hold the same number of entries, and the count is in the clear next to the encrypted
      // copy. Bounding the scan with it rather than with MAX_RECORDS keeps it inside the table: 21 records of 9 bytes
      // end at +0x2C1, and at least one disc carries an unrelated four-byte field further on at sector +0x2D0 that a
      // longer scan would straddle and read as a record.
      static constexpr u32 COUNT = 0x305; // u8, 1..21.
      static constexpr u8 MAX_COUNT = 21;
    };

    //
    // The raw security-sector records, for the answers +0x204 does not carry.
    //

    // Types 1 and 0xE0 have no record in the challenge table. Their answers are in a second table in the part of the
    // sector the host never sees: READ DVD STRUCTURE returns the sector's first 1636 bytes plus a header, so everything
    // from sector +0x664 on is the drive's alone.

    // 9-byte records from sector +0x661, the layout abgx360 prints for the SS:
    //
    //   [0]    response type, mirroring the hypervisor's entry type:
    //          0x03->0x14  0x01->0x15  0x07->0x24  0x05->0x25 0x00->0x01  0xE0->0xE0
    //   [1]    challenge id, the page's CHALLENGE_INDEX, matched on.
    //   [2]    modifier, zero is what it's been seen.
    //   [3..5] first 24-bit field.
    //   [6..8] second 24-bit field.

    // For the ordinary types the fields are the sector bounds of the challenge's region and the answer comes from
    // +0x204. For types 1 and 0xE0 they carry the answer: (field1 & 0xFFFF) << 16 | (field2 & 0xFFFF).

    struct RAW_SS_RECORDS {
      static constexpr u32 BASE = 0x661; // Offset into the raw 2048-byte sector.
      static constexpr u32 RECORD_SIZE = 9;
      static constexpr u32 COUNT = 10;
      static constexpr u32 TYPE = 0;
      static constexpr u32 ID = 1;
      static constexpr u32 MODIFIER = 2;
      static constexpr u32 FIELD1 = 3; // 24 bit.
      static constexpr u32 FIELD2 = 6; // 24 bit.

      static constexpr u8 TYPE_EXACT = 0x00;   // Hypervisor entry type 0x01.
      static constexpr u8 TYPE_ANGULAR = 0xE0; // Hypervisor entry type 0xE0.

      // Only these two carry an answer here, the rest are sector bounds.
      static bool CarriesResponse(u8 type) { return type == TYPE_EXACT || type == TYPE_ANGULAR; }

      // 0xE0 is in the angular family, so HvxDvdAuthVerifyAuthPage reads its answer little endian, where type 1 is read
      // big endian.
      static bool IsAngular(u8 type) { return type == TYPE_ANGULAR; }
    };

    //
    // Where the disc's LBA 0 actually is.
    //

    // The console addresses the disc from the start of the game partition, but an image file usually carries the video
    // partition first. Serving raw file offsets is therefore wrong by a constant, and wrong everywhere at once: the
    // filesystem mounts from the wrong volume and the hypervisor's LBA hash verification reads the video volume
    // descriptor.

    // Found the way every tool finds it: the sector at partition + 0x10000 begins "MICROSOFT*XBOX*MEDIA".
    struct GAME_PARTITION {
      static constexpr u64 MAGIC_OFFSET = 0x10000; // sector 32 of the partition
      static constexpr char MAGIC[] = "MICROSOFT*XBOX*MEDIA";
      static constexpr u32 MAGIC_LENGTH = 20;

      // In the order they are tried. 0 first so an image that is already just the game partition is taken directly.
      static constexpr u64 CANDIDATES[] = {
        0x00000000, // Raw game partition, as extracted.
        0x0FD90000, // XGD2
        0x02080000, // XGD3
        0x18300000, // XGD1
      };
    };

    // Answering a challenge has to take time, this is part of the check:
    // SataCdRomSendAP20Select brackets its IOCTL with mftb and passes the timebase delta to HvxDvdAuthVerifyAuthPage,
    // which counts the challenge towards state +0x156 bits 0..3 only if the delta reaches 0x2625A0. The final round,
    // marked by bit 0x80 of state +0x150 once every quota is spent, requires at least state[0x157] >> 4 of them and
    // does not look at the response at all, so a drive that answers instantly fails with a perfect set of answers.

    struct CHALLENGE_SEEK {
      static constexpr u64 MINIMUM_TICKS = 0x2625A0; // 50 ms at 50 MHz
      static constexpr u32 DELAY_MS = 60;            // margin for scheduling jitter
    };

    // Per-disc security data, read off the physical disc and not usually present in a plain ISO:
    // The authentication page (MODE SENSE(10) page 0x3E, 42 bytes) and the control data (READ DVD STRUCTURE, 1640
    // bytes). Genuine per-disc blobs, served verbatim from the dump and verified by the guest hypervisor against the
    // console keys. Without them authentication fails the way it does on a drive that cannot read the security area.
    struct DISC_SECURITY_DATA {
      bool authPagePresent = false;
      bool controlDataPresent = false;
      u8 authPage[42] = {};      // MODE SENSE(10) page 0x3E body (incl. 8-byte header).
      u8 controlData[1640] = {}; // READ DVD STRUCTURE (0xAD) format 0 response.
      // The whole 2048-byte sector.
      bool rawSectorPresent = false;
      u8 rawSector[2048] = {};
      // Set when authPage was built from the control data rather than captured.
      bool authPageSynthesized = false;
    };

    // ATAPI Device State Structure
    struct ATAPI_DEV_STATE {
      // Register Set
      ATAPI_REG_STATE regs = {};
      // Identify Data for our ODD Drive.
      XE_ATAPI_IDENTIFY_DATA atapiIdentifyData = {};
      // Inquiry data for our ODD Drive.
      XE_ATAPI_INQUIRY_DATA atapiInquiryData = {};
      // Mounted ISO Image.
      std::unique_ptr<ReadOnlyStorage> mountedODDImage{};
      // Byte offset of the game partition within that image. Every LBA the
      // guest asks for is relative to this. See GAME_PARTITION.
      u64 gamePartitionOffset = 0;
      // Input/Output buffers.
      ODDDataBuffer dataInBuffer;
      ODDDataBuffer dataOutBuffer;
      // SCSI Command Descriptor Block
      XE_CDB scsiCBD = {};
      // DMA State
      XE_ATAPI_DMA_STATE dmaState = {};
      // Do we have an image?
      bool imageAttached = false;
      // Is there a SCSI command pending for processing?
      std::atomic<bool> scsiCommandPending{false};
      // Media presence / ready state.
      MediaState mediaState = MediaState::NoMedia;
      // Sense to report on the next REQUEST SENSE.
      SCSI_SENSE pendingSense = {};
      // Number of "not ready" replies still owed before the drive reports ready,
      // modelling spin-up latency. Decremented on each TEST UNIT READY.
      u32 spinUpRepliesRemaining = 0;
      // Per-disc security data for the authentication path.
      DISC_SECURITY_DATA security = {};
    };

    class ODD : public PCIDevice {
    public:
      ODD(const char* deviceName, u64 size, PCIBridge* parentPCIBridge, RAM* ram);

      void Read(u64 readAddress, u8* data, u64 size) override;
      void Write(u64 writeAddress, const u8* data, u64 size) override;
      void MemSet(u64 writeAddress, s32 data, u64 size) override;
      void ConfigRead(u64 readAddress, u8* data, u64 size) override;
      void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;

      // Simulate a disc being inserted or removed at runtime. InsertDisc drives the same signalling a real drive does
      // on tray-close: a unit-attention media-change followed by a spin-up delay before the drive reports ready.
      // Should be called in coordination with the SMC handlers to notify the guest kernel of a Tray/Disc state change.
      void InsertDisc();
      void EjectDisc();

      // Whether a disc is currently present (inserted or spinning up).
      bool IsDiscPresent();

      // Whether an image is configured/attached at all.
      bool HasImage() const { return atapiState.imageAttached; }

    private:
      // PCI Bridge pointer. Used for Interrupts.
      PCIBridge* parentBus;

      // RAM Pointer for DMA ops.
      RAM* ramPtr;

      // Mutex for synchronizing access to atapiState between the worker thread and PCI Read/Write methods.
      std::mutex oddMutex;

      // ATAPI Device State.
      ATAPI_DEV_STATE atapiState = {};

      // Worker Thread for DMA requests.
      std::thread oddWorkerThread;

      // Thread running
      std::atomic<bool> oddThreadRunning{false};

      //
      // State
      //

      // The console's DVD key, from keyvault of the console whose NAND is booted.
      u8 dvdKey[16] = {};
      bool dvdKeyValid = false;

      // AP2.0 drive authentication state, and the session key it recovers.
      AP20_AUTH_STATE ap20 = {};
      u8 ap20SessionKey[16] = {};
      bool ap20SessionKeyValid = false;

      // Set when a MODE SELECT(10) page 0x3E carried a challenge, so the worker thread waits before completing it.
      bool challengeNeedsSeekDelay = false;

      // Set between a PACKET command and the CDB that follows, so the CDB is dispatched exactly once.
      bool packetCdbPending = false;

      // SSC (Spindle Speed Control) state.
      SSC_STATE sscState = {};

      //
      // Worker thread, DMA and interrupts
      //

      void oddThreadLoop();
      void doDMA();
      void atapiIssueInterrupt();
      void processSCSICommand();

      //
      // Command completion helpers
      //

      // A command with no output and no error.
      void atapiNopCommand();
      // Register tail shared by every data in command: signals a transfer to the host, publishes the byte count and
      // sets DRQ (PIO) or BSY (DMA).
      void completeDataIn(u32 transferSize);
      // Serve 'length' bytes as the data in phase of the current command, never exceeding the CDB allocation length,
      // then completeDataIn.
      void scsiDataIn(const void* response, u32 length, u32 allocLen);
      // Queue the sense the next REQUEST SENSE reports, a non-zero key also raises CHECK CONDITION on the current
      // command.
      void setPendingSense(u8 key, u8 asc, u8 ascq);
      void failWithSense(u8 key, u8 asc, u8 ascq);

      //
      // ATA commands
      //

      void atapiIdentifyCommand();
      void atapiIdentifyPacketDeviceCommand();

      //
      // Mount time: image layout, security data and keys
      //

      // Find the game partition in the mounted image, every guest LBA is relative to it.
      void locateGamePartition();
      // Load the auth page and control data sitting beside the image.
      void loadDiscSecurityData();
      // Build an authentication page from the control data when none was captured, as the drive does when it detects
      // media.
      void synthesizeAuthPage();
      // Loads the DVD Key from a file.
      void loadDvdKey();

      //
      // SCSI commands
      //

      void scsiReadCapacityCommand();
      void scsiInquiryCommand();
      void scsiRead10Command();
      void scsiReadTocCommand();
      // GET CONFIGURATION (0x46): reports the current profile (DVD-ROM).
      void scsiGetConfigurationCommand();
      void scsiRequestSenseCommand();
      void scsiGetEventStatusNotificationCommand();
      // TEST UNIT READY (0x00): advances the spin-up state machine and reports GOOD or CHECK CONDITION with the
      // matching sense.
      void scsiTestUnitReadyCommand();
      // Commands that only have to succeed.
      void scsiStartStopUnitCommand();
      void scsiPreventAllowRemovalCommand();
      void scsiSetCdSpeedCommand();

      //
      // MODE SELECT(10) / MODE SENSE(10)
      //

      // MODE SELECT(10) (0x55): accepts a parameter list, whose data phase is consumed by consumeModeSelectData().
      void scsiModeSelect10Command();
      // Takes a completed parameter list out of the input buffer and routes it on its page code.
      void consumeModeSelectData();
      // MODE SENSE(10) (0x5A): dispatches on the CDB page code.
      void scsiModeSense10Command();
      void scsiModeSense6Command();
      // Page 0x20: SSC spindle-speed page.
      void scsiModeSense10Page20Command(u16 allocLen);
      // Page 0x2A: CD/DVD capabilities and mechanical status.
      void scsiModeSense10Page2ACommand(u16 allocLen);

      //
      // AP2.0 drive authentication
      //

      // MODE SENSE(10) for the AP2.0 page: decrypt the challenge with the DVD key, re-encrypt the response with the
      // session key.
      void performAP20Auth(u16 allocLen);
      // Page 0x21/0x23/0x24/0x29: the firmware challenge/response, which an emulated drive cannot answer. Fails cleanly
      // instead of pretending.
      void scsiModeSense10FwcrCommand(u8 pageCode, u16 allocLen);

      //
      // XGD2 disc authentication
      //

      // Encrypt or decrypt AUTH_PAGE::BODY_CIPHER in place under the AP2.0 session key with the page's own SESSION_IV.
      // False when there is no session key yet, in which case the page is left untouched.
      bool cryptAuthPageBody(u8* page, bool encrypt);
      // Apply a MODE SELECT(10) page 0x3E to the drive's authentication page.
      void applyAuthPageSelect(const u8* page, u32 length);
      // Answer a challenge from the disc's plaintext table. Takes the *plaintext* select page, true when a response was
      // written.
      bool answerDiscChallenge(const u8* selectPage);
      // Answer one from the raw security-sector records, for the entry types the +0x204 table does not cover.
      bool answerFromRawSecuritySector(const u8* selectPage);
      // Page 0x3E: the per-disc DVDX2 authentication page.
      void scsiModeSense10Page3ECommand(u16 allocLen);
      // Put the control data into the form the drive transmits: layer descriptor in the clear, the rest encrypted under
      // the session key. False when it could not be done, leaving the buffer untouched.
      bool encryptControlData(u8* controlData, u32 length);
      // READ DVD STRUCTURE (0xAD): serves the per-disc control data.
      void scsiReadDvdStructureCommand();

      //
      // Debug name tables
      //

      std::string getATACommandName(u32 commandID);
      std::string getSCSICommandName(u32 commandID);
      std::string getATAPIRegisterName(u32 regID);
    };

  } // namespace PCIDev
} // namespace Xe
