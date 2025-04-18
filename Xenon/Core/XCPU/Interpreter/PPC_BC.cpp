// Copyright 2025 Xenon Emulator Project

#include "Base/Config.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_bc(PPU_STATE *ppuState) {
  B_FORM_BO_BI_BD_AA_LK;

  if (!BO_GET(2)) {
    curThread.SPR.CTR -= 1;
  }

  bool ctrOk = BO_GET(2) | ((curThread.SPR.CTR != 0) ^ BO_GET(3));
  bool condOk = BO_GET(0) || (CR_GET(BI) == BO_GET(1));

  if (ctrOk && condOk) {
    curThread.NIA = (AA ? 0 : curThread.CIA) + (EXTS(BD, 14) << 2);
  }

  if (LK) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_b(PPU_STATE *ppuState) {
  I_FORM_LI_AA_LK;
  curThread.NIA = (AA ? 0 : curThread.CIA) + (EXTS(LI, 24) << 2);

  if (LK) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bcctr(PPU_STATE *ppuState) {
  XL_FORM_BO_BI_BH_LK;

  bool condOk = BO_GET(0) || (CR_GET(BI) == BO_GET(1));

  if (condOk) {
    curThread.NIA = curThread.SPR.CTR & ~3;
  }

  if (LK) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bclr(PPU_STATE *ppuState) {
  XL_FORM_BO_BI_BH_LK;

  if (!BO_GET(2)) {
    curThread.SPR.CTR -= 1;
  }

  bool ctrOk = BO_GET(2) | ((curThread.SPR.CTR != 0) ^ BO_GET(3));
  bool condOk = BO_GET(0) || (CR_GET(BI) == BO_GET(1));

  // CB/SB Hardware Init step skip (hacky)
  if (Config::xcpu.SKIP_HW_INIT) {
    if (curThread.CIA == Config::xcpu.HW_INIT_SKIP_1)
      condOk = false;

    if (curThread.CIA == Config::xcpu.HW_INIT_SKIP_2)
      condOk = true;
  }

  if (ctrOk && condOk) {
    curThread.NIA = curThread.SPR.LR & ~3;
  }

  if (LK) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}
