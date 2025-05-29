// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"
#include "KDProtocol.h"
#include "Serial.h"

PARAM(comPort, "The COM port to connect to");
s32 ToolMain() {
  std::string port = "3";
  if (PARAM_comPort.Get<u16>() != 0)
    port = PARAM_comPort.Get();
  std::string portName = "\\\\.\\COM" + port;
  Serial::Handle serialPort = Serial::OpenPort(portName.c_str());
  if ((u64)serialPort < 0) {
    std::cerr << "Failed to open serial port: " << portName << std::endl;
    return 1;
  }
  std::cout << "Listening for KD packets..." << std::endl;
  u8 buffer[1024] = {};
  u32 bytesRead = 0;
  while (true) {
    bytesRead = Serial::Read(serialPort, buffer);
    if (bytesRead != 0) {
      std::cout << "Got a packet!" << std::endl;
    }
    if (bytesRead >= sizeof(KD_PACKET)) {
      for (u64 i = 0; i + sizeof(KD_PACKET) <= bytesRead; ++i) {
        KD_PACKET *pkt = reinterpret_cast<KD_PACKET*>(&buffer[i]);
        if (IsValidKDLeader(pkt->PacketLeader) &&
          i + sizeof(KD_PACKET) + pkt->ByteCount <= bytesRead) {
          PrintKDPacketHeader(*pkt);
          switch (pkt->PacketType) {
          case PACKET_TYPE_KD_DEBUG_IO:
            ParseKDDataPacket(&buffer[i + sizeof(KD_PACKET)], pkt->ByteCount);
            break;
          case PACKET_TYPE_KD_STATE_CHANGE64:
            ParseStateChangePacket(&buffer[i + sizeof(KD_PACKET)], pkt->ByteCount, pkt->PacketId);
            SendKDAck(serialPort, pkt->PacketId);
            break;
          }
          i += sizeof(KD_PACKET) + pkt->ByteCount - 1;
        }
      }
    }
  }

  Serial::ClosePort(serialPort);
  return 0;
}

extern s32 ToolMain();
PARAM(help, "Prints this message", false);
s32 main(s32 argc, char *argv[]) {
  // Init params
  Base::Param::Init(argc, argv);
  // Handle help param
  if (PARAM_help.Present()) {
    ::Base::Param::Help();
    return 0;
  }
  return ToolMain();
}