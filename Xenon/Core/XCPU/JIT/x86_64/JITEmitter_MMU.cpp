/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#include "Core/XCPU/PPU/PPCInternal.h"
#include "Core/XCPU/XenonCPU.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

static inline x86::Gp EmitMMUGetCompareMask(JITBlockBuilder *b, x86::Gp p) {
  x86::Gp mask = newGP64();

  Label is64k = newLabel();
  Label is16m = newLabel();
  Label done = newLabel();

  COMP->cmp(p.r32(), imm<u32>(MMU_PAGE_SIZE_64KB));
  COMP->je(is64k);
  COMP->cmp(p.r32(), imm<u32>(MMU_PAGE_SIZE_16MB));
  COMP->je(is16m);

  // default: 4KB
  COMP->mov(mask, imm<u64>(TLB_COMPARE_MASK_4KB));
  COMP->jmp(done);

  COMP->bind(is64k);
  COMP->mov(mask, imm<u64>(TLB_COMPARE_MASK_64KB));
  COMP->jmp(done);

  COMP->bind(is16m);
  COMP->mov(mask, imm<u64>(TLB_COMPARE_MASK_16MB));

  COMP->bind(done);
  return mask;
}

static inline x86::Gp EmitLoadHID6LB(JITBlockBuilder *b) {
  x86::Gp hid6 = newGP64();
  x86::Gp lb   = newGP32();

  COMP->mov(hid6, SharedSPRStruct(HID6).scalar(&uHID6::hexValue));

  // LB = bits 44..47 of HID6
  // On little-endian host:
  //  - low dword = bits 0..31
  //  - so bits 44..47 -> bits 12..15 of low dword after truncation
  COMP->mov(lb, hid6.r32());
  COMP->shr(lb, 12);
  COMP->and_(lb, imm<u32>(0xF));

  return lb;
}

static inline x86::Gp EmitMMUGetPageSize(JITBlockBuilder* b, x86::Gp Lbit, x86::Gp LPbit) {
  x86::Gp p = newGP32();
  x86::Gp sel = newGP32();

  Label large = newLabel();
  Label lp1 = newLabel();
  Label set16m = newLabel();
  Label set1m  = newLabel();
  Label set64k = newLabel();
  Label done = newLabel();
  Label sel_ready  = newLabel();

  COMP->mov(p, imm<u32>(MMU_PAGE_SIZE_4KB));

  COMP->test(Lbit.r32(), Lbit.r32());
  COMP->jnz(large);
  COMP->jmp(done);

  COMP->bind(large);

  x86::Gp lb = EmitLoadHID6LB(b);

  COMP->mov(sel, lb);
  COMP->test(LPbit.r32(), LPbit.r32());
  COMP->jnz(lp1);

  COMP->and_(sel, imm<u32>(0b1100));
  COMP->shr(sel, 2);
  COMP->jmp(sel_ready);

  COMP->bind(lp1);
  COMP->and_(sel, imm<u32>(0b0011));

  COMP->bind(sel_ready);
  COMP->cmp(sel, imm<u32>(0));
  COMP->je(set16m);
  COMP->cmp(sel, imm<u32>(1));
  COMP->je(set1m);
  COMP->cmp(sel, imm<u32>(2));
  COMP->je(set64k);

  // sel == 3 -> 4KB
  COMP->mov(p, imm<u32>(MMU_PAGE_SIZE_4KB));
  COMP->jmp(done);

  COMP->bind(set16m);
  COMP->mov(p, imm<u32>(MMU_PAGE_SIZE_16MB));
  COMP->jmp(done);

  COMP->bind(set1m);
  COMP->mov(p, imm<u32>(MMU_PAGE_SIZE_1MB));
  COMP->jmp(done);

  COMP->bind(set64k);
  COMP->mov(p, imm<u32>(MMU_PAGE_SIZE_64KB));

  COMP->bind(done);
  return p;
}

static inline x86::Gp EmitMMUComputeTLBIndex(JITBlockBuilder *b, x86::Gp VA, x86::Gp p) {
  x86::Gp bits36_39 = newGP32();
  x86::Gp bits40_43 = newGP32();
  x86::Gp bits44_47 = newGP32();
  x86::Gp bits48_51 = newGP32();
  x86::Gp idx = newGP32();

  Label is64k = newLabel();
  Label is16m = newLabel();
  Label done = newLabel();

  // bits36_39 = (VA >> 24) & 0xF
  COMP->mov(bits36_39.r32(), VA.r32());
  COMP->shr(bits36_39, 24);
  COMP->and_(bits36_39, 0xF);

  // bits40_43 = (VA >> 20) & 0xF
  COMP->mov(bits40_43.r32(), VA.r32());
  COMP->shr(bits40_43, 20);
  COMP->and_(bits40_43, 0xF);

  // bits44_47 = (VA >> 16) & 0xF
  COMP->mov(bits44_47.r32(), VA.r32());
  COMP->shr(bits44_47, 16);
  COMP->and_(bits44_47, 0xF);

  // bits48_51 = (VA >> 12) & 0xF
  COMP->mov(bits48_51.r32(), VA.r32());
  COMP->shr(bits48_51, 12);
  COMP->and_(bits48_51, 0xF);

  COMP->cmp(p, imm<u32>(MMU_PAGE_SIZE_64KB));
  COMP->je(is64k);
  COMP->cmp(p, imm<u32>(MMU_PAGE_SIZE_16MB));
  COMP->je(is16m);

  // default 4KB: ((bits36_39 ^ bits44_47) << 4) | bits48_51
  COMP->mov(idx.r32(), bits36_39.r32());
  COMP->xor_(idx, bits44_47);
  COMP->shl(idx, 4);
  COMP->or_(idx, bits48_51);
  COMP->jmp(done);

  COMP->bind(is64k);
  // 64KB: ((bits36_39 ^ bits40_43) << 4) | bits44_47
  COMP->mov(idx.r32(), bits36_39.r32());
  COMP->xor_(idx, bits40_43);
  COMP->shl(idx, 4);
  COMP->or_(idx, bits44_47);
  COMP->jmp(done);

  COMP->bind(is16m);
  // 16MB: (VA >> 24) & 0xFF
  COMP->mov(idx.r32(), VA.r32());
  COMP->shr(idx, 24);
  COMP->and_(idx, 0xFF);

  COMP->bind(done);
  return idx;
}

static void QueueInvalidateRange(sPPEState *ppeState, u64 start, u64 end) {
  ppeState->jit.pending.push_back({start, end, false});
}

static void QueueInvalidateAll(sPPEState *ppeState) {
  ppeState->jit.hasFullFlush = true;
}

// TLB Synchronize
void PPCInterpreter::PPCInterpreterJIT_tlbsync(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Do nothing
}

void PPCInterpreter::PPCInterpreterJIT_slbia(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp i = newGP32();
  x86::Gp base = newGP64();
  x86::Gp ptr = newGP64();
  x86::Gp tmp = newGP64();

  auto slb = b->threadCtx->array(&sPPUThread::SLB);

  COMP->lea(base, slb.Ptr(0));
  COMP->xor_(i, i);

  Label loop = newLabel();
  Label done = newLabel();

  COMP->bind(loop);
  COMP->cmp(i, imm(SLB_ENTRY_COUNT));
  COMP->jge(done);

  COMP->mov(ptr, base);
  COMP->mov(tmp.r32(), i.r32());
  COMP->imul(tmp, imm<s32>(sizeof(sSLBEntry)));
  COMP->add(ptr, tmp);

  COMP->mov(x86::byte_ptr(ptr, offsetof(sSLBEntry, V)), 0);

  COMP->inc(i);
  COMP->jmp(loop);

  COMP->bind(done);

  EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::iERAT).Ptr());
  EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::dERAT).Ptr());

  COMP->ret();
}

void PPCInterpreter::PPCInterpreterJIT_tlbie(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rbVal = newGP64();
  x86::Gp LPbit = newGP32();
  x86::Gp Lbit = newGP32();
  x86::Gp p = newGP32();
  x86::Gp pageSize = newGP64();
  x86::Gp pageMask = newGP64();
  x86::Gp invMask  = newGP64();
  x86::Gp pageBase = newGP64();
  x86::Gp end = newGP64();

  COMP->mov(rbVal, GPRPtr(instr.rb));

  // LP = (rb & 0x1000) >> 12
  COMP->mov(LPbit, rbVal.r32());
  COMP->shr(LPbit, 12);
  COMP->and_(LPbit, 1);

  // L comes from instruction bit l10, matching interpreter call
  COMP->mov(Lbit, imm<u32>(instr.l10 ? 1 : 0));

  p = EmitMMUGetPageSize(b, Lbit, LPbit);

  // pageSize = 1ULL << p
  COMP->and_(p, imm<u32>(63));
  COMP->mov(pageSize, imm<u64>(1));
  x86::Gp shift = newGP32();
  COMP->mov(shift.r32(), p.r32());
  COMP->mov(x86::ecx, shift.r32());
  COMP->shl(pageSize, x86::cl);

  // pageMask = pageSize - 1
  COMP->mov(pageMask, pageSize);
  COMP->dec(pageMask);

  // pageBase = rbVal & ~pageMask
  COMP->mov(invMask, pageMask);
  COMP->not_(invMask);
  COMP->mov(pageBase, rbVal);
  COMP->and_(pageBase, invMask);

  EmitLRUCacheInvalidateElement(b, b->threadCtx->substruct(&sPPUThread::iERAT).Ptr(), pageBase);
  EmitLRUCacheInvalidateElement(b, b->threadCtx->substruct(&sPPUThread::dERAT).Ptr(), pageBase);

  COMP->mov(end, pageBase);
  COMP->add(end, pageSize);

  InvokeNode *inv = nullptr;
  Xe::JITCompat::Invoke(b->compiler, inv, imm((void *)QueueInvalidateRange), FuncSignature::build<void, sPPEState *, u64, u64>());
  Xe::JITCompat::SetArg(inv, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(inv, 1, pageBase);
  Xe::JITCompat::SetArg(inv, 2, end);

  COMP->ret();
}

void PPCInterpreter::PPCInterpreterJIT_tlbiel(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp rbVal = newGP64();
  x86::Gp LPbit = newGP32();
  x86::Gp invalSelector = newGP32();
  x86::Gp Lbit = newGP32();
  x86::Gp p = newGP32();

  COMP->mov(rbVal, GPRPtr(instr.rb));

  // LP = (rb >> 12) & 1
  COMP->mov(LPbit, rbVal.r32());
  COMP->shr(LPbit, 12);
  COMP->and_(LPbit, 1);

  // invalSelector = (rb >> 11) & 1
  COMP->mov(invalSelector, rbVal.r32());
  COMP->shr(invalSelector, 11);
  COMP->and_(invalSelector, 1);

  COMP->mov(Lbit, imm<u32>(instr.l10 ? 1 : 0));
  p = EmitMMUGetPageSize(b, Lbit, LPbit);

  Label selectivePath = newLabel();
  Label classPath = newLabel();
  Label done = newLabel();

  COMP->test(invalSelector, invalSelector);
  COMP->jnz(classPath);
  COMP->jmp(selectivePath);

  //
  // Selective invalidation
  //
  COMP->bind(selectivePath);
  {
    x86::Gp compareMask = EmitMMUGetCompareMask(b, p);
    x86::Gp tlbIndex = EmitMMUComputeTLBIndex(b, rbVal, p);

    x86::Gp classBase = newGP64();
    x86::Gp wayPtr = newGP64();
    x86::Gp off = newGP64();
    x86::Gp way = newGP32();

    x86::Gp vpn = newGP64();
    x86::Gp maskedVPN = newGP64();
    x86::Gp maskedRB = newGP64();

    x86::Gp pageSize = newGP64();
    x86::Gp pageMask = newGP64();
    x86::Gp invMask = newGP64();
    x86::Gp start = newGP64();
    x86::Gp end = newGP64();

    auto tlbClasses = b->ppeState->substruct(&sPPEState::TLB).array(&TLB_Reg::classes);

    Label loop = newLabel();
    Label next = newLabel();
    Label after = newLabel();

    // classBase = &TLB.classes[tlbIndex]
    COMP->lea(classBase, tlbClasses.Ptr(0));
    COMP->mov(off.r32(), tlbIndex.r32());
    COMP->imul(off, imm<s32>(sizeof(TLBCongruenceClass)));
    COMP->add(classBase, off);

    COMP->xor_(way, way);

    COMP->bind(loop);
    COMP->cmp(way, imm<u32>(4));
    COMP->jge(after);

    // wayPtr = &ways[way]
    COMP->mov(wayPtr, classBase);
    COMP->mov(off.r32(), way.r32());
    COMP->imul(off, imm<s32>(sizeof(TLBEntry)));
    COMP->add(wayPtr, off);

    // if (!V) continue
    COMP->cmp(x86::byte_ptr(wayPtr, offsetof(TLBEntry, V)), 0);
    COMP->je(next);

    // compare masked VPN
    COMP->mov(vpn, x86::qword_ptr(wayPtr, offsetof(TLBEntry, VPN)));

    COMP->mov(maskedVPN, vpn);
    COMP->and_(maskedVPN, compareMask);

    COMP->mov(maskedRB, rbVal);
    COMP->and_(maskedRB, compareMask);

    COMP->cmp(maskedVPN, maskedRB);
    COMP->jne(next);

    // invalidate entry
    COMP->mov(x86::byte_ptr(wayPtr, offsetof(TLBEntry, V)), 0);
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, VPN)),  imm<u64>(0));
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, pte0)), imm<u64>(0));
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, pte1)), imm<u64>(0));
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, RPN)),  imm<u64>(0));

    COMP->bind(next);
    COMP->inc(way);
    COMP->jmp(loop);

    COMP->bind(after);

    // ERAT flush
    EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::iERAT).Ptr());
    EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::dERAT).Ptr());

    // compute precise JIT invalidation range
    // pageSize = 1 << p
    COMP->and_(p, imm<u32>(63));
    COMP->mov(pageSize, imm<u64>(1));
    x86::Gp shift = newGP32();
    COMP->mov(shift.r32(), p.r32());
    COMP->mov(x86::ecx, shift.r32());
    COMP->shl(pageSize, x86::cl);

    COMP->mov(pageMask, pageSize);
    COMP->dec(pageMask);

    COMP->mov(invMask, pageMask);
    COMP->not_(invMask);

    COMP->mov(start, rbVal);
    COMP->and_(start, invMask);

    COMP->mov(end, start);
    COMP->add(end, pageSize);

    InvokeNode *inv = nullptr;
    Xe::JITCompat::Invoke(b->compiler, inv,
      imm((void *)QueueInvalidateRange),
      FuncSignature::build<void, sPPEState*, u64, u64>());

    Xe::JITCompat::SetArg(inv, 0, b->ppeState->Base());
    Xe::JITCompat::SetArg(inv, 1, start);
    Xe::JITCompat::SetArg(inv, 2, end);

    COMP->jmp(done);
  }

  //
  // Class invalidation
  //
  COMP->bind(classPath);
  {
    // Interpreter semantics:
    // classIndex = (rb & 0xFF000) >> 12
    x86::Gp classIndex = newGP32();
    x86::Gp classBase = newGP64();
    x86::Gp off = newGP64();
    x86::Gp way = newGP32();
    x86::Gp wayPtr = newGP64();

    auto tlbClasses = b->ppeState->substruct(&sPPEState::TLB).array(&TLB_Reg::classes);

    Label loop = newLabel();
    Label after = newLabel();

    COMP->mov(classIndex.r32(), rbVal.r32());
    COMP->and_(classIndex, imm<u32>(0xFF000));
    COMP->shr(classIndex, 12);

    // classBase = &TLB.classes[classIndex]
    COMP->lea(classBase, tlbClasses.Ptr(0));
    COMP->mov(off.r32(), classIndex.r32());
    COMP->imul(off, imm<s32>(sizeof(TLBCongruenceClass)));
    COMP->add(classBase, off);

    COMP->xor_(way, way);

    COMP->bind(loop);
    COMP->cmp(way, imm<u32>(4));
    COMP->jge(after);

    COMP->mov(wayPtr, classBase);
    COMP->mov(off.r32(), way.r32());
    COMP->imul(off, imm<s32>(sizeof(TLBEntry)));
    COMP->add(wayPtr, off);

    // Match interpreter invalidateClass()/invalidateWay()
    COMP->mov(x86::byte_ptr(wayPtr, offsetof(TLBEntry, V)), 0);
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, VPN)),  imm<u64>(0));
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, pte0)), imm<u64>(0));
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, pte1)), imm<u64>(0));
    COMP->mov(x86::qword_ptr(wayPtr, offsetof(TLBEntry, RPN)),  imm<u64>(0));

    COMP->inc(way);
    COMP->jmp(loop);

    COMP->bind(after);

    // reset LRU, like invalidateAll()
    COMP->mov(x86::byte_ptr(classBase, offsetof(TLBCongruenceClass, lruBits)), 0);

    EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::iERAT).Ptr());
    EmitLRUCacheInvalidateAll(b, b->threadCtx->substruct(&sPPUThread::dERAT).Ptr());

    // Interpreter does a full JIT flush here
    InvokeNode *invAll = nullptr;
    Xe::JITCompat::Invoke(
      b->compiler, invAll,
      imm((void *)QueueInvalidateAll),
      FuncSignature::build<void, sPPEState *>());
    Xe::JITCompat::SetArg(invAll, 0, b->ppeState->Base());

    COMP->jmp(done);
  }

  COMP->bind(done);
  COMP->ret();
}

#endif