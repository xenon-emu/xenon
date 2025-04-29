// Copyright 2025 Xenon Emulator Project

#include "JITEmitter_Helpers.h"

void PPCInterpreter::PPCInterpreterJIT_b(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  x86::Gp tmp = newGP32();
  u32 newAddr = (instr.aa ? 0 : curThread.CIA) + (EXTS(instr.li, 24) << 2);
  COMP->mov(tmp, newAddr);
  COMP->mov(NIAPtr(), tmp);

  if (instr.lk) {
    COMP->mov(tmp, curThread.CIA + 4);
    COMP->mov(LRPtr(), tmp);
  }
}