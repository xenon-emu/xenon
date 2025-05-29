// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <iostream>
#include <format>

#include "Base/Types.h"
#include "Serial.h"

//
// Magic Packet bytes
//
#define BREAKIN_PACKET                  0x62626262
#define BREAKIN_PACKET_BYTE             0x62
#define PACKET_LEADER                   0x30303030
#define PACKET_LEADER_BYTE              0x30
#define CONTROL_PACKET_LEADER           0x69696969
#define CONTROL_PACKET_LEADER_BYTE      0x69
#define PACKET_TRAILING_BYTE            0xAA

#define PACKET_MAX_SIZE                 4000
#define DBGKD_MAXSTREAM                 16

//
// Packet Types
//
#define PACKET_TYPE_UNUSED                0
#define PACKET_TYPE_KD_STATE_CHANGE32     1
#define PACKET_TYPE_KD_STATE_MANIPULATE   2
#define PACKET_TYPE_KD_DEBUG_IO           3
#define PACKET_TYPE_KD_ACKNOWLEDGE        4
#define PACKET_TYPE_KD_RESEND             5
#define PACKET_TYPE_KD_RESET              6
#define PACKET_TYPE_KD_STATE_CHANGE64     7
#define PACKET_TYPE_KD_POLL_BREAKIN       8
#define PACKET_TYPE_KD_TRACE_IO           9
#define PACKET_TYPE_KD_CONTROL_REQUEST    10
#define PACKET_TYPE_KD_FILE_IO            11
#define PACKET_TYPE_MAX                   12

//
// Debug I/O Types
//
#define DbgKdPrintStringApi               0x00003230
#define DbgKdGetStringApi                 0x00003231

//
// Wait State Change Types
//
#define DbgKdMinimumStateChange           0x00003030
#define DbgKdExceptionStateChange         0x00003030
#define DbgKdLoadSymbolsStateChange       0x00003031
#define DbgKdCommandStringStateChange     0x00003032
#define DbgKdMaximumStateChange           0x00003033

#pragma pack(push, 1)
struct KD_PACKET {
  u32 PacketLeader;
  u16 PacketType;
  u16 ByteCount;
  u32 PacketId;
  u32 Checksum;
};
#pragma pack(pop)
static_assert(sizeof(KD_PACKET) == 16);

#pragma pack(push, 1)
struct DBGKD_READ_MEMORY32 {
  u32 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesRead;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_READ_MEMORY64 {
  u64 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesRead;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_WRITE_MEMORY32 {
  u32 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesWritten;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_WRITE_MEMORY64 {
  u64 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesWritten;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_CONTEXT {
  u32 Unused;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SET_CONTEXT {
  u32 ContextFlags;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_WRITE_BREAKPOINT32 {
  u32 BreakPointAddress;
  u32 BreakPointHandle;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_WRITE_BREAKPOINT64 {
  u64 BreakPointAddress;
  u32 BreakPointHandle;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_RESTORE_BREAKPOINT {
  u32 BreakPointHandle;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_CONTINUE {
  u32 ContinueStatus;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct X86_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u32 Dr7;
  u32 CurrentSymbolStart;
  u32 CurrentSymbolEnd;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ALPHA_DBGKD_CONTROL_SET {
  u32 __padding;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct IA64_DBGKD_CONTROL_SET {
  u32 Continue;
  u64 CurrentSymbolStart;
  u64 CurrentSymbolEnd;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AMD64_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u64 Dr7;
  u64 CurrentSymbolStart;
  u64 CurrentSymbolEnd;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ARM_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u32 CurrentSymbolStart;
  u32 CurrentSymbolEnd;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PPC_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u64 CurrentSymbolStart;
  u64 CurrentSymbolEnd;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_ANY_CONTROL_SET {
  union {
    X86_DBGKD_CONTROL_SET X86ControlSet;
    ALPHA_DBGKD_CONTROL_SET AlphaControlSet;
    IA64_DBGKD_CONTROL_SET IA64ControlSet;
    AMD64_DBGKD_CONTROL_SET Amd64ControlSet;
    ARM_DBGKD_CONTROL_SET ARMControlSet;
    PPC_DBGKD_CONTROL_SET PPCControlSet;
  };
};
#pragma pack(pop)
typedef X86_DBGKD_CONTROL_SET DBGKD_CONTROL_SET;

#pragma pack(push, 1)
struct DBGKD_CONTINUE2 {
  u32 ContinueStatus;
  union {
    DBGKD_CONTROL_SET ControlSet;
    DBGKD_ANY_CONTROL_SET AnyControlSet;
  };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_READ_WRITE_IO32 {
  u32 IoAddress;
  u32 DataSize;
  u32 DataValue;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_READ_WRITE_IO64 {
  u64 IoAddress;
  u32 DataSize;
  u32 DataValue;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_READ_WRITE_IO_EXTENDED32 {
  u32 DataSize;
  u32 InterfaceType;
  u32 BusNumber;
  u32 AddressSpace;
  u32 IoAddress;
  u32 DataValue;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_READ_WRITE_IO_EXTENDED64 {
  u32 DataSize;
  u32 InterfaceType;
  u32 BusNumber;
  u32 AddressSpace;
  u64 IoAddress;
  u32 DataValue;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_QUERY_SPECIAL_CALLS {
  u32 NumberOfSpecialCalls;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SET_SPECIAL_CALL32 {
  u32 SpecialCall;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SET_SPECIAL_CALL64 {
  u64 SpecialCall;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SET_INTERNAL_BREAKPOINT32 {
  u32 BreakpointAddress;
  u32 Flags;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SET_INTERNAL_BREAKPOINT64 {
  u64 BreakpointAddress;
  u32 Flags;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_INTERNAL_BREAKPOINT32 {
  u32 BreakpointAddress;
  u32 Flags;
  u32 Calls;
  u32 MaxCallsPerPeriod;
  u32 MinInstructions;
  u32 MaxInstructions;
  u32 TotalInstructions;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_INTERNAL_BREAKPOINT64 {
  u64 BreakpointAddress;
  u32 Flags;
  u32 Calls;
  u32 MaxCallsPerPeriod;
  u32 MinInstructions;
  u32 MaxInstructions;
  u32 TotalInstructions;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_VERSION32 {
  u16 MajorVersion;
  u16 MinorVersion;
  u8 ProtocolVersion;
  u8 KdSecondaryVersion;
  u16 Flags;
  u16 MachineType;
  u8 MaxPacketType;
  u8 MaxStateChange;
  u8 MaxManipulate;
  u8 Simulation;
  u16 Unused[1];
  u32 KernBase;
  u32 PsLoadedModuleList;
  u32 DebuggerDataList;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_VERSION64 {
  u16 MajorVersion;
  u16 MinorVersion;
  u8 ProtocolVersion;
  u8 KdSecondaryVersion;
  u16 Flags;
  u16 MachineType;
  u8 MaxPacketType;
  u8 MaxStateChange;
  u8 MaxManipulate;
  u8 Simulation;
  u16 Unused[1];
  u64 KernBase;
  u64 PsLoadedModuleList;
  u64 DebuggerDataList;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_BREAKPOINTEX {
  u32 BreakPointCount;
  u32 ContinueStatus;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_READ_WRITE_MSR {
  u32 Msr;
  u32 DataValueLow;
  u32 DataValueHigh;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SEARCH_MEMORY {
  union {
    u64 SearchAddress;
    u64 FoundAddress;
  };
  u64 SearchLength;
  u32 PatternLength;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_SET_BUS_DATA {
  u32 BusDataType;
  u32 BusNumber;
  u32 SlotNumber;
  u32 Offset;
  u32 Length;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_FILL_MEMORY {
  u64 Address;
  u32 Length;
  u16 Flags;
  u16 PatternLength;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_QUERY_MEMORY {
  u64 Address;
  u64 Reserved;
  u32 AddressSpace;
  u32 Flags;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_SWITCH_PARTITION {
  u32 Partition;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_WRITE_CUSTOM_BREAKPOINT {
  u64 BreakPointAddress;
  u64 BreakPointInstruction;
  u32 BreakPointHandle;
  u8 BreakPointInstructionSize;
  u8 BreakPointInstructionAlignment;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_CONTEXT_EX {
  u32 Offset;
  u32 ByteCount;
  u32 BytesCopied;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_MANIPULATE_STATE32 {
  u32 ApiNumber;
  u16 ProcessorLevel;
  u16 Processor;
  u32 ReturnStatus;
  union {
    DBGKD_READ_MEMORY32 ReadMemory;
    DBGKD_WRITE_MEMORY32 WriteMemory;
    DBGKD_READ_MEMORY64 ReadMemory64;
    DBGKD_WRITE_MEMORY64 WriteMemory64;
    DBGKD_GET_CONTEXT GetContext;
    DBGKD_SET_CONTEXT SetContext;
    DBGKD_WRITE_BREAKPOINT32 WriteBreakPoint;
    DBGKD_RESTORE_BREAKPOINT RestoreBreakPoint;
    DBGKD_CONTINUE Continue;
    DBGKD_CONTINUE2 Continue2;
    DBGKD_READ_WRITE_IO32 ReadWriteIo;
    DBGKD_READ_WRITE_IO_EXTENDED32 ReadWriteIoExtended;
    DBGKD_QUERY_SPECIAL_CALLS QuerySpecialCalls;
    DBGKD_SET_SPECIAL_CALL32 SetSpecialCall;
    DBGKD_SET_INTERNAL_BREAKPOINT32 SetInternalBreakpoint;
    DBGKD_GET_INTERNAL_BREAKPOINT32 GetInternalBreakpoint;
    DBGKD_GET_VERSION32 GetVersion32;
    DBGKD_BREAKPOINTEX BreakPointEx;
    DBGKD_READ_WRITE_MSR ReadWriteMsr;
    DBGKD_SEARCH_MEMORY SearchMemory;
    DBGKD_GET_SET_BUS_DATA GetSetBusData;
    DBGKD_FILL_MEMORY FillMemory;
    DBGKD_QUERY_MEMORY QueryMemory;
    DBGKD_SWITCH_PARTITION SwitchPartition;
  };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_MANIPULATE_STATE64 {
  u32 ApiNumber;
  u16 ProcessorLevel;
  u16 Processor;
  u32 ReturnStatus;
  union {
    DBGKD_READ_MEMORY64 ReadMemory;
    DBGKD_WRITE_MEMORY64 WriteMemory;
    DBGKD_GET_CONTEXT GetContext;
    DBGKD_SET_CONTEXT SetContext;
    DBGKD_WRITE_BREAKPOINT64 WriteBreakPoint;
    DBGKD_RESTORE_BREAKPOINT RestoreBreakPoint;
    DBGKD_CONTINUE Continue;
    DBGKD_CONTINUE2 Continue2;
    DBGKD_READ_WRITE_IO64 ReadWriteIo;
    DBGKD_READ_WRITE_IO_EXTENDED64 ReadWriteIoExtended;
    DBGKD_QUERY_SPECIAL_CALLS QuerySpecialCalls;
    DBGKD_SET_SPECIAL_CALL64 SetSpecialCall;
    DBGKD_SET_INTERNAL_BREAKPOINT64 SetInternalBreakpoint;
    DBGKD_GET_INTERNAL_BREAKPOINT64 GetInternalBreakpoint;
    DBGKD_GET_VERSION64 GetVersion64;
    DBGKD_BREAKPOINTEX BreakPointEx;
    DBGKD_READ_WRITE_MSR ReadWriteMsr;
    DBGKD_SEARCH_MEMORY SearchMemory;
    DBGKD_GET_SET_BUS_DATA GetSetBusData;
    DBGKD_FILL_MEMORY FillMemory;
    DBGKD_QUERY_MEMORY QueryMemory;
    DBGKD_SWITCH_PARTITION SwitchPartition;
    DBGKD_WRITE_CUSTOM_BREAKPOINT WriteCustomBreakpoint;
    DBGKD_CONTEXT_EX ContextEx;
  };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_PRINT_STRING {
  u32 LengthOfString;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_GET_STRING {
  u32 LengthOfPromptString;
  u32 LengthOfStringRead;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct KD_DEBUG_IO {
  u32 ApiNumber;
  u16 ProcessorLevel;
  u16 Processor;
  union {
    DBGKD_PRINT_STRING PrintString;
    DBGKD_GET_STRING GetString;
  };
};
#pragma pack(pop)

#define EXCEPTION_MAXIMUM_PARAMETERS 15

#pragma pack(push, 1)
struct EXCEPTION_RECORD32 {
  u32 ExceptionCode;
  u32 ExceptionFlags;
  u32 ExceptionRecord;
  u32 ExceptionAddress;
  u32 NumberParameters;
  u32 ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct EXCEPTION_RECORD64 {
  u32 ExceptionCode;
  u32 ExceptionFlags;
  u64 ExceptionRecord;
  u64 ExceptionAddress;
  u32 NumberParameters;
  u32 __unusedAlignment;
  u64 ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKM_EXCEPTION32 {
  EXCEPTION_RECORD32 ExceptionRecord;
  u32 FirstChance;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKM_EXCEPTION64 {
  EXCEPTION_RECORD64 ExceptionRecord;
  u32 FirstChance;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_LOAD_SYMBOLS32 {
  u32 PathNameLength;
  u32 BaseOfDll;
  u32 ProcessId;
  u32 CheckSum;
  u32 SizeOfImage;
  u8 UnloadSymbols;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_LOAD_SYMBOLS64 {
  u32 PathNameLength;
  u64 BaseOfDll;
  u64 ProcessId;
  u32 CheckSum;
  u32 SizeOfImage;
  u8 UnloadSymbols;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_COMMAND_STRING {
  u32 Flags;
  u32 Reserved1;
  u64 Reserved2[7];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct X86_DBGKD_CONTROL_REPORT {
  u32 Dr6;
  u32 Dr7;
  u16 InstructionCount;
  u16 ReportFlags;
  u8 InstructionStream[DBGKD_MAXSTREAM];
  u16 SegCs;
  u16 SegDs;
  u16 SegEs;
  u16 SegFs;
  u32 EFlags;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ALPHA_DBGKD_CONTROL_REPORT {
  u32 InstructionCount;
  u8 InstructionStream[DBGKD_MAXSTREAM];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct IA64_DBGKD_CONTROL_REPORT {
  u32 InstructionCount;
  u8 InstructionStream[DBGKD_MAXSTREAM];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AMD64_DBGKD_CONTROL_REPORT {
  u64 Dr6;
  u64 Dr7;
  u32 EFlags;
  u16 InstructionCount;
  u16 ReportFlags;
  u8 InstructionStream[DBGKD_MAXSTREAM];
  u16 SegCs;
  u16 SegDs;
  u16 SegEs;
  u16 SegFs;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ARM_DBGKD_CONTROL_REPORT {
  u32 Cpsr;
  u32 InstructionCount;
  u8 InstructionStream[DBGKD_MAXSTREAM];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ARM64_DBGKD_CONTROL_REPORT {
  u64 Bvr;
  u64 Wvr;
  u32 InstructionCount;
  u8 InstructionStream[DBGKD_MAXSTREAM];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PPC_DBGKD_CONTROL_REPORT {
  u32 InstructionCount;
  u8 InstructionStream[DBGKD_MAXSTREAM];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_ANY_CONTROL_REPORT {
  union {
    X86_DBGKD_CONTROL_REPORT X86ControlReport;
    ALPHA_DBGKD_CONTROL_REPORT AlphaControlReport;
    IA64_DBGKD_CONTROL_REPORT IA64ControlReport;
    AMD64_DBGKD_CONTROL_REPORT Amd64ControlReport;
    ARM_DBGKD_CONTROL_REPORT ARMControlReport;
    ARM64_DBGKD_CONTROL_REPORT ARM64ControlReport;
    PPC_DBGKD_CONTROL_REPORT PPCControlReport;
  };
};
#pragma pack(pop)

#if defined(_M_IX86)
typedef X86_DBGKD_CONTROL_REPORT DBGKD_CONTROL_REPORT;
#elif defined(_M_AMD64)
typedef AMD64_DBGKD_CONTROL_REPORT DBGKD_CONTROL_REPORT;
#elif defined(_M_ARM)
typedef ARM_DBGKD_CONTROL_REPORT DBGKD_CONTROL_REPORT;
#elif defined(_M_ARM64)
typedef ARM64_DBGKD_CONTROL_REPORT DBGKD_CONTROL_REPORT;
#else
#error Unsupported Architecture
#endif

#pragma pack(push, 1)
struct DBGKD_WAIT_STATE_CHANGE32 {
  u32 NewState;
  u16 ProcessorLevel;
  u16 Processor;
  u32 NumberProcessors;
  u32 Thread;
  u32 ProgramCounter;
  union {
    DBGKM_EXCEPTION32 Exception;
    DBGKD_LOAD_SYMBOLS32 LoadSymbols;
  };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_WAIT_STATE_CHANGE64 {
  u32 NewState;
  u16 ProcessorLevel;
  u16 Processor;
  u32 NumberProcessors;
  u64 Thread;
  u64 ProgramCounter;
  union {
    DBGKM_EXCEPTION64 Exception;
    DBGKD_LOAD_SYMBOLS64 LoadSymbols;
  };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DBGKD_ANY_WAIT_STATE_CHANGE {
  u32 NewState;
  u16 ProcessorLevel;
  u16 Processor;
  u32 NumberProcessors;
  u64 Thread;
  u64 ProgramCounter;
  union {
    DBGKM_EXCEPTION64 Exception;
    DBGKD_LOAD_SYMBOLS64 LoadSymbols;
    DBGKD_COMMAND_STRING CommandString;
  };
  union {
    DBGKD_CONTROL_REPORT ControlReport;
    DBGKD_ANY_CONTROL_REPORT AnyControlReport;
  };
};
#pragma pack(pop)

bool IsValidKDLeader(u32 leader);
u16 CalculateKDChecksum(const u8 *data, u32 length);
void PrintKDPacketHeader(const KD_PACKET &header);
void ParseKDDataPacket(const u8 *data, u16 byteCount);
void ParseStateChangePacket(const u8 *data, u16 byteCount, u32 packetId);
void SendKDAck(Serial::Handle serial, u32 packetId);
