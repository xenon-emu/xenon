// Copyright 2025 Xenon Emulator Project

#include "Base/Config.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_bc(PPU_STATE &ppuState) {
  B_FORM_BO_BI_BD_AA_LK;

  if (!BO_GET(2)) {
    ppuState.ppuThread[ppuState.currentThread].SPR.CTR -= 1;
  }

  bool ctrOk =
      BO_GET(2) |
      ((ppuState.ppuThread[ppuState.currentThread].SPR.CTR != 0) ^ BO_GET(3));
  bool condOk = BO_GET(0) || (CR_GET(BI) == BO_GET(1));

  if (ctrOk && condOk) {
    ppuState.ppuThread[ppuState.currentThread].NIA =
        (AA ? 0 : ppuState.ppuThread[ppuState.currentThread].CIA) +
        (EXTS(BD, 14) << 2);
  }

  if (LK) {
    ppuState.ppuThread[ppuState.currentThread].SPR.LR =
        ppuState.ppuThread[ppuState.currentThread].CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_b(PPU_STATE &ppuState) {
  I_FORM_LI_AA_LK;
  ppuState.ppuThread[ppuState.currentThread].NIA =
      (AA ? 0 : ppuState.ppuThread[ppuState.currentThread].CIA) +
      (EXTS(LI, 24) << 2);

  if (LK) {
    ppuState.ppuThread[ppuState.currentThread].SPR.LR =
        ppuState.ppuThread[ppuState.currentThread].CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bcctr(PPU_STATE &ppuState) {
  XL_FORM_BO_BI_BH_LK;

  bool condOk = BO_GET(0) || (CR_GET(BI) == BO_GET(1));

  if (condOk) {
    ppuState.ppuThread[ppuState.currentThread].NIA =
        ppuState.ppuThread[ppuState.currentThread].SPR.CTR & ~3;
  }

  if (LK) {
    ppuState.ppuThread[ppuState.currentThread].SPR.LR =
        ppuState.ppuThread[ppuState.currentThread].CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bclr(PPU_STATE &ppuState) {
  XL_FORM_BO_BI_BH_LK;

  if (!BO_GET(2)) {
    ppuState.ppuThread[ppuState.currentThread].SPR.CTR -= 1;
  }

  bool ctrOk =
      BO_GET(2) |
      ((ppuState.ppuThread[ppuState.currentThread].SPR.CTR != 0) ^ BO_GET(3));
  bool condOk = BO_GET(0) || (CR_GET(BI) == BO_GET(1));

  // Jrunner XDK build offsets are 0x0000000003003f48 AND 0x0000000003003fdc
  // xell version are 0x0000000003003dc0 AND 0x0000000003003e54

  if (Config::HW_INIT_SKIP1() != 0) {
    if (ppuState.ppuThread[ppuState.currentThread].CIA == Config::HW_INIT_SKIP1())
      condOk = false;

    if (ppuState.ppuThread[ppuState.currentThread].CIA == Config::HW_INIT_SKIP2())
      condOk = true;
  } else {
    if (ppuState.ppuThread[ppuState.currentThread].CIA == 0x0000000003003f48)
      condOk = false;

    if (ppuState.ppuThread[ppuState.currentThread].CIA == 0x0000000003003fdc)
      condOk = true;
  }

  if (ctrOk && condOk) {
    ppuState.ppuThread[ppuState.currentThread].NIA =
        ppuState.ppuThread[ppuState.currentThread].SPR.LR & ~3;
  }

  if (LK) {
    ppuState.ppuThread[ppuState.currentThread].SPR.LR =
        ppuState.ppuThread[ppuState.currentThread].CIA + 4;
  }
}
