// Copyright 2025 Xenon Emulator Project

#include "Base/Config.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_bc(PPU_STATE *ppuState) {
  if ((_instr.bo & 0x4) == 0) {
    curThread.SPR.CTR -= 1;
  }

  bool ctrOk = ((_instr.bo & 0x4) != 0 ? 1 : 0) | ((curThread.SPR.CTR != 0) ^ ((_instr.bo & 0x2) != 0));
  bool condOk = ((_instr.bo & 0x10) != 0 ? 1 : 0) || (CR_GET(_instr.bi) == ((_instr.bo & 0x8) != 0));

  if (ctrOk && condOk) {
    curThread.NIA = (_instr.aa ? 0 : curThread.CIA) + (EXTS(_instr.ds, 14) << 2);
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
  bool condOk = ((_instr.bo & 0x10) != 0 ? 1 : 0) || (CR_GET(_instr.bi) == ((_instr.bo & 0x8) != 0));

  if (condOk) {
    curThread.NIA = curThread.SPR.CTR & ~3;
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

void PPCInterpreter::PPCInterpreter_bclr(PPU_STATE *ppuState) {
  if ((_instr.bo & 0x4) != 0 ? false : true) {
    curThread.SPR.CTR -= 1;
  }

  bool ctrOk = ((_instr.bo & 0x4) != 0 ? 1 : 0) | ((curThread.SPR.CTR != 0) ^ ((_instr.bo & 0x2) != 0));
  bool condOk = ((_instr.bo & 0x10) != 0 ? 1 : 0) || (CR_GET(_instr.bi) == ((_instr.bo & 0x8) != 0));

  // CB/SB Hardware Init step skip (hacky)
  if (Config::xcpu.skipHWInit) {
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
