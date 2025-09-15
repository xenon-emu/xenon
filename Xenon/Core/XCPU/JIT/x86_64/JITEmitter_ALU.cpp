/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

// Add (x'7C00 0214')
void PPCInterpreter::PPCInterpreterJIT_addx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr){
  /*
  rD <- (rA) + (rB)
  */

  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->add(rATemp, GPRPtr(instr.rb));
  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// Add Immediate Carrying (x'3000 0000')
void PPCInterpreter::PPCInterpreterJIT_addic(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // TODO: Overflow Enable.
  Label end = COMP->newLabel(); // Self explanatory.

  // Get rA value.
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));

  // XER[CA] Clear.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Perform the Add.
  COMP->add(rATemp, imm<s64>(instr.simm16));
  // Check for carry.
  COMP->jnc(end);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(end);
  // Set XER[CA] value.
  COMP->mov(SPRPtr(XER), xer);
  // Set rD value.
  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// Add Carrying (x'7C00 0014')
void PPCInterpreter::PPCInterpreterJIT_addcx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  rD <- (rA) + (rB)
  */

  // TODO: Overflow Enable.
  Label end = COMP->newLabel(); // Self explanatory.

  // Get rA value.
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));

  // XER[CA] Clear.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Perform the Add.
  COMP->add(rATemp, GPRPtr(instr.rb));
  // Check for carry.
  COMP->jnc(end);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(end);
  // Set XER[CA] value.
  COMP->mov(SPRPtr(XER), xer);
  // Set rD value.
  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// Add Extended (x'7C00 0114')
void PPCInterpreter::PPCInterpreterJIT_addex(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rD <- (rA) + (rB) + XER[CA]
  */

  // TODO: Overflow Enable.
  Label end = COMP->newLabel(); // Self explanatory.
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  x86::Gp xer = newGP32();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN
  COMP->adc(rATemp, GPRPtr(instr.rb));
  COMP->jnc(end);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN
  COMP->bind(end);
  COMP->mov(SPRPtr(XER), xer);
  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// Add Immediate (x'3800 0000')
void PPCInterpreter::PPCInterpreterJIT_addi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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

// Add Immediate Shifted (x'3C00 0000')
void PPCInterpreter::PPCInterpreterJIT_addis(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  s64 shImm = (s64{ instr.simm16 } << 16);

  x86::Gp rDTemp = newGP64();
  COMP->mov(rDTemp, shImm);

  if (instr.ra == 0) {
    COMP->mov(GPRPtr(instr.rd), rDTemp);
  } else {
    COMP->add(rDTemp, GPRPtr(instr.ra)); // rDT += rA
    COMP->mov(GPRPtr(instr.rd), rDTemp); // rD  = rDT
  }
}

// And (x'7C00 0038')
void PPCInterpreter::PPCInterpreterJIT_andx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  /*
    rA <- (rS) & (rB)
  */

  // rSTemp
  x86::Gp rSTemp = newGP64();
  COMP->mov(rSTemp, GPRPtr(instr.rs));

  // rS & rB
  COMP->and_(rSTemp, GPRPtr(instr.rb));

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// AND with Complement (x'7C00 0078')
void PPCInterpreter::PPCInterpreterJIT_andcx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  rA <- (rS) + ~(rB)
  */

  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));
  COMP->not_(rBTemp);
  // rS & rB
  COMP->and_(rBTemp, GPRPtr(instr.rs));

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rBTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rBTemp);
}

// And Immediate (x'7000 0000')
void PPCInterpreter::PPCInterpreterJIT_andi(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- (rS) & ((48)0 || UIMM)
  */
  x86::Gp res = newGP64();
  COMP->mov(res, GPRPtr(instr.rs));
  COMP->and_(res, imm<u16>(instr.uimm16));
  COMP->mov(GPRPtr(instr.ra), res);

  J_ppuSetCR0(b, res);
}

// And Immediate Shifted (x'7400 0000')
void PPCInterpreter::PPCInterpreterJIT_andis(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  rA <- (rS) + ((32)0 || UIMM || (16)0)
  */

  x86::Gp rsTemp = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  x86::Gp sh = newGP64();
  u64 shImm = (u64{ instr.uimm16 } << 16);
  COMP->mov(sh, shImm);
  COMP->and_(rsTemp, sh);
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  J_ppuSetCR0(b, rsTemp);
}

// Compare 
void PPCInterpreter::PPCInterpreterJIT_cmp(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rA = newGP64();
  x86::Gp rB = newGP64();
  COMP->mov(rA, GPRPtr(instr.ra));
  COMP->mov(rB, GPRPtr(instr.rb));

  if (instr.l10) {
    J_SetCRField(b, J_BuildCRS(b, rA, rB), instr.crfd);
  } else {
    J_SetCRField(b, J_BuildCRS(b, rA.r32(), rB.r32()), instr.crfd);
  }
}

// Compare Immediate
void PPCInterpreter::PPCInterpreterJIT_cmpi(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rA = newGP64();
  x86::Gp simm = newGP64();
  COMP->mov(rA, GPRPtr(instr.ra));
  COMP->mov(simm, imm<s16>(instr.simm16));

  if (instr.l10) {
    J_SetCRField(b, J_BuildCRS(b, rA, simm), instr.crfd);
  } else {
    J_SetCRField(b, J_BuildCRS(b, rA.r32(), simm.r32()), instr.crfd);
  }
}

// Compare 
void PPCInterpreter::PPCInterpreterJIT_cmpl(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rA = newGP64();
  x86::Gp rB = newGP64();
  COMP->mov(rA, GPRPtr(instr.ra));
  COMP->mov(rB, GPRPtr(instr.rb));

  if (instr.l10) {
    J_SetCRField(b, J_BuildCRU(b, rA, rB), instr.crfd);
  }
  else {
    J_SetCRField(b, J_BuildCRU(b, rA.r32(), rB.r32()), instr.crfd);
  }
}

// Compare Logical Immediate
void PPCInterpreter::PPCInterpreterJIT_cmpli(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rA = newGP64();
  x86::Gp uimm = newGP64();
  COMP->mov(rA, GPRPtr(instr.ra));
  COMP->mov(uimm, imm<u16>(instr.uimm16));

  if (instr.l10) {
    J_SetCRField(b, J_BuildCRU(b, rA, uimm), instr.crfd);
  }
  else {
    J_SetCRField(b, J_BuildCRU(b, rA.r32(), uimm.r32()), instr.crfd);
  }
}

// Multiply Low Doubleword (x'7C00 01D2')
void PPCInterpreter::PPCInterpreterJIT_mulldx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  /*
    prod[0-127] <- (rA) * (rB)
    rD <- prod[64-127]
  */

  x86::Gp rATemp = newGP64();
  x86::Gp rBTemp = newGP64();

  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // rA * rB
  COMP->imul(rATemp, rBTemp); // Multiplication is signed.

  // rD = rATemp
  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// NAND
void PPCInterpreter::PPCInterpreterJIT_nandx(sPPEState *ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- ~((rS) & (rB))
  */

  x86::Gp rSTemp = newGP64();
  x86::Gp rBTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // rS & rB
  COMP->and_(rSTemp, rBTemp);

  // ~rS
  COMP->not_(rSTemp);

  // rD = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// NOR (x'7C00 00F8')
void PPCInterpreter::PPCInterpreterJIT_norx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- ~((rS) | (rB))
  */

  x86::Gp rSTemp = newGP64();
  x86::Gp rBTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // rS | rB
  COMP->or_(rSTemp, rBTemp);

  // ~rS
  COMP->not_(rSTemp);

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// Rotate Left Word Immediate then AND with Mask (x'5400 0000')
void PPCInterpreter::PPCInterpreterJIT_rlwinmx(sPPEState *ppeState, JITBlockBuilder* b, uPPCInstr instr) {
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
    J_ppuSetCR0(b, dup);
}

// Shift Left Double Word (x'7C00 0036') STUB
void PPCInterpreter::PPCInterpreterJIT_sldx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  n <- rB[58-63]
  r <- ROTL[64](rS, n)
  if rB[57] = 0 then
    m <- MASK(0, 63 - n)
  else m <- (64)0
  rA <- r & m
  */

  Label end = COMP->newLabel();
  x86::Gp rsTemp = newGP64();
  COMP->xor_(rsTemp, rsTemp);
  x86::Gp n = newGP64();
  COMP->mov(n, GPRPtr(instr.rb));
  // Condition check.
#ifdef __LITTLE_ENDIAN__
  COMP->bt(n, 6);
#else
  COMP->bt(n, 57);
#endif // LITTLE_ENDIAN
  COMP->jc(end);
  // Do the shift.
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->shl(rsTemp, n); // Bit count is masked by instr.
  COMP->bind(end);
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, rsTemp);
}

// Shift Left Word (x'7C00 0030')
void PPCInterpreter::PPCInterpreterJIT_slwx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- rB[59-63]
    r <- ROTL[32](rS[32-63], n)
    if rB[58] = 0 then m <- MASK(32, 63 - n)
    else m <- (64)0
    rA <- r & m
  */

  Label end = COMP->newLabel();
  x86::Gp rsTemp = newGP64();
  COMP->xor_(rsTemp, rsTemp);
  x86::Gp n = newGP64();
  COMP->mov(n, GPRPtr(instr.rb));
  // Condition check.
#ifdef __LITTLE_ENDIAN__
  COMP->bt(n, 5);
#else
  COMP->bt(n, 58);
#endif // LITTLE_ENDIAN
  COMP->jc(end);
  // Do the shift.
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->shl(rsTemp.r32(), n); // Bit count is masked by instr.
  COMP->bind(end);
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, rsTemp);
}

// Shift Right Double Word (x'7C00 0436')
void PPCInterpreter::PPCInterpreterJIT_srdx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- rB[59-63]
    r <- ROTL[32](rS[32-63], n)
    if rB[58] = 0 then m <- MASK(32, 63 - n)
    else m <- (64)0
    rA <- r & m
  */

  Label end = COMP->newLabel();
  x86::Gp rsTemp = newGP64();
  COMP->xor_(rsTemp, rsTemp);
  x86::Gp n = newGP64();
  COMP->mov(n, GPRPtr(instr.rb));
  // Condition check.
#ifdef __LITTLE_ENDIAN__
  COMP->bt(n, 6);
#else
  COMP->bt(n, 57);
#endif // LITTLE_ENDIAN
  COMP->jc(end);
  // Do the shift.
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->shr(rsTemp, n); // Bit count is masked by instr.
  COMP->bind(end);
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, rsTemp);
}

// Shift Right Word (x'7C00 0430')
void PPCInterpreter::PPCInterpreterJIT_srwx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, 64 - n)
    if rB[57] = 0 then
      m <- MASK(n, 63)
    else m <- (64)0
    rA <- r & m
  */
  Label end = COMP->newLabel();
  x86::Gp rsTemp = newGP64();
  COMP->xor_(rsTemp, rsTemp);
  x86::Gp n = newGP64();
  COMP->mov(n, GPRPtr(instr.rb));
  // Condition check.
#ifdef __LITTLE_ENDIAN__
  COMP->bt(n, 5);
#else
  COMP->bt(n, 58);
#endif // LITTLE_ENDIAN
  COMP->jc(end);
  // Do the shift.
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->shr(rsTemp.r32(), n); // Bit count is masked by instr.
  COMP->bind(end);
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, rsTemp);
}

// Shift Right Algebraic Double Word Immediate (x'7C00 0674')
void PPCInterpreter::PPCInterpreterJIT_sradix(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, 64 - n)
    m <- MASK(n, 63)
    S <- rS[0]
    rA <- (r & m) | (((64)S) & ~m)
    XER[CA] <- S & ((r & ~m) != 0)
  */
  Label end = COMP->newLabel();
  x86::Gp sh = newGP64();
  COMP->mov(sh, u64(instr.sh64));
  x86::Gp rsTemp = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN
  COMP->sar(rsTemp, sh);
  COMP->mov(GPRPtr(instr.ra), rsTemp);
  COMP->cmp(rsTemp.r32(), 0);
  COMP->jae(end); // If rsTemp >= 0, then we dont set XER[CA].
  COMP->shl(rsTemp, sh);
  COMP->cmp(rsTemp, GPRPtr(instr.rs));
  COMP->je(end);
  // Set XER[CA]
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN
  COMP->bind(end);
  COMP->mov(SPRPtr(XER), xer);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, rsTemp);
}

// Rotate Left Word then AND with Mask (x'5C00 0000')
void PPCInterpreter::PPCInterpreterJIT_rlwnmx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
    J_ppuSetCR0(b, dup);
}

// XOR (x'7C00 0278')
void PPCInterpreter::PPCInterpreterJIT_xorx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
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

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// XOR Immediate (x'6800 0000')
void PPCInterpreter::PPCInterpreterJIT_xori(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- (rS) ^ ((4816)0 || UIMM)
  */
  x86::Gp rsTemp = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->xor_(rsTemp, imm<u64>(instr.uimm16));
  COMP->mov(GPRPtr(instr.ra), rsTemp);
}

// XOR Immediate Shifted (x'6C00 0000')
void PPCInterpreter::PPCInterpreterJIT_xoris(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- (rS) | ((32)0 || UIMM || (16)0)
  */
  x86::Gp tmp = newGP64();
  x86::Gp val1 = newGP64();
  u64 shImm = (u64{ instr.uimm16 } << 16);
  COMP->mov(tmp, GPRPtr(instr.rs));
  COMP->mov(val1, shImm);
  COMP->xor_(tmp, val1);
  COMP->mov(GPRPtr(instr.ra), tmp);
}

// OR (x'7C00 0378')
void PPCInterpreter::PPCInterpreterJIT_orx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- (rS) | (rB)
  */

  // rSTemp
  x86::Gp rSTemp = newGP64();
  x86::Gp rBTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // rSTemp | rBTemp
  COMP->or_(rSTemp, rBTemp);

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// OR with Complement (x'7C00 0338')
void PPCInterpreter::PPCInterpreterJIT_orcx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  rA <- (rS) | ~(rB)
  */

  x86::Gp rSTemp = newGP64();
  x86::Gp rBTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  COMP->not_(rBTemp); // rB = ~rB

  // rS | rB
  COMP->or_(rSTemp, rBTemp);

  // rA = rSTemp
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// OR Immediate (x'6000 0000')
void PPCInterpreter::PPCInterpreterJIT_ori(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    rA <- (rS) | ((4816)0 || UIMM)
  */
  x86::Gp rsTemp = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->or_(rsTemp, imm<u64>(instr.uimm16));
  COMP->mov(GPRPtr(instr.ra), rsTemp);
}

// OR Immediate Shifted (x'6400 0000')
void PPCInterpreter::PPCInterpreterJIT_oris(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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

// Rotate Left Double Word then Clear Left (x'7800 0010')
void PPCInterpreter::PPCInterpreterJIT_rldclx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, 63)
    rA <- r & m
  */

  x86::Gp rb = newGP64();
  COMP->mov(rb, GPRPtr(instr.rb));
  x86::Gp rs = newGP64();
  COMP->mov(rs, GPRPtr(instr.rs));
  COMP->rol(rs, rb); // rol64 by variable

  u64 rotMask = (~0ull >> instr.mbe64);
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(rs, mask);
  COMP->mov(GPRPtr(instr.ra), rs);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rs);
}

// Rotate Left Double Word then Clear Right (x'7800 0012')
void PPCInterpreter::PPCInterpreterJIT_rldcrx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, n)
    e <- me[5] || me[0-4]
    m <- MASK(0, e)
    rA <- r & m
  */

  x86::Gp rb = newGP64();
  COMP->mov(rb, GPRPtr(instr.rb));
  x86::Gp rs = newGP64();
  COMP->mov(rs, GPRPtr(instr.rs));
  COMP->rol(rs,rb); // rol64 by variable

  u64 rotMask = (~0ull << (instr.mbe64 ^ 63));
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(rs, mask);
  COMP->mov(GPRPtr(instr.ra), rs);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rs);
}

// Rotate Left Double Word Immediate then Clear (x'7800 0008')
void PPCInterpreter::PPCInterpreterJIT_rldicx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, ~ n)
    rA <- r & m
  */

  x86::Gp sh = newGP64();
  COMP->mov(sh, u64(instr.sh64));
  x86::Gp rs = newGP64();
  COMP->mov(rs, GPRPtr(instr.rs));
  COMP->rol(rs, sh);

  u64 rotMask = PPCRotateMask(instr.mbe64, instr.sh64 ^ 63);
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(rs, mask);
  COMP->mov(GPRPtr(instr.ra), rs);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rs);
}

// Rotate Left Double Word Immediate then Clear Left (x'7800 0000')
void PPCInterpreter::PPCInterpreterJIT_rldiclx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, 63)
    rA <- r & m
  */

  x86::Gp sh = newGP64();
  COMP->mov(sh, u64(instr.sh64));
  x86::Gp rs = newGP64();
  COMP->mov(rs, GPRPtr(instr.rs));
  COMP->rol(rs, sh);

  u64 rotMask = (~0ull >> instr.mbe64);
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(rs, mask);
  COMP->mov(GPRPtr(instr.ra), rs);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rs);
}

// Rotate Left Double Word Immediate then Clear Right (x'7800 0004')
void PPCInterpreter::PPCInterpreterJIT_rldicrx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    e <- me[5] || me[0-4]
    m <- MASK(0, e)
    rA <- r & m
  */

  x86::Gp sh = newGP64();
  COMP->mov(sh, u64(instr.sh64));
  x86::Gp rs = newGP64();
  COMP->mov(rs, GPRPtr(instr.rs));
  COMP->rol(rs, sh);

  u64 rotMask = (~0ull << (instr.mbe64 ^ 63));
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(rs, mask);
  COMP->mov(GPRPtr(instr.ra), rs);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rs);
}

// Rotate Left Double Word Immediate then Mask Insert (x'7800 000C')
void PPCInterpreter::PPCInterpreterJIT_rldimix(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, ~n)
    rA <- (r & m) | (rA & ~m)
  */

  x86::Gp sh = newGP64();
  COMP->mov(sh, u64(instr.sh64));
  x86::Gp rs = newGP64();
  COMP->mov(rs, GPRPtr(instr.rs));
  x86::Gp ra = newGP64();
  COMP->mov(ra, GPRPtr(instr.ra));

  COMP->rol(rs, sh); // Rotate left.
  u64 rotMask = PPCRotateMask(instr.mbe64, instr.sh64 ^ 63); // Create mask.
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(rs, mask); // AND rotation result with mask.
  COMP->not_(mask); // Invert mask.
  COMP->and_(ra, mask); // And ra with mask.
  COMP->or_(rs, ra); // Or rs with ra.
  COMP->mov(GPRPtr(instr.ra), rs); // Store rs in ra.

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rs);
}

// Rotate Left Word Immediate then Mask Insert (x'5000 0000')
void PPCInterpreter::PPCInterpreterJIT_rlwimix(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- (r & m) | (rA & ~m)
  */

  x86::Gp sh = newGP32();
  COMP->mov(sh, u64(instr.sh32));
  x86::Gp rs = newGP32();
  COMP->mov(rs, GPRPtr(instr.rs));
  x86::Gp ra = newGP64();
  COMP->mov(ra, GPRPtr(instr.ra));

  COMP->rol(rs, sh); // Rotate left.
  x86::Gp dup = Jduplicate32(b, rs);
  u64 rotMask = PPCRotateMask(32 + instr.mb32, 32 + instr.me32); // Create mask.
  x86::Gp mask = newGP64();
  COMP->mov(mask, rotMask);
  COMP->and_(dup, mask); // AND rotation result with mask.
  COMP->not_(mask); // Invert mask.
  COMP->and_(ra, mask); // And ra with mask.
  COMP->or_(ra, dup); // Or rs with ra.
  COMP->mov(GPRPtr(instr.ra), ra); // Store rs in ra.

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, ra);
}

// Count Leading Zeros Double Word (x'7C00 0074')
void PPCInterpreter::PPCInterpreterJIT_cntlzdx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  n <- 0
  do while n < 64
    if rS[n] = 1 then leave
    n <- n + 1
  rA <- n
  */
  x86::Gp tmp = newGP64();
  COMP->lzcnt(tmp, GPRPtr(instr.rs));
  COMP->mov(GPRPtr(instr.ra), tmp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, tmp);
}
#endif