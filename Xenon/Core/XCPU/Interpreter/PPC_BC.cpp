// Copyright 2025 Xenon Emulator Project

#include "Base/Config.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_bc(PPU_STATE *ppuState) {
  const bool bo0 = (_instr.bo & 0x10) != 0;
  const bool bo1 = (_instr.bo & 0x08) != 0;
  const bool bo2 = (_instr.bo & 0x04) != 0;
  const bool bo3 = (_instr.bo & 0x02) != 0;

  if (!bo2) {
    curThread.SPR.CTR -= 1;
  }

  bool ctrOk = bo2 | ((curThread.SPR.CTR != 0) ^ bo3);
  bool condOk = bo0 | (!!(CRBi(bi)) ^ (bo1 ^ true));

  if (ctrOk && condOk) {
    curThread.NIA = (_instr.aa ? 0 : curThread.CIA) + _instr.bt14;
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_b(PPU_STATE *ppuState) {
  curThread.NIA = (_instr.aa ? 0 : curThread.CIA) + _instr.bt24;

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bcctr(PPU_STATE *ppuState) {
  XL_FORM_BO_BI_BH_LK;

  bool condOk = (_instr.bo & 0x10 || CRBi(bi)
    == ((_instr.bo & 0x8) != 0));

  if (condOk) {
    curThread.NIA = curThread.SPR.CTR & ~3;
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bclr(PPU_STATE *ppuState) {
  const bool bo0 = (_instr.bo & 0x10) != 0;
  const bool bo1 = (_instr.bo & 0x08) != 0;
  const bool bo2 = (_instr.bo & 0x04) != 0;
  const bool bo3 = (_instr.bo & 0x02) != 0;

  if (!bo2) {
    curThread.SPR.CTR -= 1;
  }

  bool ctrOk = bo2 | ((curThread.SPR.CTR != 0) ^ bo3);
  bool condOk = bo0 | (!!(CRBi(bi)) ^ (bo1 ^ true));

  // Jrunner XDK build offsets are 0x3003F48 AND 0x3003FDC
  // XeLL offsets are 0x3003DC0 AND 0x3003E54
  if ((Config::xcpu.HW_INIT_SKIP_1 && Config::xcpu.HW_INIT_SKIP_2) == 0) {
    if (curThread.CIA == 0x3003F48)
      condOk = false;

    if (curThread.CIA == 0x3003FDC)
      condOk = true;
  } else {
    if (curThread.CIA == Config::xcpu.HW_INIT_SKIP_1)
      condOk = false;

    if (curThread.CIA == Config::xcpu.HW_INIT_SKIP_2)
      condOk = true;
  }

  if (ctrOk && condOk) {
    curThread.NIA = curThread.SPR.LR & ~3;
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}
