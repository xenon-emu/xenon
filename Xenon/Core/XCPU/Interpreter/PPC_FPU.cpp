// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

static inline void checkFpuAvailable(PPU_STATE *ppuState) {
  if (curThread.SPR.MSR.FP != 1) {
    _ex |= PPU_EX_FPU;
    return;
  }
}

void PPCInterpreter::PPCInterpreter_mffsx(PPU_STATE *ppuState) {
  checkFpuAvailable(ppuState);

  if (_instr.rc) {
    LOG_CRITICAL(Xenon, "FPU: mffs_rc, record not implemented.");
  }

  FPRi(frd).valueAsU64 = static_cast<u64>(GET_FPSCR);
}

void PPCInterpreter::PPCInterpreter_mtfsfx(PPU_STATE *ppuState) {
  checkFpuAvailable(ppuState);

  if (_instr.rc) {
    LOG_CRITICAL(Xenon, "FPU: mtfsf_rc, record not implemented.");
  }

  u32 mask = 0;
  for (u32 b = 0x80; b; b >>= 1) {
    mask <<= 4;

    if (_instr.flm & b) {
      mask |= 0xF;
    }
  }

  SET_FPSCR((static_cast<u32>(FPRi(frb).valueAsU64 & mask)) | (GET_FPSCR & ~mask));
}
