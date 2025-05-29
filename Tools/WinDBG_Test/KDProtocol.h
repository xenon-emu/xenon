// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <iostream>
#include <format>

#include "Types.h"
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
#define DbgKdPrintStringApi         0x00003230
#define DbgKdGetStringApi           0x00003231

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
struct KD_ACK_PACKET {
  u32 PacketLeader; // 0x30303030 (PACKET_LEADER)
  u16 PacketType; // 0x0004 = PACKET_TYPE_KD_ACKNOWLEDGE
  u16 ByteCount; // 0
  u32 PacketId; // Same as received
  u32 Checksum; // 0
};
#pragma pack(pop)
static_assert(sizeof(KD_ACK_PACKET) == 16);

struct DBGKD_READ_MEMORY32 {
  u32 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesRead;
};

struct DBGKD_READ_MEMORY64 {
  u64 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesRead;
};

struct DBGKD_WRITE_MEMORY32 {
  u32 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesWritten;
};

struct DBGKD_WRITE_MEMORY64 {
  u64 TargetBaseAddress;
  u32 TransferCount;
  u32 ActualBytesWritten;
};

struct DBGKD_GET_CONTEXT {
  u32 Unused;
};

struct DBGKD_SET_CONTEXT {
  u32 ContextFlags;
};

struct DBGKD_WRITE_BREAKPOINT32 {
  u32 BreakPointAddress;
  u32 BreakPointHandle;
};

struct DBGKD_WRITE_BREAKPOINT64 {
  u64 BreakPointAddress;
  u32 BreakPointHandle;
};

struct DBGKD_RESTORE_BREAKPOINT {
  u32 BreakPointHandle;
};

struct DBGKD_CONTINUE {
  u32 ContinueStatus;
};

struct X86_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u32 Dr7;
  u32 CurrentSymbolStart;
  u32 CurrentSymbolEnd;
};

struct ALPHA_DBGKD_CONTROL_SET {
  u32 __padding;
};

struct IA64_DBGKD_CONTROL_SET {
  u32 Continue;
  u64 CurrentSymbolStart;
  u64 CurrentSymbolEnd;
};

struct AMD64_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u64 Dr7;
  u64 CurrentSymbolStart;
  u64 CurrentSymbolEnd;
};

struct ARM_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u32 CurrentSymbolStart;
  u32 CurrentSymbolEnd;
};

struct PPC_DBGKD_CONTROL_SET {
  u32 TraceFlag;
  u64 CurrentSymbolStart;
  u64 CurrentSymbolEnd;
};

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
typedef X86_DBGKD_CONTROL_SET DBGKD_CONTROL_SET;

struct DBGKD_CONTINUE2 {
  u32 ContinueStatus;
  union {
    DBGKD_CONTROL_SET ControlSet;
    DBGKD_ANY_CONTROL_SET AnyControlSet;
  };
};

struct DBGKD_READ_WRITE_IO32 {
  u32 IoAddress;
  u32 DataSize;
  u32 DataValue;
};

struct DBGKD_READ_WRITE_IO64 {
  u64 IoAddress;
  u32 DataSize;
  u32 DataValue;
};

struct DBGKD_READ_WRITE_IO_EXTENDED32 {
  u32 DataSize;
  u32 InterfaceType;
  u32 BusNumber;
  u32 AddressSpace;
  u32 IoAddress;
  u32 DataValue;
};

struct DBGKD_READ_WRITE_IO_EXTENDED64 {
  u32 DataSize;
  u32 InterfaceType;
  u32 BusNumber;
  u32 AddressSpace;
  u64 IoAddress;
  u32 DataValue;
};

struct DBGKD_QUERY_SPECIAL_CALLS {
  u32 NumberOfSpecialCalls;
};

struct DBGKD_SET_SPECIAL_CALL32 {
  u32 SpecialCall;
};

struct DBGKD_SET_SPECIAL_CALL64 {
  u64 SpecialCall;
};

struct DBGKD_SET_INTERNAL_BREAKPOINT32 {
  u32 BreakpointAddress;
  u32 Flags;
};

struct DBGKD_SET_INTERNAL_BREAKPOINT64 {
  u64 BreakpointAddress;
  u32 Flags;
};

struct DBGKD_GET_INTERNAL_BREAKPOINT32 {
  u32 BreakpointAddress;
  u32 Flags;
  u32 Calls;
  u32 MaxCallsPerPeriod;
  u32 MinInstructions;
  u32 MaxInstructions;
  u32 TotalInstructions;
};

struct DBGKD_GET_INTERNAL_BREAKPOINT64 {
  u64 BreakpointAddress;
  u32 Flags;
  u32 Calls;
  u32 MaxCallsPerPeriod;
  u32 MinInstructions;
  u32 MaxInstructions;
  u32 TotalInstructions;
};

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

struct DBGKD_BREAKPOINTEX {
  u32 BreakPointCount;
  u32 ContinueStatus;
};

struct DBGKD_READ_WRITE_MSR {
  u32 Msr;
  u32 DataValueLow;
  u32 DataValueHigh;
};

struct DBGKD_SEARCH_MEMORY {
  union {
    u64 SearchAddress;
    u64 FoundAddress;
  };
  u64 SearchLength;
  u32 PatternLength;
};
struct DBGKD_GET_SET_BUS_DATA {
  u32 BusDataType;
  u32 BusNumber;
  u32 SlotNumber;
  u32 Offset;
  u32 Length;
};

struct DBGKD_FILL_MEMORY {
  u64 Address;
  u32 Length;
  u16 Flags;
  u16 PatternLength;
};

struct DBGKD_QUERY_MEMORY {
  u64 Address;
  u64 Reserved;
  u32 AddressSpace;
  u32 Flags;
};

struct DBGKD_SWITCH_PARTITION {
  u32 Partition;
};

struct DBGKD_WRITE_CUSTOM_BREAKPOINT {
  u64 BreakPointAddress;
  u64 BreakPointInstruction;
  u32 BreakPointHandle;
  u8 BreakPointInstructionSize;
  u8 BreakPointInstructionAlignment;
};

struct DBGKD_CONTEXT_EX {
  u32 Offset;
  u32 ByteCount;
  u32 BytesCopied;
};

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

bool IsValidKDLeader(u32 leader);
u16 CalculateKDChecksum(const u8 *data, u32 length);
void PrintKDPacketHeader(const KD_PACKET &header);
void ParseKDDataPacket(const u8 *data, u16 byteCount);
void ParseStateChangePacket(const u8 *data, u16 byteCount, u32 packetId);
void SendKDAck(Serial::Handle serial, u32 packetId);
