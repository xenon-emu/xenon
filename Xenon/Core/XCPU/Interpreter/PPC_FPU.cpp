// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

#define FPR(x)      ppuState.ppuThread[ppuState.currentThread].FPR[x]
#define GET_FPSCR    ppuState.ppuThread[ppuState.currentThread].FPSCR.FPSCR_Hex
#define SET_FPSCR(x)  ppuState.ppuThread[ppuState.currentThread].FPSCR.FPSCR_Hex = x

static inline void checkFpuAvailable(PPU_STATE &ppuState) {
  if (ppuState.ppuThread[ppuState.currentThread].SPR.MSR.FP != 1) {
    ppuState.ppuThread[ppuState.currentThread].exceptReg |= PPU_EX_FPU;
    return;
  }
}

void PPCInterpreter::PPCInterpreter_mffsx(PPU_STATE &ppuState) {
  X_FORM_FrD_RC;

  checkFpuAvailable(ppuState);

  if (RC) {
    LOG_CRITICAL(Xenon, "FPU: mffs_rc, record not implemented.");
  }

  FPR(FrD).valueAsU64 = static_cast<u64>(GET_FPSCR);
}

void PPCInterpreter::PPCInterpreter_mtfsfx(PPU_STATE &ppuState) {
  XFL_FORM_FLM_FrB_RC;

  u32 mask = 0;
  u32 b = 0x80;

  checkFpuAvailable(ppuState);

  if (RC) {
    LOG_CRITICAL(Xenon, "FPU: mtfsf_rc, record not implemented.");
  }

  for (; b; b >>= 1) {
    mask <<= 4;

    if (FLM & b){
      mask |= 0xF;
    }
  }

  SET_FPSCR(((u32)FPR(FrB).valueAsU64 & mask) | (GET_FPSCR & ~mask));
}
