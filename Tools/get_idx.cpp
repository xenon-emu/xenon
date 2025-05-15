// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"

#include "Core/XGPU/XenosRegisters.h"

REQ_PARAM(address, "Kernel-space address to translate and parse");
s32 ToolMain() {
  u64 addr = PARAM_address.Get<u64>();
  u64 baseAddr = 0x7FC80000;
  u64 offset = addr - baseAddr;
  u64 correctedAddr = 0xEC800000 + offset;
  const u32 regIndex = (correctedAddr & 0xFFFFF) / 4;
  LOG_INFO(Main, "Address: 0x{:08X}", addr);
  LOG_INFO(Main, "Offset: 0x{:08X}", offset);
  LOG_INFO(Main, "Corrected Address: 0x{:08X}", correctedAddr);
  LOG_INFO(Main, "Register: {}, 0x{:04X}", Xe::XGPU::GetRegisterNameById(regIndex), regIndex);
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