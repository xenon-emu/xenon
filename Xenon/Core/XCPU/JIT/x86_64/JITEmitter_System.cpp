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

#endif