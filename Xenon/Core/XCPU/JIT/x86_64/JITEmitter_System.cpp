/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
void PPCInterpreter::PPCInterpreterJIT_mfspr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u32 sprNum = instr.spr;
  sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);
  u64 value = 0;

  x86::Gp rSValue = newGP64();

  switch (static_cast<eXenonSPR>(sprNum)) {
  case eXenonSPR::XER:
    COMP->mov(rSValue, SPRPtr(XER));
    break;
  case eXenonSPR::LR:
    COMP->mov(rSValue, SPRPtr(LR));
    break;
  case eXenonSPR::CTR:
    COMP->mov(rSValue, SPRPtr(CTR));
    break;
  case eXenonSPR::DSISR:
    COMP->mov(rSValue, SPRPtr(DSISR));
    break;
  case eXenonSPR::DAR:
    COMP->mov(rSValue, SPRPtr(DAR));
    break;
  case eXenonSPR::DEC:
    COMP->mov(rSValue, SPRPtr(DEC));
    break;
  case eXenonSPR::SDR1:
    COMP->mov(rSValue, SharedSPRPtr(SDR1));
    break;
  case eXenonSPR::SRR0:
    COMP->mov(rSValue, SPRPtr(SRR0));
    break;
  case eXenonSPR::SRR1:
    COMP->mov(rSValue, SPRPtr(SRR1));
    break;
  case eXenonSPR::CFAR:
    COMP->mov(rSValue, SPRPtr(CFAR));
    break;
  case eXenonSPR::CTRLRD:
    COMP->mov(rSValue, SharedSPRPtr(CTRL));
    break;
  case eXenonSPR::VRSAVE:
    COMP->mov(rSValue, SPRPtr(VRSAVE));
    break;
  case eXenonSPR::TBLRO:
    COMP->mov(rSValue, 0x00000000FFFFFFFF);
    COMP->and_(rSValue, SharedSPRPtr(TB));
    break;
  case eXenonSPR::TBURO:
    COMP->mov(rSValue, 0xFFFFFFFF00000000);
    COMP->and_(rSValue, SharedSPRPtr(TB));
    break;
  case eXenonSPR::SPRG0:
    COMP->mov(rSValue, SPRPtr(SPRG0));
    break;
  case eXenonSPR::SPRG1:
    COMP->mov(rSValue, SPRPtr(SPRG1));
    break;
  case eXenonSPR::SPRG2:
    COMP->mov(rSValue, SPRPtr(SPRG2));
    break;
  case eXenonSPR::SPRG3:
    COMP->mov(rSValue, SPRPtr(SPRG3));
    break;
  case eXenonSPR::PVR:
    COMP->mov(rSValue, SharedSPRPtr(PVR));
    break;
  case eXenonSPR::HSPRG0:
    COMP->mov(rSValue, SPRPtr(HSPRG0));
    break;
  case eXenonSPR::HSPRG1:
    COMP->mov(rSValue, SPRPtr(HSPRG1));
    break;
  case eXenonSPR::RMOR:
    COMP->mov(rSValue, SharedSPRPtr(RMOR));
    break;
  case eXenonSPR::HRMOR:
    COMP->mov(rSValue, SharedSPRPtr(HRMOR));
    break;
  case eXenonSPR::LPCR:
    COMP->mov(rSValue, SharedSPRPtr(LPCR));
    break;
  case eXenonSPR::TSCR:
    COMP->mov(rSValue, SharedSPRPtr(TSCR));
    break;
  case eXenonSPR::TTR:
    COMP->mov(rSValue, SharedSPRPtr(TTR));
    break;
  case eXenonSPR::PPE_TLB_Index_Hint:
    COMP->mov(rSValue, SPRPtr(PPE_TLB_Index_Hint));
    break;
  case eXenonSPR::HID0:
    COMP->mov(rSValue, SharedSPRPtr(HID0));
    break;
  case eXenonSPR::HID1:
    COMP->mov(rSValue, SharedSPRPtr(HID1));
    break;
  case eXenonSPR::HID4:
    COMP->mov(rSValue, SharedSPRPtr(HID4));
    break;
  case eXenonSPR::DABR:
    COMP->mov(rSValue, SPRPtr(DABR));
    break;
  case eXenonSPR::HID6:
    COMP->mov(rSValue, SharedSPRPtr(HID6));
    break;
  case eXenonSPR::PIR:
    COMP->mov(rSValue, SPRPtr(PIR));
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: {:#x}", ppeState->ppuName, static_cast<u8>(curThreadId), sprNum);
    break;
  }

  COMP->mov(GPRPtr(instr.rs), rSValue);
}

// Move from One Condition Register Field (x'7C20 0026') 
void PPCInterpreter::PPCInterpreterJIT_mfocrf(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Temp storage for the CR current value.
  x86::Gp crValue = newGP32();
  // Load CR value to temp storage.
  COMP->mov(crValue, CRValPtr());
  if (instr.l11) {
    // MFOCRF
    u32 crMask = 0;
    u32 bit = 0x80;
    u32 count = 0;

    for (; bit; bit >>= 1) {
      crMask <<= 4;
      if (instr.crm & bit) {
        crMask |= 0xF;
        count++;
      }
    }

    if (count == 1) {
      COMP->and_(crValue, crMask);
      COMP->mov(GPRPtr(instr.rd), crValue);
    } else {
      // Undefined behavior.
      COMP->mov(GPRPtr(instr.rd), imm<u64>(0));
    }
  } else {
    // MFCR
    COMP->mov(GPRPtr(instr.rd), crValue);
  }
}

// System Call
void PPCInterpreter::PPCInterpreterJIT_sc(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp exReg = newGP16();
  COMP->mov(exReg, EXPtr());
  COMP->or_(exReg, ppuSystemCallEx);
  COMP->mov(EXPtr(), exReg);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::exHVSysCall).Base(), imm<bool>(instr.lev & 1));
}

void PPCInterpreter::PPCInterpreterJIT_mftb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  const u32 spr = (instr.spr >> 5) | ((instr.spr & 0x1f) << 5);
  x86::Gp tbData = newGP64();
  COMP->mov(tbData, SharedSPRPtr(TB));

  if (spr == TBLRO) {
    COMP->mov(GPRPtr(instr.rd), tbData);
  } else { // TBURO
    COMP->shr(tbData, 32);
    COMP->mov(GPRPtr(instr.rd), tbData);
  }
}

// Return from interrupt doubleword
void PPCInterpreter::PPCInterpreterJIT_rfid(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp srr1 = newGP64();
  x86::Gp msr  = newGP64();
  x86::Gp tmp  = newGP64();

  Label skipHVMESet = COMP->newLabel();
  Label use64    = COMP->newLabel();

  // MSR[1-2,4-32,37-41,49-50,52-57,60-63] <- SRR1[1-2,4-32,37-41,49-50,52-57,60-63]
  COMP->mov(srr1, SPRPtr(SRR1));
  COMP->mov(msr, srr1);

  // MSR[0] <- SRR1[0] | SRR1[1]

  Label setMSRSF = COMP->newLabel();
  Label doneMSRSF = COMP->newLabel();

  COMP->bt(srr1, 63); // SRR1[0]
  COMP->jc(setMSRSF);
  COMP->bt(srr1, 62); // SRR1[1]
  COMP->jc(setMSRSF);
  // SRR1[0] || SRR1[1] == 0
  COMP->btr(msr, 63);
  COMP->jmp(doneMSRSF);
  COMP->bind(setMSRSF);
  COMP->bts(msr, 63);
  // Check was done and MSR was set.
  COMP->bind(doneMSRSF);

  // MSR[58] = SRR1[58] | SRR1[49]

  Label setMSRIR = COMP->newLabel();
  Label doneMSRIR = COMP->newLabel();

  COMP->bt(srr1, 5); // SRR1[58]
  COMP->jc(setMSRIR);
  COMP->bt(srr1, 14); // SRR1[49]
  COMP->jc(setMSRIR);
  // SRR1[58] || SRR1[49] == 0
  COMP->btr(msr, 5);
  COMP->jmp(doneMSRIR);
  COMP->bind(setMSRIR);
  COMP->bts(msr, 5);
  // Check was done and MSR was set.
  COMP->bind(doneMSRIR);

  // MSR[59] = SRR1[59] | SRR1[49]

  Label setMSRDR = COMP->newLabel();
  Label doneMSRDR = COMP->newLabel();

  COMP->bt(srr1, 4); // SRR1[59]
  COMP->jc(setMSRDR);
  COMP->bt(srr1, 14); // SRR1[49]
  COMP->jc(setMSRDR);
  // SRR1[58] || SRR1[49] == 0
  COMP->btr(msr, 4);
  COMP->jmp(doneMSRDR);
  COMP->bind(setMSRDR);
  COMP->bts(msr, 4);
  // Check was done and MSR was set.
  COMP->bind(doneMSRDR);

  // Check for HV bit in current MSR to skip setting HV and ME bits
  x86::Gp currentMSR = newGP64();
  COMP->mov(currentMSR, SPRPtr(MSR));
  COMP->bt(currentMSR, 60); // MSR.HV
  COMP->jnc(skipHVMESet);   // Jump if not in HV mode.

  Label skipMSRSF = COMP->newLabel();
  Label skipMSRME = COMP->newLabel();

  // Set MSR[HV]
  COMP->bt(srr1, 60); // SRR1[3]
  COMP->jnc(skipMSRSF);
  COMP->bts(msr, 60);
  COMP->bind(skipMSRSF);
  // Set MSR[ME]
  COMP->bt(srr1, 12); // SRR1[51]
  COMP->jnc(skipMSRME);
  COMP->bts(msr, 12);
  COMP->bind(skipMSRME);

  // Skip setting HV and ME bits if not in HV mode
  COMP->bind(skipHVMESet);
  // Store composed MSR
  COMP->mov(SPRPtr(MSR), msr);

  // NIA <- SRR0 & ~3
  x86::Gp srr0 = newGP64();
  x86::Gp nia  = newGP64();
  COMP->mov(srr0, SPRPtr(SRR0));
  COMP->mov(nia, srr0);
  COMP->and_(nia, imm<u64>(~3ULL));
  COMP->mov(NIAPtr(), nia);

  // If MSR.SF == 0 (32-bit mode), truncate NIA to 32 bits
  COMP->bt(msr, 63);  // MSR.SF
  COMP->jc(use64);    // if set -> 64-bit mode, skip truncation
  COMP->and_(nia, imm<u32>(0xFFFFFFFF));
  COMP->mov(NIAPtr(), nia);

  COMP->bind(use64);
}

// Data Cache Block Zero
void PPCInterpreter::PPCInterpreterJIT_dcbz(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Instruction Synchronize
void PPCInterpreter::PPCInterpreterJIT_isync(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Synchronize
void PPCInterpreter::PPCInterpreterJIT_sync(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Flush
void PPCInterpreter::PPCInterpreterJIT_dcbf(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Invalidate
void PPCInterpreter::PPCInterpreterJIT_dcbi(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Touch
void PPCInterpreter::PPCInterpreterJIT_dcbt(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Store
void PPCInterpreter::PPCInterpreterJIT_dcbst(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Touch for Store
void PPCInterpreter::PPCInterpreterJIT_dcbtst(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

// Instruction Cache Block Invalidate
void PPCInterpreter::PPCInterpreterJIT_icbi(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Do nothing
}

#endif