/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
void PPCInterpreter::PPCInterpreterJIT_b(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp CIA = newGP64();  // current instruction address
  x86::Gp target = newGP64();

  // target = (AA ? 0 : CIA) + EXTS(LI) << 2
  COMP->mov(CIA, CIAPtr());  // load runtime CIA

  int32_t offset = EXTS(instr.li, 24) << 2;
  if (instr.aa) {
    COMP->mov(target, offset);
  } else {
    COMP->mov(target, CIA);
    COMP->add(target, offset);
  }

  COMP->mov(NIAPtr(), target); // NIA = target

  if (instr.lk) {
    // LR = CIA + 4
    COMP->add(CIA, 4);
    COMP->mov(LRPtr(), CIA);
  }
}

// Branch Conditional
void PPCInterpreter::PPCInterpreterJIT_bc(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label fail = COMP->newLabel();
  Label done = COMP->newLabel();
  Label use64 = COMP->newLabel();

  // If BO[2] == 0 then CTR -= 1
  if ((instr.bo & 0x4) == 0) {
    x86::Gp ctr = newGP64();
    COMP->mov(ctr, SPRPtr(CTR));
    COMP->sub(ctr, imm<u64>(1));
    COMP->mov(SPRPtr(CTR), ctr);
  }

  // CTR check: if BO[2] set => ok, else check CTR vs zero and BO[1] for invert
  if (!(instr.bo & 0x4)) {
    x86::Gp ctr = newGP64();
    COMP->mov(ctr, SPRPtr(CTR));
    COMP->test(ctr, ctr);
    if (instr.bo & 0x2) {
      // BO[1] == 1 -> branch if CTR == 0
      COMP->jne(fail);
    }
    else {
      // BO[1] == 0 -> branch if CTR != 0
      COMP->je(fail);
    }
  }

  // CR condition: if BO[4] set (0x10), then unconditional on CR else evaluate CR[bi] == BO[3]
  if (!(instr.bo & 0x10)) {
    x86::Gp crVal = newGP32();
    x86::Gp tmp = newGP32();

    COMP->mov(crVal, CRValPtr());
    const u32 shift = 31 - instr.bi;
    COMP->mov(tmp, crVal);
    COMP->shr(tmp, imm(shift));
    COMP->and_(tmp, imm(1));

    if (instr.bo & 0x8) {
      // Expect CR bit == 1
      COMP->test(tmp, tmp);
      COMP->je(fail);
    } else {
      // Expect CR bit == 0
      COMP->test(tmp, tmp);
      COMP->jne(fail);
    }
  }

  // All good, compute target and set NIA.
  x86::Gp CIA = newGP64();
  x86::Gp target = newGP64();

  COMP->mov(CIA, CIAPtr());

  int32_t offset = EXTS(instr.ds, 14) << 2;
  if (instr.aa) {
  COMP->mov(target, offset);
  } else {
  COMP->mov(target, CIA);
  COMP->add(target, offset);
  }

  // Write NIA.
  COMP->mov(NIAPtr(), target);

  // Set LR if needed.
  if (instr.lk) {
  COMP->add(CIA, imm<u32>(4));
  COMP->mov(LRPtr(), CIA);
  }

  // Truncate NIA and LR to 32 bits if MSR.SF == 0 (32-bit mode).
  x86::Gp tmpMSR = newGP64();
  COMP->mov(tmpMSR, SPRPtr(MSR));
  COMP->bt(tmpMSR, 63); // test MSR.SF (bit 63)
  COMP->jc(use64);

  // 32-bit mode:
  x86::Gp tmp32 = newGP32();
  // Truncate NIA to 32 bits.
  COMP->mov(tmp32, NIAPtr());
  COMP->and_(tmp32, imm<u32>(0xFFFFFFFF));
  COMP->mov(NIAPtr(), tmp32);
  COMP->bind(use64);
  // Fail, do nothing.
  COMP->bind(fail);
}

// Branch Conditional to Count Register
void PPCInterpreter::PPCInterpreterJIT_bcctr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label condTrue = COMP->newLabel();
  Label condEnd = COMP->newLabel();
  Label use64 = COMP->newLabel();

  // If BO[4] set (unconditional on CR) -> condition true
  if (instr.bo & 0x10) {
    COMP->jmp(condTrue);
  } else {
    // Evaluate CR bit CR[bi]
    x86::Gp crVal = newGP32();
    x86::Gp tmp = newGP32();

    COMP->mov(crVal, CRValPtr()); // load CR value
    const u32 shift = 31 - instr.bi; // CR bit position mapping
    COMP->mov(tmp, crVal);
    COMP->shr(tmp, imm(shift)); // shift desired bit to LSB
    COMP->and_(tmp, imm(1));    // isolate bit 0

    // Desired value = (BO[3] == 1)
    if (instr.bo & 0x8) {
      // branch if cr bit == 1
      COMP->test(tmp, tmp);
      COMP->jne(condTrue);
    } else {
      // branch if cr bit == 0
      COMP->test(tmp, tmp);
      COMP->je(condTrue);
    }
  }

  // Condition false, do nothing.
  COMP->jmp(condEnd);

  // Condition true, go ahead.
  COMP->bind(condTrue);

  x86::Gp ctr = newGP64();
  COMP->mov(ctr, SPRPtr(CTR));
  COMP->and_(ctr, imm<u64>(~3ULL));
  COMP->mov(NIAPtr(), ctr);

  if (instr.lk) {
    x86::Gp CIA = newGP64();
    COMP->mov(CIA, CIAPtr());
    COMP->add(CIA, imm<u32>(4));
    COMP->mov(LRPtr(), CIA);
  }

  // Truncate NIA and LR to 32 bits if MSR.SF == 0 (32-bit mode).
  x86::Gp tmpMSR = newGP64();
  COMP->mov(tmpMSR, SPRPtr(MSR));
  COMP->bt(tmpMSR, 63);
  COMP->jc(use64);

  // 32-bit mode
  x86::Gp tmp32 = newGP32();
  COMP->mov(tmp32, NIAPtr());
  COMP->and_(tmp32, imm<u32>(0xFFFFFFFF));
  COMP->mov(NIAPtr(), tmp32);
  COMP->bind(use64);

  COMP->bind(condEnd);
}

#endif