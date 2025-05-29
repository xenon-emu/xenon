// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"

#include "KDProtocol.h"

bool IsValidKDLeader(u32 leader) {
  return leader == PACKET_LEADER || leader == CONTROL_PACKET_LEADER;
}

u16 CalculateKDChecksum(const u8 *data, u32 length) {
  u32 sum = 0;
  for (u32 i = 0; i < length; ++i) {
    sum += data[i];
  }
  return static_cast<u16>(sum & 0xFFFF);
}

void PrintKDPacketHeader(const KD_PACKET &header) {
  LOG_DEBUG(KD_Packet, "Leader: 0x{:X}, Type: 0x{:X}, ByteCount: {}, ID: 0x{:X}, Checksum: 0x{:X}",
    header.PacketLeader,
    header.PacketType,
    header.ByteCount,
    header.PacketId,
    header.Checksum);
}

void ParseKDDataPacket(const u8 *data, u16 byteCount) {
  if (byteCount < sizeof(KD_DEBUG_IO))
    return;

  auto *debugIo = reinterpret_cast<const KD_DEBUG_IO*>(data);
  if (debugIo->ApiNumber == 0x0002) { // DbgKdPrintStringApi
    if (byteCount >= sizeof(KD_DEBUG_IO) + debugIo->PrintString.LengthOfString) {
      const char *msg = reinterpret_cast<const char*>(data + sizeof(KD_DEBUG_IO));
      std::string out(msg, debugIo->PrintString.LengthOfString);
      LOG_DEBUG(KD_DebugIO, "{}", out);
    }
  } else {
    LOG_DEBUG(KD_DebugIO, "API Number: {}", debugIo->ApiNumber);
  }
}

void ParseStateChangePacket(const u8 *data, u16 byteCount, u32 packetId) {
  if (byteCount < sizeof(DBGKD_ANY_WAIT_STATE_CHANGE))
    return;

  auto *waitStateChange = reinterpret_cast<const DBGKD_ANY_WAIT_STATE_CHANGE *>(data);
  LOG_DEBUG(KD, "New state: 0x{:X}", waitStateChange->NewState);
  LOG_DEBUG(KD, "Processor level: 0x{:X}", waitStateChange->ProcessorLevel);
  LOG_DEBUG(KD, "Processor: {}", waitStateChange->Processor);
  LOG_DEBUG(KD, "Number of processors online: {}", waitStateChange->NumberProcessors);
  LOG_DEBUG(KD, "Thread: 0x{:X}", waitStateChange->Thread);
  LOG_DEBUG(KD, "Program counter: 0x{:X}", waitStateChange->ProgramCounter);
  LOG_DEBUG(KD, "Instruction count: 0x{:X}", waitStateChange->AnyControlReport.PPCControlReport.InstructionCount);
  switch (waitStateChange->NewState) {
  case DbgKdLoadSymbolsStateChange: {
    auto &LoadSymbols = waitStateChange->LoadSymbols;
    LOG_DEBUG(KD, "Path name length: 0x{:X}", LoadSymbols.PathNameLength);
    LOG_DEBUG(KD, "DLL Base: 0x{:X}", LoadSymbols.BaseOfDll);
    LOG_DEBUG(KD, "PID: 0x{:X}", LoadSymbols.ProcessId);
    LOG_DEBUG(KD, "Checksum: 0x{:X}", LoadSymbols.CheckSum);
    LOG_DEBUG(KD, "SizeOfImage: 0x{:X}", LoadSymbols.SizeOfImage);
    LOG_DEBUG(KD, "UnloadSymbols: 0x{:X}", LoadSymbols.UnloadSymbols);
    //const u8 *data = reinterpret_cast<const u8*>(&LoadSymbols.PathNameLength) + 4;
    //LOG_DEBUG(KD, "String: {}", (const char*)data);
    LOG_DEBUG(KD, "UNK at address 0x{:X} - 0x{:X}", LoadSymbols.BaseOfDll, (LoadSymbols.BaseOfDll + LoadSymbols.SizeOfImage));
  } break;
  case DbgKdExceptionStateChange: {
    auto &Exception = waitStateChange->Exception;
    auto &ExceptionRecord = Exception.ExceptionRecord;
    LOG_DEBUG(KD, "Exception!");
    LOG_DEBUG(KD, "Code: 0x{:X}", ExceptionRecord.ExceptionCode);
    LOG_DEBUG(KD, "Flags: 0x{:X}", ExceptionRecord.ExceptionFlags);
    LOG_DEBUG(KD, "Record: 0x{:X}", ExceptionRecord.ExceptionRecord);
    LOG_DEBUG(KD, "Address: 0x{:X}", ExceptionRecord.ExceptionAddress);
    LOG_DEBUG(KD, "Parameters: {}", ExceptionRecord.NumberParameters);
    for (u32 i = 0; i != ExceptionRecord.NumberParameters; ++i)
      LOG_DEBUG(KD, "[{}] 0x{:X}", i, ExceptionRecord.ExceptionInformation[i]);
    LOG_DEBUG(KD, "First Chance: 0x{:X}", Exception.FirstChance);
  } break;
  }
}

void SendKDAck(Serial::Handle serial, u32 packetId) {
  KD_PACKET ack = {
    CONTROL_PACKET_LEADER,
    PACKET_TYPE_KD_ACKNOWLEDGE,
    0,
    packetId,
    0
  };

  // Send the initial ACK packet
  Serial::Write(serial, ack);
  // Write the state change

  // Terminate packet
  Serial::Write<u8>(serial, PACKET_TRAILING_BYTE);
}