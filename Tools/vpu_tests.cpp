// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"
#include "Base/Vector128.h"

Base::Vector128 VR[128]{};
struct DummyInstr {
  u8 vsh = 0;
  u8 va = 0;
  u8 vb = 1;
  u8 vc = 2;
  u8 vd = 3;
} _instr;
#define VRi(x) VR[_instr.x]

static inline u8 vsldoiHelper(u8 sh, Base::Vector128 vra, Base::Vector128 vrb) {
  return (sh < 16) ? vra.bytes[sh] : vrb.bytes[sh & 15];
}

s32 ToolMain() {
  VRi(va).dword[0] = 0x00010203;
  VRi(va).dword[1] = 0x04050607;
  VRi(va).dword[2] = 0x08090A0B;
  VRi(va).dword[3] = 0x0C0D0E0F;

  VRi(vb).dword[0] = 0x10111213;
  VRi(vb).dword[1] = 0x14151617;
  VRi(vb).dword[2] = 0x18191A1B;
  VRi(vb).dword[3] = 0x1C1D1E1F;

  VRi(va).dword[0] = byteswap_be<u32>(VRi(va).dword[0]);
  VRi(va).dword[1] = byteswap_be<u32>(VRi(va).dword[1]);
  VRi(va).dword[2] = byteswap_be<u32>(VRi(va).dword[2]);
  VRi(va).dword[3] = byteswap_be<u32>(VRi(va).dword[3]);

  VRi(vb).dword[0] = byteswap_be<u32>(VRi(vb).dword[0]);
  VRi(vb).dword[1] = byteswap_be<u32>(VRi(vb).dword[1]);
  VRi(vb).dword[2] = byteswap_be<u32>(VRi(vb).dword[2]);
  VRi(vb).dword[3] = byteswap_be<u32>(VRi(vb).dword[3]);

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vsldoiHelper(_instr.vsh + idx, VRi(va), VRi(vb));
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);

  for (u8 i = 0; i != 4; ++i) {
    LOG_INFO(Main, "VD{}: 0x{:08X}", i, VRi(vd).dword[i]);
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