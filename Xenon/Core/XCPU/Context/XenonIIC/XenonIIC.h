/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <mutex>
#include <queue>

namespace Xe::XCPU {

  // Interrupt Vectors
  enum eXeIntVectors : u8 {
    prioIPI4        = 0x08, // Inter Processor Interrupt 4
    prioIPI3        = 0x10, // Inter Processor Interrupt 3
    prioSMM         = 0x14, // System Management Mode (SMC FIFO Request Interrupt)
    prioSFCX        = 0x18, // Secure Flash Controller for Xbox Interrupt
    prioSATAHDD     = 0x20, // SATA Hard Drive Disk Interrupt
    prioSATAODD     = 0x24, // SATA Optical Disk Drive Interrupt
    prioOHCI0       = 0x2C, // OHCI USB Controller 0 Interrupt
    prioEHCI0       = 0x30, // EHCI USB Controller 0 Interrupt
    prioOHCI1       = 0x34, // OHCI USB Controller 1 Interrupt
    prioEHCI1       = 0x38, // EHCI USB Controller 1 Interrupt
    prioXMA         = 0x40, // Xbox Media Audio Interrupt
    prioAUDIO       = 0x44, // Audio Controller Interrupt
    prioENET        = 0x4C, // Ethernet Controller Interrupt
    prioXPS         = 0x54, // Xbox Procedural Synthesis Interrupt
    prioGRAPHICS    = 0x58, // Xenos Graphics Engine Interrupt
    prioPROFILER    = 0x60, // Profiler Interrupt
    prioBIU         = 0x64, // BUS Interface Unit Interrupt
    prioIOC         = 0x68, // I/O Controller Interrupt
    prioFSB         = 0x6C, // Front Side Bus Interrupt
    prioIPI2        = 0x70, // Inter Processor Interrupt 2
    prioCLOCK       = 0x74, // Clock Interrupt (SMC Timer)
    prioIPI1        = 0x78, // Inter Processor Interrupt 1
    prioNONE        = 0x7C  // No Interrupt
  };

  //
  // System On Chip Interrupt Register Block
  // Offset: 0x80000200_00050000
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
#endif // __LITTLE_ENDIAN__
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
#endif // __LITTLE_ENDIAN__
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
#endif // __LITTLE_ENDIAN__
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
#endif // __LITTLE_ENDIAN__
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
#endif // __LITTLE_ENDIAN__
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
#endif // __LITTLE_ENDIAN__
    u64 AsULONGLONG;
  } SOCINTS_LIDR;

  typedef struct _SOCINTS_PROCESSOR_BLOCK {
    SOCINTS_LIDR LogicalIdentification;     // 0
    SOCINTS_VECTOR InterruptTaskPriority;     // 8
    SOCINTS_IPIGR IpiGeneration;        // 16
    u64 Reserved1;                // 24
    // The IRR specifies which interrupts are pending acknowledgement
    u64 InterruptCapture;             // 32
    u64 InterruptAssertion;           // 40
    // The ISR register specifies which interrupts have been acknowledged,
    // but are still waiting for an End Of Interrupt (EOI)
    u64 InterruptInService;           // 48
    u64 InterruptTriggerMode;           // 56
    u64 Reserved2[2];               // 64
    SOCINTS_VECTOR InterruptAcknowledge;    // 80
    SOCINTS_VECTOR InterruptAcknowledgeAutoUpdate;// 88
    u64 EndOfInterrupt;             // 96
    SOCINTS_VECTOR EndOfInterruptAutoUpdate;  // 104
    SOCINTS_VECTOR SpuriousVector;        // 112
    u64 Reserved3[15];              // 120
    u64 ThreadReset;              // 240
    u64 Reserved4[481];             // 248
  } SOCINTS_PROCESSOR_BLOCK, * PSOCINTS_PROCESSOR_BLOCK;

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
  } SOCINTS_BLOCK, * PSOCINTS_BLOCK;

  // Structure represnting an Interrupt Packet
    struct sInterruptPacket {
    u8 interruptType = prioNONE;
    bool acknowledged = false;

    // Highest priority comes first
    friend constexpr bool operator<(const sInterruptPacket& lhs, const sInterruptPacket& rhs) noexcept {
      return lhs.interruptType > rhs.interruptType;
    }
  };

  // Structure tracking the state of interrupts for each PPU Thread.
  struct sInterruptState {
    std::priority_queue<sInterruptPacket> pendingInterrupts;
  };

  class XenonIIC {
  public:
    XenonIIC();
    ~XenonIIC();

    // Read/Write routines
    void Write(u64 writeAddress, const u8* data, u64 size);
    void Read(u64 readAddress, u8* data, u64 size);

    // Interrupt Generation Routine
    void generateInterrupt(u8 interruptType, u8 cpusToInterrupt);
    // Cancels a previously generated pending interrupt.
    void cancelInterrupt(u8 interruptType, u8 cpusToInterrupt);
    // Returns true if there are pending interrupts for the given thread.
    bool hasPendingInterrupts(u8 threadID, bool ignorePendingACKd = false);

  private:
    // Our Interrupt Block
    std::unique_ptr<SOCINTS_BLOCK> socINTBlock = {};

    // Interrupt States for each PPU Thread
    sInterruptState interruptState[6] = {};

    // Mutex for thread safety
    Base::FutexRecursiveMutex iicMutex;

    // Erases the first element in the queue that has been ack'd.
    void removeFirstACKdInterrupt(u8 threadID);

    // Reads out the first element that has not been ACk'd and marks it as ack'd.
    u8 acknowledgeInterrupt(u8 threadID);

    // Processes an access offset and returns a string from where it belongs to.
    std::string getSOCINTAccess(u32 offset);

    // Returns the name of the interrupt based on its type
    std::string getIntName(eXeIntVectors interruptType);

    // Globals
    static constexpr u32 ProcessorBlockSize = 0x1000;
    static constexpr u32 ProcessorBlocksEnd = 6 * ProcessorBlockSize; // 0x6000
  };
}