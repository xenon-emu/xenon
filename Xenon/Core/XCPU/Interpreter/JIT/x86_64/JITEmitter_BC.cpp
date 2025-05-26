// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
void PPCInterpreter::PPCInterpreterJIT_b(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  x86::Gp cia = newGP32();  // current instruction address
  x86::Gp target = newGP32();

  // target = (AA ? 0 : CIA) + EXTS(LI) << 2
  COMP->mov(cia, CIAPtr());  // load runtime CIA

  int32_t offset = EXTS(instr.li, 24) << 2;
  if (instr.aa) {
    COMP->mov(target, offset);
  } else {
    COMP->mov(target, cia);
    COMP->add(target, offset);
  }

  COMP->mov(NIAPtr(), target);  // NIA = target

  if (instr.lk) {
    // LR = CIA + 4
    COMP->add(cia, 4);
    COMP->mov(LRPtr(), cia);
  }
}

#endif