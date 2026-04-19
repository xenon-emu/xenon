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

void PPCInterpreter::PPCInterpreterJIT_mtspr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  u32 spr = instr.spr;
  spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F);

  x86::Gp value = newGP64();
  COMP->mov(value, GPRPtr(instr.rs));

  switch (static_cast<eXenonSPR>(spr)) {
  case eXenonSPR::XER: {
    COMP->and_(value, imm<u64>(0xE000007F));
    COMP->mov(SPRPtr(XER), value);
    break;
  }
  case eXenonSPR::LR:
    COMP->mov(SPRPtr(LR), value);
    break;
  case eXenonSPR::CTR:
    COMP->mov(SPRPtr(CTR), value);
    break;
  case eXenonSPR::DSISR:
    COMP->mov(SPRPtr(DSISR), value);
    break;
  case eXenonSPR::DAR:
    COMP->mov(SPRPtr(DAR), value);
    break;
  case eXenonSPR::DEC:
    COMP->mov(SPRPtr(DEC), value.r32());
    break;
  case eXenonSPR::SDR1:
    COMP->mov(SharedSPRPtr(SDR1), value);
    break;
  case eXenonSPR::SRR0:
    COMP->mov(SPRPtr(SRR0), value);
    break;
  case eXenonSPR::SRR1:
    COMP->mov(SPRPtr(SRR1), value);
    break;
  case eXenonSPR::CFAR:
    COMP->mov(SPRPtr(CFAR), value);
    break;
  case eXenonSPR::VRSAVE:
    COMP->mov(SPRPtr(VRSAVE), value.r32());
    break;
  case eXenonSPR::SPRG0:
    COMP->mov(SPRPtr(SPRG0), value);
    break;
  case eXenonSPR::SPRG1:
    COMP->mov(SPRPtr(SPRG1), value);
    break;
  case eXenonSPR::SPRG2:
    COMP->mov(SPRPtr(SPRG2), value);
    break;
  case eXenonSPR::SPRG3:
    COMP->mov(SPRPtr(SPRG3), value);
    break;
  case eXenonSPR::TBLWO:
    COMP->mov(SharedSPRPtr(TB), value.r32());
    break;
  case eXenonSPR::TBUWO: {
    x86::Gp tb = newGP64();
    COMP->mov(tb, SharedSPRPtr(TB));
    COMP->and_(tb, imm<u64>(0x00000000FFFFFFFFull));
    x86::Gp hi = newGP64();
    COMP->mov(hi, value);
    COMP->shl(hi, 32);
    COMP->or_(tb, hi);
    COMP->mov(SharedSPRPtr(TB), tb);
    break;
  }
  case eXenonSPR::HSPRG0:
    COMP->mov(SPRPtr(HSPRG0), value);
    break;
  case eXenonSPR::HSPRG1:
    COMP->mov(SPRPtr(HSPRG1), value);
    break;
  case eXenonSPR::HDEC:
    COMP->mov(SharedSPRPtr(HDEC), value.r32());
    break;
  case eXenonSPR::RMOR:
    COMP->mov(SharedSPRPtr(RMOR), value);
    break;
  case eXenonSPR::HRMOR:
    COMP->mov(SharedSPRPtr(HRMOR), value);
    break;
  case eXenonSPR::LPCR:
    COMP->mov(SharedSPRPtr(LPCR), value);
    break;
  case eXenonSPR::LPIDR:
    COMP->mov(SharedSPRPtr(LPIDR), value.r32());
    break;
  case eXenonSPR::TSCR:
    COMP->mov(SharedSPRPtr(TSCR), value.r32());
    break;
  case eXenonSPR::TTR:
    COMP->mov(SharedSPRPtr(TTR), value);
    break;
  case eXenonSPR::PPE_TLB_Index:
    COMP->mov(SharedSPRPtr(PPE_TLB_Index), value);
    break;
  case eXenonSPR::PPE_TLB_Index_Hint:
    COMP->mov(SPRPtr(PPE_TLB_Index_Hint), value);
    break;
  case eXenonSPR::PPE_TLB_RPN:
    COMP->mov(SharedSPRPtr(PPE_TLB_RPN), value);
    break;
  case eXenonSPR::HID0:
    COMP->mov(SharedSPRPtr(HID0), value);
    break;
  case eXenonSPR::HID1:
    COMP->mov(SharedSPRPtr(HID1), value);
    break;
  case eXenonSPR::HID4:
    COMP->mov(SharedSPRPtr(HID4), value);
    break;
  case eXenonSPR::HID6:
    COMP->mov(SharedSPRPtr(HID6), value);
    break;
  case eXenonSPR::DABR:
    COMP->mov(SPRPtr(DABR), value);
    break;
  case eXenonSPR::DABRX:
    COMP->mov(SPRPtr(DABRX), value);
    break;
  case eXenonSPR::PPE_TLB_VPN: {
    COMP->mov(SharedSPRPtr(PPE_TLB_VPN), value);

    // Call helper: mmuAddTlbEntry(ppeState)
    InvokeNode *call = nullptr;
    Xe::JITCompat::Invoke(b->compiler, call, imm((void*)mmuAddTlbEntry), FuncSignature::build<void, sPPEState *>());
    Xe::JITCompat::SetArg(call, 0, b->ppeState->Base());
    break;
  }
  case eXenonSPR::CTRLWR: {
    // Fastest safe route: helper trampoline.
    InvokeNode *call = nullptr;
    Xe::JITCompat::Invoke(b->compiler, call, imm((void*)PPCInterpreter::PPCInterpreter_mtspr), FuncSignature::build<void, sPPEState *>());
    Xe::JITCompat::SetArg(call, 0, b->ppeState->Base());
    break;
  }
  default:
    LOG_ERROR(Xenon, "JIT mtspr: unhandled SPR 0x{:X}", spr);
    break;
  }
}

// Move from One Condition Register Field (x'7C20 0026')
void PPCInterpreter::PPCInterpreterJIT_mfocrf(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
  COMP->or_(exReg, imm<u16>(ppuSystemCallEx));
  COMP->mov(EXPtr(), exReg);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::exHVSysCall).Ptr(), imm<bool>(instr.lev & 1));
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

void PPCInterpreter::PPCInterpreterJIT_mfmsr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp value = newGP64();
  COMP->mov(value, SPRPtr(MSR));
  COMP->mov(GPRPtr(instr.rd), value);
}

void PPCInterpreter::PPCInterpreterJIT_mtmsr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp value = newGP64();
  COMP->mov(value, GPRPtr(instr.rs));
  COMP->mov(SPRPtr(MSR), value);
}

void PPCInterpreter::PPCInterpreterJIT_mtmsrd(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rsValue = newGP64();
  COMP->mov(rsValue, GPRPtr(instr.rs));

  if (instr.l15) {
    // Bits 48 and 62 of RS are placed into the corresponding bits of MSR.
    // Remaining bits are unchanged.

    x86::Gp msr = newGP64();
    COMP->mov(msr, SPRPtr(MSR));

    // Clear MSR[EE] and MSR[RI] first.
    // EE = bit 48, RI = bit 62
    // Using your bit numbering in the interpreter:
    // EE mask = 0x8000
    // RI mask = 0x2
    COMP->and_(msr, imm<u64>(~(0x8000ull | 0x2ull)));

    x86::Gp temp = newGP64();
    COMP->mov(temp, rsValue);
    COMP->and_(temp, imm<u64>(0x8000ull | 0x2ull));
    COMP->or_(msr, temp);

    COMP->mov(SPRPtr(MSR), msr);
  } else {
    // Full write with derived bit fixups.
    x86::Gp msr = newGP64();
    COMP->mov(msr, rsValue);

    // --- SF = RS[0] | RS[1]
    {
      Label sfDone = newLabel();
      Label sfSet  = newLabel();

      // Clear SF bit first.
      COMP->btr(msr, 63);

      x86::Gp temp = newGP64();
      COMP->mov(temp, rsValue);
      COMP->test(temp, imm<u64>(0x8000000000000000ull | 0x4000000000000000ull));
      COMP->jnz(sfSet);
      COMP->jmp(sfDone);

      COMP->bind(sfSet);
      COMP->bts(msr, 63);

      COMP->bind(sfDone);
    }

    // --- EE = RS[48] | RS[49]
    {
      Label eeDone = newLabel();
      Label eeSet  = newLabel();

      // Clear EE bit first.
      COMP->btr(msr, 15);

      x86::Gp temp = newGP64();
      COMP->mov(temp, rsValue);
      COMP->test(temp, imm<u64>(0x8000ull | 0x4000ull));
      COMP->jnz(eeSet);
      COMP->jmp(eeDone);

      COMP->bind(eeSet);
      COMP->bts(msr, 15);

      COMP->bind(eeDone);
    }

    // --- IR = RS[58] | RS[49]
    {
      Label irDone = newLabel();
      Label irSet  = newLabel();

      // Clear IR bit first.
      COMP->btr(msr, 5);

      x86::Gp temp = newGP64();
      COMP->mov(temp, rsValue);
      COMP->test(temp, imm<u64>(0x20ull | 0x4000ull));
      COMP->jnz(irSet);
      COMP->jmp(irDone);

      COMP->bind(irSet);
      COMP->bts(msr, 5);

      COMP->bind(irDone);
    }

    // --- DR = RS[59] | RS[49]
    {
      Label drDone = newLabel();
      Label drSet  = newLabel();

      // Clear DR bit first.
      COMP->btr(msr, 4);

      x86::Gp temp = newGP64();
      COMP->mov(temp, rsValue);
      COMP->test(temp, imm<u64>(0x10ull | 0x4000ull));
      COMP->jnz(drSet);
      COMP->jmp(drDone);

      COMP->bind(drSet);
      COMP->bts(msr, 4);

      COMP->bind(drDone);
    }

    COMP->mov(SPRPtr(MSR), msr);
  }
}

void PPCInterpreter::PPCInterpreterJIT_slbmte(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rsVal = newGP64();
  x86::Gp rbVal = newGP64();
  x86::Gp vsid  = newGP64();
  x86::Gp esid  = newGP64();
  x86::Gp tmp   = newGP64();
  x86::Gp index = newGP32();
  x86::Gp bit   = newGP32();
  x86::Gp vbit  = newGP32();
  x86::Gp lp    = newGP32();

  COMP->mov(rsVal, GPRPtr(instr.rs));
  COMP->mov(rbVal, GPRPtr(instr.rb));

  // Index = QGET(rb, 52, 63) = low 12 bits
  COMP->mov(index, rbVal.r32());
  COMP->and_(index, imm<u32>(0xFFF));

  auto slb = b->threadCtx->array(&sPPUThread::SLB);

  x86::Gp entryPtr = newGP64();
  COMP->lea(entryPtr, slb.Ptr(0));

  COMP->mov(tmp.r32(), index.r32());
  if (sizeof(sSLBEntry) == 64) {
    COMP->shl(tmp, 6);
  } else {
    COMP->imul(tmp, imm<s32>(sizeof(sSLBEntry)));
  }
  COMP->add(entryPtr, tmp);

  // VSID = QGET(rs, 0, 51) = rs >> 12
  COMP->mov(vsid, rsVal);
  COMP->shr(vsid, 12);
  // then VSID <<= 28
  COMP->shl(vsid, 28);

  // ESID = QGET(rb, 0, 35) = rb >> 28
  COMP->mov(esid, rbVal);
  COMP->shr(esid, 28);

  // V = QGET(rb, 36, 36) = bit 27
  COMP->mov(vbit, rbVal.r32());
  COMP->shr(vbit, 27);
  COMP->and_(vbit, 1);

  // Store core fields
  COMP->mov(x86::qword_ptr(entryPtr, offsetof(sSLBEntry, ESID)), esid);
  COMP->mov(x86::qword_ptr(entryPtr, offsetof(sSLBEntry, VSID)), vsid);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, V)), vbit.r8());

  // Ks = QGET(rs, 52, 52) = bit 11
  COMP->mov(bit, rsVal.r32());
  COMP->shr(bit, 11);
  COMP->and_(bit, 1);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, Ks)), bit.r8());

  // Kp = QGET(rs, 53, 53) = bit 10
  COMP->mov(bit, rsVal.r32());
  COMP->shr(bit, 10);
  COMP->and_(bit, 1);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, Kp)), bit.r8());

  // N = QGET(rs, 54, 54) = bit 9
  COMP->mov(bit, rsVal.r32());
  COMP->shr(bit, 9);
  COMP->and_(bit, 1);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, N)), bit.r8());

  // L = QGET(rs, 55, 55) = bit 8
  COMP->mov(bit, rsVal.r32());
  COMP->shr(bit, 8);
  COMP->and_(bit, 1);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, L)), bit.r8());

  // C = QGET(rs, 56, 56) = bit 7
  COMP->mov(bit, rsVal.r32());
  COMP->shr(bit, 7);
  COMP->and_(bit, 1);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, C)), bit.r8());

  // LP = QGET(rs, 57, 59) = bits 6:4
  COMP->mov(lp, rsVal.r32());
  COMP->shr(lp, 4);
  COMP->and_(lp, 0x7);
  COMP->mov(x86::byte_ptr(entryPtr, offsetof(sSLBEntry, LP)), lp.r8());

  // Save original regs
  COMP->mov(x86::qword_ptr(entryPtr, offsetof(sSLBEntry, vsidReg)), rsVal);
  COMP->mov(x86::qword_ptr(entryPtr, offsetof(sSLBEntry, esidReg)), rbVal);

  // Translation-visible state changed
  COMP->ret();
}

void PPCInterpreter::PPCInterpreterJIT_slbie(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rbVal = newGP64();
  x86::Gp esid = newGP64();
  x86::Gp cbit = newGP32();
  x86::Gp i = newGP32();
  x86::Gp base = newGP64();
  x86::Gp ptr = newGP64();
  x86::Gp tmp = newGP64();
  x86::Gp entryC = newGP32();
  x86::Gp entryEsid = newGP64();

  auto slb = b->threadCtx->array(&sPPUThread::SLB);

  COMP->mov(rbVal, GPRPtr(instr.rb));

  // ESID = QGET(rb, 0, 35) = rb >> 28
  COMP->mov(esid, rbVal);
  COMP->shr(esid, 28);

  // C = QGET(rb, 36, 36) = bit 27
  COMP->mov(cbit, rbVal.r32());
  COMP->shr(cbit, 27);
  COMP->and_(cbit, 1);

  // base = &curThread.SLB[0]
  COMP->lea(base, slb.Ptr(0));
  COMP->xor_(i, i);

  Label loop = newLabel();
  Label next = newLabel();
  Label done = newLabel();

  COMP->bind(loop);
  COMP->cmp(i, imm(SLB_ENTRY_COUNT));
  COMP->jge(done);

  // ptr = &SLB[i]
  COMP->mov(ptr, base);
  COMP->mov(tmp, i);
  COMP->imul(tmp, imm<int>(sizeof(sSLBEntry)));
  COMP->add(ptr, tmp);

  // if (!slbEntry.V) continue;
  COMP->cmp(x86::byte_ptr(ptr, offsetof(sSLBEntry, V)), 0);
  COMP->je(next);

  // if (slbEntry.C != C) continue;
  COMP->movzx(entryC, x86::byte_ptr(ptr, offsetof(sSLBEntry, C)));
  COMP->cmp(entryC, cbit);
  COMP->jne(next);

  // if (slbEntry.ESID != ESID) continue;
  COMP->mov(entryEsid, x86::qword_ptr(ptr, offsetof(sSLBEntry, ESID)));
  COMP->cmp(entryEsid, esid);
  COMP->jne(next);

  // slbEntry.V = false;
  COMP->mov(x86::byte_ptr(ptr, offsetof(sSLBEntry, V)), 0);

  COMP->bind(next);
  COMP->inc(i);
  COMP->jmp(loop);

  COMP->bind(done);

  EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::iERAT).Ptr());
  EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::dERAT).Ptr());
}

// Return from interrupt doubleword
void PPCInterpreter::PPCInterpreterJIT_rfid(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp srr1 = newGP64();
  x86::Gp msr = newGP64();
  x86::Gp tmp = newGP64();

  Label skipHVMESet = newLabel();
  Label use64 = newLabel();

  // MSR[1-2,4-32,37-41,49-50,52-57,60-63] <- SRR1[1-2,4-32,37-41,49-50,52-57,60-63]
  COMP->mov(srr1, SPRPtr(SRR1));
  COMP->mov(msr, srr1);

  // MSR[0] <- SRR1[0] | SRR1[1]

  Label setMSRSF = newLabel();
  Label doneMSRSF = newLabel();

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

  Label setMSRIR = newLabel();
  Label doneMSRIR = newLabel();

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

  Label setMSRDR = newLabel();
  Label doneMSRDR = newLabel();

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

  Label skipMSRSF = newLabel();
  Label skipMSRME = newLabel();

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
void PPCInterpreter::PPCInterpreterJIT_dcbz(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Instruction Synchronize
void PPCInterpreter::PPCInterpreterJIT_isync(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Synchronize
void PPCInterpreter::PPCInterpreterJIT_sync(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Flush
void PPCInterpreter::PPCInterpreterJIT_dcbf(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Invalidate
void PPCInterpreter::PPCInterpreterJIT_dcbi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Touch
void PPCInterpreter::PPCInterpreterJIT_dcbt(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Store
void PPCInterpreter::PPCInterpreterJIT_dcbst(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Data Cache Block Touch for Store
void PPCInterpreter::PPCInterpreterJIT_dcbtst(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Instruction Cache Block Invalidate
void PPCInterpreter::PPCInterpreterJIT_icbi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

// Enforce In Order Execution of IO
void PPCInterpreter::PPCInterpreterJIT_eieio(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

#endif