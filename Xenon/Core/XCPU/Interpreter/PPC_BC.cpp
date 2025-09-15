/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Config.h"
#include "Core/XeMain.h"

#include "PPCInterpreter.h"

// Branch Conditional
void PPCInterpreter::PPCInterpreter_bc(sPPEState *ppeState) {
  if ((_instr.bo & 0x4) == 0) {
    curThread.SPR.CTR -= 1;
  }

  const bool ctrOk = ((_instr.bo & 0x4) != 0 ? 1 : 0) | ((curThread.SPR.CTR != 0) ^ ((_instr.bo & 0x2) != 0));
  const bool condOk = ((_instr.bo & 0x10) != 0 ? 1 : 0) || (CR_GET(_instr.bi) == ((_instr.bo & 0x8) != 0));

  if (ctrOk && condOk) {
    curThread.NIA = (_instr.aa ? 0 : curThread.CIA) + (EXTS(_instr.ds, 14) << 2);
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

// Branch
void PPCInterpreter::PPCInterpreter_b(sPPEState *ppeState) {
  curThread.NIA = (_instr.aa ? 0 : curThread.CIA) + _instr.bt24;

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

// Branch Conditional to Count Register
void PPCInterpreter::PPCInterpreter_bcctr(sPPEState *ppeState) {
  const bool condOk = ((_instr.bo & 0x10) != 0 ? 1 : 0) || (CR_GET(_instr.bi) == ((_instr.bo & 0x8) != 0));

  if (condOk) {
    curThread.NIA = curThread.SPR.CTR & ~3;
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}

// Branch Conditional to Link Register
void PPCInterpreter::PPCInterpreter_bclr(sPPEState *ppeState) {
  if ((_instr.bo & 0x4) != 0 ? false : true) {
    curThread.SPR.CTR -= 1;
  }

  const bool ctrOk = ((_instr.bo & 0x4) != 0 ? 1 : 0) | ((curThread.SPR.CTR != 0) ^ ((_instr.bo & 0x2) != 0));
  bool condOk = ((_instr.bo & 0x10) != 0 ? 1 : 0) || (CR_GET(_instr.bi) == ((_instr.bo & 0x8) != 0));

  // CB/SB Hardware Init step skip (hacky)
  if (XeMain::sfcx && XeMain::sfcx->initSkip1 && XeMain::sfcx->initSkip2) {
    if (curThread.CIA == XeMain::sfcx->initSkip1)
      condOk = false;

    if (curThread.CIA == XeMain::sfcx->initSkip2)
      condOk = true;
  }

  if (ctrOk && condOk) {
    curThread.NIA = curThread.SPR.LR & ~3;
  }

  if (_instr.lk) {
    curThread.SPR.LR = curThread.CIA + 4;
  }
}
