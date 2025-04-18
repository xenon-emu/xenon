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

namespace Xe::Xenon::SOC {
//
// System On Chip Interrupt Register Block
// Offset: 0x8000020000050000
//

typedef union _INT_VECTOR {
#ifdef __LITTLE_ENDIAN__
  u32 INT_MASK : 1;
  u32 INT_LATCHED : 1;
  u32 INT_SENT : 1;
  u32 INT_DESTID : 8;
  u32 INT_FLAGS : 2;
  u32 INT_VECTOR : 6;
#else
  u32 INT_VECTOR : 6;
  u32 INT_FLAGS : 2;
  u32 INT_DESTID : 8;
  u32 INT_SENT : 1;
  u32 INT_LATCHED : 1;
  u32 INT_MASK : 1;
#endif
} INT_VECTOR;

typedef union _SOCINTS_IRER {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 RecoverableError : 8;
    u64 RecoverableErrorCapture : 8;
    u64 RecoverableErrorMask : 8;
    u64 RecoverableErrorDebug : 8;
    u64 Reserved1 : 32;
  } AsBITS;
#else
  struct {
    u64 Reserved1 : 32;
    u64 RecoverableErrorDebug : 8;
    u64 RecoverableErrorMask : 8;
    u64 RecoverableErrorCapture : 8;
    u64 RecoverableError : 8;
  } AsBITS;
#endif
  u64 AsULONGLONG;
} SOCINTS_IRER;

typedef union _SOCINTS_MIGR {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 2;
    u64 VectorNumber : 5;
    u64 Reserved2 : 1;
    u64 TriggerMode : 1;
    u64 Polarity : 1;
    u64 Mask : 1;
    u64 EoiPending : 1;
    u64 DeliveryStatus : 1;
    u64 InterruptState : 1;
    u64 Reserved3 : 2;
    u64 DestinationId : 6;
    u64 Reserved4 : 42;
  } AsBITS;
#else
  struct {
    u64 Reserved4 : 42;
    u64 DestinationId : 6;
    u64 Reserved3 : 2;
    u64 InterruptState : 1;
    u64 DeliveryStatus : 1;
    u64 EoiPending : 1;
    u64 Mask : 1;
    u64 Polarity : 1;
    u64 TriggerMode : 1;
    u64 Reserved2 : 1;
    u64 VectorNumber : 5;
    u64 Reserved1 : 2;
  } AsBITS;
#endif
  u64 AsULONGLONG;
} SOCINTS_MIGR;

typedef union _SOCINTS_IPIGR {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 2;
    u64 VectorNumber : 5;
    u64 Reserved2 : 9;
    // Which cores is this IPI taegeted to, 1 << corenum/LogicalIdentification.
    u64 DestinationId : 6;
    u64 Reserved3 : 42;
  } AsBITS;
#else
  struct {
    u64 Reserved3 : 42;
    u64 DestinationId : 6;
    u64 Reserved2 : 9;
    u64 VectorNumber : 5;
    u64 Reserved1 : 2;
  } AsBITS;
#endif
  u64 AsULONGLONG;
} SOCINTS_IPIGR;

typedef union _SOCINTS_VECTOR {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 Reserved1 : 2;
    u64 VectorNumber : 5;
    u64 Reserved2 : 57;
  } AsBITS;
#else
  struct {
    u64 Reserved2 : 57;
    u64 VectorNumber : 5;
    u64 Reserved1 : 2;
  } AsBITS;
#endif
  u64 AsULONGLONG;
  } SOCINTS_VECTOR;

  typedef union _SOCINTS_LIDR {
#ifdef __LITTLE_ENDIAN__
  struct {
    u64 LogicalId : 6;
    u64 Reserved1 : 58;
  } AsBITS;
#else
  struct {
    u64 Reserved1 : 58;
    u64 LogicalId : 6;
  } AsBITS;
#endif
  u64 AsULONGLONG;
} SOCINTS_LIDR;

typedef struct _SOCINTS_PROCESSOR_BLOCK {
  SOCINTS_LIDR LogicalIdentification;       // 0
  SOCINTS_VECTOR InterruptTaskPriority;       // 8
  SOCINTS_IPIGR IpiGeneration;          // 16
  u64 Reserved1;                  // 24
  // The IRR specifies which interrupts are pending acknowledgement
  u64 InterruptCapture;               // 32
  u64 InterruptAssertion;             // 40
  // The ISR register specifies which interrupts have been acknowledged,
  // but are still waiting for an End Of Interrupt (EOI)
  u64 InterruptInService;             // 48
  u64 InterruptTriggerMode;             // 56
  u64 Reserved2[2];                 // 64
  SOCINTS_VECTOR InterruptAcknowledge;      // 80
  SOCINTS_VECTOR InterruptAcknowledgeAutoUpdate;  // 88
  u64 EndOfInterrupt;               // 96
  SOCINTS_VECTOR EndOfInterruptAutoUpdate;    // 104
  SOCINTS_VECTOR SpuriousVector;          // 112
  u64 Reserved3[15];                // 120
  u64 ThreadReset;                // 240
  u64 Reserved4[481];               // 248
} SOCINTS_PROCESSOR_BLOCK, *PSOCINTS_PROCESSOR_BLOCK;

typedef struct _SOCINTS_BLOCK {
  SOCINTS_PROCESSOR_BLOCK ProcessorBlock[6]; // 0 - one for each hardware thread...
  SOCINTS_MIGR MiscellaneousInterruptGeneration0; // 24576
  u64 Reserved1; // 24584
  SOCINTS_MIGR MiscellaneousInterruptGeneration1; // 24592
  u64 Reserved2; // 24600
  SOCINTS_MIGR MiscellaneousInterruptGeneration2; // 24608
  u64 Reserved3; // 24616
  SOCINTS_MIGR MiscellaneousInterruptGeneration3; // 24624
  u64 Reserved4; // 24632
  SOCINTS_MIGR MiscellaneousInterruptGeneration4; // 24640
  u64 Reserved5[5]; // 24648
  u64 EndOfInterruptBaseAddress; // 24688
  u64 Reserved6[495]; // 24696
  SOCINTS_IRER InterruptRecoverableError; // 28656
  u64 Reserved7; // 28664
  SOCINTS_IRER InterruptRecoverableErrorOrMask; // 28672
  u64 Reserved8; // 28680
  SOCINTS_IRER InterruptRecoverableErrorAndMask; // 28688
  u64 Reserved9; // 28696
  u64 InterruptDebugConfiguration; // 28704
  u64 Reserved10; // 28712
  u64 InterruptPerformanceMeasurementCounter; // 28720
  u64 Reserved11[9]; // 28728
  u64 EndOfInterruptGeneration; // 28800
  u64 Reserved12[495]; // 28808
} SOCINTS_BLOCK, *PSOCINTS_BLOCK;

} // namespace Xe::Xenon::SOC