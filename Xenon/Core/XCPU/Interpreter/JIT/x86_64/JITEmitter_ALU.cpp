// Copyright 2025 Xenon Emulator Project

#include "JITEmitter_Helpers.h"

// Add Immediate (x'3800 0000')
void PPCInterpreter::PPCInterpreterJIT_addi(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  /*
    if rA = 0 then rD <- EXTS(SIMM)
    else rD <- (rA) + EXTS(SIMM)
  */
  int16_t immVal = instr.simm16;
  // rDT = imm
  x86::Gp rDTemp = newGP64();
  COMP->mov(rDTemp, immVal); 

  if (instr.ra == 0) {
    COMP->mov(GPRPtr(instr.rd), rDTemp);
  }
  else {
    COMP->add(rDTemp, GPRPtr(instr.ra)); // rDT += rA
    COMP->mov(GPRPtr(instr.rd), rDTemp); // rD  = rDT
  } 
}


void PPCInterpreter::PPCInterpreterJIT_rlwinmx(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode t_instr) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- (r & m)
  */
  u64 mask = PPCRotateMask(32 + t_instr.mb32, 32 + t_instr.me32);
  //
  x86::Gp rol = Jrotl32(b, GPRPtr(t_instr.rs), t_instr.sh32);
  x86::Gp dup = Jduplicate32(b, rol);
  COMP->and_(dup, mask); // mask

  /*/ _rc
  if (_instr.rc) {
      RECORD_CR0(GPRi(ra));
  }*/
}


// And Immediate (x'7000 0000')
void PPCInterpreter::PPCInterpreterJIT_andi(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode t_instr) {
  /*
    rA <- (rS) & ((48)0 || UIMM)
  */
  x86::Gp res = newGP64();

  int16_t immVal = t_instr.simm16;
  COMP->mov(res, GPRPtr(t_instr.rs));
  COMP->and_(res, immVal);
  COMP->mov(GPRPtr(t_instr.ra), res);

  J_ppuSetCR(b, res, 0);
}

// OR Immediate Shifted (x'6400 0000')
void PPCInterpreter::PPCInterpreterJIT_oris(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode t_instr) {
  /*
    rA <- (rS) | ((32)0 || UIMM || (16)0)
  */
  x86::Gp tmp = newGP64();
  u64 shImm = t_instr.uimm16 << 16;
  COMP->mov(tmp, GPRPtr(t_instr.rs));
  COMP->or_(tmp, shImm);
  COMP->mov(GPRPtr(t_instr.ra), tmp);
}