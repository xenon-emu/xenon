// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"

#include "Core/XGPU/XenosRegisters.h"

REQ_PARAM(value, "Value to byteswap");
PARAM(testValue, "Value to test against");
PARAM(testSwapped, "Includes extra tests against the value, including byteswapping the result");
s32 ToolMain() {
  u64 value = PARAM_value.Get<u64>();
  u64 valueSwapped = std::byteswap<u32>(value);
  LOG_INFO(Main, "Value: 0x{:08X}", value);
  LOG_INFO(Main, " Swapped: 0x{:08X}", valueSwapped);
  if (!PARAM_testValue.Present()) {
    return 0;
  }
  bool swap = PARAM_testSwapped.Present();
  u64 testValue = PARAM_testValue.Get<u64>();
  LOG_INFO(Main, " Test: 0x{:08X}", testValue);
  LOG_INFO(Main, " Result: 0x{:08X}", value & testValue);
  if (swap) {
    LOG_INFO(Main, " Result (Value Swapped): 0x{:08X}", valueSwapped & testValue);
    testValue = std::byteswap<u64>(testValue);
    LOG_INFO(Main, " Result (Test Swapped): 0x{:08X}", std::byteswap<u64>(value & testValue));
    LOG_INFO(Main, " Result (Both Swapped): 0x{:08X}", std::byteswap<u64>(valueSwapped & testValue));
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