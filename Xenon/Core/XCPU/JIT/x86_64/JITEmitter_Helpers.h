/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Logging/Log.h"

#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/JIT/PPU_JIT.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
using namespace asmjit;

#define COMP b->compiler

//
// Allocates a new general purpose x86 register
//
#define newGP64()  b->compiler->new_gpq()
#define newGP32()  b->compiler->new_gpd()
#define newGP16()  b->compiler->new_gpw()
#define newGP8()   b->compiler->new_gpb()
#define newGPptr() b->compiler->new_gpz()

//
// Pointer Helpers
//

#define GPRPtr(x) b->threadCtx->array(&sPPUThread::GPR).Ptr(x)
#define SPRStruct(x) b->threadCtx->substruct(&sPPUThread::SPR).substruct(&sPPUThreadSPRs::x)
#define SPRPtr(x) b->threadCtx->substruct(&sPPUThread::SPR).scalar(&sPPUThreadSPRs::x)
#define SharedSPRStruct(x) b->ppeState->substruct(&sPPEState::SPR).substruct(&sPPESPRs::x)
#define SharedSPRPtr(x) b->ppeState->substruct(&sPPEState::SPR).scalar(&sPPESPRs::x)
#define CRValPtr() b->threadCtx->scalar(&sPPUThread::CR)
#define CIAPtr() b->threadCtx->scalar(&sPPUThread::CIA)
#define NIAPtr() b->threadCtx->scalar(&sPPUThread::NIA)
#define LRPtr() SPRPtr(LR)

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
  Label gt = COMP->new_label(); // Self explanatory.
  Label lt = COMP->new_label(); // Self explanatory.
  Label end = COMP->new_label(); // Self explanatory.

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
  // TODO: Check this.
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
inline x86::Gp J_BuildCRS(JITBlockBuilder* b, x86::Gp lhs, x86::Gp rhs) {
  x86::Gp crValue = newGP32();
  x86::Gp tmp = newGP8();

  COMP->xor_(crValue, crValue);

  // Declare labels:
  Label gt = COMP->new_label(); // Self explanatory.
  Label lt = COMP->new_label(); // Self explanatory.
  Label end = COMP->new_label(); // Self explanatory.

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
  // TODO: Check this.
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
  // Shift formula
  u32 sh = (7 - index) * 4;
  uint32_t clearMask = ~(0xF << sh);

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
inline void J_ppuSetCR0(JITBlockBuilder* b, x86::Gp inValue) {
  // Declare labels:
  Label sfBitMode = COMP->new_label(); // Determines if the compare is done using 64 bit mode.
  Label end = COMP->new_label(); // Self explanatory.

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
  Label use64 = COMP->new_label();
  Label done = COMP->new_label();

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
inline void J_AddDidCarrySetCarry(JITBlockBuilder* b, x86::Gp a, x86::Gp result) {
  Label use64 = COMP->new_label();
  Label resultCheck = COMP->new_label();
  Label setTrue = COMP->new_label();
  Label done = COMP->new_label();

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

#ifdef __LITTLE_ENDIAN__
  COMP->btr(xer, 29); // Clear XER[CA] bit.
#else
  COMP->btr(xer, 2); // Clear XER[CA] bit.
#endif // LITTLE_ENDIAN
  COMP->jmp(done);

  COMP->bind(setTrue);
#ifdef __LITTLE_ENDIAN__
  COMP->bts(xer, 29); // Set XER[CA] bit.
#else
  COMP->bts(xer, 2); // Set XER[CA] bit.
#endif // LITTLE_ENDIAN

  COMP->bind(done);
  // Set XER[CA] value.
  COMP->mov(SPRPtr(XER), xer);
}
#endif