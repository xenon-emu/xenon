// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"

#include "Core/XGPU/XenosRegisters.h"
#include "Core/XGPU/PM4Opcodes.h"

REQ_PARAM(opcodes, "PM4 Opcodes to break down", true, true);
s32 ToolMain() {
  u64 opcodeCount = PARAM_opcodes.Count();
  for (u64 i = 0; i != opcodeCount; ++i) {
    u32 packetData = PARAM_opcodes.Get<u32>(i);
    const u32 packetType = packetData >> 30;
    LOG_SECTIONLESS("Type{}: \n", packetType);
    LOG_SECTIONLESS(" Packet: 0x{:08X}\n", packetData);
    switch (packetType) {
    case 0: {
      // Base register to start from.
      const u32 baseIndex = (packetData & 0x7FFF);
      // Tells wheter the write is to one or multiple regs starting at specified register at base index.
      const u32 singleRegWrite = (packetData >> 15) & 0x1;
      const u32 regCount = ((packetData >> 16) & 0x3FFF) + 1;
      LOG_SECTIONLESS(" Register Count: {}\n", regCount);
      LOG_SECTIONLESS(" Simple Register Write: {}\n", singleRegWrite ? "Yes" : "No");
      for (u64 idx = 0; idx < regCount; idx++) {
        u32 reg = singleRegWrite ? baseIndex : baseIndex + idx;
        LOG_SECTIONLESS("  Register: {}, 0x{:04X}\n", Xe::XGPU::GetRegisterNameById(reg), reg);
      }
    } break;
    case 1: {
      const u32 regIndex0 = packetData & 0x7FF;
      const u32 regIndex1 = (packetData >> 11) & 0x7FF;
      LOG_SECTIONLESS(" Register0: {}, 0x{:04X}\n", Xe::XGPU::GetRegisterNameById(regIndex0), regIndex0);
      LOG_SECTIONLESS(" Register1: {}, 0x{:04X}\n", Xe::XGPU::GetRegisterNameById(regIndex1), regIndex1);
    } break;
    case 2: {
      LOG_SECTIONLESS(" No-operation\n");
    } break;
    case 3: {
      u32 currentOpcode = (packetData >> 8) & 0x7F;
      const u32 dataCount = ((packetData >> 16) & 0x3FFF) + 1;
      LOG_SECTIONLESS(" Data Count: {}\n", dataCount);
      LOG_SECTIONLESS(" Opcode: {}, 0x{:04X}\n", Xe::XGPU::GetPM4Opcode(currentOpcode), currentOpcode);
    } break;
    }
    if ((i + 1) != opcodeCount)
      LOG_SECTIONLESS("\n");
  }
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