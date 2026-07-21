/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

 // Modified for use on the Xenon Emulator.

#include "Base/Assert.h"
#include "Core/XCPU/JIT/HIR/HIREmitters/HIREmitters.h"

namespace Xe::XCPU::HIR {

//
// VMX 128 Instruction bitfields
//

// Re declared here because of the way JIT uses CI

#define J_VMX128_VD128   (instr.VMX128.VD128l | (instr.VMX128.VD128h << 5))
#define J_VMX128_VA128   (instr.VMX128.VA128l | (instr.VMX128.VA128h << 5) | (instr.VMX128.VA128H << 6))
#define J_VMX128_VB128   (instr.VMX128.VB128l | (instr.VMX128.VB128h << 5))

#define J_VMX128_1_VD128 (instr.VMX128_1.VD128l | (instr.VMX128_1.VD128h << 5))

#define J_VMX128_2_VD128 (instr.VMX128_2.VD128l | (instr.VMX128_2.VD128h << 5))
#define J_VMX128_2_VA128 (instr.VMX128_2.VA128l | (instr.VMX128_2.VA128h << 5) | (instr.VMX128_2.VA128H << 6))
#define J_VMX128_2_VB128 (instr.VMX128_2.VB128l | (instr.VMX128_2.VB128h << 5))
#define J_VMX128_2_VC    (instr.VMX128_2.VC)

#define J_VMX128_3_VD128 (instr.VMX128_3.VD128l | (instr.VMX128_3.VD128h << 5))
#define J_VMX128_3_VB128 (instr.VMX128_3.VB128l | (instr.VMX128_3.VB128h << 5))
#define J_VMX128_3_IMM   (instr.VMX128_3.IMM)

#define J_VMX128_5_VD128 (instr.VMX128_5.VD128l | (instr.VMX128_5.VD128h << 5))
#define J_VMX128_5_VA128 ((instr.VMX128_5.VA128l | (instr.VMX128_5.VA128h << 5)) | (instr.VMX128_5.VA128H << 6))
#define J_VMX128_5_VB128 (instr.VMX128_5.VB128l | (instr.VMX128_5.VB128h << 5))
#define J_VMX128_5_SH    (instr.VMX128_5.SH)

#define J_VMX128_R_VD128 (instr.VMX128_R.VD128l | (instr.VMX128_R.VD128h << 5))
#define J_VMX128_R_VA128 (instr.VMX128_R.VA128l | (instr.VMX128_R.VA128h << 5) | (instr.VMX128_R.VA128H << 6))
#define J_VMX128_R_VB128 (instr.VMX128_R.VB128l | (instr.VMX128_R.VB128h << 5))

#define SHUFPS_SWAP_DWORDS 0x1B

// Most of this file comes from:
// http://biallas.net/doc/vmx128/vmx128.txt
// https://github.com/kakaroto/ps3ida/blob/master/plugins/PPCAltivec/src/main.cpp
// https://sannybuilder.com/forums/viewtopic.php?id=190

//
// Helpers
//

// Form: instr.ra ? GPR[rA] : 0 + GPR[rB]
Value *GetEA_rA_0_rB(HIRBuilder &b, u32 rA, u32 rB) {
  // instr.RA ? GPR[rA] : 0
  if (rA == 0) { return b.LoadGPR(rB); }
  else { return b.Add(b.LoadGPR(rA), b.LoadGPR(rB)); }
}

//
// Load Vector
//

// NOTE: lve* instructions behave exaclty like lvx, software should only be worried about the contents of the 
// target b/h/w as all other data is set to undefined values.

// Load Vector Element Byte Indexed (x'7C00 000E')
s32 HIRInstrEmit_lvebx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, instr.ra, instr.rb), b.LoadConstantUint64(~0xFull));
  // Perform the store
  b.StoreVR(instr.rd, b.ByteSwap(b.Load(EA, VEC128_TYPE)));
  return 0;
}

// Load Vector Element Halfword Indexed (x'7C00 004E')
s32 HIRInstrEmit_lvehx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, instr.ra, instr.rb), b.LoadConstantUint64(~0xFull));
  // Perform the store
  b.StoreVR(instr.rd, b.ByteSwap(b.Load(EA, VEC128_TYPE)));
  return 0;
}

// Helper to perform lvewx*
s32 LoadVectorElementWord(HIRBuilder &b, const uPPCInstr &instr, u32 vd, u32 ra, u32 rb) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, ra, rb), b.LoadConstantUint64(~0xFull));
  // Perform the store
  b.StoreVR(vd, b.ByteSwap(b.Load(EA, VEC128_TYPE)));
  return 0;
}

// Load Vector Element Word Indexed (x'7C00 008E')
s32 HIRInstrEmit_lvewx(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorElementWord(b, instr, instr.rd, instr.ra, instr.rb);
}

// Load Vector 128 Element Word Indexed
s32 HIRInstrEmit_lvewx128(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorElementWord(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Helper to perform lvsl*
s32 LoadVectorShiftLeft(HIRBuilder &b, const uPPCInstr &instr, u32 vd, u32 ra, u32 rb) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, ra, rb);
  // Get SH
  Value *SH = b.Truncate(b.And(EA, b.LoadConstantInt64(0xF)), INT8_TYPE);
  // Load the vector using the shift amount 
  Value *v = b.LoadVectorShl(SH);
  // Store loaded vector into target reg
  b.StoreVR(vd, v);
  return 0;
}

// Load Vector for Shift Left (x'7C00 000C')
s32 HIRInstrEmit_lvsl(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorShiftLeft(b, instr, instr.rd, instr.ra, instr.rb);
}

// Load Vector 128 for Shift Left
s32 HIRInstrEmit_lvsl128(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorShiftLeft(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Helper to perform lvsr*
s32 LoadVectorShiftRight(HIRBuilder &b, const uPPCInstr &instr, u32 vd, u32 ra, u32 rb) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, ra, rb);
  // Get SH
  Value *SH = b.Truncate(b.And(EA, b.LoadConstantInt64(0xF)), INT8_TYPE);
  // Load the vector using the shift amount 
  Value *v = b.LoadVectorShr(SH);
  // Store loaded vector into target reg
  b.StoreVR(vd, v);
  return 0;
}

// Load Vector for Shift Right (x'7C00 004C')
s32 HIRInstrEmit_lvsr(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorShiftRight(b, instr, instr.rd, instr.ra, instr.rb);
}

// Load Vector 128 for Shift Right
s32 HIRInstrEmit_lvsr128(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorShiftRight(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Helper to perform lvx*
s32 LoadVectorIndexed(HIRBuilder &b, const uPPCInstr &instr, u32 vd, u32 ra, u32 rb) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, ra, rb), b.LoadConstantInt64(~0xFull));
  // Perform the store
  b.StoreVR(vd, b.ByteSwap(b.Load(EA, VEC128_TYPE)));
  return 0;
}

// Load Vector Indexed (x'7C00 00CE')
s32 HIRInstrEmit_lvx(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorIndexed(b, instr, instr.rd, instr.ra, instr.rb);
}

// Load Vector 128 Indexed
s32 HIRInstrEmit_lvx128(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorIndexed(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Load Vector Indexed Last (x'7C00 02CE')
s32 HIRInstrEmit_lvxl(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_lvx(b, instr);
}

// Load Vector 128 Indexed Last
s32 HIRInstrEmit_lvxl128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_lvx128(b, instr);
}

// Helper to perform lvlx*
s32 LoadVectorLeftIndexed(HIRBuilder &b, const uPPCInstr &instr, u32 vD, u32 rA, u32 rB) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, rA, rB);
  // Get EB
  Value *EB = b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantInt8(0xF));
  // Align EA
  EA = b.And(EA, b.LoadConstantUint64(~0xFull));
  // Load the corresponding vector and perform the shit using EB
  Value *v = b.Permute(b.LoadVectorShl(EB), b.ByteSwap(b.Load(EA, VEC128_TYPE)), b.LoadZeroVec128(), INT8_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreVR(vD, v);
  return 0;
}

// Load Vector Left Indexed (x'7C00 040E')
s32 HIRInstrEmit_lvlx(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorLeftIndexed(b, instr, instr.rd, instr.ra, instr.rb);
}

// Load Vector Left Indexed 128
s32 HIRInstrEmit_lvlx128(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorLeftIndexed(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Load Vector Left Indexed Last (x'7C00 060E')
s32 HIRInstrEmit_lvlxl(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_lvlx(b, instr);
}

// Load Vector 128 Left Indexed Last
s32 HIRInstrEmit_lvlxl128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_lvlx128(b, instr);
}

// Helper to perform lvrx*
s32 LoadVectorRightIndexed(HIRBuilder &b, const uPPCInstr &instr, u32 vD, u32 rA, u32 rB) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, rA, rB);
  // Get EB
  Value *EB = b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantInt8(0xF));
  // Align EA
  EA = b.And(EA, b.LoadConstantUint64(~0xFull));
  // Load the corresponding vector and perform the shift using EB
  Value *v = b.Permute(b.LoadVectorShl(EB), b.LoadZeroVec128(), b.ByteSwap(b.Load(EA, VEC128_TYPE)), INT8_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreVR(vD, v);
  return 0;
}

// Load Vector Right Indexed (x'7C00 044E')
s32 HIRInstrEmit_lvrx(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorRightIndexed(b, instr, instr.rd, instr.ra, instr.rb);
}

// Load Vector 128 Right Indexed
s32 HIRInstrEmit_lvrx128(HIRBuilder &b, const uPPCInstr &instr) {
  return LoadVectorRightIndexed(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Load Vector Right Indexed Last (x'7C00 064E')
s32 HIRInstrEmit_lvrxl(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_lvrx(b, instr);
}

// Load Vector 128 Right Indexed Last
s32 HIRInstrEmit_lvrxl128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_lvrx128(b, instr);
}


//
// Vector stores
//

// Store Vector Element Byte Indexed (x'7C00 010E')
s32 HIRInstrEmit_stvebx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr.ra, instr.rb);
  // Get element index
  Value *el = b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantUint8(0xF));
  // Extract element to be stored from rD
  Value *v = b.Extract(b.LoadVR(instr.rd), el, INT8_TYPE);
  // Perform the store
  b.Store(EA, v);
  return 0;
}

// Store Vector Element Halfword Indexed (x'7C00 014E')
s32 HIRInstrEmit_stvehx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, instr.ra, instr.rb), b.LoadConstantUint64(~0x1ull));
  // Get element index
  Value *el = b.Shr(b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantUint8(0xF)), 1);
  // Extract element to be stored from rD
  Value *v = b.Extract(b.LoadVR(instr.rd), el, INT16_TYPE);
  // Perform the store
  b.Store(EA, v);
  return 0;
}

// Helper to perform stvewx*
s32 StoreVectorElementWord(HIRBuilder &b, const uPPCInstr &instr, u32 vd, u32 ra, u32 rb) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, ra, rb), b.LoadConstantUint64(~0x3ull));
  // Get element index
  Value *el = b.Shr(b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantUint8(0xF)), 2);
  // Extract element to be stored from rD
  Value *v = b.Extract(b.LoadVR(vd), el, INT32_TYPE);
  // Perform the store
  b.Store(EA, v);
  return 0;
}

// Store Vector Element Word Indexed (x'7C00 018E')
s32 HIRInstrEmit_stvewx(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorElementWord(b, instr, instr.rd, instr.ra, instr.rb);
}

// Store Vector 128 Element Word Indexed
s32 HIRInstrEmit_stvewx128(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorElementWord(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Helper to perform stvx*
s32 StoreVectorIndexed(HIRBuilder &b, const uPPCInstr &instr, u32 vD, u32 vA, u32 vB) {
  // Get EA
  Value *EA = b.And(GetEA_rA_0_rB(b, vA, vB), b.LoadConstantUint64(~0xFull));
  // Perform the store
  b.Store(EA, b.ByteSwap(b.LoadVR(vD)));
  return 0;
}

// Store Vector Indexed (x'7C00 01CE')
s32 HIRInstrEmit_stvx(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorIndexed(b, instr, instr.rd, instr.ra, instr.rb);
}

// Store Vector 128 Indexed
s32 HIRInstrEmit_stvx128(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorIndexed(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Store Vector Indexed Last (x'7C00 03CE')
s32 HIRInstrEmit_stvxl(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_stvx(b, instr);
}

// Store Vector 128 Indexed Last
s32 HIRInstrEmit_stvxl128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_stvx128(b, instr);
}

// Helper to perform stvlx*
s32 StoreVectorLeftIndexed(HIRBuilder &b, const uPPCInstr &instr, u32 vD, u32 rA, u32 rB) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, rA, rB);
  // Get element index
  Value *eb = b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantInt8(0xF));
  // Align EA
  EA = b.And(EA, b.LoadConstantUint64(~0xFull));
  // v = (old & ~mask) | ((new >> eb) & mask)
  Value *new_value = b.Permute(b.LoadVectorShr(eb), b.LoadZeroVec128(), b.LoadVR(vD), INT8_TYPE);
  Value *old_value = b.ByteSwap(b.Load(EA, VEC128_TYPE));
  // mask = FFFF... >> eb
  Value *mask = b.Permute(b.LoadVectorShr(eb), b.LoadZeroVec128(), b.Not(b.LoadZeroVec128()), INT8_TYPE);
  Value *v = b.Or(b.AndNot(old_value, mask), b.And(new_value, mask));
  // Perform the store
  b.Store(EA, b.ByteSwap(v));
  return 0;
}

// Store Vector Left Indexed (x'7C00 050E')
s32 HIRInstrEmit_stvlx(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorLeftIndexed(b, instr, instr.rd, instr.ra, instr.rb);
}

// Store Vector 128 Left Indexed
s32 HIRInstrEmit_stvlx128(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorLeftIndexed(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Store Vector Left Indexed Last (x'7C00 070E')
s32 HIRInstrEmit_stvlxl(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_stvlx(b, instr);
}

// Store Vector 128 Left Indexed Last
s32 HIRInstrEmit_stvlxl128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_stvlx128(b, instr);
}

// Helper to perform stvrx*
s32 StoreVectorRightIndexed(HIRBuilder &b, const uPPCInstr &instr, u32 vd, u32 ra, u32 rb) {
  // NOTE: if eb == 0 (16b aligned) then no data should be stored. The aligned
  // address may hang off the end of a valid page (memcpy tail handling).
  // When eb==0, LoadVectorShr(0) makes the mask all-ones, which would incorrectly
  // overwrite memory. We guard the mask with a condition so it becomes all-zeros
  // when eb==0, making the store write back old_value unchanged (semantic no-op).

  // Get EA
  Value *EA = GetEA_rA_0_rB(b, ra, rb);
  // Get element index
  Value *eb = b.And(b.Truncate(EA, INT8_TYPE), b.LoadConstantInt8(0xF));
  // Check if EB == 0 (no data is to be loaded)
  Value *isNonZero = b.IsTrue(eb);
  // Aligned EA
  EA = b.And(EA, b.LoadConstantUint64(~0xFull));
  // v = (old & ~mask) | ((new << eb) & mask)
  Value *new_value = b.Permute(b.LoadVectorShr(eb), b.LoadVR(vd), b.LoadZeroVec128(), INT8_TYPE);
  Value *old_value = b.ByteSwap(b.Load(EA, VEC128_TYPE));
  // mask = ~FFFF... >> eb
  Value *rawMask = b.Permute(b.LoadVectorShr(eb), b.Not(b.LoadZeroVec128()), b.LoadZeroVec128(), INT8_TYPE);
  // Guard the mask: when eb==0, force it to all-zeros so v == old_value.
  // Splat the condition byte (0x00 or 0xFF) to all 16 lanes, then AND with mask.
  Value *condByte = b.Select(isNonZero, b.LoadConstantUint8(0xFF), b.LoadConstantUint8(0x00));
  Value *condVec = b.Splat(condByte, VEC128_TYPE);
  Value *mask = b.And(rawMask, condVec);
  Value *v = b.Or(b.AndNot(old_value, mask), b.And(new_value, mask));
  // Perform the store
  b.Store(EA, b.ByteSwap(v));
  return 0;
}

// Store Vector Right Indexed (x'7C00 054E')
s32 HIRInstrEmit_stvrx(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorRightIndexed(b, instr, instr.rd, instr.ra, instr.rb);
}

// Store Vector 128 Right Indexed
s32 HIRInstrEmit_stvrx128(HIRBuilder &b, const uPPCInstr &instr) {
  return StoreVectorRightIndexed(b, instr, J_VMX128_1_VD128, instr.VMX128_1.RA, instr.VMX128_1.RB);
}

// Store Vector Right Indexed Last (x'7C00 074E')
s32 HIRInstrEmit_stvrxl(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_stvrx(b, instr);
}

// Store Vector 128 Right Indexed Last
s32 HIRInstrEmit_stvrxl128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_stvrx128(b, instr);
}


//
// Vector Integer and Floating pos32 operations
//


s32 HIRInstrEmit_vaddcuw(HIRBuilder &b, const uPPCInstr &instr) {
  Value *sum = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_UNSIGNED);
  Value *overflow = b.VectorCompareUGT(b.LoadVR(instr.va), sum, INT32_TYPE);
  Value *carry =
    b.VectorShr(overflow, b.LoadConstantVec128(Base::Vector128i(31)), INT32_TYPE);
  b.StoreVR(instr.vd, carry);
  return 0;
}

s32 HIRInstrEmit_vaddfp_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- (VA) + (VB) (4 x fp)
  Value *v = b.VectorAdd(b.LoadVR(va), b.LoadVR(vb), FLOAT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vaddfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vaddfp_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vaddfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vaddfp_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vaddsbs(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vaddshs(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vaddsws(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vaddubm(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vaddubs(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_UNSIGNED | ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vadduhm(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vadduhs(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_UNSIGNED | ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vadduwm(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vadduws(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAdd(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE, ARITHMETIC_UNSIGNED | ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vand_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // VD <- (VA) & (VB)
  Value *v = b.And(b.LoadVR(va), b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vand(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vand_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vand128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vand_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vandc_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // VD <- (VA) & �(VB)
  Value *v = b.AndNot(b.LoadVR(va), b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vandc(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vandc_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vandc128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vandc_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vavgsb(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAverage(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE, 0);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vavgsh(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v =
    b.VectorAverage(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE, 0);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vavgsw(HIRBuilder &b, const uPPCInstr &instr) {
  // do i = 0 to 127 by 32
  //   aop = EXTS((VRA)i:i + 31)
  //   bop = EXTS((VRB)i:i + 31)
  //   VRTi:i + 31 = Chop((aop + s32 bop + s32 1) >> 1, 32)
  Value *v =
    b.VectorAverage(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE, 0);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vavgub(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAverage(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vavguh(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAverage(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vavguw(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorAverage(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vcfsx_(HIRBuilder &b, u32 vd, u32 vb,
  u32 uimm) {
  // (VD) <- float(VB as signed) / 2^uimm
  Value *v = b.VectorConvertI2F(b.LoadVR(vb));
  if (uimm) {
    float fuimm = std::ldexp(1.0f, -int(uimm));
    v = b.Mul(v, b.Splat(b.LoadConstantFloat32(fuimm), VEC128_TYPE));
  }
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vcfsx(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcfsx_(b, instr.vd, instr.vb, instr.va);
}
s32 HIRInstrEmit_vcsxwfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcfsx_(b, J_VMX128_3_VD128, J_VMX128_3_VB128, J_VMX128_3_IMM);
}

s32 HIRInstrEmit_vcfux_(HIRBuilder &b, u32 vd, u32 vb,
  u32 uimm) {
  // (VD) <- float(VB as unsigned) / 2^uimm
  Value *v = b.VectorConvertI2F(b.LoadVR(vb), ARITHMETIC_UNSIGNED);
  if (uimm) {
    float fuimm = std::ldexp(1.0f, -int(uimm));
    v = b.Mul(v, b.Splat(b.LoadConstantFloat32(fuimm), VEC128_TYPE));
  }
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vcfux(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcfux_(b, instr.vd, instr.vb, instr.va);
}
s32 HIRInstrEmit_vcuxwfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcfux_(b, J_VMX128_3_VD128, J_VMX128_3_VB128, J_VMX128_3_IMM);
}

s32 HIRInstrEmit_vctsxs_(HIRBuilder &b, u32 vd, u32 vb, u32 uimm) {
  // (VD) <- int_sat(VB as signed * 2^uimm)
  float fuimm = static_cast<float>(std::exp2(uimm));
  Value *v = b.Mul(b.LoadVR(vb), b.Splat(b.LoadConstantFloat32(fuimm), VEC128_TYPE));
  v = b.VectorConvertF2I(v);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vctsxs(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vctsxs_(b, instr.vd, instr.vb, instr.va);
}
s32 HIRInstrEmit_vcfpsxws128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vctsxs_(b, J_VMX128_3_VD128, J_VMX128_3_VB128, J_VMX128_3_IMM);
}

s32 HIRInstrEmit_vctuxs_(HIRBuilder &b, u32 vd, u32 vb, u32 uimm) {
  // (VD) <- int_sat(VB as unsigned * 2^uimm)
  float fuimm = static_cast<float>(std::exp2(uimm));
  Value *v = b.Mul(b.LoadVR(vb), b.Splat(b.LoadConstantFloat32(fuimm), VEC128_TYPE));
  v = b.VectorConvertF2I(v, ARITHMETIC_UNSIGNED);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vctuxs(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vctuxs_(b, instr.vd, instr.vb, instr.va);
}
s32 HIRInstrEmit_vcfpuxws128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vctuxs_(b, J_VMX128_3_VD128, J_VMX128_3_VB128, J_VMX128_3_IMM);
}

s32 HIRInstrEmit_vcmpbfp_(HIRBuilder &b, const uPPCInstr &instr, u32 vd,
  u32 va, u32 vb, u32 rc) {
  // if vA or vB are NaN, the 2 high-order bits are set (0xC0000000)
  Value *va_value = b.LoadVR(va);
  Value *vb_value = b.LoadVR(vb);
  Value *gt = b.VectorCompareSGT(va_value, vb_value, FLOAT32_TYPE);
  Value *lt =
    b.Not(b.VectorCompareSGE(va_value, b.Neg(vb_value), FLOAT32_TYPE));
  Value *v =
    b.Or(b.And(gt, b.LoadConstantVec128(Base::Vector128i(0x80000000, 0x80000000,
      0x80000000, 0x80000000))),
      b.And(lt, b.LoadConstantVec128(Base::Vector128i(0x40000000, 0x40000000,
        0x40000000, 0x40000000))));
  b.StoreVR(vd, v);
  if (rc) {
    // CR0:4 = 0; CR0:5 = VT == 0; CR0:6 = CR0:7 = 0;
    // If all of the elements are within bounds, CR6[2] is set
    // FIXME: Does not affect CR6[0], but the following function does.
    b.UpdateCR6(b.Or(gt, lt));
  }
  return 0;
}
s32 HIRInstrEmit_vcmpbfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpbfp_(b, instr, instr.vd, instr.va, instr.vb, instr.vrc);
}
s32 HIRInstrEmit_vcmpbfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpbfp_(b, instr, J_VMX128_R_VD128, J_VMX128_R_VA128, J_VMX128_R_VB128,
    instr.VMX128_R.Rc);
}

enum vcmpxxfp_op {
  vcmpxxfp_eq,
  vcmpxxfp_gt,
  vcmpxxfp_ge,
};
s32 HIRInstrEmit_vcmpxxfp_(HIRBuilder &b, const uPPCInstr &instr, vcmpxxfp_op cmpop,
  u32 vd, u32 va, u32 vb, u32 rc) {
  // (VD.xyzw) = (VA.xyzw) OP (VB.xyzw) ? 0xFFFFFFFF : 0x00000000
  // if (Rc) CR6 = all_equal | 0 | none_equal | 0
  // If an element in either VA or VB is NaN the result will be 0x00000000
  Value *v;
  switch (cmpop) {
  case vcmpxxfp_eq:
    v = b.VectorCompareEQ(b.LoadVR(va), b.LoadVR(vb), FLOAT32_TYPE);
    break;
  case vcmpxxfp_gt:
    v = b.VectorCompareSGT(b.LoadVR(va), b.LoadVR(vb), FLOAT32_TYPE);
    break;
  case vcmpxxfp_ge:
    v = b.VectorCompareSGE(b.LoadVR(va), b.LoadVR(vb), FLOAT32_TYPE);
    break;
  default:
    UNREACHABLE();
    return 1;
  }
  if (rc) {
    b.UpdateCR6(v);
  }
  b.StoreVR(vd, v);
  return 0;
}

s32 HIRInstrEmit_vcmpeqfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxfp_(b, instr, vcmpxxfp_eq, instr.vd, instr.va, instr.vb,
    instr.vrc);
}
s32 HIRInstrEmit_vcmpeqfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxfp_(b, instr, vcmpxxfp_eq, J_VMX128_R_VD128, J_VMX128_R_VA128,
    J_VMX128_R_VB128, instr.VMX128_R.Rc);
}
s32 HIRInstrEmit_vcmpgefp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxfp_(b, instr, vcmpxxfp_ge, instr.vd, instr.va, instr.vb,
    instr.vrc);
}
s32 HIRInstrEmit_vcmpgefp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxfp_(b, instr, vcmpxxfp_ge, J_VMX128_R_VD128, J_VMX128_R_VA128,
    J_VMX128_R_VB128, instr.VMX128_R.Rc);
}
s32 HIRInstrEmit_vcmpgtfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxfp_(b, instr, vcmpxxfp_gt, instr.vd, instr.va, instr.vb,
    instr.vrc);
}
s32 HIRInstrEmit_vcmpgtfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxfp_(b, instr, vcmpxxfp_gt, J_VMX128_R_VD128, J_VMX128_R_VA128,
    J_VMX128_R_VB128, instr.VMX128_R.Rc);
}

enum vcmpxxi_op {
  vcmpxxi_eq,
  vcmpxxi_gt_signed,
  vcmpxxi_gt_unsigned,
};
s32 HIRInstrEmit_vcmpxxi_(HIRBuilder &b, const uPPCInstr &instr, vcmpxxi_op cmpop,
  u32 width, u32 vd, u32 va, u32 vb,
  u32 rc) {
  // (VD.xyzw) = (VA.xyzw) OP (VB.xyzw) ? 0xFFFFFFFF : 0x00000000
  // if (Rc) CR6 = all_equal | 0 | none_equal | 0
  // If an element in either VA or VB is NaN the result will be 0x00000000
  Value *v;
  switch (cmpop) {
  case vcmpxxi_eq:
    switch (width) {
    case 1:
      v = b.VectorCompareEQ(b.LoadVR(va), b.LoadVR(vb), INT8_TYPE);
      break;
    case 2:
      v = b.VectorCompareEQ(b.LoadVR(va), b.LoadVR(vb), INT16_TYPE);
      break;
    case 4:
      v = b.VectorCompareEQ(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
      break;
    default:
      UNREACHABLE();
      return 1;
    }
    break;
  case vcmpxxi_gt_signed:
    switch (width) {
    case 1:
      v = b.VectorCompareSGT(b.LoadVR(va), b.LoadVR(vb), INT8_TYPE);
      break;
    case 2:
      v = b.VectorCompareSGT(b.LoadVR(va), b.LoadVR(vb), INT16_TYPE);
      break;
    case 4:
      v = b.VectorCompareSGT(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
      break;
    default:
      UNREACHABLE();
      return 1;
    }
    break;
  case vcmpxxi_gt_unsigned:
    switch (width) {
    case 1:
      v = b.VectorCompareUGT(b.LoadVR(va), b.LoadVR(vb), INT8_TYPE);
      break;
    case 2:
      v = b.VectorCompareUGT(b.LoadVR(va), b.LoadVR(vb), INT16_TYPE);
      break;
    case 4:
      v = b.VectorCompareUGT(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
      break;
    default:
      UNREACHABLE();
      return 1;
    }
    break;
  default:
    UNREACHABLE();
    return 1;
  }
  if (rc) {
    b.UpdateCR6(v);
  }
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vcmpequb(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_eq, 1, instr.vd, instr.va, instr.vb,
    instr.vrc);
}
s32 HIRInstrEmit_vcmpequh(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_eq, 2, instr.vd, instr.va, instr.vb,
    instr.vrc);
}
s32 HIRInstrEmit_vcmpequw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_eq, 4, instr.vd, instr.va, instr.vb,
    instr.vrc);
}
s32 HIRInstrEmit_vcmpequw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_eq, 4, J_VMX128_R_VD128, J_VMX128_R_VA128,
    J_VMX128_R_VB128, instr.VMX128_R.Rc);
}
s32 HIRInstrEmit_vcmpgtsb(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_gt_signed, 1, instr.vd, instr.va,
    instr.vb, instr.vrc);
}
s32 HIRInstrEmit_vcmpgtsh(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_gt_signed, 2, instr.vd, instr.va,
    instr.vb, instr.vrc);
}
s32 HIRInstrEmit_vcmpgtsw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_gt_signed, 4, instr.vd, instr.va,
    instr.vb, instr.vrc);
}
s32 HIRInstrEmit_vcmpgtub(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_gt_unsigned, 1, instr.vd, instr.va,
    instr.vb, instr.vrc);
}
s32 HIRInstrEmit_vcmpgtuh(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_gt_unsigned, 2, instr.vd, instr.va,
    instr.vb, instr.vrc);
}
s32 HIRInstrEmit_vcmpgtuw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vcmpxxi_(b, instr, vcmpxxi_gt_unsigned, 4, instr.vd, instr.va,
    instr.vb, instr.vrc);
}

s32 HIRInstrEmit_vexptefp_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- pow2(VB)
  Value *v = b.Pow2(b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vexptefp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vexptefp_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vexptefp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vexptefp_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vlogefp_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- log2(VB)
  Value *v = b.Log2(b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vlogefp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vlogefp_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vlogefp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vlogefp_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vmaddfp_(HIRBuilder &b, u32 vd, u32 va, u32 vb,
  u32 vc) {
  // (VD) <- ((VA) * (VC)) + (VB)
  Value *v = b.MulAdd(b.LoadVR(va), b.LoadVR(vc), b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vmaddfp(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- ((VA) * (VC)) + (VB)
  return HIRInstrEmit_vmaddfp_(b, instr.vd, instr.va, instr.vb, instr.vc);
}
s32 HIRInstrEmit_vmaddfp128(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- ((VA) * (VB)) + (VD)
  // NOTE: this resuses VD and swaps the arg order!
  return HIRInstrEmit_vmaddfp_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VD128,
    J_VMX128_VB128);
}

s32 HIRInstrEmit_vmaddcfp128(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- ((VA) * (VD)) + (VB)
  Value *v = b.MulAdd(b.LoadVR(J_VMX128_VA128), b.LoadVR(J_VMX128_VD128),
    b.LoadVR(J_VMX128_VB128));
  b.StoreVR(J_VMX128_VD128, v);
  return 0;
}

s32 HIRInstrEmit_vmaxfp_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- max((VA), (VB))
  Value *v = b.Max(b.LoadVR(va), b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vmaxfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vmaxfp_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vmaxfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vmaxfp_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vmaxsb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- max((VA), (VB)) (signed int8)
  Value *v = b.VectorMax(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmaxsh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- max((VA), (VB)) (signed int16)
  Value *v = b.VectorMax(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmaxsw(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- max((VA), (VB)) (signed int32)
  Value *v = b.VectorMax(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmaxub(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- max((VA), (VB)) (unsigned int8)
  Value *v = b.VectorMax(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmaxuh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- max((VA), (VB)) (unsigned int16)
  Value *v = b.VectorMax(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmaxuw(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- max((VA), (VB)) (unsigned int32)
  Value *v = b.VectorMax(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmhaddshs(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vmhraddshs(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vminfp_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- min((VA), (VB))
  Value *v = b.Min(b.LoadVR(va), b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vminfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vminfp_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vminfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vminfp_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vminsb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- min((VA), (VB)) (signed int8)
  Value *v = b.VectorMin(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vminsh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- min((VA), (VB)) (signed int16)
  Value *v = b.VectorMin(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vminsw(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- min((VA), (VB)) (signed int32)
  Value *v = b.VectorMin(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vminub(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- min((VA), (VB)) (unsigned int8)
  Value *v = b.VectorMin(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vminuh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- min((VA), (VB)) (unsigned int16)
  Value *v = b.VectorMin(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vminuw(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- min((VA), (VB)) (unsigned int32)
  Value *v = b.VectorMin(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmladduhm(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vmrghb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.b[i]) = (VA.b[i])
  // (VD.b[i+1]) = (VB.b[i+1])
  // ...
  Value *v = b.Permute(b.LoadConstantVec128(Base::Vector128b(0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23)), 
    b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmrghh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.w[i]) = (VA.w[i])
  // (VD.w[i+1]) = (VB.w[i+1])
  // ...
  Value *v = b.Permute(b.LoadConstantVec128(Base::Vector128s(0, 8, 1, 9, 2, 10, 3, 11)),
    b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmrghw_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD.x) = (VA.x)
  // (VD.y) = (VB.x)
  // (VD.z) = (VA.y)
  // (VD.w) = (VB.y)
  Value *v =
    b.Permute(b.LoadConstantUint32(MakePermuteMask(0, 0, 1, 0, 0, 1, 1, 1)),
      b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vmrghw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vmrghw_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vmrghw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vmrghw_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vmrglb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.b[i]) = (VA.b[i])
  // (VD.b[i+1]) = (VB.b[i+1])
  // ...
  Value *v =
    b.Permute(b.LoadConstantVec128(Base::Vector128b(8, 24, 9, 25, 10, 26, 11, 27, 12,
      28, 13, 29, 14, 30, 15, 31)),
      b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmrglh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.w[i]) = (VA.w[i])
  // (VD.w[i+1]) = (VB.w[i+1])
  // ...
  Value *v =
    b.Permute(b.LoadConstantVec128(Base::Vector128s(4, 12, 5, 13, 6, 14, 7, 15)),
      b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vmrglw_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD.x) = (VA.z)
  // (VD.y) = (VB.z)
  // (VD.z) = (VA.w)
  // (VD.w) = (VB.w)
  Value *v =
    b.Permute(b.LoadConstantUint32(MakePermuteMask(0, 2, 1, 2, 0, 3, 1, 3)),
      b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vmrglw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vmrglw_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vmrglw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vmrglw_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vmsum3fp128(HIRBuilder &b, const uPPCInstr &instr) {
  // Dot product XYZ.
  // (VD.xyzw) = (VA.x * VB.x) + (VA.y * VB.y) + (VA.z * VB.z)
  Value *v = b.DotProduct3(b.LoadVR(J_VMX128_VA128), b.LoadVR(J_VMX128_VB128));
  v = b.Splat(v, VEC128_TYPE);
  b.StoreVR(J_VMX128_VD128, v);
  return 0;
}

s32 HIRInstrEmit_vmsum4fp128(HIRBuilder &b, const uPPCInstr &instr) {
  // Dot product XYZW.
  // (VD.xyzw) = (VA.x * VB.x) + (VA.y * VB.y) + (VA.z * VB.z) + (VA.w * VB.w)
  Value *v = b.DotProduct4(b.LoadVR(J_VMX128_VA128), b.LoadVR(J_VMX128_VB128));
  v = b.Splat(v, VEC128_TYPE);
  b.StoreVR(J_VMX128_VD128, v);
  return 0;
}

s32 HIRInstrEmit_vmulfp128(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (VA) * (VB) (4 x fp)
  Value *v = b.Mul(b.LoadVR(J_VMX128_VA128), b.LoadVR(J_VMX128_VB128));
  b.StoreVR(J_VMX128_VD128, v);
  return 0;
}

s32 HIRInstrEmit_vnmsubfp_(HIRBuilder &b, u32 vd, u32 va, u32 vb,
  u32 vc) {
  // (VD) <- -(((VA) * (VC)) - (VB))
  // NOTE: only one rounding should take place, but that's hard...
  // This really needs VFNMSUB132PS/VFNMSUB213PS/VFNMSUB231PS but that's AVX.
  Value *v = b.Neg(b.MulSub(b.LoadVR(va), b.LoadVR(vc), b.LoadVR(vb)));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vnmsubfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vnmsubfp_(b, instr.vd, instr.va, instr.vb, instr.vc);
}
s32 HIRInstrEmit_vnmsubfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vnmsubfp_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VD128,
    J_VMX128_VB128);
}

s32 HIRInstrEmit_vnor_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // VD <- �((VA) | (VB))
  Value *v = b.Not(b.Or(b.LoadVR(va), b.LoadVR(vb)));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vnor(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vnor_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vnor128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vnor_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vor_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // VD <- (VA) | (VB)
  if (va == vb) {
    // Copy VA==VB into VD.
    b.StoreVR(vd, b.LoadVR(va));
  }
  else {
    Value *v = b.Or(b.LoadVR(va), b.LoadVR(vb));
    b.StoreVR(vd, v);
  }
  return 0;
}
s32 HIRInstrEmit_vor(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vor_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vor128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vor_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vperm_(HIRBuilder &b, u32 vd, u32 va, u32 vb,
  u32 vc) {
  Value *v = b.Permute(b.LoadVR(vc), b.LoadVR(va), b.LoadVR(vb), INT8_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vperm(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vperm_(b, instr.vd, instr.va, instr.vb, instr.vc);
}
s32 HIRInstrEmit_vperm128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vperm_(b, J_VMX128_2_VD128, J_VMX128_2_VA128, J_VMX128_2_VB128,
    J_VMX128_2_VC);
}

s32 HIRInstrEmit_vpermwi128(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.x) = (VB.uimm[6-7])
  // (VD.y) = (VB.uimm[4-5])
  // (VD.z) = (VB.uimm[2-3])
  // (VD.w) = (VB.uimm[0-1])
  const u32 vd = instr.VMX128_P.VD128l | (instr.VMX128_P.VD128h << 5);
  const u32 vb = instr.VMX128_P.VB128l | (instr.VMX128_P.VB128h << 5);
  u32 uimm = instr.VMX128_P.PERMl | (instr.VMX128_P.PERMh << 5);
  u32 mask = MakeSwizzleMask(uimm >> 6, uimm >> 4, uimm >> 2, uimm >> 0);
  Value *v = b.Swizzle(b.LoadVR(vb), INT32_TYPE, mask);
  b.StoreVR(vd, v);
  return 0;
}

s32 HIRInstrEmit_vrefp_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- 1/(VB)
  Value *v = b.Recip(b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrefp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrefp_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vrefp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrefp_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vrfim_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- RndToFPInt32Floor(VB)
  Value *v = b.Round(b.LoadVR(vb), ROUND_TO_MINUS_INFINITY);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrfim(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfim_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vrfim128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfim_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vrfin_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- RoundToNearest(VB)
  Value *v = b.Round(b.LoadVR(vb), ROUND_TO_NEAREST);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrfin(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfin_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vrfin128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfin_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vrfip_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- RndToFPInt32Ceil(VB)
  Value *v = b.Round(b.LoadVR(vb), ROUND_TO_POSITIVE_INFINITY);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrfip(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfip_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vrfip128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfip_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vrfiz_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- RndToFPInt32Trunc(VB)
  Value *v = b.Round(b.LoadVR(vb), ROUND_TO_ZERO);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrfiz(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfiz_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vrfiz128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrfiz_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vrlb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- ROTL((VA), (VB)&0x3)
  Value *v =
    b.VectorRotateLeft(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vrlh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- ROTL((VA), (VB)&0xF)
  Value *v =
    b.VectorRotateLeft(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vrlw_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- ROTL((VA), (VB)&0x1F)
  Value *v = b.VectorRotateLeft(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrlw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrlw_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vrlw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrlw_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vrlimi128(HIRBuilder &b, const uPPCInstr &instr) {
  const u32 vd = instr.VMX128_4.VD128l | (instr.VMX128_4.VD128h << 5);
  const u32 vb = instr.VMX128_4.VB128l | (instr.VMX128_4.VB128h << 5);
  u32 blend_mask_src = instr.VMX128_4.IMM;
  u32 blend_mask = 0;
  blend_mask |= (((blend_mask_src >> 3) & 0x1) ? 0 : 4) << 0;
  blend_mask |= (((blend_mask_src >> 2) & 0x1) ? 1 : 5) << 8;
  blend_mask |= (((blend_mask_src >> 1) & 0x1) ? 2 : 6) << 16;
  blend_mask |= (((blend_mask_src >> 0) & 0x1) ? 3 : 7) << 24;
  u32 rotate = instr.VMX128_4.z;
  // This is just a fancy permute.
  // X Y Z W, rotated left by 2 = Z W X Y
  // Then mask select the results into the dest.
  // Sometimes rotation is zero, so fast path.
  Value *v;
  if (rotate) {
    // TODO(benvanik): constants need conversion.
    u32 swizzle_mask;
    switch (rotate) {
    case 1:
      // X Y Z W -> Y Z W X
      swizzle_mask = SWIZZLE_XYZW_TO_YZWX;
      break;
    case 2:
      // X Y Z W -> Z W X Y
      swizzle_mask = SWIZZLE_XYZW_TO_ZWXY;
      break;
    case 3:
      // X Y Z W -> W X Y Z
      swizzle_mask = SWIZZLE_XYZW_TO_WXYZ;
      break;
    default:
      INSTRNOTIMPLEMENTED();
      return 1;
    }
    v = b.Swizzle(b.LoadVR(vb), FLOAT32_TYPE, swizzle_mask);
  }
  else {
    v = b.LoadVR(vb);
  }
  if (blend_mask != kIdentityPermuteMask) {
    v = b.Permute(b.LoadConstantUint32(blend_mask), v, b.LoadVR(vd),
      INT32_TYPE);
  }
  b.StoreVR(vd, v);
  return 0;
}

s32 HIRInstrEmit_vrsqrtefp_(HIRBuilder &b, u32 vd, u32 vb) {
  // (VD) <- 1 / sqrt(VB)
  // There are a lot of rules in the Altivec_PEM docs for handlings that
  // result in nan/infinity/etc. They are ignored here. I hope games would
  // never rely on them.
  Value *v = b.RSqrt(b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vrsqrtefp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrsqrtefp_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vrsqrtefp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vrsqrtefp_(b, J_VMX128_3_VD128, J_VMX128_3_VB128);
}

s32 HIRInstrEmit_vsel_(HIRBuilder &b, u32 vd, u32 va, u32 vb,
  u32 vc) {
  // For each bit:
  // VRTi <- ((VRC)i=0) ? (VRA)i : (VRB)i
  Value *v = b.Select(b.LoadVR(vc), b.LoadVR(va), b.LoadVR(vb));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vsel(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsel_(b, instr.vd, instr.va, instr.vb, instr.vc);
}
s32 HIRInstrEmit_vsel128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsel_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128, J_VMX128_VD128);
}

s32 HIRInstrEmit_vsl(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.Shl(b.LoadVR(instr.va), b.And(b.Extract(b.LoadVR(instr.vb), 15, INT8_TYPE), b.LoadConstantInt8(0b111)));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vslb(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorShl(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vslh(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.VectorShl(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vslw_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // VA = |xxxxx|yyyyy|zzzzz|wwwww|
  // VB = |...sh|...sh|...sh|...sh|
  // VD = |x<<sh|y<<sh|z<<sh|w<<sh|
  Value *v = b.VectorShl(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vslw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vslw_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vslw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vslw_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

static const vec128_t __vsldoi_table[16] = {
    Base::Vector128b(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
    Base::Vector128b(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
    Base::Vector128b(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17),
    Base::Vector128b(3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18),
    Base::Vector128b(4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19),
    Base::Vector128b(5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20),
    Base::Vector128b(6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21),
    Base::Vector128b(7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22),
    Base::Vector128b(8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23),
    Base::Vector128b(9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24),
    Base::Vector128b(10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25),
    Base::Vector128b(11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    Base::Vector128b(12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27),
    Base::Vector128b(13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28),
    Base::Vector128b(14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29),
    Base::Vector128b(15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30),
};
s32 HIRInstrEmit_vsldoi_(HIRBuilder &b, u32 vd, u32 va, u32 vb,
  u32 sh) {
  // (VD) <- ((VA) || (VB)) << (SH << 3)
  if (!sh) {
    b.StoreVR(vd, b.LoadVR(va));
    return 0;
  }
  else if (sh == 16) {
    b.StoreVR(vd, b.LoadVR(vb));
    return 0;
  }
  // TODO(benvanik): optimize for the rotation case:
  // vsldoi128 vr63,vr63,vr63,4
  // (ABCD ABCD) << 4b = (BCDA)
  // (VA << SH) OR (VB >> (16 - SH))
  Value *control = b.LoadConstantVec128(__vsldoi_table[sh]);
  Value *v = b.Permute(control, b.LoadVR(va), b.LoadVR(vb), INT8_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vsldoi(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsldoi_(b, instr.vd, instr.va, instr.vb, instr.vc & 0xF);
}
s32 HIRInstrEmit_vsldoi128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsldoi_(b, J_VMX128_5_VD128, J_VMX128_5_VA128, J_VMX128_5_VB128,
    J_VMX128_5_SH);
}

s32 HIRInstrEmit_vslo_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- (VA) << (VB.b[F] & 0x78) (by octet)
  // TODO(benvanik): flag for shift-by-octet as optimization.
  Value *sh = b.Shr(
    b.And(b.Extract(b.LoadVR(vb), 15, INT8_TYPE), b.LoadConstantInt8(0x78)),
    3);
  Value *v = b.Permute(b.LoadVectorShl(sh), b.LoadVR(va), b.LoadZeroVec128(),
    INT8_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vslo(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vslo_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vslo128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vslo_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vspltb(HIRBuilder &b, const uPPCInstr &instr) {
  // b <- UIMM*8
  // do i = 0 to 127 by 8
  //  (VD)[i:i+7] <- (VB)[b:b+7]
  Value *byte = b.Extract(b.LoadVR(instr.vb), instr.va & 0xF, INT8_TYPE);
  Value *v = b.Splat(byte, VEC128_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsplth(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.xyzw) <- (VB.uimm)
  Value *h = b.Extract(b.LoadVR(instr.vb), instr.va & 0x7, INT16_TYPE);
  Value *v = b.Splat(h, VEC128_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vspltw_(HIRBuilder &b, u32 vd, u32 vb,
  u32 uimm) {
  // (VD.xyzw) <- (VB.uimm)
  Value *w = b.Extract(b.LoadVR(vb), uimm & 0x3, INT32_TYPE);
  Value *v = b.Splat(w, VEC128_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vspltw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vspltw_(b, instr.vd, instr.vb, instr.va);
}
s32 HIRInstrEmit_vspltw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vspltw_(b, J_VMX128_3_VD128, J_VMX128_3_VB128, J_VMX128_3_IMM);
}

s32 HIRInstrEmit_vspltisb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.xyzw) <- sign_extend(uimm)
  Value *v;
  if (instr.va) {
    // Sign extend from 5bits -> 8 and load.
    int8_t simm = (instr.va & 0x10) ? (instr.va | 0xF0) : instr.va;
    v = b.Splat(b.LoadConstantInt8(simm), VEC128_TYPE);
  } else {
    // Zero out the register.
    v = b.LoadZeroVec128();
  }
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vspltish(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD.xyzw) <- sign_extend(uimm)
  Value *v;
  if (instr.va) {
    // Sign extend from 5bits -> 16 and load.
    int16_t simm = (instr.va & 0x10) ? (instr.va | 0xFFF0) : instr.va;
    v = b.Splat(b.LoadConstantInt16(simm), VEC128_TYPE);
  }
  else {
    // Zero out the register.
    v = b.LoadZeroVec128();
  }
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vspltisw_(HIRBuilder &b, u32 vd, u32 uimm) {
  // (VD.xyzw) <- sign_extend(uimm)
  Value *v;
  if (uimm) {
    // Sign extend from 5bits -> 32 and load.
    int32_t simm = (uimm & 0x10) ? (uimm | 0xFFFFFFF0) : uimm;
    v = b.Splat(b.LoadConstantInt32(simm), VEC128_TYPE);
  }
  else {
    // Zero out the register.
    v = b.LoadZeroVec128();
  }
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vspltisw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vspltisw_(b, instr.vd, instr.va);
}
s32 HIRInstrEmit_vspltisw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vspltisw_(b, J_VMX128_3_VD128, J_VMX128_3_IMM);
}

s32 HIRInstrEmit_vsr(HIRBuilder &b, const uPPCInstr &instr) {
  Value *v = b.Shr(b.LoadVR(instr.va),
    b.And(b.Extract(b.LoadVR(instr.vb), 15, INT8_TYPE),
      b.LoadConstantInt8(0b111)));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsrab(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (VA) >>a (VB) by bytes
  Value *v = b.VectorSha(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsrah(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (VA) >>a (VB) by halfwords
  Value *v = b.VectorSha(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsraw_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- (VA) >>a (VB) by words
  Value *v = b.VectorSha(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vsraw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsraw_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vsraw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsraw_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vsrb(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (VA) >> (VB) by bytes
  Value *v = b.VectorShr(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsrh(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (VA) >> (VB) by halfwords
  Value *v = b.VectorShr(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsro_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- (VA) >> (VB.b[F] & 0x78) (by octet)
  // TODO(benvanik): flag for shift-by-octet as optimization.
  Value *sh = b.Shr(
    b.And(b.Extract(b.LoadVR(vb), 15, INT8_TYPE), b.LoadConstantInt8(0x78)),
    3);
  Value *v = b.Permute(b.LoadVectorShr(sh), b.LoadZeroVec128(), b.LoadVR(va),
    INT8_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vsro(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsro_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vsro128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsro_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vsrw_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- (VA) >> (VB) by words
  Value *v = b.VectorShr(b.LoadVR(va), b.LoadVR(vb), INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vsrw(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsrw_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vsrw128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsrw_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vsubcuw(HIRBuilder &b, const uPPCInstr &instr) {
  Value *underflow =
    b.VectorCompareUGE(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE);
  Value *borrow =
    b.VectorShr(underflow, b.LoadConstantVec128(Base::Vector128i(31)), INT32_TYPE);
  b.StoreVR(instr.vd, borrow);
  return 1;
}

s32 HIRInstrEmit_vsubfp_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // (VD) <- (VA) - (VB) (4 x fp)
  Value *v = b.VectorSub(b.LoadVR(va), b.LoadVR(vb), FLOAT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vsubfp(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsubfp_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vsubfp128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vsubfp_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vsubsbs(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- clamp(EXTS(VA) + �EXTS(VB) + 1, -128, 127)
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsubshs(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- clamp(EXTS(VA) + �EXTS(VB) + 1, -2^15, 2^15-1)
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsubsws(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- clamp(EXTS(VA) + �EXTS(VB) + 1, -2^31, 2^31-1)
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsububm(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (EXTZ(VA) + �EXTZ(VB) + 1) % 256
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsubuhm(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (EXTZ(VA) + �EXTZ(VB) + 1) % 2^16
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsubuwm(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- (EXTZ(VA) + �EXTZ(VB) + 1) % 2^32
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_UNSIGNED);
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsububs(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- clamp(EXTZ(VA) + �EXTZ(VB) + 1, 0, 256)
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT8_TYPE,
    ARITHMETIC_SATURATE | ARITHMETIC_UNSIGNED);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsubuhs(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- clamp(EXTZ(VA) + �EXTZ(VB) + 1, 0, 2^16-1)
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT16_TYPE,
    ARITHMETIC_SATURATE | ARITHMETIC_UNSIGNED);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsubuws(HIRBuilder &b, const uPPCInstr &instr) {
  // (VD) <- clamp(EXTZ(VA) + �EXTZ(VB) + 1, 0, 2^32-1)
  Value *v = b.VectorSub(b.LoadVR(instr.va), b.LoadVR(instr.vb), INT32_TYPE,
    ARITHMETIC_SATURATE | ARITHMETIC_UNSIGNED);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(instr.vd, v);
  return 0;
}

s32 HIRInstrEmit_vsumsws(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vsum2sws(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vsum4sbs(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vsum4shs(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vsum4ubs(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vpkpx(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vpkshss_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Signed Halfword Signed Saturate
  // Convert VA and VB from signed words to signed saturated bytes then
  // concat:
  // for each i in VA + VB:
  //   i = int8_t(Clamp(EXTS(int16_t(t)), -128, 127))
  // dest = VA | VB (lower 8bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_8_IN_16 | PACK_TYPE_IN_SIGNED |
    PACK_TYPE_OUT_SIGNED | PACK_TYPE_OUT_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkshss(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkshss_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkshss128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkshss_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkswss_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Signed Word Signed Saturate
  // Convert VA and VB from signed s32 words to signed saturated shorts then
  // concat:
  // for each i in VA + VB:
  //   i = int16_t(Clamp(EXTS(int32_t(t)), -2^15, 2^15-1))
  // dest = VA | VB (lower 16bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_16_IN_32 | PACK_TYPE_IN_SIGNED |
    PACK_TYPE_OUT_SIGNED | PACK_TYPE_OUT_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkswss(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkswss_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkswss128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkswss_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkswus_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Signed Word Unsigned Saturate
  // Convert VA and VB from signed s32 words to unsigned saturated shorts then
  // concat:
  // for each i in VA + VB:
  //   i = uint16_t(Clamp(EXTS(int32_t(t)), 0, 2^16-1))
  // dest = VA | VB (lower 16bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_16_IN_32 | PACK_TYPE_IN_SIGNED |
    PACK_TYPE_OUT_UNSIGNED | PACK_TYPE_OUT_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkswus(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkswus_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkswus128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkswus_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkuhum_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Unsigned Halfword Unsigned Modulo
  // Convert VA and VB from unsigned shorts to unsigned bytes then concat:
  // for each i in VA + VB:
  //   i = uint8_t(uint16_t(i))
  // dest = VA | VB (lower 8bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_8_IN_16 | PACK_TYPE_IN_UNSIGNED |
    PACK_TYPE_OUT_UNSIGNED | PACK_TYPE_OUT_UNSATURATE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkuhum(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuhum_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkuhum128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuhum_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkuhus_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Unsigned Halfword Unsigned Saturate
  // Convert VA and VB from unsigned shorts to unsigned saturated bytes then
  // concat:
  // for each i in VA + VB:
  //   i = uint8_t(Clamp(EXTZ(uint16_t(i)), 0, 255))
  // dest = VA | VB (lower 8bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_8_IN_16 | PACK_TYPE_IN_UNSIGNED |
    PACK_TYPE_OUT_UNSIGNED | PACK_TYPE_OUT_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkuhus(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuhus_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkuhus128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuhus_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkshus_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Signed Halfword Unsigned Saturate
  // Convert VA and VB from signed shorts to unsigned saturated bytes then
  // concat:
  // for each i in VA + VB:
  //   i = uint8_t(Clamp(EXTS(int16_t(i)), 0, 255))
  // dest = VA | VB (lower 8bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_8_IN_16 | PACK_TYPE_IN_SIGNED |
    PACK_TYPE_OUT_UNSIGNED | PACK_TYPE_OUT_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkshus(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkshus_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkshus128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkshus_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkuwum_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Unsigned Word Unsigned Modulo
  // Concat low shorts from VA + VB:
  // for each i in VA + VB:
  //   i = uint16_t(u32(i))
  // dest = VA | VB (lower 16bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_16_IN_32 | PACK_TYPE_IN_UNSIGNED |
    PACK_TYPE_OUT_UNSIGNED | PACK_TYPE_OUT_UNSATURATE);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkuwum(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuwum_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkuwum128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuwum_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkuwus_(HIRBuilder &b, u32 vd, u32 va,
  u32 vb) {
  // Vector Pack Unsigned Word Unsigned Saturate
  // Convert VA and VB from unsigned s32 words to unsigned saturated shorts then
  // concat:
  // for each i in VA + VB:
  //   i = uint16_t(Clamp(EXTZ(u32(t)), 0, 2^16-1))
  // dest = VA | VB (lower 16bit values)
  Value *v = b.Pack(b.LoadVR(va), b.LoadVR(vb),
    PACK_TYPE_16_IN_32 | PACK_TYPE_IN_UNSIGNED |
    PACK_TYPE_OUT_UNSIGNED | PACK_TYPE_OUT_SATURATE);
  b.StoreSAT(b.DidSaturate(v));
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vpkuwus(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuwus_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vpkuwus128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vpkuwus_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vupkhpx(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vupklpx(HIRBuilder &b, const uPPCInstr &instr) {
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_vupkhsh_(HIRBuilder &b, u32 vd, u32 vb) {
  // Vector Unpack High Signed Halfword
  // halfwords 0-3 expanded to words 0-3 and sign extended
  Value *v =
    b.Unpack(b.LoadVR(vb), PACK_TYPE_TO_HI | PACK_TYPE_16_IN_32 |
      PACK_TYPE_IN_SIGNED | PACK_TYPE_OUT_SIGNED);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vupkhsh(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vupkhsh_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vupkhsh128(HIRBuilder &b, const uPPCInstr &instr) {
  u32 va = J_VMX128_VA128;
  ASSERT_ZERO(va);
  return HIRInstrEmit_vupkhsh_(b, J_VMX128_VD128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vupklsh_(HIRBuilder &b, u32 vd, u32 vb) {
  // Vector Unpack Low Signed Halfword
  // halfwords 4-7 expanded to words 0-3 and sign extended
  Value *v =
    b.Unpack(b.LoadVR(vb), PACK_TYPE_TO_LO | PACK_TYPE_16_IN_32 |
      PACK_TYPE_IN_SIGNED | PACK_TYPE_OUT_SIGNED);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vupklsh(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vupklsh_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vupklsh128(HIRBuilder &b, const uPPCInstr &instr) {
  u32 va = J_VMX128_VA128;
  ASSERT_ZERO(va);
  return HIRInstrEmit_vupklsh_(b, J_VMX128_VD128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vupkhsb_(HIRBuilder &b, u32 vd, u32 vb) {
  // Vector Unpack High Signed Byte
  // bytes 0-7 expanded to halfwords 0-7 and sign extended
  Value *v =
    b.Unpack(b.LoadVR(vb), PACK_TYPE_TO_HI | PACK_TYPE_8_IN_16 |
      PACK_TYPE_IN_SIGNED | PACK_TYPE_OUT_SIGNED);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vupkhsb(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vupkhsb_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vupkhsb128(HIRBuilder &b, const uPPCInstr &instr) {
  u32 va = J_VMX128_VA128;
  if (va == 0x60) {
    // Hrm, my instruction tables suck.
    return HIRInstrEmit_vupkhsh_(b, J_VMX128_VD128, J_VMX128_VB128);
  }
  return HIRInstrEmit_vupkhsb_(b, J_VMX128_VD128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vupklsb_(HIRBuilder &b, u32 vd, u32 vb) {
  // Vector Unpack Low Signed Byte
  // bytes 8-15 expanded to halfwords 0-7 and sign extended
  Value *v =
    b.Unpack(b.LoadVR(vb), PACK_TYPE_TO_LO | PACK_TYPE_8_IN_16 |
      PACK_TYPE_IN_SIGNED | PACK_TYPE_OUT_SIGNED);
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vupklsb(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vupklsb_(b, instr.vd, instr.vb);
}
s32 HIRInstrEmit_vupklsb128(HIRBuilder &b, const uPPCInstr &instr) {
  u32 va = J_VMX128_VA128;
  if (va == 0x60) {
    // Hrm, my instruction tables suck.
    return HIRInstrEmit_vupklsh_(b, J_VMX128_VD128, J_VMX128_VB128);
  }
  return HIRInstrEmit_vupklsb_(b, J_VMX128_VD128, J_VMX128_VB128);
}

s32 HIRInstrEmit_vpkd3d128(HIRBuilder &b, const uPPCInstr &instr) {
  const u32 vd = instr.VMX128_4.VD128l | (instr.VMX128_4.VD128h << 5);
  const u32 vb = instr.VMX128_4.VB128l | (instr.VMX128_4.VB128h << 5);
  u32 type = instr.VMX128_4.IMM >> 2;
  u32 pack = instr.VMX128_4.IMM & 0x3;
  u32 shift = instr.VMX128_4.z;
  Value *v = b.LoadVR(vb);
  switch (type) {
  case 0:  // VPACK_D3DCOLOR
    v = b.Pack(v, PACK_TYPE_D3DCOLOR);
    break;
  case 1:  // VPACK_NORMSHORT2
    v = b.Pack(v, PACK_TYPE_SHORT_2);
    break;
  case 2:  // VPACK_NORMPACKED32 2_10_10_10 w_z_y_x
    v = b.Pack(v, PACK_TYPE_UINT_2101010);
    break;
  case 3:  // VPACK_FLOAT16_2 DXGI_FORMAT_R16G16_FLOAT
    v = b.Pack(v, PACK_TYPE_FLOAT16_2);
    break;
  case 4:  // VPACK_NORMSHORT4
    v = b.Pack(v, PACK_TYPE_SHORT_4);
    break;
  case 5:  // VPACK_FLOAT16_4 DXGI_FORMAT_R16G16B16A16_FLOAT
    v = b.Pack(v, PACK_TYPE_FLOAT16_4);
    break;
  case 6:  // VPACK_NORMPACKED64 4_20_20_20 w_z_y_x
    // Used in 54540829 and other installments in the series, pretty rarely in
    // general.
    v = b.Pack(v, PACK_TYPE_ULONG_4202020);
    break;
  default:
    UNREACHABLE();
    return 1;
  }
  // https://hlssmod.net/he_code/public/pixelwriter.h
  // control = prev:0123 | new:4567
  u32 control = kIdentityPermuteMask;  // original
  switch (pack) {
  case 1:  // VPACK_32
    // VPACK_32 & shift = 3 puts lower 32 bits in x (leftmost slot).
    switch (shift) {
    case 0:
      control = MakePermuteMask(0, 0, 0, 1, 0, 2, 1, 3);
      break;
    case 1:
      control = MakePermuteMask(0, 0, 0, 1, 1, 3, 0, 3);
      break;
    case 2:
      control = MakePermuteMask(0, 0, 1, 3, 0, 2, 0, 3);
      break;
    case 3:
      control = MakePermuteMask(1, 3, 0, 1, 0, 2, 0, 3);
      break;
    default:
      UNREACHABLE();
      return 1;
    }
    break;
  case 2:  // 64bit
    switch (shift) {
    case 0:
      control = MakePermuteMask(0, 0, 0, 1, 1, 2, 1, 3);
      break;
    case 1:
      control = MakePermuteMask(0, 0, 1, 2, 1, 3, 0, 3);
      break;
    case 2:
      control = MakePermuteMask(1, 2, 1, 3, 0, 2, 0, 3);
      break;
    case 3:
      control = MakePermuteMask(1, 3, 0, 1, 0, 2, 0, 3);
      break;
    default:
      UNREACHABLE();
      return 1;
    }
    break;
  case 3:  // 64bit
    switch (shift) {
    case 0:
      control = MakePermuteMask(0, 0, 0, 1, 1, 2, 1, 3);
      break;
    case 1:
      control = MakePermuteMask(0, 0, 1, 2, 1, 3, 0, 3);
      break;
    case 2:
      control = MakePermuteMask(1, 2, 1, 3, 0, 2, 0, 3);
      break;
    case 3:
      control = MakePermuteMask(0, 0, 0, 1, 0, 2, 1, 2);
      break;
    default:
      UNREACHABLE();
      return 1;
    }
    break;
  default:
    UNREACHABLE();
    return 1;
  }
  v = b.Permute(b.LoadConstantUint32(control), b.LoadVR(vd), v, INT32_TYPE);
  b.StoreVR(vd, v);
  return 0;
}

s32 HIRInstrEmit_vupkd3d128(HIRBuilder &b, const uPPCInstr &instr) {
  // Can't find many docs on this. Best reference is
  // https://code.google.com/archive/p/worldcraft/source/default/source?page=4
  // (qylib/math/xmmatrix.inl)
  // which shows how it's used in some cases. Since it's all intrinsics,
  // finding it in code is pretty easy.
  const u32 vd = instr.VMX128_3.VD128l | (instr.VMX128_3.VD128h << 5);
  const u32 vb = instr.VMX128_3.VB128l | (instr.VMX128_3.VB128h << 5);
  const u32 type = instr.VMX128_3.IMM >> 2;
  Value *v = b.LoadVR(vb);
  switch (type) {
  case 0:  // VPACK_D3DCOLOR
    v = b.Unpack(v, PACK_TYPE_D3DCOLOR);
    break;
  case 1:  // VPACK_NORMSHORT2
    v = b.Unpack(v, PACK_TYPE_SHORT_2);
    break;
  case 2:  // VPACK_NORMPACKED32 2_10_10_10 w_z_y_x
    v = b.Unpack(v, PACK_TYPE_UINT_2101010);
    break;
  case 3:  // VPACK_FLOAT16_2 DXGI_FORMAT_R16G16_FLOAT
    v = b.Unpack(v, PACK_TYPE_FLOAT16_2);
    break;
  case 4:  // VPACK_NORMSHORT4
    v = b.Unpack(v, PACK_TYPE_SHORT_4);
    break;
  case 5:  // VPACK_FLOAT16_4 DXGI_FORMAT_R16G16B16A16_FLOAT
    v = b.Unpack(v, PACK_TYPE_FLOAT16_4);
    break;
  case 6:  // VPACK_NORMPACKED64 4_20_20_20 w_z_y_x
    v = b.Unpack(v, PACK_TYPE_ULONG_4202020);
    break;
  default:
    UNREACHABLE();
    return 1;
  }
  b.StoreVR(vd, v);
  return 0;
}

s32 HIRInstrEmit_vxor_(HIRBuilder &b, u32 vd, u32 va, u32 vb) {
  // VD <- (VA) ^ (VB)
  Value *v;
  if (va == vb) {
    // Fast clear.
    v = b.LoadZeroVec128();
  }
  else {
    v = b.Xor(b.LoadVR(va), b.LoadVR(vb));
  }
  b.StoreVR(vd, v);
  return 0;
}
s32 HIRInstrEmit_vxor(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vxor_(b, instr.vd, instr.va, instr.vb);
}
s32 HIRInstrEmit_vxor128(HIRBuilder &b, const uPPCInstr &instr) {
  return HIRInstrEmit_vxor_(b, J_VMX128_VD128, J_VMX128_VA128, J_VMX128_VB128);
}


} // namespace Xe::XCPU::HIR