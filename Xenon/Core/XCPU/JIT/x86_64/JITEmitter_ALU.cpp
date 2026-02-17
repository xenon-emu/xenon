/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

// Trap Doubleword Immediate
void PPCInterpreter::PPCInterpreterJIT_tdi(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  x86::Gp simm = newGP64();
  COMP->mov(simm, imm<s64>(instr.simm16));
  TrapCheck(b, rATemp, simm, static_cast<u32>(instr.bo));
}

// Trap Word Immediate
void PPCInterpreter::PPCInterpreterJIT_twi(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rATemp = newGP32();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  x86::Gp simm = newGP32();
  COMP->mov(simm, imm<s32>(instr.simm16));
  TrapCheck(b, rATemp, simm, static_cast<u32>(instr.bo));
}

// Trap Doubleword
void PPCInterpreter::PPCInterpreterJIT_td(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));
  TrapCheck(b, rATemp, rBTemp, static_cast<u32>(instr.bo));
}

// Trap Word
void PPCInterpreter::PPCInterpreterJIT_tw(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rATemp = newGP32();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  x86::Gp rBTemp = newGP32();
  COMP->mov(rBTemp, GPRPtr(instr.rb));
  TrapCheck(b, rATemp, rBTemp, static_cast<u32>(instr.bo));
}

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
  Label sfBitMode = COMP->newLabel();

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

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63);  // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode);    // If set, use 64-bit add. (checks for carry flag from previous operation).
  // Perform 32bit addition to check for carry.
  COMP->add(rATemp.r32(), imm<s32>(instr.simm16));
  // Get back the value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Check for carry.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Perform the 64 bit Add.
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

  if (instr.main & 1)
    J_ppuSetCR0(b, rATemp);
}

// Add Carrying (x'7C00 0014')
void PPCInterpreter::PPCInterpreterJIT_addcx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  /*
  rD <- (rA) + (rB)
  */

  // TODO: Overflow Enable.

  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value.
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Get rB value.
  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // XER[CA] Clear.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit add. (checks for carry flag from previous operation).
  // Perform 32bit addition to check for carry.
  COMP->add(rATemp.r32(), rBTemp.r32());
  // Get back the value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Check for carry.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Perform the 64 bit Add.
  COMP->add(rATemp, rBTemp);
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
  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value.
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Get rB value.
  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // Load XER and get CA bit into carry flag, then clear it.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag, then clear it.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN

  // Save the carry flag state before checking MSR.
  x86::Gp carryIn = newGP64();
  COMP->setc(carryIn.r8());

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit add.

  // Perform 32bit addition to check for carry.
  // Restore carry flag from saved state.
  COMP->bt(carryIn, 0);
  COMP->adc(rATemp.r32(), rBTemp.r32());
  // Get back the value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Check for carry from 32-bit operation.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Restore carry flag from saved state for 64-bit operation.
  COMP->bt(carryIn, 0);
  // Perform the 64 bit Add with carry.
  COMP->adc(rATemp, rBTemp);
  // Check for carry from 64-bit operation.
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

// Add to Zero Extended (x'7C00 0194')
void PPCInterpreter::PPCInterpreterJIT_addzex(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // TODO: Overflow Enable.
  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));

  // Load XER and get CA bit into carry flag, then clear it.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag, then clear it.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN

  // Save the carry flag state before checking MSR.
  x86::Gp carryIn = newGP64();
  COMP->setc(carryIn.r8());

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit add.

  // Perform 32bit addition to check for carry.
  // Restore carry flag from saved state.
  COMP->bt(carryIn, 0);
  COMP->adc(rATemp.r32(), imm<u32>(0));
  // Get back the value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Check for carry from 32-bit operation.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Restore carry flag from saved state for 64-bit operation.
  COMP->bt(carryIn, 0);
  // Perform the 64 bit Add with carry: rA + CA
  COMP->adc(rATemp, imm<u64>(0));
  // Check for carry from 64-bit operation.
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

// Add to Minus One Extended (x'7C00 01D4')
void PPCInterpreter::PPCInterpreterJIT_addmex(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // TODO: Overflow Enable.
  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));

  // Load XER and get CA bit into carry flag, then clear it.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag, then clear it.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN

  // Save the carry flag state before checking MSR.
  x86::Gp carryIn = newGP64();
  COMP->setc(carryIn.r8());

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit add.

  // Perform 32bit addition to check for carry.
  // Restore carry flag from saved state.
  COMP->bt(carryIn, 0);
  COMP->adc(rATemp.r32(), imm<s32>(-1));
  // Get back the value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  // Check for carry from 32-bit operation.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Restore carry flag from saved state for 64-bit operation.
  COMP->bt(carryIn, 0);
  // Perform the 64 bit Add with carry: rA + CA + (-1)
  COMP->adc(rATemp, imm<s64>(-1));
  // Check for carry from 64-bit operation.
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

// Divide Double Word (x'7C00 03D2')
void PPCInterpreter::PPCInterpreterJIT_divdx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label setZero = COMP->newLabel();
  Label doDiv = COMP->newLabel();
  Label end = COMP->newLabel();

  // Cargar rA (dividendo) y rB (divisor)
  x86::Gp rATemp = newGP64();
  x86::Gp rBTemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // Zero divide check
  COMP->test(rBTemp, rBTemp);
  COMP->jz(setZero);

  // Overflow check: if rA == INT64_MIN and rB == -1
  x86::Gp int64Min = newGP64();
  COMP->mov(int64Min, imm<u64>(0x8000000000000000ull));
  COMP->cmp(rATemp, int64Min);
  COMP->jne(doDiv);
  COMP->cmp(rBTemp, imm<s64>(-1));
  COMP->je(setZero);

  // Divide
  COMP->bind(doDiv);
  x86::Gp rax = newGP64();
  x86::Gp rdx = newGP64();
  COMP->mov(rax, rATemp);
  COMP->cqo(rdx, rax);
  COMP->idiv(rdx, rax, rBTemp);
  COMP->jmp(end);
  COMP->bind(setZero);
  COMP->xor_(rax, rax);
  COMP->bind(end);
  COMP->mov(GPRPtr(instr.rd), rax);

  if (instr.rc)
    J_ppuSetCR0(b, rax);
}

// Divide Word (x'7C00 03D6')
void PPCInterpreter::PPCInterpreterJIT_divwx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label setZero = COMP->newLabel();
  Label doDiv = COMP->newLabel();
  Label end = COMP->newLabel();

  // Load rA and rB (32-bit values)
  x86::Gp rATemp = newGP32();
  x86::Gp rBTemp = newGP32();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // Result register (declared before branches)
  x86::Gp result = newGP64();

  // Zero divide check
  COMP->test(rBTemp, rBTemp);
  COMP->jz(setZero);

  // Overflow check: if rA == INT32_MIN and rB == -1
  COMP->cmp(rATemp, imm<s32>(0x80000000));
  COMP->jne(doDiv);
  COMP->cmp(rBTemp, imm<s32>(-1));
  COMP->je(setZero);

  // Divide (signed 32-bit)
  COMP->bind(doDiv);
  x86::Gp eax = newGP32();
  x86::Gp edx = newGP32();
  COMP->mov(eax, rATemp);
  COMP->cdq(edx, eax); // Sign-extend EAX into EDX:EAX
  COMP->idiv(edx, eax, rBTemp); // Signed divide: EAX = EDX:EAX / rBTemp

  // Zero-extend result to 64 bits (NOT sign-extend per PPC spec)
  COMP->mov(result.r32(), eax);
  COMP->mov(GPRPtr(instr.rd), result);
  COMP->jmp(end);

  // Zero divide / overflow case: rD = 0
  COMP->bind(setZero);
  COMP->xor_(result, result);
  COMP->mov(GPRPtr(instr.rd), result);

  COMP->bind(end);

  if (instr.rc)
    J_ppuSetCR0(b, result);
}

// Divide Double Word Unsigned (x'7C00 0392')
void PPCInterpreter::PPCInterpreterJIT_divdux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label setZero = COMP->newLabel();
  Label doDiv = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp rATemp = newGP64();
  x86::Gp rBTemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // Zero divide check
  COMP->test(rBTemp, rBTemp);
  COMP->jz(setZero);

  // Divide (unsigned)
  COMP->bind(doDiv);
  x86::Gp rax = newGP64();
  x86::Gp rdx = newGP64();
  COMP->mov(rax, rATemp);
  COMP->xor_(rdx, rdx); // Clear RDX for unsigned division
  COMP->div(rdx, rax, rBTemp); // Unsigned divide: RAX = RDX:RAX / rBTemp
  COMP->jmp(end);
  COMP->bind(setZero);
  COMP->xor_(rax, rax);
  COMP->bind(end);
  COMP->mov(GPRPtr(instr.rd), rax);

  if (instr.rc)
    J_ppuSetCR0(b, rax);
}

// Divide Word Unsigned (x'7C00 0396')
void PPCInterpreter::PPCInterpreterJIT_divwux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label setZero = COMP->newLabel();
  Label doDiv = COMP->newLabel();
  Label end = COMP->newLabel();

  // Load rA and rB (32-bit values)
  x86::Gp rATemp = newGP32();
  x86::Gp rBTemp = newGP32();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // Zero divide check
  COMP->test(rBTemp, rBTemp);
  COMP->jz(setZero);

  // Divide (unsigned 32-bit)
  COMP->bind(doDiv);
  x86::Gp eax = newGP32();
  x86::Gp edx = newGP32();
  COMP->mov(eax, rATemp);
  COMP->xor_(edx, edx); // Clear EDX for unsigned division
  COMP->div(edx, eax, rBTemp); // Unsigned divide: EAX = EDX:EAX / rBTemp

  // Zero-extend result to 64 bits and store
  x86::Gp result = newGP64();
  COMP->mov(result.r32(), eax);
  COMP->mov(GPRPtr(instr.rd), result);
  COMP->jmp(end);

  // Zero divide case: rD = 0
  COMP->bind(setZero);
  x86::Gp zero = newGP64();
  COMP->xor_(zero, zero);
  COMP->mov(GPRPtr(instr.rd), zero);

  COMP->bind(end);

  if (instr.rc)
    J_ppuSetCR0(b, result);
}

// 
void PPCInterpreter::PPCInterpreterJIT_ecowx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
}

// Multiply High Word (x'7C00 0096')
void PPCInterpreter::PPCInterpreterJIT_mulhwx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp rATemp = newGP32();
  x86::Gp rBTemp = newGP32();
  x86::Gp result = newGP64();

  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->mov(rBTemp, GPRPtr(instr.rb));
  COMP->imul(result.r32(), rATemp, rBTemp); // Multiplication is signed.
  COMP->movsxd(result, result);
  COMP->mov(GPRPtr(instr.rd), result);

  if (instr.rc)
    J_ppuSetCR0(b, result);
}

// Multiply Low Doubleword (x'7C00 01D2')
void PPCInterpreter::PPCInterpreterJIT_mulldx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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

// Multiply Low Word (x'7C00 01D6')
void PPCInterpreter::PPCInterpreterJIT_mullwx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rATemp = newGP64();
  x86::Gp rBTemp = newGP64();

  // Load 32-bit values and sign-extend to 64-bit
  COMP->movsxd(rATemp, GPRPtr(instr.ra));
  COMP->movsxd(rBTemp, GPRPtr(instr.rb));

  // Multiply (signed)
  COMP->imul(rATemp, rBTemp);

  // Store result
  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// Multiply Low Immediate (x'1C00 0000')
void PPCInterpreter::PPCInterpreterJIT_mulli(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rATemp = newGP64();

  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->imul(rATemp, imm<s64>(instr.simm16));
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

// Negate
void PPCInterpreter::PPCInterpreterJIT_negx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rATemp = newGP64();

  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->neg(rATemp);
  COMP->mov(GPRPtr(instr.rs), rATemp);

  // _rc
  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
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

// Shift Right Algebraic Double Word (x'7C00 0634')
void PPCInterpreter::PPCInterpreterJIT_sradx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label shiftOver63 = COMP->newLabel();
  Label setCA = COMP->newLabel();
  Label end = COMP->newLabel();

  // Load rS (64-bit value) and rB (shift amount)
  x86::Gp rsTemp = newGP64();
  x86::Gp shift = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->mov(shift, GPRPtr(instr.rb));
  COMP->and_(shift, 127); // Mask to 7 bits

  // Load XER and clear CA bit
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));

#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Check if shift > 63 (bit 6 set in rB)
  COMP->cmp(shift, 63);
  COMP->ja(shiftOver63);

  // Normal shift (0-63)
  // Save original value for CA check
  x86::Gp original = newGP64();
  COMP->mov(original, rsTemp);

  // Perform arithmetic shift right on 64-bit value
  COMP->sar(rsTemp, shift);
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // Check for CA: if original < 0 and bits were shifted out
  COMP->test(original, original);
  COMP->jns(end); // If original >= 0, no CA

  // Reconstruct and compare to check if bits were lost
  x86::Gp reconstructed = newGP64();
  COMP->mov(reconstructed, rsTemp);
  COMP->shl(reconstructed, shift);
  COMP->cmp(reconstructed, original);
  COMP->jne(setCA);
  COMP->jmp(end);

  // Shift >= 64: result is 0 or -1 depending on sign
  COMP->bind(shiftOver63);
  COMP->mov(original, rsTemp); // Save for CA check
  COMP->sar(rsTemp, 63); // All sign bits
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // CA = 1 if original was negative
  COMP->test(original, original);
  COMP->jns(end);

  COMP->bind(setCA);
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

// Shift Right Algebraic Word (x'7C00 0630')
void PPCInterpreter::PPCInterpreterJIT_srawx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label shiftOver31 = COMP->newLabel();
  Label setCA = COMP->newLabel();
  Label end = COMP->newLabel();

  // Load rS (32-bit value) and rB (shift amount)
  x86::Gp rsTemp = newGP64();
  x86::Gp shift = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));
  COMP->mov(shift, GPRPtr(instr.rb));
  COMP->and_(shift, 63); // Mask to 6 bits

  // Load XER and clear CA bit
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));

#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Check if shift > 31
  COMP->cmp(shift, 31);
  COMP->ja(shiftOver31);

  // Normal shift (0-31)
  // Save original value for CA check
  x86::Gp original = newGP32();
  COMP->mov(original, rsTemp.r32());

  // Perform arithmetic shift right on 32-bit value
  COMP->sar(rsTemp.r32(), shift);

  // Sign-extend result to 64 bits
  COMP->movsxd(rsTemp, rsTemp.r32());
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // Check for CA: if original < 0 and bits were shifted out
  COMP->test(original, original);
  COMP->jns(end); // If original >= 0, no CA

  // Reconstruct and compare to check if bits were lost
  x86::Gp reconstructed = newGP32();
  COMP->mov(reconstructed, rsTemp.r32());
  COMP->shl(reconstructed, shift);
  COMP->cmp(reconstructed, original);
  COMP->jne(setCA);
  COMP->jmp(end);

  // Shift >= 32: result is 0 or -1 depending on sign
  COMP->bind(shiftOver31);
  COMP->mov(original, rsTemp.r32()); // Save for CA check
  COMP->sar(rsTemp.r32(), 31); // All sign bits
  COMP->movsxd(rsTemp, rsTemp.r32());
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // CA = 1 if original was negative
  COMP->test(original, original);
  COMP->jns(end);

  COMP->bind(setCA);
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

// Shift Right Algebraic Word Immediate (x'7C00 0670')
void PPCInterpreter::PPCInterpreterJIT_srawix(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label setCA = COMP->newLabel();
  Label end = COMP->newLabel();

  u32 sh = instr.sh32;

  // Load rS (32-bit value)
  x86::Gp rsTemp = newGP64();
  COMP->mov(rsTemp, GPRPtr(instr.rs));

  // Save original 32-bit value for CA check
  x86::Gp original = newGP32();
  COMP->mov(original, rsTemp.r32());

  // Load XER and clear CA bit
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));

#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Perform arithmetic shift right on 32-bit value
  COMP->sar(rsTemp.r32(), imm<u8>(sh));

  // Sign-extend result to 64 bits
  COMP->movsxd(rsTemp, rsTemp.r32());
  COMP->mov(GPRPtr(instr.ra), rsTemp);

  // Check for CA: if original < 0 and bits were shifted out
  COMP->test(original, original);
  COMP->jns(end); // If original >= 0, no CA

  // Reconstruct and compare to check if bits were lost
  x86::Gp reconstructed = newGP32();
  COMP->mov(reconstructed, rsTemp.r32());
  COMP->shl(reconstructed, imm<u8>(sh));
  COMP->cmp(reconstructed, original);
  COMP->jne(setCA);
  COMP->jmp(end);

  COMP->bind(setCA);
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

// Subtract From (x'7C00 0050')
void PPCInterpreter::PPCInterpreterJIT_subfx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {

  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));
  COMP->sub(rBTemp, GPRPtr(instr.ra));
  COMP->mov(GPRPtr(instr.rs), rBTemp);
  // RC
  if (instr.rc)
    J_ppuSetCR0(b, rBTemp);
}

// Subtract from Carrying (x'7C00 0010')
void PPCInterpreter::PPCInterpreterJIT_subfcx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value and complement it
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);

  // Get rB value
  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // XER[CA] Clear.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));

#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64();
  COMP->mov(tempMSR, SPRPtr(MSR));
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit mode.

  // Set Carry flag for +1
  COMP->stc();
  // Perform 32bit addition to check for carry: ~rA + rB + 1
  COMP->adc(rATemp.r32(), rBTemp.r32());
  // Get back the complemented value of rA
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);
  // Check for carry.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Set Carry flag for +1
  COMP->stc();
  // Perform the 64 bit Add: ~rA + rB + 1
  COMP->adc(rATemp, rBTemp);
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

// Subtract from Immediate Carrying (x'2000 0000')
void PPCInterpreter::PPCInterpreterJIT_subfic(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // TODO: Overflow Enable.
  Label end = COMP->newLabel(); // Self explanatory.
  Label sfBitMode = COMP->newLabel();

  // Get rA value.
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);

  // XER[CA] Clear.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63);  // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode);    // If set, use 64-bit add. (checks for carry flag from previous operation).
  // Set Carry flag
  COMP->stc();
  // Perform 32bit addition to check for carry.
  COMP->adc(rATemp.r32(), imm<s32>(instr.simm16));
  // Check for carry.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);

  // Get back the value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);
  // Set Carry flag
  COMP->stc();
  // Perform the 64 bit Add.
  COMP->adc(rATemp, imm<s64>(instr.simm16));
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
}

// Subtract from Extended (x'7C00 0110')
void PPCInterpreter::PPCInterpreterJIT_subfex(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // TODO: Overflow Enable.
  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value and complement it
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);

  // Get rB value
  x86::Gp rBTemp = newGP64();
  COMP->mov(rBTemp, GPRPtr(instr.rb));

  // Load XER and get CA bit into carry flag, then clear it.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag, then clear it.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN

  // Save the carry flag state before checking MSR.
  x86::Gp carryIn = newGP64();
  COMP->setc(carryIn.r8());

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit add.

  // Perform 32bit addition to check for carry.
  // Restore carry flag from saved state.
  COMP->bt(carryIn, 0);
  COMP->adc(rATemp.r32(), rBTemp.r32());
  // Get back the complemented value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);
  // Check for carry from 32-bit operation.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Restore carry flag from saved state for 64-bit operation.
  COMP->bt(carryIn, 0);
  // Perform the 64 bit Add with carry: ~rA + rB + CA
  COMP->adc(rATemp, rBTemp);
  // Check for carry from 64-bit operation.
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

// Subtract from Zero Extended (x'7C00 0190')
void PPCInterpreter::PPCInterpreterJIT_subfzex(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // TODO: Overflow Enable.
  Label end = COMP->newLabel();
  Label sfBitMode = COMP->newLabel();

  // Get rA value and complement it
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);

  // Load XER and get CA bit into carry flag, then clear it.
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag, then clear it.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN

  // Save the carry flag state before checking MSR.
  x86::Gp carryIn = newGP64();
  COMP->setc(carryIn.r8());

  // Check for 32bit mode of operation.
  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit add.

  // Perform 32bit addition to check for carry.
  // Restore carry flag from saved state.
  COMP->bt(carryIn, 0);
  COMP->adc(rATemp.r32(), imm<u32>(0));
  // Get back the complemented value of rA.
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);
  // Check for carry from 32-bit operation.
  COMP->jnc(sfBitMode);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(sfBitMode);
  // Restore carry flag from saved state for 64-bit operation.
  COMP->bt(carryIn, 0);
  // Perform the 64 bit Add with carry: ~rA + CA
  COMP->adc(rATemp, imm<u64>(0));
  // Check for carry from 64-bit operation.
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
  x86::Gp rSTemp = newGP64();
  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->or_(rSTemp, GPRPtr(instr.rb));
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
  x86::Gp tmp = newGP64();
  COMP->lzcnt(tmp, GPRPtr(instr.rs));
  COMP->mov(GPRPtr(instr.ra), tmp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, tmp);
}

// Count Leading Zeros Word (x'7C00 0034')
void PPCInterpreter::PPCInterpreterJIT_cntlzwx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp tmp = newGP32();
  COMP->lzcnt(tmp, GPRPtr(instr.rs));
  COMP->mov(GPRPtr(instr.ra), tmp);

  // RC
  if (instr.rc)
    J_ppuSetCR0(b, tmp);
}

// Condition Register AND
void PPCInterpreter::PPCInterpreterJIT_crand(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Bit position in CRBA
  u8 shiftCra = 31 - instr.crba;
  // Bit position in CRBB
  u8 shiftCrb = 31 - instr.crbb;
  // Bit position in CRBD
  u8 shiftCrd = 31 - instr.crbd;

  Label clearCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();

  COMP->mov(crData, CRValPtr());
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->jnc(clearCRD);
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->jnc(clearCRD);
  // Both bits are set, set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(clearCRD);
  // One bit is missing, clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register OR
void PPCInterpreter::PPCInterpreterJIT_cror(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Bit position in CRBA
  u8 shiftCra = 31 - instr.crba;
  // Bit position in CRBB
  u8 shiftCrb = 31 - instr.crbb;
  // Bit position in CRBD
  u8 shiftCrd = 31 - instr.crbd;

  Label setCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();

  COMP->mov(crData, CRValPtr());
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->jc(setCRD);
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->jc(setCRD);
  // No bits are set, clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(setCRD);
  // One bit is set, set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register NOR
void PPCInterpreter::PPCInterpreterJIT_crnor(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u8 shiftCra = 31 - instr.crba;
  u8 shiftCrb = 31 - instr.crbb;
  u8 shiftCrd = 31 - instr.crbd;

  Label clearCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();

  COMP->mov(crData, CRValPtr());
  // NOR: result is 1 only if both bits are 0
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->jc(clearCRD);  // If A is set, result is 0
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->jc(clearCRD);  // If B is set, result is 0
  // Both bits are clear, set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(clearCRD);
  // At least one bit is set, clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register AND with Complement
void PPCInterpreter::PPCInterpreterJIT_crandc(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u8 shiftCra = 31 - instr.crba;
  u8 shiftCrb = 31 - instr.crbb;
  u8 shiftCrd = 31 - instr.crbd;

  Label clearCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();

  COMP->mov(crData, CRValPtr());
  // ANDC: result is 1 if A is 1 AND B is 0
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->jnc(clearCRD);  // If A is clear, result is 0
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->jc(clearCRD);   // If B is set, result is 0
  // A is set and B is clear, set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(clearCRD);
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register XOR
void PPCInterpreter::PPCInterpreterJIT_crxor(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u8 shiftCra = 31 - instr.crba;
  u8 shiftCrb = 31 - instr.crbb;
  u8 shiftCrd = 31 - instr.crbd;

  Label setCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();
  x86::Gp bitA = newGP32();
  x86::Gp bitB = newGP32();

  COMP->mov(crData, CRValPtr());
  // Extract bit A
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->setc(bitA.r8());
  // Extract bit B
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->setc(bitB.r8());
  // XOR the two bits
  COMP->xor_(bitA, bitB);
  COMP->test(bitA, bitA);
  COMP->jnz(setCRD);
  // Result is 0, clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(setCRD);
  // Result is 1, set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register NAND
void PPCInterpreter::PPCInterpreterJIT_crnand(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u8 shiftCra = 31 - instr.crba;
  u8 shiftCrb = 31 - instr.crbb;
  u8 shiftCrd = 31 - instr.crbd;

  Label setCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();

  COMP->mov(crData, CRValPtr());
  // NAND: result is 0 only if both bits are 1
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->jnc(setCRD);  // If A is clear, result is 1
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->jnc(setCRD);  // If B is clear, result is 1
  // Both bits are set, clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(setCRD);
  // At least one bit is clear, set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register Equivalent (XNOR)
void PPCInterpreter::PPCInterpreterJIT_creqv(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u8 shiftCra = 31 - instr.crba;
  u8 shiftCrb = 31 - instr.crbb;
  u8 shiftCrd = 31 - instr.crbd;

  Label setCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();
  x86::Gp bitA = newGP32();
  x86::Gp bitB = newGP32();

  COMP->mov(crData, CRValPtr());
  // Extract bit A
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->setc(bitA.r8());
  // Extract bit B
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->setc(bitB.r8());
  // XNOR: result is 1 if both bits are the same (XOR then invert)
  COMP->xor_(bitA, bitB);
  COMP->test(bitA, bitA);
  COMP->jz(setCRD);
  // Result is 0 (bits differ), clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(setCRD);
  // Result is 1 (bits same), set CRBD
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Condition Register OR with Complement
void PPCInterpreter::PPCInterpreterJIT_crorc(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u8 shiftCra = 31 - instr.crba;
  u8 shiftCrb = 31 - instr.crbb;
  u8 shiftCrd = 31 - instr.crbd;

  Label setCRD = COMP->newLabel();
  Label end = COMP->newLabel();

  x86::Gp crData = newGP32();

  COMP->mov(crData, CRValPtr());
  // ORC: result is 1 if A is 1 OR B is 0
  COMP->bt(crData, imm<u8>(shiftCra));
  COMP->jc(setCRD);   // If A is set, result is 1
  COMP->bt(crData, imm<u8>(shiftCrb));
  COMP->jnc(setCRD);  // If B is clear, result is 1
  // A is clear and B is set, clear CRBD
  COMP->btr(crData, imm<u8>(shiftCrd));
  COMP->jmp(end);
  COMP->bind(setCRD);
  COMP->bts(crData, imm<u8>(shiftCrd));
  COMP->bind(end);
  COMP->mov(CRValPtr(), crData);
}

// Extend Sign Byte (x'7C00 0774')
void PPCInterpreter::PPCInterpreterJIT_extsbx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rSTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->movsx(rSTemp, rSTemp.r8()); // Sign-extend lower 8 bits to 64 bits.
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// Extend Sign Half Word (x'7C00 0734')
void PPCInterpreter::PPCInterpreterJIT_extshx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rSTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->movsx(rSTemp, rSTemp.r16()); // Sign-extend lower 16 bits to 64 bits.
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// Extend Sign Word (x'7C00 07B4')
void PPCInterpreter::PPCInterpreterJIT_extswx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rSTemp = newGP64();

  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->movsxd(rSTemp, rSTemp.r32()); // Sign-extend lower 32 bits to 64 bits.
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// Equivalent (x'7C00 0238')
void PPCInterpreter::PPCInterpreterJIT_eqvx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rSTemp = newGP64();
  COMP->mov(rSTemp, GPRPtr(instr.rs));
  COMP->xor_(rSTemp, GPRPtr(instr.rb));
  COMP->not_(rSTemp);
  COMP->mov(GPRPtr(instr.ra), rSTemp);

  if (instr.rc)
    J_ppuSetCR0(b, rSTemp);
}

// Multiply High Word Unsigned (x'7C00 0016')
void PPCInterpreter::PPCInterpreterJIT_mulhwux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rATemp = newGP64();
  x86::Gp rBTemp = newGP64();

  // Load 32-bit values as unsigned (zero-extend to 64-bit)
  COMP->mov(rATemp.r32(), GPRPtr(instr.ra));
  COMP->mov(rBTemp.r32(), GPRPtr(instr.rb));

  // Multiply: 32-bit * 32-bit = 64-bit result
  COMP->imul(rATemp, rBTemp);

  // Shift right by 32 to get high 32 bits
  COMP->shr(rATemp, 32);

  COMP->mov(GPRPtr(instr.rd), rATemp);

  if (instr.rc)
    J_ppuSetCR0(b, rATemp);
}

// Subtract from Minus One Extended (x'7C00 01D0')
void PPCInterpreter::PPCInterpreterJIT_subfmex(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label end = COMP->newLabel();

  // Get rA value and complement it
  x86::Gp rATemp = newGP64();
  COMP->mov(rATemp, GPRPtr(instr.ra));
  COMP->not_(rATemp);

  // Load XER and get CA bit into carry flag
  x86::Gp xer = newGP64();
  COMP->mov(xer, SPRPtr(XER));
#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Get CA bit of XER and store it in Carry flag, then clear it.
#else
  COMP->btr(xer, 2);
#endif // LITTLE_ENDIAN

  // Perform: ~rA + CA + (-1)
  COMP->adc(rATemp, imm<s64>(-1));

  // Check for carry out
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

#endif
