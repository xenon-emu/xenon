/*
* Copyright 2006-2025 c0z, tmbinc, tydye81, and unknown hackers.
*
* Adapted by xenon-emu, for use in Xenon.
*
* All rights reserved, all code provided without any warranty.
* Copyright belongs to their respective authors.
* Source originally provided in various forms.
* If you believe this is incorrect, contact us on GitHub.
*
* Huge thanks to the person who provided all of this to us, digging in old archives of theirs,
* free of charge so long as credits are provided to their respective authors.
* They asked to remain anonymous, as they're not apart of anything anymore. You know who you are.
*
* Much love to c0z, for all of his work in the Xbox 360 scene.
*/

#include "Base/Types.h"

namespace Xe::XCPU::SOC {

//
// Xenon CPU System On Chip structures.
//

// | Block              | Offset in memory    | Size (Hex)  | Size (Dec)
// =====================================================================
// | SOCROM_BLOCK      	| 0x80000200_00000000	| 0x8000      | 32768
// | SOCROM_BLOCK      	| 0x80000200_00008000	| 0x8000      | 32768
// | SOCSRAM_BLOCK      | 0x80000200_00010000	| 0x10000     | 32768
// | SOCSECOTP_BLOCK    | 0x80000200_00020000	| 0x4000      | 16384
// | SOCSECENG_BLOCK    | 0x80000200_00024000	| 0x2000      | 8192
// | SOCSECRNG_BLOCK    | 0x80000200_00026000	| 0x2000      | 8192
// | SOCCBI_BLOCK      	| 0x80000200_00028000	| 0x8000      | 32768
// | SOCFSBTTX_BLOCK    | 0x80000200_00030000	| 0x8000      | 32768
// | SOCFSBTRX_BLOCK    | 0x80000200_00038000	| 0x8000      | 32768
// | SOCFSBLTX_BLOCK    | 0x80000200_00040000	| 0x8000      | 32768
// | SOCFSBLRX_BLOCK    | 0x80000200_00048000	| 0x8000      | 32768
// | SOCINTS_BLOCK      | 0x80000200_00050000	| 0x8000      | 32768
// | SOCPMW_BLOCK      	| 0x80000200_00060000	| 0x1000/200  | 4096
// | SOCPRV_BLOCK      	| 0x80000200_00061000	| 0x1000/200  | 4096

// Block Addresses in MMIO and sizes:

// Secure ROM Block
#define XE_SECROM_BLOCK_START     0x00000000ULL
#define XE_SECROM_BLOCK_SIZE      0x10000

// Secure RAM Block
#define XE_SECRAM_BLOCK_START     0x00010000ULL
#define XE_SECRAM_BLOCK_SIZE      0x10000

// Security One-Time-Programmable Block
#define XE_SOCSECOTP_BLOCK_START  0x00020000ULL
#define XE_SOCSECOTP_BLOCK_SIZE   0x4000

// Security Engine Block
#define XE_SOCSECENG_BLOCK_START  0x00024000ULL
#define XE_SOCSECENG_BLOCK_SIZE   0x2000

// Secure Random Number Generator Block
#define XE_SOCSECRNG_BLOCK_START  0x00026000ULL
#define XE_SOCSECRNG_BLOCK_SIZE   0x2000

// CBI Block
#define XE_SOCCBI_BLOCK_START     0x00028000ULL
#define XE_SOCCBI_BLOCK_SIZE      0x8000

// Front Side Bus TTX Block
#define XE_SOCFSBTTX_BLOCK_START  0x00030000ULL
#define XE_SOCFSBTTX_BLOCK_SIZE   0x8000

// Front Side Bus TRX Block
#define XE_SOCFSBTRX_BLOCK_START  0x00038000ULL
#define XE_SOCFSBTRX_BLOCK_SIZE   0x8000

// Front Side Bus LTX Block
#define XE_SOCFSBLTX_BLOCK_START  0x00040000ULL
#define XE_SOCFSBLTX_BLOCK_SIZE   0x8000

// Front Side Bus LRX Block
#define XE_SOCFSBLRX_BLOCK_START  0x00048000ULL
#define XE_SOCFSBLRX_BLOCK_SIZE   0x8000

// Interrupts Block
#define XE_SOCINTS_BLOCK_START    0x00050000ULL
#define XE_SOCINTS_BLOCK_SIZE     0x8000

// Power Management Block
#define XE_SOCPMW_BLOCK_START     0x00060000ULL
#define XE_SOCPMW_BLOCK_SIZE      0x1000

// Pervasive Logic Block
#define XE_SOCPRV_BLOCK_START     0x00061000ULL
#define XE_SOCPRV_BLOCK_SIZE      0x1000

//
// System On Chip Secure ROM Block
// Offset: 0x80000200_00000000
//

// Contains 1BL.
typedef struct _SOCSECROM_BLOCK {
  u64 Array[0x1000];    // Offset 0x0    Size: 0x8000
  u64 ArrayAlias[0x1000]; // Offset 0x8000 Size: 0x8000
} XE_SOCSECROM_BLOCK, * XE_PSOCSECROM_BLOCK;// Size: 0x10000

//
// System On Chip Secure RAM Block
// Offset: 0x80000200_00010000
//

// Where CB is copied/executed and Hashes for SECENG are stored.
typedef struct _SOCSECRAM_BLOCK {
  u64 Array[0x2000]; // Offset: 0x0 Size:0x10000
} XE_SOCSECRAM_BLOCK, * PXE_SOCSECRAM_BLOCK; // Size: 0x10000

//
// System On Chip One-Time-Programmable Block
// Offset: 0x80000200_00020000
//

// The eFuse array starts at 800002000_00020000:
// * Each eFuse array repeats each u64/8bytes of the fuse line 64 times.
// * Each subsequent fuse line is 0x200 bytes apart.
// * There are 12 fuse lines total.

typedef union _SECURITY_BITS {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 56; // 0xFF Filled.
    u64 Eeprom : 2;
    u64 Unlock : 2;// Typically only these two bits are set to 1, aside from reserved.
    u64 Secure : 2;
    u64 NotValid : 2;
  } AsBITS;
#else
  struct {
    u64 NotValid : 2;
    u64 Secure : 2;
    u64 Unlock : 2;// Typically only these two bits are set to 1, aside from reserved.
    u64 Eeprom : 2;
    u64 Reserved1 : 56; // 0xFF Filled.
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG;
} SECURITY_BITS;

// These are the 768 bits of eFuses.
// Ideally you would only use the first item of each of these because of the repeating fuselines.
typedef struct _SOCSECOTP_ARRAY {
  SECURITY_BITS sec[64];
  u64 ConsoleType[64];
  u64 ConsoleSequence[64];
  u64 UniqueId1[64]; // ID1 and ID2 are or'd together, as are ID3 and ID4. These concatenated form your unique CPU key.
  u64 UniqueId2[64]; // Unique ID mask   FF FF FF FF FF FF FF FF FF FF FF FF FF 03 00 00 - 53 of 106 bits MUST be set exactly for the cpu key to be valid.
  u64 UniqueId3[64]; // Hamming/ECD mask 00 00 00 00 00 00 00 00 00 00 00 00 00 FC FF FF - 22 bits error correction data.
  u64 UniqueId4[64];
  u64 UpdateSequence[64];
  u64 EepromKey1[64];
  u64 EepromKey2[64];
  u64 EepromHash1[64];
  u64 EepromHash2[64];
} SOCSECOTP_ARRAY;

typedef union _SECOTP_BUSY {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 63;
    u64 Busy : 1;
  } AsBITS;
#else
  struct {
    u64 Busy : 1;
    u64 Reserved1 : 63;
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG;
} SECOTP_BUSY;

typedef union _SECOTP_PARAMETERS {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 11;
    u64 TimeToPullFSourceToZero : 5;
    u64 TimeToDisconnectFSource : 5;
    u64 TimeToBlowFuse : 9;
    u64 Time400 : 10;
    u64 TimeToStabilizeBlowVoltage : 9;
    u64 SlowClockSelect : 1;
    u64 WidthOfYWindow : 8;
    u64 WidthOfXWindow : 6;
  } AsBITS;
#else
  struct {
    u64 WidthOfXWindow : 6;
    u64 WidthOfYWindow : 8;
    u64 SlowClockSelect : 1;
    u64 TimeToStabilizeBlowVoltage : 9;
    u64 Time400 : 10;
    u64 TimeToBlowFuse : 9;
    u64 TimeToDisconnectFSource : 5;
    u64 TimeToPullFSourceToZero : 5;
    u64 Reserved1 : 11;
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG;
} SECOTP_PARAMETERS;

typedef union _SECOTP_SENSE_CONTROL {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 63;
    u64 TriggerSenseTransaction : 1;
  } AsBITS;
#else
  struct {
    u64 TriggerSenseTransaction : 1;
    u64 Reserved1 : 63;
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG;
} SECOTP_SENSE_CONTROL;

typedef union _SECOTP_BLOW_CONTROL {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 63;
    u64 EnableBlowFuseOperation : 1;
  } AsBITS;
#else
  struct {
    u64 EnableBlowFuseOperation : 1;
    u64 Reserved1 : 63;
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG;
} SECOTP_BLOW_CONTROL;

typedef struct _SOCSECOTP_BLOCK {
  SOCSECOTP_ARRAY Array; // 0 the full fuse array, see note above on fuselines
  u64 Reserved1[256]; // 6144
  SECOTP_BUSY BusyFlag; // 8192
  SECOTP_PARAMETERS Parameters; // 8200
  SECOTP_SENSE_CONTROL SenseControl; // 8208
  SECOTP_BLOW_CONTROL BlowControl; // 8216
  u64 TraceLogicArrayControl; // 8224
  u64 Reserved2[1019]; // 8232
} XE_SOCSECOTP_BLOCK, * PXE_SOCSECOTP_BLOCK; // Size: 0x4000

//
// System On Chip Security Engine Block
// Offset: 0x80000200_00024000
//

typedef union _SECENG_FAULT_ISOLATION {
  u64 AsULONGLONG; // 0x0 sz:0x8
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 63; // 0x0 bfo:0x0
    u64 IntegrityViolation : 1; // 0x0 bfo:0x63
  }AsBits;
#else
  struct {
    u64 IntegrityViolation : 1; // 0x0 bfo:0x63
    u64 Reserved1 : 63; // 0x0 bfo:0x0
  }AsBits;
#endif // __LITTLE_ENDIAN__
} SECENG_FAULT_ISOLATION, * PSECENG_FAULT_ISOLATION; // size 8

typedef struct _SECENG_KEYS {
  u64 WhiteningKey0High; // Offset: 0x0 sz:0x8
  u64 WhiteningKey0Low; // Offset: 0x8 sz:0x8
  u64 WhiteningKey1High; // Offset: 0x10 sz:0x8
  u64 WhiteningKey1Low; // Offset: 0x18 sz:0x8
  u64 WhiteningKey2High; // Offset: 0x20 sz:0x8
  u64 WhiteningKey2Low; // Offset: 0x28 sz:0x8
  u64 WhiteningKey3High; // Offset: 0x30 sz:0x8
  u64 WhiteningKey3Low; // Offset: 0x38 sz:0x8
  u64 AESKey0High; // Offset: 0x40 sz:0x8
  u64 AESKey0Low; // Offset: 0x48 sz:0x8
  u64 AESKey1High; // Offset: 0x50 sz:0x8
  u64 AESKey1Low; // Offset: 0x58 sz:0x8
  u64 AESKey2High; // Offset: 0x60 sz:0x8
  u64 AESKey2Low; // Offset: 0x68 sz:0x8
  u64 AESKey3High; // Offset: 0x70 sz:0x8
  u64 AESKey3Low; // Offset: 0x78 sz:0x8
  u64 HashKey0High; // Offset: 0x80 sz:0x8
  u64 HashKey0Low; // Offset: 0x88 sz:0x8
  u64 HashKey1High; // Offset: 0x90 sz:0x8
  u64 HashKey1Low; // Offset: 0x98 sz:0x8
} SECENG_KEYS, * PSECENG_KEYS; // size 160

typedef struct _SOCSECENG_BLOCK {
  SECENG_KEYS WritePathKeys; // Offset: 0x0 sz:0xA0
  u64 TraceLogicArrayWritePathControl; // Offset: 0xA0 sz:0x8
  u64 qwUnkn1;
  u64 Reserved1[0x1EA]; // Offset: 0xA8 sz:0xF58
  SECENG_KEYS ReadPathKeys; // Offset: 0x1000 sz:0xA0
  u64 TraceLogicArrayReadPathControl; // Offset: 0x10A0 sz:0x8
  SECENG_FAULT_ISOLATION FaultIsolationMask; // Offset: 0x10A8 sz:0x8
  SECENG_FAULT_ISOLATION FaultIsolation; // Offset: 0x10B0 sz:0x8
  u64 IntegrityViolationSignature; // Offset: 0x10B8 sz:0x8
  u64 qwUnkn2;
  u64 Reserved2[0x1E7]; // Offset: 0x10C0 sz:0xF40
} SOCSECENG_BLOCK, * PSOCSECENG_BLOCK; // Size: 0x2000

//
// System On Chip Secure Random Number Generator Block
// Offset: 0x80000200_00026000
//

typedef union _SECRNG_STATUS {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 63; // 0x0 bfo:0x0
    u64 FifoEmpty : 1; // 0x0 bfo:0x63
  } AsBITS;
#else
  struct {
    u64 FifoEmpty : 1; // 0x0 bfo:0x63
    u64 Reserved1 : 63; // 0x0 bfo:0x0
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG; // 0x0 sz:0x8
} SECRNG_STATUS, * PSECRNG_STATUS; // size 8

typedef union _SECRNG_CONFIGURATION {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 44; // 0x0 bfo:0x0
    u64 TestingControl : 10; // 0x0 bfo:0x44
    u64 ChannelEnable : 4; // 0x0 bfo:0x54
    u64 BitStreamEnable : 6; // 0x0 bfo:0x58
  } AsBITS;
#else
  struct {
    u64 BitStreamEnable : 6; // 0x0 bfo:0x58
    u64 ChannelEnable : 4; // 0x0 bfo:0x54
    u64 TestingControl : 10; // 0x0 bfo:0x44
    u64 Reserved1 : 44; // 0x0 bfo:0x0
  } AsBITS;
#endif // __LITTLE_ENDIAN__
  u64 AsULONGLONG; // 0x0 sz:0x8
} SECRNG_CONFIGURATION, * PSECRNG_CONFIGURATION; // size 8

typedef struct _SOCSECRNG_BLOCK {
  SECRNG_STATUS SecRngStatus; // 0x0 sz:0x8
  u64 Fifo; // 0x8 sz:0x8
  SECRNG_CONFIGURATION Configuration; // 0x10 sz:0x8
  u64 TraceLogicArrayControl; // 0x18 sz:0x8
  u64 Reserved[0x3FC]; // 0x20 sz:0x1FE0
} SOCSECRNG_BLOCK, * PSOCSECRNG_BLOCK; // Size: 0x2000

//
// System On Chip CBI Block
// Offset: 0x80000200_00028000
//

// CBI (Cross-Bar Interface? Computer Based Instruction? Computer Based Interlocking?) Block.
typedef union _CBI_CONFIGURATION {
  struct {
#ifdef __LITTLE_ENDIAN__
  u64 Reserved1 : 58;
  u64 FastLoadEnable : 1;
  u64 SnoopDelay : 5;
#else
  u64 SnoopDelay : 5;
  u64 FastLoadEnable : 1;
  u64 Reserved1 : 58;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CBI_CONFIGURATION;

typedef union _CBI_FAULT_ISOLATION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 54;
    u64 BadPacketFromFSB : 1;
    u64 NullCombinedResponseForPMWCommand : 1;
    u64 SharedInterventionReplyFromPMWOnLoadCommand : 1;
    u64 ModifiedReplyFromPMWOnFlushCommand : 1;
    u64 NullSnoopReplyFromPMWReceivedOnValidGPULoad : 1;
    u64 BadLengthField : 1;
    u64 PMWStoreCommandToFiTPMemory : 1;
    u64 BadAlignmentOnLoadsFromFiTPMemory : 1;
    u64 BadAlignmentOnStoresToMainMemory : 1;
    u64 BadMMIOAccess : 1;
#else
    u64 BadMMIOAccess : 1;
    u64 BadAlignmentOnStoresToMainMemory : 1;
    u64 BadAlignmentOnLoadsFromFiTPMemory : 1;
    u64 PMWStoreCommandToFiTPMemory : 1;
    u64 BadLengthField : 1;
    u64 NullSnoopReplyFromPMWReceivedOnValidGPULoad : 1;
    u64 ModifiedReplyFromPMWOnFlushCommand : 1;
    u64 SharedInterventionReplyFromPMWOnLoadCommand : 1;
    u64 NullCombinedResponseForPMWCommand : 1;
    u64 BadPacketFromFSB : 1;
    u64 Reserved1 : 54;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CBI_FAULT_ISOLATION;

typedef union _CBI_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 57;
    u64 StarvationAvoidanceModeEnable : 1;
    u64 DisplayAlterModeEnable : 1;
    u64 SingleThreadModeEnable : 1;
    u64 EvenRetryDelay : 2;
    u64 OddRetryDelay : 2;
#else
    u64 OddRetryDelay : 2;
    u64 EvenRetryDelay : 2;
    u64 SingleThreadModeEnable : 1;
    u64 DisplayAlterModeEnable : 1;
    u64 StarvationAvoidanceModeEnable : 1;
    u64 Reserved1 : 57;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CBI_CONTROL;

typedef struct _SOCCBI_BLOCK {
    CBI_CONFIGURATION CBIConfiguration; // 0
    CBI_CONTROL CBIControl; // 8
    CBI_FAULT_ISOLATION FaultIsolation; // 16
    CBI_FAULT_ISOLATION FaultIsolationAndMask; // 24
    CBI_FAULT_ISOLATION FaultIsolationOrMask; // 32
    CBI_FAULT_ISOLATION FaultIsolationMask; // 40
    CBI_FAULT_ISOLATION FirstErrorCapture; // 48
    CBI_FAULT_ISOLATION FirstErrorCaptureAndMask; // 56
    CBI_FAULT_ISOLATION FirstErrorCaptureOrMask; // 64
    CBI_FAULT_ISOLATION FaultIsolationCheckstop; // 72
    CBI_FAULT_ISOLATION FaultIsolationDebug; // 80
    u64 MPIRetryCounter; // 88
    u64 PAAMCollisionCounter; // 96
    u64 MPITraceSelect; // 104
    u64 TBIUTraceSelect; // 112
    u64 RBIUTraceSelect; // 120
    u64 RIUTraceSelect; // 128
    u64 CBITraceSelect; // 136
    u64 FSBDisplayAlterCommand; // 144
    u64 FSBDispalyAlterAddress; // 152
    u64 FSBDisplayAlterData; // 160
    u64 MPIDisplayAlterCommand; // 168
    u64 Reserved[4074]; // 176
} SOCCBI_BLOCK, * PSOCCBI_BLOCK; // Size: 0x8000

//
// System On Chip Power Management/Bus interface Unit Block
// Offset: 0x80000200_00060000
//

// Core Interface Unit (CIU) MMIO Registers
// Part of the PMW/BIU Block

typedef union _CIU_FAULT_ISOLATION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 53;
    u64 NCUTimeoutICBIQ : 1;
    u64 NCUTimeoutTLBIQ : 1;
    u64 NCUTimeoutStore : 1;
    u64 NCUTimeoutLoad : 1;
    u64 MMULoadStoreHang : 1;
    u64 MMUParityErrorTLB : 1;
    u64 MMUParityErrorSLB : 1;
    u64 PPUDebugCheckstop : 1;
    u64 PPUNonrecoverableError : 1;
    u64 PPUDataCacheParityError : 1;
    u64 PPUInstructionCacheParityError : 1;
#else
    u64 PPUInstructionCacheParityError : 1;
    u64 PPUDataCacheParityError : 1;
    u64 PPUNonrecoverableError : 1;
    u64 PPUDebugCheckstop : 1;
    u64 MMUParityErrorSLB : 1;
    u64 MMUParityErrorTLB : 1;
    u64 MMULoadStoreHang : 1;
    u64 NCUTimeoutLoad : 1;
    u64 NCUTimeoutStore : 1;
    u64 NCUTimeoutTLBIQ : 1;
    u64 NCUTimeoutICBIQ : 1;
    u64 Reserved1 : 53;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CIU_FAULT_ISOLATION;

typedef union _CIU_RECOVERABLE_ERROR_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 62;
    u64 PPUDataCacheParityErrorEnable : 1;
    u64 PPUInstructionCacheParityErrorEnable : 1;
#else
    u64 PPUInstructionCacheParityErrorEnable : 1;
    u64 PPUDataCacheParityErrorEnable : 1;
    u64 Reserved1 : 62;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CIU_RECOVERABLE_ERROR_CONTROL;

typedef union _CIU_MODE_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 60;
    u64 LimitLoadCreditsToFour : 1;
    u64 LimitLoadCreditsToOne : 1;
    u64 CLQAlwaysCorrectMode : 1;
    u64 CLQInstructionHighPriorityMode : 1;
#else
    u64 CLQInstructionHighPriorityMode : 1;
    u64 CLQAlwaysCorrectMode : 1;
    u64 LimitLoadCreditsToOne : 1;
    u64 LimitLoadCreditsToFour : 1;
    u64 Reserved1 : 60;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CIU_MODE_SETUP;

typedef union _CIU_TRACE_ENABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 40;
    u64 SelectHalfwordFor48To63 : 3;
    u64 Reserved2 : 1;
    u64 SelectHalfwordFor32To47 : 3;
    u64 Reserved3 : 1;
    u64 SelectHalfwordFor16To31 : 3;
    u64 Reserved4 : 1;
    u64 SelectHalfwordFor0To15 : 3;
    u64 Reserved5 : 1;
    u64 EnableHalfword48To63 : 1;
    u64 EnableHalfword32To47 : 1;
    u64 EnableHalfword16To31 : 1;
    u64 EnableHalfword0To15 : 1;
    u64 Reserved6 : 2;
    u64 TraceEnable : 1;
    u64 TraceMasterEnable : 1;
#else
    u64 TraceMasterEnable : 1;
    u64 TraceEnable : 1;
    u64 Reserved6 : 2;
    u64 EnableHalfword0To15 : 1;
    u64 EnableHalfword16To31 : 1;
    u64 EnableHalfword32To47 : 1;
    u64 EnableHalfword48To63 : 1;
    u64 Reserved5 : 1;
    u64 SelectHalfwordFor0To15 : 3;
    u64 Reserved4 : 1;
    u64 SelectHalfwordFor16To31 : 3;
    u64 Reserved3 : 1;
    u64 SelectHalfwordFor32To47 : 3;
    u64 Reserved2 : 1;
    u64 SelectHalfwordFor48To63 : 3;
    u64 Reserved1 : 40;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CIU_TRACE_ENABLE;

typedef union _CIU_TRACE_TRIGGER_ENABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 28;
    u64 TriggerCompareMask : 16;
    u64 TriggerCompareValue : 16;
    u64 Reserved2 : 2;
    u64 Enable4GHzTrigger : 2;
#else
    u64 Enable4GHzTrigger : 2;
    u64 Reserved2 : 2;
    u64 TriggerCompareValue : 16;
    u64 TriggerCompareMask : 16;
    u64 Reserved1 : 28;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} CIU_TRACE_TRIGGER_ENABLE;

typedef union _CIU_RECOVERABLE_ERROR_COUNTER {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 ErrorCounter : 6; // 0x0 bfo:0x58
    u64 Reserved1 : 58; // 0x0 bfo:0x0
#else
    u64 Reserved1 : 58; // 0x0 bfo:0x0
    u64 ErrorCounter : 6; // 0x0 bfo:0x58
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} CIU_RECOVERABLE_ERROR_COUNTER, * PCIU_RECOVERABLE_ERROR_COUNTER; // size 8

typedef struct _SOCCIU_BLOCK {
  CIU_FAULT_ISOLATION FaultIsolation; // 0
  CIU_FAULT_ISOLATION FaultIsolationErrorMask; // 8
  CIU_FAULT_ISOLATION FaultIsolationOrMask; // 16
  CIU_FAULT_ISOLATION FaultIsolationErrorMaskOrMask; // 24
  CIU_FAULT_ISOLATION FaultIsolationAndMask; // 32
  CIU_FAULT_ISOLATION FaultIsolationErrorMaskAndMask; // 40
  CIU_FAULT_ISOLATION FaultIsolationCheckstopEnable; // 48
  CIU_RECOVERABLE_ERROR_CONTROL RecoverableErrorControl; // 56 0x38
  CIU_RECOVERABLE_ERROR_COUNTER RecoverableErrorCounter; // 64 0x40
  CIU_MODE_SETUP ModeSetup; // 72
  u64 Reserved1; // 80
  CIU_TRACE_ENABLE TraceEnable; // 88
  CIU_TRACE_TRIGGER_ENABLE TraceTriggerEnable; // 96
  u64 Reserved2[3]; // 104
} SOCCIU_BLOCK; // size 128 or 0x80

// Noncacheable Unit (NCU) MMIO Registers
// Part of the PMW/BIU Block.

typedef union _NCU_PM_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 36;
    u64 UnitBitRotate : 4;
    u64 UnitBitEnable : 16;
    u64 Reserved2 : 6;
    u64 UnitEnable : 1;
    u64 UnitMasterEnable : 1;
#else
    u64 UnitMasterEnable : 1;
    u64 UnitEnable : 1;
    u64 Reserved2 : 6;
    u64 UnitBitEnable : 16;
    u64 UnitBitRotate : 4;
    u64 Reserved1 : 36;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} NCU_PM_SETUP;

typedef union _NCU_MODE_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 56;
    u64 IsyncMapping : 1;
    u64 IsyncBusOperation : 1;
    u64 StoreGatherTimeoutDisable : 1;
    u64 StoreGatherTimeoutCount : 4;
    u64 StoreGatherDisable : 1;
#else
    u64 StoreGatherDisable : 1;
    u64 StoreGatherTimeoutCount : 4;
    u64 StoreGatherTimeoutDisable : 1;
    u64 IsyncBusOperation : 1;
    u64 IsyncMapping : 1;
    u64 Reserved1 : 56;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} NCU_MODE_SETUP;

typedef union _NCU_DEBUG_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 28;
    u64 MUXControl : 8;
    u64 SelectHalfwordFor48To63 : 4;
    u64 SelectHalfwordFor32To47 : 4;
    u64 SelectHalfwordFor16To31 : 4;
    u64 SelectHalfwordFor0To15 : 4;
    u64 FourGHzEnableHalfword48To63 : 1;
    u64 FourGHzEnableHalfword32To47 : 1;
    u64 FourGHzEnableHalfword16To31 : 1;
    u64 FourGHzEnableHalfword0To15 : 1;
    u64 TwoGHzEnableHalfword48To63 : 1;
    u64 TwoGHzEnableHalfword32To47 : 1;
    u64 TwoGHzEnableHalfword16To31 : 1;
    u64 TwoGHzEnableHalfword0To15 : 1;
    u64 TraceEnable : 1;
    u64 Reserved2 : 1;
    u64 FourGHzTraceMasterEnable : 1;
    u64 TwoGHzTraceMasterEnable : 1;
#else
    u64 TwoGHzTraceMasterEnable : 1;
    u64 FourGHzTraceMasterEnable : 1;
    u64 TraceEnable : 1;
    u64 Reserved2 : 1;
    u64 TwoGHzEnableHalfword0To15 : 1;
    u64 TwoGHzEnableHalfword16To31 : 1;
    u64 TwoGHzEnableHalfword32To47 : 1;
    u64 TwoGHzEnableHalfword48To63 : 1;
    u64 FourGHzEnableHalfword0To15 : 1;
    u64 FourGHzEnableHalfword16To31 : 1;
    u64 FourGHzEnableHalfword32To47 : 1;
    u64 FourGHzEnableHalfword48To63 : 1;
    u64 SelectHalfwordFor0To15 : 4;
    u64 SelectHalfwordFor16To31 : 4;
    u64 SelectHalfwordFor32To47 : 4;
    u64 SelectHalfwordFor48To63 : 4;
    u64 MUXControl : 8;
    u64 Reserved1 : 28;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} NCU_DEBUG_SETUP;

typedef union _NCU_TRACE_TRIGGER_ENABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 28;
    u64 TriggerCompareMask : 16;
    u64 TriggerCompareValue : 16;
    u64 EnableFourGHzTrigger : 2;
    u64 EnableTwoGHzTrigger : 2;
#else
    u64 EnableTwoGHzTrigger : 2;
    u64 EnableFourGHzTrigger : 2;
    u64 TriggerCompareValue : 16;
    u64 TriggerCompareMask : 16;
    u64 Reserved1 : 28;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} NCU_TRACE_TRIGGER_ENABLE;

typedef struct _SOCNCU_BLOCK {
  NCU_PM_SETUP PMSetup; // 0
  NCU_MODE_SETUP ModeSetup; // 8
  NCU_DEBUG_SETUP DebugSetup; // 16
  NCU_TRACE_TRIGGER_ENABLE TraceTriggerEnable; // 24
  u64 Reserved1[12]; // 32
} SOCNCU_BLOCK; // size 128 0x80

// Beginning of the PMW/BIU Block structures.

typedef union _L2_RMT_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 RMT7 : 8;
    u64 RMT6 : 8;
    u64 RMT5 : 8;
    u64 RMT4 : 8;
    u64 RMT3 : 8;
    u64 RMT2 : 8;
    u64 RMT1 : 8;
    u64 RMT0 : 8;
#else
    u64 RMT0 : 8;
    u64 RMT1 : 8;
    u64 RMT2 : 8;
    u64 RMT3 : 8;
    u64 RMT4 : 8;
    u64 RMT5 : 8;
    u64 RMT6 : 8;
    u64 RMT7 : 8;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_RMT_SETUP; // size 8

typedef union _L2_PM_SELECT {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 14;
    u64 LowerModeSelect : 25;
    u64 UpperModeSelect : 25;
#else
    u64 UpperModeSelect : 25;
    u64 LowerModeSelect : 25;
    u64 Reserved1 : 14;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_PM_SELECT; // size 8

typedef union _L2_PM_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 47;
    u64 UnitBitEnable : 16;
    u64 UnitMasterEnable : 1;
#else
    u64 UnitMasterEnable : 1;
    u64 UnitBitEnable : 16;
    u64 Reserved1 : 47;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_PM_SETUP; // size 8

typedef union _L2_DEBUG_SELECT {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 GroupEnable : 21;
    u64 Reserved1 : 43;
#else
    u64 Reserved1 : 43;
    u64 GroupEnable : 21;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_DEBUG_SELECT; // size 8

typedef union _L2_DEBUG_TRIGGER_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Quartile3TriggerSelect : 5;
    u64 Quartile2TriggerSelect : 5;
    u64 Quartile1TriggerSelect : 5;
    u64 Quartile0TriggerSelect : 5;
    u64 L2TriggerStopEnable : 1;
    u64 L2TriggerStartEnable : 1;
    u64 FIRTriggerEnable : 1;
    u64 Quartile3TriggerEnable : 1;
    u64 Quartile2TriggerEnable : 1;
    u64 Quartile1TriggerEnable : 1;
    u64 Quartile0TriggerEnable : 1;
    u64 Reserved1 : 37;
#else
    u64 Reserved1 : 37;
    u64 Quartile0TriggerEnable : 1;
    u64 Quartile1TriggerEnable : 1;
    u64 Quartile2TriggerEnable : 1;
    u64 Quartile3TriggerEnable : 1;
    u64 FIRTriggerEnable : 1;
    u64 L2TriggerStartEnable : 1;
    u64 L2TriggerStopEnable : 1;
    u64 Quartile0TriggerSelect : 5;
    u64 Quartile1TriggerSelect : 5;
    u64 Quartile2TriggerSelect : 5;
    u64 Quartile3TriggerSelect : 5;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_DEBUG_TRIGGER_CONTROL; // size 8

typedef union _L2_DEBUG_TRIGGER_MASK {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Quartile3Mask : 16;
    u64 Quartile2Mask : 16;
    u64 Quartile1Mask : 16;
    u64 Quartile0Mask : 16;
#else
    u64 Quartile0Mask : 16;
    u64 Quartile1Mask : 16;
    u64 Quartile2Mask : 16;
    u64 Quartile3Mask : 16;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_DEBUG_TRIGGER_MASK; // size 8

typedef union _L2_DEBUG_TRIGGER_MATCH {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Quartile3Mask : 16;
    u64 Quartile2Mask : 16;
    u64 Quartile1Mask : 16;
    u64 Quartile0Mask : 16;
#else
    u64 Quartile0Mask : 16;
    u64 Quartile1Mask : 16;
    u64 Quartile2Mask : 16;
    u64 Quartile3Mask : 16;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_DEBUG_TRIGGER_MATCH; // size 8

typedef union _CIU_SLICE_MODE_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 53;
    u64 LivelockControlSelect : 2;
    u64 EnableLivelockBreak : 1;
    u64 HangPulseConfig : 2;
    u64 LoadQueueBypassDisable2 : 2;
    u64 LoadQueueBypassDisable1 : 2;
    u64 LoadQueueBypassDisable0 : 2;
#else
    u64 LoadQueueBypassDisable0 : 2;
    u64 LoadQueueBypassDisable1 : 2;
    u64 LoadQueueBypassDisable2 : 2;
    u64 HangPulseConfig : 2;
    u64 EnableLivelockBreak : 1;
    u64 LivelockControlSelect : 2;
    u64 Reserved1 : 53;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} CIU_SLICE_MODE_SETUP; // size 8

typedef union _CIU_SLICE_PM_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 36;
    u64 UnitBitRotate : 4;
    u64 UnitBitEnable : 16;
    u64 Reserved2 : 6;
    u64 UnitEnable : 1;
    u64 UnitMasterEnable : 1;
#else
    u64 UnitMasterEnable : 1;
    u64 UnitEnable : 1;
    u64 Reserved2 : 6;
    u64 UnitBitEnable : 16;
    u64 UnitBitRotate : 4;
    u64 Reserved1 : 36;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} CIU_SLICE_PM_SETUP; // size 8

typedef union _CIU_SLICE_TRACE_ENABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 35;
    u64 MUXControl : 1;
    u64 SelectHalfwordFor48To63 : 4;
    u64 SelectHalfwordFor32To47 : 4;
    u64 SelectHalfwordFor16To31 : 4;
    u64 SelectHalfwordFor0To15 : 4;
    u64 FourGHzEnableHalfword48To63 : 1;
    u64 FourGHzEnableHalfword32To47 : 1;
    u64 FourGHzEnableHalfword16To31 : 1;
    u64 FourGHzEnableHalfword0To15 : 1;
    u64 TwoGHzEnableHalfword48To63 : 1;
    u64 TwoGHzEnableHalfword32To47 : 1;
    u64 TwoGHzEnableHalfword16To31 : 1;
    u64 TwoGHzEnableHalfword0To15 : 1;
    u64 Reserved2 : 1;
    u64 TraceEnable : 1;
    u64 FourGHzTraceMasterEnable : 1;
    u64 TwoGHzTraceMasterEnable : 1;
#else
    u64 TwoGHzTraceMasterEnable : 1;
    u64 FourGHzTraceMasterEnable : 1;
    u64 TraceEnable : 1;
    u64 Reserved2 : 1;
    u64 TwoGHzEnableHalfword0To15 : 1;
    u64 TwoGHzEnableHalfword16To31 : 1;
    u64 TwoGHzEnableHalfword32To47 : 1;
    u64 TwoGHzEnableHalfword48To63 : 1;
    u64 FourGHzEnableHalfword0To15 : 1;
    u64 FourGHzEnableHalfword16To31 : 1;
    u64 FourGHzEnableHalfword32To47 : 1;
    u64 FourGHzEnableHalfword48To63 : 1;
    u64 SelectHalfwordFor0To15 : 4;
    u64 SelectHalfwordFor16To31 : 4;
    u64 SelectHalfwordFor32To47 : 4;
    u64 SelectHalfwordFor48To63 : 4;
    u64 MUXControl : 1;
    u64 Reserved1 : 35;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} CIU_SLICE_TRACE_ENABLE; // size 8

typedef union _CIU_SLICE_TRACE_TRIGGER_ENABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 28;
    u64 TriggerCompareMask : 16;
    u64 TriggerCompareValue : 16;
    u64 EnableFourGHzTrigger : 2;
    u64 EnableTwoGHzTrigger : 2;
#else
    u64 EnableTwoGHzTrigger : 2;
    u64 EnableFourGHzTrigger : 2;
    u64 TriggerCompareValue : 16;
    u64 TriggerCompareMask : 16;
    u64 Reserved1 : 28;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} CIU_SLICE_TRACE_TRIGGER_ENABLE; // size 8

typedef union _L2_FAULT_ISOLATION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 A1Q3CorrectableErrorThreshhold : 1;
    u64 A0Q3CorrectableErrorThreshhold : 1;
    u64 A1Q2CorrectableErrorThreshhold : 1;
    u64 A0Q2CorrectableErrorThreshhold : 1;
    u64 A1Q1CorrectableErrorThreshhold : 1;
    u64 A0Q1CorrectableErrorThreshhold : 1;
    u64 A1Q0CorrectableErrorThreshhold : 1;
    u64 A0Q0CorrectableErrorThreshhold : 1;
    u64 MultipleDirectoryParityErrors : 1;
    u64 MultipleCorrectableErrors : 1;
    u64 SnoopPAAMError : 1;
    u64 ControlErrorStoreQueue : 1;
    u64 RCUnexpectedMERSI : 1;
    u64 RCUnexpectedCRESP : 1;
    u64 RCUnexpectedData : 1;
    u64 RCOrNCCTLDataHang : 1;
    u64 StoreQueue2DataParityError : 1;
    u64 StoreQueue1DataParityError : 1;
    u64 StoreQueue0DataParityError : 1;
    u64 NCCTLHangDetect : 1;
    u64 FSMHangDetect : 1;
    u64 DirectoryCheckstop : 1;
    u64 DirectoryParityError : 1;
    u64 SpecialUncorrectableErrorNonCacheableSide : 1;
    u64 SpecialUncorrectableErrorCacheableSide : 1;
    u64 UncorrectableError : 1;
    u64 CorrectableError : 1;
    u64 Reserved1 : 37;
#else
    u64 Reserved1 : 37;
    u64 CorrectableError : 1;
    u64 UncorrectableError : 1;
    u64 SpecialUncorrectableErrorCacheableSide : 1;
    u64 SpecialUncorrectableErrorNonCacheableSide : 1;
    u64 DirectoryParityError : 1;
    u64 DirectoryCheckstop : 1;
    u64 FSMHangDetect : 1;
    u64 NCCTLHangDetect : 1;
    u64 StoreQueue0DataParityError : 1;
    u64 StoreQueue1DataParityError : 1;
    u64 StoreQueue2DataParityError : 1;
    u64 RCOrNCCTLDataHang : 1;
    u64 RCUnexpectedData : 1;
    u64 RCUnexpectedCRESP : 1;
    u64 RCUnexpectedMERSI : 1;
    u64 ControlErrorStoreQueue : 1;
    u64 SnoopPAAMError : 1;
    u64 MultipleCorrectableErrors : 1;
    u64 MultipleDirectoryParityErrors : 1;
    u64 A0Q0CorrectableErrorThreshhold : 1;
    u64 A1Q0CorrectableErrorThreshhold : 1;
    u64 A0Q1CorrectableErrorThreshhold : 1;
    u64 A1Q1CorrectableErrorThreshhold : 1;
    u64 A0Q2CorrectableErrorThreshhold : 1;
    u64 A1Q2CorrectableErrorThreshhold : 1;
    u64 A0Q3CorrectableErrorThreshhold : 1;
    u64 A1Q3CorrectableErrorThreshhold : 1;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_FAULT_ISOLATION; // size 8

typedef union _L2_ERROR_INJECTION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 ErrorInjectionType : 2;
    u64 InjectCacheError : 2;
    u64 InjectDirectoryError : 2;
    u64 Reserved1 : 58;
#else
    u64 Reserved1 : 58;
    u64 InjectDirectoryError : 2;
    u64 InjectCacheError : 2;
    u64 ErrorInjectionType : 2;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_ERROR_INJECTION; // size 8

typedef union _L2_MODE_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 RCsAvailableForLoads : 8;
    u64 RCsAvailableForStores : 8;
    u64 StoreQueue2GatherWaitCount : 4;
    u64 StoreQueue1GatherWaitCount : 4;
    u64 StoreQueue0GatherWaitCount : 4;
    u64 DirectMapEnable : 1;
    u64 LRURMTFunctionDisable : 1;
    u64 ECCErrorCountRevertToPreset : 1;
    u64 FSMHangPulseDividerCounter : 4;
    u64 ConvertTouchAroundL2ToDSideDemandLoad : 1;
    u64 LPWaitCount : 6;
    u64 RcDispatchThrottleSelect : 2;
    u64 RcDispatchThrottleControl : 2;
    u64 DisableCacheRequestorArbitrationBlocking : 1;
    u64 Reserved : 17;
#else
    u64 Reserved : 17;
    u64 DisableCacheRequestorArbitrationBlocking : 1;
    u64 RcDispatchThrottleControl : 2;
    u64 RcDispatchThrottleSelect : 2;
    u64 LPWaitCount : 6;
    u64 ConvertTouchAroundL2ToDSideDemandLoad : 1;
    u64 FSMHangPulseDividerCounter : 4;
    u64 ECCErrorCountRevertToPreset : 1;
    u64 LRURMTFunctionDisable : 1;
    u64 DirectMapEnable : 1;
    u64 StoreQueue0GatherWaitCount : 4;
    u64 StoreQueue1GatherWaitCount : 4;
    u64 StoreQueue2GatherWaitCount : 4;
    u64 RCsAvailableForStores : 8;
    u64 RCsAvailableForLoads : 8;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_MODE_SETUP; // size 8

typedef union _L2_MODE_SETUP_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 7;
    u64 RCsAvailableForLoadsUseMMIO : 1;
    u64 Reserved2 : 7;
    u64 RCsAvailableForStoresUseMMIO : 1;
    u64 Reserved3 : 3;
    u64 StoreQueue2GatherWaitCountUseMMIO : 1;
    u64 Reserved4 : 3;
    u64 StoreQueue1GatherWaitCountUseMMIO : 1;
    u64 Reserved5 : 3;
    u64 StoreQueue0GatherWaitCountUseMMIO : 1;
    u64 DirectMapEnableUseMMIO : 1;
    u64 LRURMTFunctionDisableUseMMIO : 1;
    u64 Reserved6 : 11;
    u64 LPWaitCountUseMMIO : 1;
    u64 Reserved7 : 22;
#else
    u64 Reserved7 : 22;
    u64 LPWaitCountUseMMIO : 1;
    u64 Reserved6 : 11;
    u64 LRURMTFunctionDisableUseMMIO : 1;
    u64 DirectMapEnableUseMMIO : 1;
    u64 StoreQueue0GatherWaitCountUseMMIO : 1;
    u64 Reserved5 : 3;
    u64 StoreQueue1GatherWaitCountUseMMIO : 1;
    u64 Reserved4 : 3;
    u64 StoreQueue2GatherWaitCountUseMMIO : 1;
    u64 Reserved3 : 3;
    u64 RCsAvailableForStoresUseMMIO : 1;
    u64 Reserved2 : 7;
    u64 RCsAvailableForLoadsUseMMIO : 1;
    u64 Reserved1 : 7;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_MODE_SETUP_CONTROL; // size 8

typedef union _L2_MACHINE_CHECK {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 MachineCheckEnable : 1;
    u64 Reserved1 : 63;
#else
    u64 Reserved1 : 63;
    u64 MachineCheckEnable : 1;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_MACHINE_CHECK; // size 8

typedef union _L2_ECC_ERROR_COUNT {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 A1Q3ECCErrorCount : 6;
    u64 Reserved1 : 2;
    u64 A0Q3ECCErrorCount : 6;
    u64 Reserved2 : 2;
    u64 A1Q2ECCErrorCount : 6;
    u64 Reserved3 : 2;
    u64 A0Q2ECCErrorCount : 6;
    u64 Reserved4 : 2;
    u64 A1Q1ECCErrorCount : 6;
    u64 Reserved5 : 2;
    u64 A0Q1ECCErrorCount : 6;
    u64 Reserved6 : 2;
    u64 A1Q0ECCErrorCount : 6;
    u64 Reserved7 : 2;
    u64 A0Q0ECCErrorCount : 6;
    u64 Reserved8 : 2;
#else
    u64 Reserved8 : 2;
    u64 A0Q0ECCErrorCount : 6;
    u64 Reserved7 : 2;
    u64 A1Q0ECCErrorCount : 6;
    u64 Reserved6 : 2;
    u64 A0Q1ECCErrorCount : 6;
    u64 Reserved5 : 2;
    u64 A1Q1ECCErrorCount : 6;
    u64 Reserved4 : 2;
    u64 A0Q2ECCErrorCount : 6;
    u64 Reserved3 : 2;
    u64 A1Q2ECCErrorCount : 6;
    u64 Reserved2 : 2;
    u64 A0Q3ECCErrorCount : 6;
    u64 Reserved1 : 2;
    u64 A1Q3ECCErrorCount : 6;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} L2_ECC_ERROR_COUNT; // size 8

typedef union _BIU_FAULT_ISOLATION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 ReceivedIllegalTSizeOnMMIOAccess : 1;
    u64 MMIOErrorRegisterIsSet : 1;
    u64 SentIllegalTSizeOnACommand : 1;
    u64 SentIllegalWIMGOnACommand : 1;
    u64 SentIllegalTTypeOnACommand : 1;
    u64 NoAckReceivedOnCombinedResponse : 1;
    u64 SentReflectedCommandToL2BackToBack : 1;
    u64 InterventionOnCombinedResponseWithoutModifiedOrShared : 1;
    u64 BusGrantedMoreCreditsThanBIUCanQueue : 1;
    u64 Reserved1 : 55;
#else
    u64 Reserved1 : 55;
    u64 BusGrantedMoreCreditsThanBIUCanQueue : 1;
    u64 InterventionOnCombinedResponseWithoutModifiedOrShared : 1;
    u64 SentReflectedCommandToL2BackToBack : 1;
    u64 NoAckReceivedOnCombinedResponse : 1;
    u64 SentIllegalTTypeOnACommand : 1;
    u64 SentIllegalWIMGOnACommand : 1;
    u64 SentIllegalTSizeOnACommand : 1;
    u64 MMIOErrorRegisterIsSet : 1;
    u64 ReceivedIllegalTSizeOnMMIOAccess : 1;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} BIU_FAULT_ISOLATION; // size 8

typedef union _BIU_PM_SETUP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 36;
    u64 UnitBitRotate : 4;
    u64 UnitBitEnable : 16;
    u64 AlternateEventsEnable : 1;
    u64 Reserved2 : 6;
    u64 UnitMasterEnable : 1;
#else
    u64 UnitMasterEnable : 1;
    u64 Reserved2 : 6;
    u64 AlternateEventsEnable : 1;
    u64 UnitBitEnable : 16;
    u64 UnitBitRotate : 4;
    u64 Reserved1 : 36;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} BIU_PM_SETUP; // size 8

typedef union _BIU_DEBUG_1 {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 5;
    u64 b_reg : 1;
    u64 b_rcv_snp : 5;
    u64 b_cmd_l2 : 1;
    u64 b_snp_reply : 1;
    u64 b_mmio : 1;
    u64 b_wr_darb : 1;
    u64 b_wr_cntl : 2;
    u64 b_cmd : 2;
    u64 b_ad_mch : 1;
    u64 b_arb : 12;
    u64 Reserved2 : 16;
    u64 DataSelectForTrace_32_63 : 4;
    u64 DataSelectForTrace_0_31 : 4;
    u64 TriggerSelect : 1;
    u64 Reserved3 : 6;
    u64 DebugBusEnable : 1;
#else
    u64 DebugBusEnable : 1;
    u64 Reserved3 : 6;
    u64 TriggerSelect : 1;
    u64 DataSelectForTrace_0_31 : 4;
    u64 DataSelectForTrace_32_63 : 4;
    u64 Reserved2 : 16;
    u64 b_arb : 12;
    u64 b_ad_mch : 1;
    u64 b_cmd : 2;
    u64 b_wr_cntl : 2;
    u64 b_wr_darb : 1;
    u64 b_mmio : 1;
    u64 b_snp_reply : 1;
    u64 b_cmd_l2 : 1;
    u64 b_rcv_snp : 5;
    u64 b_reg : 1;
    u64 Reserved1 : 5;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} BIU_DEBUG_1; // size 8

typedef union _BIU_DEBUG_2 {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 TriggerCompareMask : 16;
    u64 TriggerCompareData : 16;
    u64 Reserved1 : 26;
    u64 TriggerSelectBits : 6;
#else
    u64 TriggerSelectBits : 6;
    u64 Reserved1 : 26;
    u64 TriggerCompareData : 16;
    u64 TriggerCompareMask : 16;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} BIU_DEBUG_2; // size 8

typedef struct _SOCPMW_BLOCK {
  u64 Reserved1[96]; // 0x0 sz:0x300
  L2_RMT_SETUP L2RMTSetup; // 0x300 sz:0x8
  u64 Reserved2; // 0x308 sz:0x8
  L2_PM_SELECT L2PMSelect; // 0x310 sz:0x8
  L2_PM_SETUP L2PMSetup; // 0x318 sz:0x8
  L2_DEBUG_SELECT L2DebugEventSelect; // 0x320 sz:0x8
  L2_DEBUG_TRIGGER_CONTROL L2DebugTriggerControl; // 0x328 sz:0x8
  L2_DEBUG_TRIGGER_MASK L2DebugTriggerMask; // 0x330 sz:0x8
  L2_DEBUG_TRIGGER_MATCH L2DebugTriggerMatch; // 0x338 sz:0x8
  u64 Reserved3[24]; // 0x340 sz:0xC0
  SOCCIU_BLOCK CIU0; // 0x400 sz:0x80
  SOCNCU_BLOCK NCU0; // 0x480 sz:0x80
  SOCCIU_BLOCK CIU1; // 0x500 sz:0x80
  SOCNCU_BLOCK NCU1; // 0x580 sz:0x80
  SOCCIU_BLOCK CIU2; // 0x600 sz:0x80
  SOCNCU_BLOCK NCU2; // 0x680 sz:0x80
  CIU_SLICE_MODE_SETUP CIUSliceModeSetup; // 0x700 sz:0x8
  CIU_SLICE_PM_SETUP CIUSlicePMSetup; // 0x708 sz:0x8
  CIU_SLICE_TRACE_ENABLE CIUSliceTraceEnableForLoads; // 0x710 sz:0x8
  CIU_SLICE_TRACE_ENABLE CIUSliceTraceEnableForStores; // 0x718 sz:0x8
  CIU_SLICE_TRACE_TRIGGER_ENABLE CIUSliceTraceTriggerEnableForLoads; // 0x720 sz:0x8
  CIU_SLICE_TRACE_TRIGGER_ENABLE CIUSliceTraceTriggerEnableForStores; // 0x728 sz:0x8
  u64 Reserved4[26]; // 0x730 sz:0xD0
  L2_FAULT_ISOLATION L2FaultIsolation; // 0x800 sz:0x8
  L2_FAULT_ISOLATION L2FaultIsolationErrorMask; // 0x808 sz:0x8
  L2_FAULT_ISOLATION L2FaultIsolationOrMask; // 0x810 sz:0x8
  L2_FAULT_ISOLATION L2FaultIsolationErrorMaskOrMask; // 0x818 sz:0x8
  L2_FAULT_ISOLATION L2FaultIsolationAndMask; // 0x820 sz:0x8
  L2_FAULT_ISOLATION L2FaultIsolationErrorMaskAndMask; // 0x828 sz:0x8
  L2_FAULT_ISOLATION L2FaultIsolationCheckstopEnable; // 0x830 sz:0x8
  L2_ERROR_INJECTION L2ErrorInjection; // 0x838 sz:0x8
  L2_MODE_SETUP L2ModeSetup; // 0x840 sz:0x8
  L2_MODE_SETUP_CONTROL L2ModeSetupControl; // 0x848 sz:0x8
  u64 Reserved5; // 0x850 sz:0x8
  L2_MACHINE_CHECK L2MachineCheck; // 0x858 sz:0x8
  L2_ECC_ERROR_COUNT L2ECCErrorCount; // 0x860 sz:0x8
  L2_ECC_ERROR_COUNT L2ECCErrorCountPreset; // 0x868 sz:0x8
  u64 Reserved6[82]; // 0x870 sz:0x290
  BIU_FAULT_ISOLATION BIUFaultIsolation; // 0xB00 sz:0x8
  BIU_FAULT_ISOLATION BIUFaultIsolationErrorMask; // 0xB08 sz:0x8
  BIU_FAULT_ISOLATION BIUFaultIsolationOrMask; // 0xB10 sz:0x8
  BIU_FAULT_ISOLATION BIUFaultIsolationErrorMaskOrMask; // 0xB18 sz:0x8
  BIU_FAULT_ISOLATION BIUFaultIsolationAndMask; // 0xB20 sz:0x8
  BIU_FAULT_ISOLATION BIUFaultIsolationErrorMaskAndMask; // 0xB28 sz:0x8
  BIU_FAULT_ISOLATION BIUFaultIsolationCheckstop; // 0xB30 sz:0x8
  u64 Reserved7[3]; // 0xB38 sz:0x18
  BIU_PM_SETUP BIUPMSetup; // 0xB50 sz:0x8
  BIU_DEBUG_1 BIUDebug1; // 0xB58 sz:0x8
  BIU_DEBUG_2 BIUDebug2; // 0xB60 sz:0x8
  u64 BIUDebug3; // 0xB68 sz:0x8
  u64 BIUDebug4; // 0xB70 sz:0x8
  u64 Reserved8[17]; // 0xB78 sz:0x88
  u64 FullSpeedTraceArray; // 0xC00 sz:0x8
  u64 Reserved9[7]; // 0xC08 sz:0x38
  u64 HalfSpeedTraceArray; // 0xC40 sz:0x8
  u64 Reserved10[119]; // 0xC48 sz:0x3B8
} SOCPMW_BLOCK, * PSOCPMW_BLOCK; // size 4096

//
// System On Chip Pervasive Logic Block
// Offset: 0x80000200_00061000
//

typedef union _PRV_POST_INOUT {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 56;
    u64 Value : 8;
#else
    u64 Value : 8;
    u64 Reserved1 : 56;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_POST_INOUT;

typedef union _PRV_POR_STATUS {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 38;
    u64 ConfigRing2Active : 1;
    u64 ConfigRing1Active : 1;
    u64 LastSecurityTask : 7;
    u64 Reserved2 : 6;
    u64 Executing : 1;
    u64 Phase2Active : 1;
    u64 Checkstop : 1;
    u64 InWaitStatePhase2 : 1;
    u64 AtWaitInstruction : 1;
    u64 TimeOutError : 1;
    u64 ExternalConfigFuseBlown : 1;
    u64 UnlockMode : 1;
    u64 SecureMode : 1;
    u64 NotSecureMode : 1;
    u64 Reserved3 : 1;
#else
    u64 Reserved3 : 1;
    u64 NotSecureMode : 1;
    u64 SecureMode : 1;
    u64 UnlockMode : 1;
    u64 ExternalConfigFuseBlown : 1;
    u64 TimeOutError : 1;
    u64 AtWaitInstruction : 1;
    u64 InWaitStatePhase2 : 1;
    u64 Checkstop : 1;
    u64 Phase2Active : 1;
    u64 Executing : 1;
    u64 Reserved2 : 6;
    u64 LastSecurityTask : 7;
    u64 ConfigRing1Active : 1;
    u64 ConfigRing2Active : 1;
    u64 Reserved1 : 38;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_POR_STATUS;

typedef union _PRV_POWER_MANAGEMENT_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 PLLRatio : 3;
    u64 PLLSet : 1;
    u64 Core2PauseDisable : 1;
    u64 Core1PauseDisable : 1;
    u64 Core0PauseDisable : 1;
    u64 Reserved1 : 1;
    u64 VIDValue : 6;
    u64 VIDSet : 1;
    u64 PowerManagementPauseDisable : 1;
    u64 PLLDelay : 8;
    u64 VIDDelay : 8;
    u64 PowerManagementInterruptFlag : 1;
    u64 Core2Paused : 1;
    u64 Core1Paused : 1;
    u64 Core0Paused : 1;
    u64 JTAGOverride : 1;
    u64 Reserved2 : 3;
    u64 VIDFullPower : 3;
    u64 Reserved3 : 5;
    u64 VIDLowPower : 6;
    u64 Reserved4 : 2;
    u64 VIDPowerUp : 6;
    u64 Reserved5 : 2;
#else
    u64 Reserved5 : 2;
    u64 VIDPowerUp : 6;
    u64 Reserved4 : 2;
    u64 VIDLowPower : 6;
    u64 Reserved3 : 5;
    u64 VIDFullPower : 3;
    u64 Reserved2 : 3;
    u64 JTAGOverride : 1;
    u64 Core0Paused : 1;
    u64 Core1Paused : 1;
    u64 Core2Paused : 1;
    u64 PowerManagementInterruptFlag : 1;
    u64 VIDDelay : 8;
    u64 PLLDelay : 8;
    u64 PowerManagementPauseDisable : 1;
    u64 VIDSet : 1;
    u64 VIDValue : 6;
    u64 Reserved1 : 1;
    u64 Core0PauseDisable : 1;
    u64 Core1PauseDisable : 1;
    u64 Core2PauseDisable : 1;
    u64 PLLSet : 1;
    u64 PLLRatio : 3;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_POWER_MANAGEMENT_CONTROL;

typedef union _PRV_SPI_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 8;
    u64 Address : 8;
    u64 Command : 8;
    u64 WriteData : 32;
    u64 ValidBytes : 3;
    u64 EnableAction : 1;
    u64 ExtensionMode : 1;
    u64 ClockRateControl : 3;
#else
    u64 ClockRateControl : 3;
    u64 ExtensionMode : 1;
    u64 EnableAction : 1;
    u64 ValidBytes : 3;
    u64 WriteData : 32;
    u64 Command : 8;
    u64 Address : 8;
    u64 Reserved1 : 8;
#endif // __LITTLE_ENDIAN__
  } control;
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 ReadData : 32;
    u64 Reserved2 : 32;
#else
#endif // __LITTLE_ENDIAN__
  } read;
  u64 AsULONGLONG;
} PRV_SPI_CONTROL;

typedef union _PRV_TIMEBASE_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 TimebaseDivider : 8;
    u64 TimebaseEnable : 1;
    u64 Reserved1 : 55;
#else
    u64 Reserved1 : 55;
    u64 TimebaseEnable : 1;
    u64 TimebaseDivider : 8;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_TIMEBASE_CONTROL;

typedef union _PRV_THERMAL_DIODE_CALIBRATION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 PSROCalibration : 11;
    u64 Reserved1 : 5;
    u64 ThermalDiodeElevated : 12;
    u64 Reserved2 : 4;
    u64 ThermalDiodeLow : 12;
    u64 Reserved3 : 4;
    u64 ElevatedTemperature : 7;
    u64 Reserved4 : 1;
    u64 LowTemperature : 7;
    u64 Reserved5 : 1;
#else
    u64 Reserved5 : 1;
    u64 LowTemperature : 7;
    u64 Reserved4 : 1;
    u64 ElevatedTemperature : 7;
    u64 Reserved3 : 4;
    u64 ThermalDiodeLow : 12;
    u64 Reserved2 : 4;
    u64 ThermalDiodeElevated : 12;
    u64 Reserved1 : 5;
    u64 PSROCalibration : 11;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_THERMAL_DIODE_CALIBRATION;

typedef union _PRV_PSRO_COUNT {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 MaxCount : 11;
    u64 Reserved2 : 4;
    u64 Overflow : 1;
    u64 LatestCount : 11;
    u64 Reserved3 : 4;
    u64 Enable : 1;
#else
    u64 Enable : 1;
    u64 Reserved3 : 4;
    u64 LatestCount : 11;
    u64 Overflow : 1;
    u64 Reserved2 : 4;
    u64 MaxCount : 11;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_PSRO_COUNT;

typedef union _PRV_LOCAL_ERROR_COUNTER_STATUS {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 PB2 : 1;
    u64 PB1 : 1;
    u64 PB0 : 1;
    u64 Reserved2 : 29;
#else
    u64 Reserved2 : 29;
    u64 PB0 : 1;
    u64 PB1 : 1;
    u64 PB2 : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_LOCAL_ERROR_COUNTER_STATUS;

typedef union _PRV_FAULT_ISOLATION_RECOVERABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 AnyLocalRecoverableErrorCounter : 1;
    u64 INTS : 1;
    u64 FSB : 1;
    u64 CBI : 1;
    u64 BIU : 1;
    u64 L2 : 1;
    u64 PB2MMUNCU : 1;
    u64 PB1MMUNCU : 1;
    u64 PB0MMUNCU : 1;
    u64 Reserved2 : 23;
#else
    u64 Reserved2 : 23;
    u64 PB0MMUNCU : 1;
    u64 PB1MMUNCU : 1;
    u64 PB2MMUNCU : 1;
    u64 L2 : 1;
    u64 BIU : 1;
    u64 CBI : 1;
    u64 FSB : 1;
    u64 INTS : 1;
    u64 AnyLocalRecoverableErrorCounter : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_FAULT_ISOLATION_RECOVERABLE;

typedef union _PRV_FAULT_ISOLATION_MODE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 LocalRecoverableErrorCounterCheckstopEnable : 1;
    u64 QuiescedCheckstopEnable : 1;
    u64 HoldRecoverableFaultIsolation : 1;
    u64 MaskMachineCheckInterrupt : 1;
    u64 MaskRecoverableErrorInterrupt : 1;
    u64 GlobalFaultIsolationDebugMode : 1;
    u64 Reserved2 : 26;
#else
    u64 Reserved2 : 26;
    u64 GlobalFaultIsolationDebugMode : 1;
    u64 MaskRecoverableErrorInterrupt : 1;
    u64 MaskMachineCheckInterrupt : 1;
    u64 HoldRecoverableFaultIsolation : 1;
    u64 QuiescedCheckstopEnable : 1;
    u64 LocalRecoverableErrorCounterCheckstopEnable : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_FAULT_ISOLATION_MODE;

typedef union _PRV_FAULT_ISOLATION_MACHINE_CHECK {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 L2 : 1;
    u64 Quiesced : 1;
    u64 Reserved2 : 30;
#else
    u64 Reserved2 : 30;
    u64 Quiesced : 1;
    u64 L2 : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_FAULT_ISOLATION_MACHINE_CHECK;

typedef union _PRV_FAULT_ISOLATION_ENABLE {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 MachineCheckEnable : 1;
    u64 QuiescedEnable : 1;
    u64 AnyLocalRecoverableErrorCounterRecoverable : 1;
    u64 INTSRecoverable : 1;
    u64 FSBRecoverable : 1;
    u64 CBIRecoverable : 1;
    u64 BIURecoverable : 1;
    u64 L2Recoverable : 1;
    u64 PB2MMUNCURecoverable : 1;
    u64 PB1MMUNCURecoverable : 1;
    u64 PB0MMUNCURecoverable : 1;
    u64 QuiescedCheckstop : 1;
    u64 AnyLocalRecoverableErrorCounterCheckstop : 1;
    u64 TLACheckstop : 1;
    u64 Reserved2 : 1;
    u64 PORCheckstop : 1;
    u64 SECCheckstop : 1;
    u64 FSBCheckstop : 1;
    u64 CBICheckstop : 1;
    u64 BIUCheckstop : 1;
    u64 L2Checkstop : 1;
    u64 PB2Checkstop : 1;
    u64 PB1Checkstop : 1;
    u64 PB0Checkstop : 1;
    u64 PB2NCUCheckstop : 1;
    u64 PB1NCUCheckstop : 1;
    u64 PB0NCUCheckstop : 1;
    u64 Reserved3 : 5;
#else
    u64 Reserved3 : 5;
    u64 PB0NCUCheckstop : 1;
    u64 PB1NCUCheckstop : 1;
    u64 PB2NCUCheckstop : 1;
    u64 PB0Checkstop : 1;
    u64 PB1Checkstop : 1;
    u64 PB2Checkstop : 1;
    u64 L2Checkstop : 1;
    u64 BIUCheckstop : 1;
    u64 CBICheckstop : 1;
    u64 FSBCheckstop : 1;
    u64 SECCheckstop : 1;
    u64 PORCheckstop : 1;
    u64 Reserved2 : 1;
    u64 TLACheckstop : 1;
    u64 AnyLocalRecoverableErrorCounterCheckstop : 1;
    u64 QuiescedCheckstop : 1;
    u64 PB0MMUNCURecoverable : 1;
    u64 PB1MMUNCURecoverable : 1;
    u64 PB2MMUNCURecoverable : 1;
    u64 L2Recoverable : 1;
    u64 BIURecoverable : 1;
    u64 CBIRecoverable : 1;
    u64 FSBRecoverable : 1;
    u64 INTSRecoverable : 1;
    u64 AnyLocalRecoverableErrorCounterRecoverable : 1;
    u64 QuiescedEnable : 1;
    u64 MachineCheckEnable : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_FAULT_ISOLATION_ENABLE;

typedef union _PRV_FAULT_ISOLATION_CHECKSTOP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 Quiesced : 1;
    u64 AnyLocalRecoverableErrorCounter : 1;
    u64 TLA : 1;
    u64 Reserved2 : 1;
    u64 POR : 1;
    u64 SEC : 1;
    u64 FSB : 1;
    u64 CBI : 1;
    u64 BIU : 1;
    u64 L2 : 1;
    u64 PB2 : 1;
    u64 PB1 : 1;
    u64 PB0 : 1;
    u64 PB2NCU : 1;
    u64 PB1NCU : 1;
    u64 PB0NCU : 1;
    u64 Reserved3 : 16;
#else
    u64 Reserved3 : 16;
    u64 PB0NCU : 1;
    u64 PB1NCU : 1;
    u64 PB2NCU : 1;
    u64 PB0 : 1;
    u64 PB1 : 1;
    u64 PB2 : 1;
    u64 L2 : 1;
    u64 BIU : 1;
    u64 CBI : 1;
    u64 FSB : 1;
    u64 SEC : 1;
    u64 POR : 1;
    u64 Reserved2 : 1;
    u64 TLA : 1;
    u64 AnyLocalRecoverableErrorCounter : 1;
    u64 Quiesced : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_FAULT_ISOLATION_CHECKSTOP;

typedef union _PRV_ERROR_INJECT_SELECT {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 32;
    u64 Retry : 1;
    u64 Checkstop : 1;
    u64 InjectError : 1;
    u64 INTS : 1;
    u64 POR : 1;
    u64 SEC : 1;
    u64 FSB : 1;
    u64 CBI : 1;
    u64 BIU : 1;
    u64 L2 : 1;
    u64 PB2 : 1;
    u64 PB1 : 1;
    u64 PB0 : 1;
    u64 PB2NCU : 1;
    u64 PB1NCU : 1;
    u64 PB0NCU : 1;
    u64 Reserved2 : 16;
#else
    u64 Reserved2 : 16;
    u64 PB0NCU : 1;
    u64 PB1NCU : 1;
    u64 PB2NCU : 1;
    u64 PB0 : 1;
    u64 PB1 : 1;
    u64 PB2 : 1;
    u64 L2 : 1;
    u64 BIU : 1;
    u64 CBI : 1;
    u64 FSB : 1;
    u64 SEC : 1;
    u64 POR : 1;
    u64 INTS : 1;
    u64 InjectError : 1;
    u64 Checkstop : 1;
    u64 Retry : 1;
    u64 Reserved1 : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PRV_ERROR_INJECT_SELECT;

typedef union _PFM_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 49;
    u64 CountQualifiers : 2;
    u64 TraceDestination : 1;
    u64 TraceMode : 2;
    u64 CountModePB2 : 2;
    u64 CountModePB1 : 2;
    u64 CountModePB0 : 2;
    u64 Freeze : 2;
    u64 StopAtMax : 1;
    u64 Enable : 1;
#else
    u64 Enable : 1;
    u64 StopAtMax : 1;
    u64 Freeze : 2;
    u64 CountModePB0 : 2;
    u64 CountModePB1 : 2;
    u64 CountModePB2 : 2;
    u64 TraceMode : 2;
    u64 TraceDestination : 1;
    u64 CountQualifiers : 2;
    u64 Reserved1 : 49;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PFM_CONTROL;

typedef union _PFM_TRIGGER_START_STOP {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 34;
    u64 Bank2And3StopControl : 5;
    u64 Bank1StopControl : 5;
    u64 Bank0StopControl : 5;
    u64 Bank2And3StartControl : 5;
    u64 Bank1StartControl : 5;
    u64 Bank0StartControl : 5;
#else
    u64 Bank0StartControl : 5;
    u64 Bank1StartControl : 5;
    u64 Bank2And3StartControl : 5;
    u64 Bank0StopControl : 5;
    u64 Bank1StopControl : 5;
    u64 Bank2And3StopControl : 5;
    u64 Reserved1 : 34;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PFM_TRIGGER_START_STOP;

typedef union _PFM_COUNTER {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Value : 32;
    u64 Reserved1 : 32;
#else
    u64 Reserved1 : 32;
    u64 Value : 32;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PFM_COUNTER;

typedef union _PFM_COUNTER_CONTROL {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 56;
    u64 CountCycles : 1;
    u64 Polarity : 1;
    u64 CountEnable : 1;
    u64 InputSelect : 5;
#else
    u64 InputSelect : 5;
    u64 CountEnable : 1;
    u64 Polarity : 1;
    u64 CountCycles : 1;
    u64 Reserved1 : 56;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PFM_COUNTER_CONTROL;

typedef union _PFM_INPUT_SELECTION {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 Reserved1 : 40;
    u64 InputSelection : 24;
#else
    u64 InputSelection : 24;
    u64 Reserved1 : 40;
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG;
} PFM_INPUT_SELECTION;

typedef union _FPM_STATUS {
  struct {
#ifdef __LITTLE_ENDIAN__
    u64 StatusAndInterruptEnableCounters : 16; // 0x0 bfo:0x48
    u64 StatusAndInterruptEnableIntervalTimer : 1; // 0x0 bfo:0x47
    u64 Reserved1 : 47; // 0x0 bfo:0x0
#else
    u64 Reserved1 : 47; // 0x0 bfo:0x0
    u64 StatusAndInterruptEnableIntervalTimer : 1; // 0x0 bfo:0x47
    u64 StatusAndInterruptEnableCounters : 16; // 0x0 bfo:0x48
#endif // __LITTLE_ENDIAN__
  } AsBITS;
  u64 AsULONGLONG; // 0x0 sz:0x8
} FPM_STATUS, * PFPM_STATUS; // size 8

typedef struct _SOCPRV_BLOCK {
  PRV_POR_STATUS PowerOnResetStatus; // 0
  PRV_POST_INOUT PowerOnSelfTestInput; // 8
  PRV_POST_INOUT PowerOnSelfTestOutput; // 16
  PRV_SPI_CONTROL SPIControl; // 24
  u64 Reserved1[4]; // 32 << slim has a value from CBB stored at 48 based on info from 392/_PRV_POWER_MANAGEMENT_CONTROL
  PRV_FAULT_ISOLATION_CHECKSTOP FaultIsolationCheckstop; // 64
  PRV_FAULT_ISOLATION_RECOVERABLE FaultIsolationRecoverable; // 72
  PRV_FAULT_ISOLATION_MACHINE_CHECK FaultIsolationMachineCheck; // 80
  PRV_FAULT_ISOLATION_MODE FaultIsolationMode; // 88
  PRV_FAULT_ISOLATION_ENABLE FaultIsolationEnableMask; // 96
  PRV_LOCAL_ERROR_COUNTER_STATUS LocalErrorCounterStatus; // 104
  PRV_ERROR_INJECT_SELECT ErrorInjectSelect; // 112
  u64 Reserved2; // 120
  u64 XLTLTraceArrayData; // 128
  u64 XLTLControl; // 136
  u64 XLTLCompareCareMasks; // 144
  u64 XLTLPattern0; // 152
  u64 XLTLPattern1; // 160
  u64 XLTLAuxCareMask; // 168
  u64 XLTLTracenInit; // 176
  u64 Reserved3; // 184
  u64 SLTLTraceArrayData; // 192
  u64 SLTLControl; // 200
  u64 SLTLCompareCareMasks; // 208
  u64 SLTLPattern0; // 216
  u64 SLTLPattern1; // 224
  u64 SLTLAuxCareMask; // 232
  u64 SLTLTracenInit; // 240
  u64 Reserved4; // 248
  u64 TLCControlAndStatus; // 256
  u64 TLCState_control; // 264
  u64 TLCCount0Init; // 272
  u64 TLCCount1Init; // 280
  u64 TLCCount2Init; // 288
  u64 TLCCount3Init; // 296
  u64 TLCCount4Init; // 304
  u64 TLCCount5Init; // 312
  u64 TLCActionControl; // 320
  u64 TLCTracenInit; // 328
  u64 TLCFreezeControl; // 336
  u64 TLCCondition0; // 344
  u64 TLCCondition1; // 352
  u64 TLCCondition2; // 360
  u64 TLCCondition3; // 368
  u64 TLCCondition4; // 376
  u64 TrainData; // 384
  PRV_POWER_MANAGEMENT_CONTROL PowerManagementControl; // 392
  PRV_THERMAL_DIODE_CALIBRATION ThermalDiodeCalibration; // 400
  PRV_PSRO_COUNT PSROCount; // 408
  PRV_TIMEBASE_CONTROL TimebaseControl; // 416
  u64 Reserved5[3]; // 424
  PFM_CONTROL PFMControl; // 448
  FPM_STATUS PFMStatus; // 456
  PFM_INPUT_SELECTION PFMInputSelection; // 464
  PFM_TRIGGER_START_STOP PFMTriggerStartStop; // 472
  PFM_COUNTER PFMIntervalTimer; // 480
  PFM_COUNTER PFMIntervalTimerReload; // 488
  u64 Reserved6[2]; // 496
  PFM_COUNTER PFMCounter0; // 512
  PFM_COUNTER PFMCounter1; // 520
  PFM_COUNTER PFMCounter2; // 528
  PFM_COUNTER PFMCounter3; // 536
  PFM_COUNTER PFMCounter4; // 544
  PFM_COUNTER PFMCounter5; // 552
  PFM_COUNTER PFMCounter6; // 560
  PFM_COUNTER PFMCounter7; // 568
  PFM_COUNTER PFMCounter8; // 576
  PFM_COUNTER PFMCounter9; // 584
  PFM_COUNTER PFMCounter10; // 592
  PFM_COUNTER PFMCounter11; // 600
  PFM_COUNTER PFMCounter12; // 608
  PFM_COUNTER PFMCounter13; // 616
  PFM_COUNTER PFMCounter14; // 624
  PFM_COUNTER PFMCounter15; // 632
  PFM_COUNTER_CONTROL PFMCounterControl0; // 640
  PFM_COUNTER_CONTROL PFMCounterControl1; // 648
  PFM_COUNTER_CONTROL PFMCounterControl2; // 656
  PFM_COUNTER_CONTROL PFMCounterControl3; // 664
  PFM_COUNTER_CONTROL PFMCounterControl4; // 672
  PFM_COUNTER_CONTROL PFMCounterControl5; // 680
  PFM_COUNTER_CONTROL PFMCounterControl6; // 688
  PFM_COUNTER_CONTROL PFMCounterControl7; // 696
  PFM_COUNTER_CONTROL PFMCounterControl8; // 704
  PFM_COUNTER_CONTROL PFMCounterControl9; // 712
  PFM_COUNTER_CONTROL PFMCounterControl10; // 720
  PFM_COUNTER_CONTROL PFMCounterControl11; // 728
  PFM_COUNTER_CONTROL PFMCounterControl12; // 736
  PFM_COUNTER_CONTROL PFMCounterControl13; // 744
  PFM_COUNTER_CONTROL PFMCounterControl14; // 752
  PFM_COUNTER_CONTROL PFMCounterControl15; // 760
  u64 Reserved7[416]; // 768
} SOCPRV_BLOCK, * PSOCPRV_BLOCK; // size 4096

} // namespace Xe::Xenon::SOC
