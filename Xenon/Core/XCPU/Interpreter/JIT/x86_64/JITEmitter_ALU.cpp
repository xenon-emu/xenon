// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
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
  } else {
    COMP->add(rDTemp, GPRPtr(instr.ra)); // rDT += rA
    COMP->mov(GPRPtr(instr.rd), rDTemp); // rD  = rDT
  }
}

// And (x'7C00 0038')
void PPCInterpreter::PPCInterpreterJIT_andx(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  /*
    rA <- (rS) & (rB)
  */

  // rSTemp
  x86::Gp rSTemp = newGP64();
  COMP->mov(rSTemp, GPRPtr(instr.rs));

  // rSTemp & rB
  COMP->and_(rSTemp, GPRPtr(instr.rb));

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);
}

// Multiply Low Doubleword (x'7C00 01D2')
void PPCInterpreter::PPCInterpreterJIT_mulld(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  /*
    prod[0-127] <- (rA) * (rB)
    rD <- prod[64-127]
  */

  // rATemp
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));

  // rATemp * rB
  COMP->mul(rATemp, GPRPtr(instr.rb));

  // rD = rATemp
  COMP->mov(GPRPtr(instr.rd), rATemp);
}

// Rotate Left Word Immediate then AND with Mask (x'5400 0000')
void PPCInterpreter::PPCInterpreterJIT_rlwinmx(PPU_STATE *ppuState, JITBlockBuilder* b, PPCOpcode instr) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- (r & m)
  */
  x86::Gp n = newGP32();
  COMP->mov(n, imm<u16>(instr.sh32));

  x86::Gp rol = newGP32();
  COMP->mov(rol, GPRPtr(instr.rs));
  COMP->rol(rol, n); // rol32 by variable

  x86::Gp dup = Jduplicate32(b, rol);
  u64 mask = PPCRotateMask(32 + instr.mb32, 32 + instr.me32);
  COMP->and_(dup, mask);
  COMP->mov(GPRPtr(instr.ra), dup);

  // _rc
  if (instr.rc)
    J_ppuSetCR_LOGICAL(b, dup, 0);
}

// Rotate Left Word then AND with Mask (x'5C00 0000')
void PPCInterpreter::PPCInterpreterJIT_rlwnmx(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  /*
    n <- rB[59-63]27-31
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- r & m
  */
  x86::Gp n = newGP32();
  COMP->mov(n, GPRPtr(instr.rb));
  COMP->and_(n, 0x1F); // n = rB & 0x1F (rot amount)

  x86::Gp rol = newGP32();
  COMP->mov(rol, GPRPtr(instr.rs));
  COMP->rol(rol, n); // rol32 by variable

  x86::Gp dup = Jduplicate32(b, rol);
  u64 mask = PPCRotateMask(32 + instr.mb32, 32 + instr.me32);
  COMP->and_(dup, mask);
  COMP->mov(GPRPtr(instr.ra), dup);

  // _rc
  if (instr.rc)
    J_ppuSetCR(b, dup, 0);
}


// And Immediate (x'7000 0000')
void PPCInterpreter::PPCInterpreterJIT_andi(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  /*
    rA <- (rS) & ((48)0 || UIMM)
  */
  x86::Gp res = newGP64();
  COMP->mov(res, GPRPtr(instr.rs)); 
  COMP->and_(res, imm<u16>(instr.uimm16));
  COMP->mov(GPRPtr(instr.ra), res);

  J_ppuSetCR_LOGICAL(b, res, 0);
}

// XOR (x'7C00 0278')
void PPCInterpreter::PPCInterpreterJIT_xor(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode instr) {
  /*
    rA <- (rS) ^ (rB)
  */

  // rS
  x86::Gp rSTemp = newGP64();
  COMP->mov(rSTemp, GPRPtr(instr.rs));

  // rSTemp ^ rB
  COMP->xor_(rSTemp, GPRPtr(instr.rb));

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);
}

// XOR Immediate (x'6800 0000')
void PPCInterpreter::PPCInterpreterJIT_xori(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode t_instr) {
  /*
    rA <- (rS) ^ ((4816)0 || UIMM)
  */
  x86::Gp tmp = newGP64();
  x86::Gp val1 = newGP64();
  u64 shImm = (u64{ t_instr.uimm16 });
  COMP->mov(tmp, GPRPtr(t_instr.rs));
  COMP->mov(val1, shImm);
  COMP->xor_(tmp, val1);
  COMP->mov(GPRPtr(t_instr.ra), tmp);
}

// XOR Immediate Shifted (x'6C00 0000')
void PPCInterpreter::PPCInterpreterJIT_xoris(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode t_instr) {
  /*
    rA <- (rS) | ((32)0 || UIMM || (16)0)
  */
  x86::Gp tmp = newGP64();
  x86::Gp val1 = newGP64();
  u64 shImm = (u64{ t_instr.uimm16 } << 16);
  COMP->mov(tmp, GPRPtr(t_instr.rs));
  COMP->mov(val1, shImm);
  COMP->xor_(tmp, val1);
  COMP->mov(GPRPtr(t_instr.ra), tmp);
}

// OR (x'7C00 0378')
void PPCInterpreter::PPCInterpreterJIT_or(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode instr) {
  /*
    rA <- (rS) | (rB)
  */

  // rSTemp
  x86::Gp rSTemp = newGP64();
  COMP->mov(rSTemp, GPRPtr(instr.rs));

  // rSTemp | rB
  COMP->or_(rSTemp, GPRPtr(instr.rb));

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);
}

// OR Immediate (x'6000 0000')
void PPCInterpreter::PPCInterpreterJIT_ori(PPU_STATE* ppuState, JITBlockBuilder* b, PPCOpcode t_instr) {
  /*
    rA <- (rS) | ((4816)0 || UIMM)
  */
  x86::Gp tmp = newGP64();
  x86::Gp val1 = newGP64();
  u64 shImm = (u64{ t_instr.uimm16 });
  COMP->mov(tmp, GPRPtr(t_instr.rs));
  COMP->mov(val1, shImm);
  COMP->or_(tmp, val1);
  COMP->mov(GPRPtr(t_instr.ra), tmp);
}

// OR Immediate Shifted (x'6400 0000')
void PPCInterpreter::PPCInterpreterJIT_oris(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
  /*
    rA <- (rS) | ((32)0 || UIMM || (16)0)
  */
  x86::Gp tmp = newGP64();
  x86::Gp val1 = newGP64();
  u64 shImm = (u64{ instr.uimm16 } << 16);
  COMP->mov(tmp, GPRPtr(instr.rs));
  COMP->mov(val1, shImm);
  COMP->or_(tmp, val1);
  COMP->mov(GPRPtr(instr.ra), tmp);
}
#endif