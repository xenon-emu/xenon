// Copyright 2025 Xenon Emulator Project. All rights reserved.

#ifdef _WIN32
#include <windows.h>
#endif

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
  std::cout << std::format("[KD Packet] Leader: 0x{:X}, Type: 0x{:X}, ByteCount: {}, ID: 0x{:X}, Checksum: 0x{:X}\n",
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
      std::cout << "[KD DebugPrint] " << out << std::endl;
    }
  } else {
      std::cout << "[KD DebugIO] ApiNumber: " << debugIo->ApiNumber << std::endl;
  }
}

void ParseStateChangePacket(const u8 *data, u16 byteCount, u32 packetId) {
  
}

void SendKDAck(Serial::Handle serial, u32 packetId) {
  KD_ACK_PACKET ack = {
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