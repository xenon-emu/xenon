/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Logging/Log.h"

#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/JIT/PPU_JIT.h"

#include "Core/XCPU/JIT/JITCompat.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
using namespace asmjit;

#define COMP b->compiler

//
// Allocates a new general purpose x86 register
//
#define newGP64()  Xe::JITCompat::NewGP64(b->compiler)
#define newGP32()  Xe::JITCompat::NewGP32(b->compiler)
#define newGP16()  Xe::JITCompat::NewGP16(b->compiler)
#define newGP8()   Xe::JITCompat::NewGP8(b->compiler)
#define newGPptr() Xe::JITCompat::NewGPZ(b->compiler)

#define newLabel() Xe::JITCompat::NewLabel(b->compiler)
#define newStack(x, y) Xe::JITCompat::NewStack(b->compiler, x, y)

//
// Floating Point Register Pointer Helper
//
#define FPRPtr(x) b->threadCtx->array(&sPPUThread::FPR).Ptr(x)

//
// Allocates a new XMM register for floating-point operations
//
#define newXMM() Xe::JITCompat::NewXmm(b->compiler)

#define newYMM() Xe::JITCompat::NewYmm(b->compiler)
#define newZMM() Xe::JITCompat::NewZmm(b->compiler)

//
// FPSCR Pointer Helper
//
#define FPSCRPtr() b->threadCtx->scalar(&sPPUThread::FPSCR)

//
// Vector Register Pointer Helper
//
#define VPRPtr(x) b->threadCtx->array(&sPPUThread::VR).Ptr(x)

//
// Pointer Helpers
//

#define GPRPtr(x) b->threadCtx->array(&sPPUThread::GPR).Ptr(x)
#define SPRStruct(x) b->threadCtx->substruct(&sPPUThread::SPR).substruct(&sPPUThreadSPRs::x)
#define SPRPtr(x) b->threadCtx->substruct(&sPPUThread::SPR).scalar(&sPPUThreadSPRs::x)
#define SharedSPRStruct(x) b->ppeState->substruct(&sPPEState::SPR).substruct(&sPPUGlobalSPRs::x)
#define SharedSPRPtr(x) b->ppeState->substruct(&sPPEState::SPR).scalar(&sPPUGlobalSPRs::x)
#define CRValPtr() b->threadCtx->scalar(&sPPUThread::CR)
#define CIAPtr() b->threadCtx->scalar(&sPPUThread::CIA)
#define NIAPtr() b->threadCtx->scalar(&sPPUThread::NIA)
#define EXPtr() b->threadCtx->scalar(&sPPUThread::exceptReg)
#define LRPtr() SPRPtr(LR)

// XER CA bit position (platform-dependent)
#ifdef __LITTLE_ENDIAN__
  constexpr u32 XER_CA_BIT = 29;
#else
  constexpr u32 XER_CA_BIT = 2;
#endif

inline x86::Gp Jrotl32(JITBlockBuilder *b, x86::Mem x, u32 n) {
  x86::Gp tmp = newGP32();
  COMP->mov(tmp, x); // Cast value to 32 bit register
  COMP->rol(tmp, n);
  return tmp;
}

// Duplicates a u32 value left, used in rotate instructions that duplicate the
// lower 32 bits
inline x86::Gp Jduplicate32(JITBlockBuilder *b, x86::Gp origin) {
  x86::Gp cast64 = newGP64();
  COMP->mov(cast64, origin.r64());  // copy and cast to 64 bit
  COMP->shl(cast64, 32);        // shift left 32 bits
  COMP->or_(cast64, origin.r64());  // or with original value
  return cast64;
}

// CR Unsigned comparison. Uses x86's JA and JB.
inline x86::Gp J_BuildCRU(JITBlockBuilder *b, x86::Gp lhs, x86::Gp rhs) {
  x86::Gp crValue = newGP32();
  x86::Gp tmp = newGP8();

  COMP->xor_(crValue, crValue);

  // Declare labels:
  Label gt = newLabel(); // Self explanatory.
  Label lt = newLabel(); // Self explanatory.
  Label end = newLabel(); // Self explanatory.

  COMP->cmp(lhs, rhs); // Compare lhs and rhs
  // Check Greater Than.
  COMP->ja(gt);
  // Check Lower Than.
  COMP->jb(lt);
  // Equal to zero.
  COMP->mov(tmp, imm(2));
  COMP->or_(crValue.r8(), tmp.r8());
  COMP->jmp(end);

  // Above than:
  COMP->bind(gt);
  COMP->mov(tmp, imm(4));
  COMP->or_(crValue.r8(), tmp.r8());
  COMP->jmp(end);

  // Lower than:
  COMP->bind(lt);
  COMP->mov(tmp, imm(8));
  COMP->or_(crValue.r8(), tmp.r8());

  COMP->bind(end);

  // SO bit (summary overflow)
#ifdef __LITTLE_ENDIAN__
  COMP->mov(tmp.r32(), SPRPtr(XER));
  COMP->shr(tmp.r32(), imm(31));
#else
  COMP->mov(tmp.r32(), SPRPtr(XER));
  COMP->and_(tmp.r32(), imm(1));
#endif
  COMP->shl(tmp, imm(3 - CR_BIT_SO));
  COMP->or_(crValue.r8(), tmp.r8());

  return crValue;
}

// CR Signed comparison. Uses x86's JG and JL.
inline x86::Gp J_BuildCRS(JITBlockBuilder *b, x86::Gp lhs, x86::Gp rhs) {
  x86::Gp crValue = newGP32();
  x86::Gp tmp = newGP8();

  COMP->xor_(crValue, crValue);

  // Declare labels:
  Label gt = newLabel(); // Self explanatory.
  Label lt = newLabel(); // Self explanatory.
  Label end = newLabel(); // Self explanatory.

  COMP->cmp(lhs, rhs); // Compare lhs and rhs
  // Check Greater Than.
  COMP->jg(gt);
  // Check Lower Than.
  COMP->jl(lt);
  // Equal to zero.
  COMP->mov(tmp, imm(2));
  COMP->or_(crValue.r8(), tmp.r8());
  COMP->jmp(end);

  // Greater than:
  COMP->bind(gt);
  COMP->mov(tmp, imm(4));
  COMP->or_(crValue.r8(), tmp.r8());
  COMP->jmp(end);

  // Lower than:
  COMP->bind(lt);
  COMP->mov(tmp, imm(8));
  COMP->or_(crValue.r8(), tmp.r8());

  COMP->bind(end);

  // SO bit (summary overflow)
#ifdef __LITTLE_ENDIAN__
  COMP->mov(tmp.r32(), SPRPtr(XER));
  COMP->shr(tmp.r32(), imm(31));
#else
  COMP->mov(tmp.r32(), SPRPtr(XER));
  COMP->and_(tmp.r32(), imm(1));
#endif
  COMP->shl(tmp, imm(3 - CR_BIT_SO));
  COMP->or_(crValue.r8(), tmp.r8());

  return crValue;
}


// Sets a given CR field using the specified value.
inline void J_SetCRField(JITBlockBuilder *b, x86::Gp field, u32 index) {
  // Temp storage for the CR current value.
  x86::Gp tempCR = newGP32();
  // Shift formula - computed at compile time
  const u32 sh = (7 - index) * 4;
  const u32 clearMask = ~(0xFu << sh);

  // Load CR value to temp storage.
  COMP->mov(tempCR, CRValPtr());
  // Clear field to be modified.
  COMP->and_(tempCR, clearMask);
  // Left shift field bits to position.
  COMP->shl(field, sh);
  // Apply bits.
  COMP->or_(tempCR, field);
  // Store updated value back to CR.
  COMP->mov(CRValPtr(), tempCR);
}

// Performs a comparison between the given input value and zero, and stores it in CR0 field.
// * Takes into account the current computation mode (MSR[SF]).
inline void J_ppuSetCR0(JITBlockBuilder *b, x86::Gp inValue) {
  // Declare labels:
  Label sfBitMode = newLabel(); // Determines if the compare is done using 64 bit mode.
  Label end = newLabel(); // Self explanatory.

  // Check for MSR[SF]:
  x86::Gp tempMSR = newGP64(); // MSR is 64 bits wide.
  COMP->mov(tempMSR, SPRPtr(MSR)); // Get MSR value.
  COMP->bt(tempMSR, 63); // Check for bit 0(BE)(SF) on MSR.
  COMP->jc(sfBitMode); // If set, use 64-bit compare. (checks for carry flag from previous operation).

  // 32 Bit mode:
  {
    // Set a zero filled 32 bit value for the comaprison.
    x86::Gp zero32 = newGP32();
    COMP->xor_(zero32, zero32);
    // Compare and store in CR0 based on input value and zero filled variable.
    x86::Gp field = J_BuildCRS(b, inValue.r32(), zero32);
    J_SetCRField(b, field, 0);
    // Done.
    COMP->jmp(end);
  }

  // 64 Bit mode:
  COMP->bind(sfBitMode);
  {
    // Set a zero filled 64 bit value for the comaprison.
    x86::Gp zero64 = newGP64();
    COMP->xor_(zero64, zero64);
    // Compare and store in CR0 based on input value and zero filled variable.
    x86::Gp field = J_BuildCRS(b, inValue.r64(), zero64);
    J_SetCRField(b, field, 0);
  }

  COMP->bind(end);
}

inline void J_ppuSetCR(JITBlockBuilder *b, x86::Gp value, u32 index) {
  Label use64 = newLabel();
  Label done = newLabel();

  x86::Gp tempMSR = newGP64();
  x86::Gp tempCR = newGP32();

  // Load MSR and check SF bit
  COMP->mov(tempMSR, SPRPtr(MSR));
  COMP->bt(tempMSR, 0); // Bit 0 (SF) on MSR
  COMP->jc(use64); // If set, use 64-bit compare

  // 32-bit compare
  {
    x86::Gp zero32 = newGP32();
    COMP->xor_(zero32, zero32);
    x86::Gp field = J_BuildCRS(b, value.r32(), zero32);
  J_SetCRField(b, field, index);
    COMP->jmp(done);
  }

  // 64-bit compare
COMP->bind(use64);
  {
    x86::Gp zero64 = newGP64();
    COMP->xor_(zero64, zero64);
    x86::Gp field = J_BuildCRS(b, value.r64(), zero64);
    J_SetCRField(b, field, index);
  }

  COMP->bind(done);
}

// Check if carry took place according to computation modes and set XER[CA] depending on the result.
inline void J_AddDidCarrySetCarry(JITBlockBuilder *b, x86::Gp a, x86::Gp result) {
  Label use64 = newLabel();
  Label resultCheck = newLabel();
  Label setTrue = newLabel();
  Label done = newLabel();

  // Get XER
  x86::Gp xer = newGP32();
  COMP->mov(xer, SPRPtr(XER));

  // Load MSR and check SF bit
  x86::Gp tempMSR = newGP64();
  COMP->mov(tempMSR, SPRPtr(MSR));
  COMP->bt(tempMSR, 0); // Bit 0 (SF) on MSR
  COMP->jc(use64); // If set, use 64-bit carry check


  // 32-bit carry
  {
    COMP->cmp(result.r32(), a.r32());
    COMP->jmp(resultCheck);
  }

  // 64-bit carry
  COMP->bind(use64);
  {
    COMP->cmp(result, a);
  }

  COMP->bind(resultCheck);
  COMP->jl(setTrue);

  COMP->btr(xer, XER_CA_BIT); // Clear XER[CA] bit.
  COMP->jmp(done);

  COMP->bind(setTrue);
  COMP->bts(xer, XER_CA_BIT); // Set XER[CA] bit.

  COMP->bind(done);
  // Set XER[CA] value.
  COMP->mov(SPRPtr(XER), xer);
}

#define FAST_TRAP

// Trap Helper
inline void TrapCheck(JITBlockBuilder *b, x86::Gp ra, x86::Gp rb, u32 TO) {
  // if (a < b) & TO[0] then TRAP
  // if (a > b) & TO[1] then TRAP
  // if (a = b) & TO[2] then TRAP
  // if (a < unsigned b) & TO[3] then TRAP
  // if (a > unsigned b) & TO[4] then TRAP

  // Check TO - early exit if no conditions to check
  if (!TO) { return; }

  Label end = newLabel();
  Label doTrap = newLabel();

  // Compare our values
  COMP->cmp(ra, rb);

  // Generate conditional jumps only for enabled conditions
  if (TO & (1 << 4)) {
    // a < b (signed)
    COMP->jl(doTrap);
  }
  if (TO & (1 << 3)) {
    // a > b (signed)
    COMP->jg(doTrap);
  }
  if (TO & (1 << 2)) {
    // a = b
    COMP->je(doTrap);
  }
  if (TO & (1 << 1)) {
    // a <u b (unsigned)
 COMP->jb(doTrap);
  }
  if (TO & (1 << 0)) {
  // a >u b (unsigned)
    COMP->ja(doTrap);
  }

  // No trap condition met, skip to end
  COMP->jmp(end);

  // Trap was valid, invoke our trap handler.
COMP->bind(doTrap);
#ifdef FAST_TRAP
  // Faster path for trap processing, directly raise exception and set exception type.
  x86::Gp exceptReg = newGP16();
  COMP->mov(exceptReg, EXPtr());
  COMP->or_(exceptReg, ppuProgramEx);
  COMP->mov(EXPtr(), exceptReg);
  COMP->mov(exceptReg, b->threadCtx->scalar(&sPPUThread::progExceptionType));
  COMP->mov(exceptReg, ppuProgExTypeTRAP);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::progExceptionType), exceptReg);
#else
  // Slow but pretty, use our old function to print out debug messages, etc...
  InvokeNode *inv = nullptr;
  b->compiler->invoke(Out(inv, imm((void*)PPCInterpreter::ppcInterpreterTrap), FuncSignature::build<void, void*, u32>());
  inv->setArg(0, b->ppeState->Base());
  inv->setArg(1, rb);
#endif // FAST_TRAP
  COMP->bind(end);
}

static constexpr size_t kLRUCacheNumSets = 256;
static constexpr size_t kLRUCacheNumWays = 2;
static constexpr u64 kLRUCacheInvalidKey = ~0ULL;
static constexpr size_t kLRUCacheEntrySize = 24;
static constexpr size_t kLRUCacheSetSize = kLRUCacheEntrySize * kLRUCacheNumWays; // 48
static constexpr size_t kLRUEntryKeyOff = 0;
static constexpr size_t kLRUEntryValueOff = 8;
static constexpr size_t kLRUEntryValidOff = 16;
static constexpr size_t kLRUEntriesOff = offsetof(LRUCache, entries);
static constexpr size_t kLRULruBitsOff = offsetof(LRUCache, lruBits);

static inline void EmitLRUCacheInvalidateAll(JITBlockBuilder *b, const x86::Mem &cacheMem) {
  x86::Gp cacheBase = newGP64();
  x86::Gp entriesBase = newGP64();
  x86::Gp lruBase = newGP64();
  x86::Gp i = newGP32();
  x86::Gp off = newGP64();

  Label loop = newLabel();
  Label done = newLabel();

  COMP->lea(cacheBase, cacheMem);

  COMP->mov(entriesBase, cacheBase);
  if constexpr (kLRUEntriesOff != 0)
    COMP->add(entriesBase, imm(static_cast<s32>(kLRUEntriesOff)));

  COMP->mov(lruBase, cacheBase);
  COMP->add(lruBase, imm(static_cast<s32>(kLRULruBitsOff)));

  COMP->xor_(i, i);

  COMP->bind(loop);
  COMP->cmp(i, imm(static_cast<s32>(kLRUCacheNumSets)));
  COMP->jge(done);

  COMP->mov(off.r32(), i.r32());
  COMP->imul(off, imm(static_cast<s32>(kLRUCacheSetSize)));

  COMP->mov(x86::byte_ptr(entriesBase, off, kLRUEntryValidOff), 0);
  COMP->mov(x86::qword_ptr(entriesBase, off, kLRUEntryKeyOff), imm<u64>(kLRUCacheInvalidKey));

  COMP->mov(x86::byte_ptr(entriesBase, off, kLRUCacheEntrySize + kLRUEntryValidOff), 0);
  COMP->mov(x86::qword_ptr(entriesBase, off, kLRUCacheEntrySize + kLRUEntryKeyOff), imm<u64>(kLRUCacheInvalidKey));

  COMP->mov(x86::byte_ptr(lruBase, i), 0);

  COMP->inc(i);
  COMP->jmp(loop);

  COMP->bind(done);
}

static inline void EmitLRUCacheInvalidateElement(JITBlockBuilder *b, const x86::Mem &cacheMem, x86::Gp key) {
  x86::Gp cacheBase = newGP64();
  x86::Gp entriesBase = newGP64();
  x86::Gp setIdx = newGP64();
  x86::Gp tmp = newGP64();
  x86::Gp off = newGP64();
  x86::Gp entryKey = newGP64();
  x86::Gp valid = newGP32();

  Label checkWay1 = newLabel(), done = newLabel();

  COMP->lea(cacheBase, cacheMem);

  COMP->mov(entriesBase, cacheBase);
  if constexpr (kLRUEntriesOff != 0)
    COMP->add(entriesBase, imm(static_cast<s32>(kLRUEntriesOff)));

  COMP->mov(setIdx, key);
  COMP->shr(setIdx, 12);

  COMP->mov(tmp, key);
  COMP->shr(tmp, 17);

  COMP->xor_(setIdx, tmp);
  COMP->and_(setIdx, imm<u64>(kLRUCacheNumSets - 1));

  COMP->mov(off.r32(), setIdx.r32());
  COMP->imul(off, imm(static_cast<s32>(kLRUCacheSetSize)));

  COMP->movzx(valid, x86::byte_ptr(entriesBase, off, kLRUEntryValidOff));
  COMP->test(valid, valid);
  COMP->jz(checkWay1);

  COMP->mov(entryKey, x86::qword_ptr(entriesBase, off, kLRUEntryKeyOff));
  COMP->cmp(entryKey, key);
  COMP->jne(checkWay1);

  COMP->mov(x86::byte_ptr(entriesBase, off, kLRUEntryValidOff), 0);
  COMP->mov(x86::qword_ptr(entriesBase, off, kLRUEntryKeyOff), imm<u64>(kLRUCacheInvalidKey));
  COMP->jmp(done);

  COMP->bind(checkWay1);
  COMP->movzx(valid, x86::byte_ptr(entriesBase, off, kLRUCacheEntrySize + kLRUEntryValidOff));
  COMP->test(valid, valid);
  COMP->jz(done);

  COMP->mov(entryKey, x86::qword_ptr(entriesBase, off, kLRUCacheEntrySize + kLRUEntryKeyOff));
  COMP->cmp(entryKey, key);
  COMP->jne(done);

  COMP->mov(x86::byte_ptr(entriesBase, off, kLRUCacheEntrySize + kLRUEntryValidOff), 0);
  COMP->mov(x86::qword_ptr(entriesBase, off, kLRUCacheEntrySize + kLRUEntryKeyOff), imm<u64>(kLRUCacheInvalidKey));

  COMP->bind(done);
}

static inline x86::Gp EmitEA_DForm_Halfword(JITBlockBuilder *b, uPPCInstr instr) {
  x86::Compiler *cc = b->compiler;
  x86::Gp EA = newGP64();

  if (instr.ra) {
    cc->mov(EA, GPRPtr(instr.ra));
    cc->add(EA, imm<s64>(instr.simm16));
  } else {
    cc->mov(EA, imm<s64>(instr.simm16));
  }

  return EA;
}

static inline x86::Gp EmitEA_XForm_Halfword(JITBlockBuilder *b, uPPCInstr instr, bool ra_zero_means_zero) {
  x86::Compiler *cc = b->compiler;
  x86::Gp EA = newGP64();

  if (ra_zero_means_zero) {
    if (instr.ra) {
      cc->mov(EA, GPRPtr(instr.ra));
      cc->add(EA, GPRPtr(instr.rb));
    } else {
      cc->mov(EA, GPRPtr(instr.rb));
    }
  } else {
    cc->mov(EA, GPRPtr(instr.ra));
    cc->add(EA, GPRPtr(instr.rb));
  }

  return EA;
}

static inline x86::Gp EmitMMURead16(JITBlockBuilder *b, x86::Gp EA) {
  x86::Gp data16 = newGP16();
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)PPCInterpreter::MMURead16), FuncSignature::build<u16, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data16);
  return data16;
}

static inline void EmitMMUWrite16(JITBlockBuilder* b, x86::Gp EA, x86::Gp value32) {
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void*)PPCInterpreter::MMUWrite16), FuncSignature::build<void, sPPEState *, u64, u16, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, value32.r16());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

static inline x86::Gp EmitMMURead32(JITBlockBuilder* b, x86::Gp EA) {
  x86::Gp data32 = newGP32();
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void*)PPCInterpreter::MMURead32), FuncSignature::build<u32, sPPEState*, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data32);
  return data32;
}

static inline x86::Gp EmitMMURead64(JITBlockBuilder* b, x86::Gp EA) {
  x86::Gp data64 = newGP64();
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)PPCInterpreter::MMURead64), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64);
  return data64;
}

static inline void EmitDataExceptionEarlyExit(JITBlockBuilder *b) {
  Label noEx = newLabel();
  x86::Gp exceptReg = newGP32();
  COMP->movzx(exceptReg, b->threadCtx->scalar(&sPPUThread::exceptReg));
  COMP->test(exceptReg, imm<u32>(ppuDataSegmentEx | ppuDataStorageEx));
  COMP->jz(noEx);
  COMP->ret();
  COMP->bind(noEx);
}

static inline void EmitDataAccessEarlyExit(JITBlockBuilder *b) {
  Label noEx = newLabel();
  x86::Gp exceptReg = newGP32();
  COMP->movzx(exceptReg, b->threadCtx->scalar(&sPPUThread::exceptReg));
  COMP->test(exceptReg, imm<u32>(ppuDataSegmentEx | ppuDataStorageEx));
  COMP->jz(noEx);
  COMP->ret();
  COMP->bind(noEx);
}

static inline x86::Gp EmitDSFormEA(JITBlockBuilder* b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  // ds-form immediate is simm16 with low 2 bits forced 0
  const s64 ds = static_cast<s64>(instr.simm16 & ~3);
  if (instr.ra) {
    COMP->mov(EA, GPRPtr(instr.ra));
    COMP->add(EA, imm(ds));
  } else {
    COMP->mov(EA, imm(ds));
  }
  return EA;
}

#endif