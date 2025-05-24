#pragma once
#include "Base/Logging/Log.h"
#include "../../PPCInterpreter.h"
#include "../../../PPU/PPU_JIT.h"

using namespace asmjit;

#define COMP b->compiler

//
// Offset Helpers
//
#define GPROffest(x) (offsetof(PPU_THREAD_REGISTERS, GPR) + (x * 8))
#define SPROffest(x) (offsetof(PPU_THREAD_REGISTERS, SPR) + offsetof(PPU_THREAD_SPRS, x))
#define SharedSPROffest(x) (offsetof(PPU_STATE, SPR) + offsetof(PPU_STATE_SPRS, x))

//
// Allocates a new general purpose x86 register
//
#define newGP64()  b->compiler->newGpq()
#define newGP32()  b->compiler->newGpd()
#define newGP16()  b->compiler->newGpw()
#define newGP8()   b->compiler->newGpb()
#define newGPptr() b->compiler->newGpz()

//
// Pointer Helpers
//
#define GPRPtr(x) x86::ptr(b->threadCtx, GPROffest(x))
#define SPRPtr(x) x86::ptr(b->threadCtx, SPROffest(x))
#define SharedSPRPtr(x) x86::ptr(b->ppuState, SharedSPROffest(x))
#define CRValPtr() x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, CR))
#define CIAPtr() x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, CIA))
#define NIAPtr() x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, NIA))
#define LRPtr() x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, SPR) + offsetof(PPU_THREAD_SPRS, LR))

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

inline x86::Gp J_BuildCR_0(JITBlockBuilder *b, x86::Gp value) {
  x86::Gp crValue = newGP32();
  x86::Gp tmp = newGP8();
  COMP->cmp(value, imm(0)); // cmp with zero
  COMP->xor_(crValue, crValue);

  // lt
  COMP->setl(tmp);
  COMP->shl(tmp, imm(3 - CR_BIT_LT));
  COMP->or_(crValue.r8(), tmp.r8());
  // gt
  COMP->setg(tmp);
  COMP->shl(tmp, imm(3 - CR_BIT_GT));
  COMP->or_(crValue.r8(), tmp.r8());
  // eq
  COMP->sete(tmp);
  COMP->shl(tmp, imm(3 - CR_BIT_EQ));
  COMP->or_(crValue.r8(), tmp.r8());

  // Handle SO flag
#ifdef __LITTLE_ENDIAN__
  COMP->mov(tmp.r32(), SPRPtr(XER.XER_Hex));
  COMP->shr(tmp.r32(), imm(31));
#else
  COMP->mov(tmp.r32(), SPRPtr(XER.XER_Hex));
  COMP->and_(tmp.r32(), imm(1));
#endif
  COMP->shl(tmp, imm(3 - CR_BIT_SO));
  COMP->or_(crValue.r8(), tmp.r8());

  return crValue;
}

inline void J_SetCRField(JITBlockBuilder *b, x86::Gp field, u32 index) {
  x86::Gp tempCR = newGP32();

  u32 sh = (7 - index) * 4; // Shift formula
  uint32_t clearMask = ~(0xF << sh);

  COMP->mov(tempCR, CRValPtr());
  COMP->and_(tempCR, clearMask); // Clear field 
  COMP->shl(field, sh); // Shift field bits to position
  COMP->or_(tempCR, field); // Apply bits

  COMP->mov(CRValPtr(), tempCR); // Store updated value back
}

inline void J_ppuSetCR(JITBlockBuilder *b, x86::Gp value, u32 index) {
  Label continueLabel = COMP->newLabel();
  x86::Gp temp8 = newGP8();

  //
  // Cast Check
  //
#ifdef __LITTLE_ENDIAN__
  COMP->mov(temp8.r32(), SPRPtr(MSR.MSR_Hex));
  COMP->shr(temp8.r32(), imm(31));
#else
  COMP->mov(temp8.r32(), SPRPtr(MSR.MSR_Hex));
  COMP->and_(temp8.r32(), imm(1));
#endif
  COMP->test(temp8, temp8);
  COMP->jnz(continueLabel);
  COMP->mov(value.r32(), value.r32()); // Cast 64 TO 32
  COMP->bind(continueLabel);

  x86::Gp field = J_BuildCR_0(b, value);
  J_SetCRField(b, field, index);
}