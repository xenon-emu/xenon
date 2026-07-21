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

#include "Core/XCPU/JIT/HIR/HIREmitters/HIREmitters.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

namespace Xe::XCPU::HIR {

// Good source of information:
// https://github.com/mamedev/historic-mame/blob/master/src/emu/cpu/powerpc/ppc_ops.c
// The correctness of that code is not reflected here yet -_-

// Floating-pos32 arithmetic (A-8)

s32 HIRInstrEmit_faddx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA) + (frB)
  Value *v = b.Add(b.LoadFPR(instr.fra), b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_faddsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA) + (frB)
  Value *v = b.Add(b.LoadFPR(instr.fra), b.LoadFPR(instr.frb));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fdivx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- frA / frB
  Value *v = b.Div(b.LoadFPR(instr.fra), b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fdivsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- frA / frB
  Value *v = b.Div(b.LoadFPR(instr.fra), b.LoadFPR(instr.frb));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fmulx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA) x (frC)
  Value *v = b.Mul(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fmulsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA) x (frC)
  Value *v = b.Mul(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fresx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- 1.0 / (frB)
  Value *v = b.Convert(b.Div(b.LoadConstantFloat32(1.0f),
    b.Convert(b.LoadFPR(instr.frb), FLOAT32_TYPE)),
    FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_frsqrtex(HIRBuilder &b, const uPPCInstr &instr) {
  // Double precision:
  // frD <- 1/sqrt(frB)
  Value *v = b.RSqrt(b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fsubx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA) - (frB)
  Value *v = b.Sub(b.LoadFPR(instr.fra), b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fsubsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA) - (frB)
  Value *v = b.Sub(b.LoadFPR(instr.fra), b.LoadFPR(instr.frb));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fselx(HIRBuilder &b, const uPPCInstr &instr) {
  // if (frA) >= 0.0
  // then frD <- (frC)
  // else frD <- (frB)
  Value *ge = b.CompareSGE(b.LoadFPR(instr.fra), b.LoadZeroFloat64());
  Value *v = b.Select(ge, b.LoadFPR(instr.frc), b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fsqrtx(HIRBuilder &b, const uPPCInstr &instr) {
  // Double precision:
  // frD <- sqrt(frB)
  Value *v = b.Sqrt(b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fsqrtsx(HIRBuilder &b, const uPPCInstr &instr) {
  // Single precision:
  // frD <- sqrt(frB)
  Value *v = b.Sqrt(b.LoadFPR(instr.frb));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

// Floating-pos32 multiply-add (A-9)

s32 HIRInstrEmit_fmaddx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA x frC) + frB
  Value *v =
    b.MulAdd(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fmaddsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA x frC) + frB
  Value *v =
    b.MulAdd(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fmsubx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA x frC) - frB
  Value *v =
    b.MulSub(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fmsubsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frA x frC) - frB
  Value *v =
    b.MulSub(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fnmaddx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- -([frA x frC] + frB)
  Value *v = b.Neg(
    b.MulAdd(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb)));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fnmaddsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- -([frA x frC] + frB)
  Value *v = b.Neg(
    b.MulAdd(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb)));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fnmsubx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- -([frA x frC] - frB)
  Value *v = b.Neg(
    b.MulSub(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb)));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fnmsubsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- -([frA x frC] - frB)
  Value *v = b.Neg(
    b.MulSub(b.LoadFPR(instr.fra), b.LoadFPR(instr.frc), b.LoadFPR(instr.frb)));
  v = b.Convert(b.Convert(v, FLOAT32_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

// Floating-pos32 rounding and conversion (A-10)

s32 HIRInstrEmit_fcfidx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- signed_int64_to_double( frB )
  // Converts a 64-bit signed integer in frB to a double-precision float
  Value *v = b.Convert(b.Cast(b.LoadFPR(instr.frb), INT64_TYPE), FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

// Helper for fctidx and fctidzx - converts double to signed int64
static s32 HIRInstrEmit_fctidxx_(HIRBuilder &b, const uPPCInstr &instr, RoundMode roundMode) {
  // frD <- double_to_signed_int64( frB )
  // If frB is NaN, the result is 0x8000000000000000
  
  Value *frb = b.LoadFPR(instr.frb);
  
  // Check if the value is NaN
  Value *isNan = b.IsNan(frb);
  
  // Convert to int64 with specified rounding mode
  Value *converted = b.Convert(frb, INT64_TYPE, roundMode);
  
  // If NaN, use 0x8000000000000000, otherwise use converted value
  Value *nanResult = b.LoadConstantUint64(0x8000000000000000ULL);
  Value *result = b.Select(isNan, nanResult, converted);
  
  // Cast back to float64 for storage in FPR
  Value *v = b.Cast(result, FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fctidx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- double_to_signed_int64( frB )
  // Uses dynamic rounding mode from FPSCR
  return HIRInstrEmit_fctidxx_(b, instr, ROUND_DYNAMIC);
}

s32 HIRInstrEmit_fctidzx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- double_to_signed_int64_round_to_zero( frB )
  return HIRInstrEmit_fctidxx_(b, instr, ROUND_TO_ZERO);
}

// Helper for fctiwx and fctiwzx - converts double to signed int32
static s32 HIRInstrEmit_fctiwxx_(HIRBuilder &b, const uPPCInstr &instr, RoundMode roundMode) {
  // frD <- double_to_signed_int32( frB )
  // If frB is NaN, the result is 0x80000000
  // The result is placed in the low-order 32 bits of frD
  
  Value *frb = b.LoadFPR(instr.frb);
  
  // Check if the value is NaN
  Value *isNan = b.IsNan(frb);
  
  // Convert to int32 with specified rounding mode
  Value *converted = b.Convert(frb, INT32_TYPE, roundMode);
  
  // If NaN, use 0x80000000, otherwise use converted value
  Value *nanResult = b.LoadConstantUint32(0x80000000U);
  Value *result32 = b.Select(isNan, nanResult, converted);
  
  // Sign-extend to 64 bits and cast to float64 for FPR storage
  Value *result64 = b.SignExtend(result32, INT64_TYPE);
  Value *v = b.Cast(result64, FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fctiwx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- double_to_signed_int32( frB )
  // Uses dynamic rounding mode from FPSCR
  return HIRInstrEmit_fctiwxx_(b, instr, ROUND_DYNAMIC);
}

s32 HIRInstrEmit_fctiwzx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- double_to_signed_int32_round_to_zero( frB )
  return HIRInstrEmit_fctiwxx_(b, instr, ROUND_TO_ZERO);
}

s32 HIRInstrEmit_frspx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- Round_single(frB)
  // Rounds double-precision frB to single-precision, then extends back to double
  Value *v = b.Convert(b.LoadFPR(instr.frb), FLOAT32_TYPE, ROUND_DYNAMIC);
  v = b.Convert(v, FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

// Floating-pos32 move (A-21)

s32 HIRInstrEmit_fabsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- abs(frB)
  Value *v = b.Abs(b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fmrx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- (frB)
  Value *v = b.LoadFPR(instr.frb);
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fnabsx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- -abs(frB)
  Value *v = b.Neg(b.Abs(b.LoadFPR(instr.frb)));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

s32 HIRInstrEmit_fnegx(HIRBuilder &b, const uPPCInstr &instr) {
  // frD <- ¬ frB[0] || frB[1-63]
  Value *v = b.Neg(b.LoadFPR(instr.frb));
  b.StoreFPR(instr.frd, v);
  b.UpdateFPSCR(v, instr.rc);
  return 0;
}

// Floating-pos32 compare (A-11)

s32 HIRInstrEmit_fcmpo(HIRBuilder &b, const uPPCInstr &instr) {
  // Floating-pos32 Compare Ordered
  // if (FRA) is a NaN or (FRB) is a NaN then
  //   c <- 0b0001 (FU - unordered)
  // else if (FRA) < (FRB) then
  //   c <- 0b1000 (FL - less than)
  // else if (FRA) > (FRB) then
  //   c <- 0b0100 (FG - greater than)
  // else {
  //   c <- 0b0010 (FE - equal)
  // }
  // FPCC <- c
  // CR[4*BF:4*BF+3] <- c
  // if (FRA) is an SNaN or (FRB) is an SNaN then VXSNAN <- 1
  // For ordered compare: if either is a NaN, VXVC is set
  
  const u32 crfD = instr.crfd;
  Value *fra = b.LoadFPR(instr.fra);
  Value *frb = b.LoadFPR(instr.frb);

  // Check for NaN in either operand
  Value *nanA = b.IsNan(fra);
  Value *nanB = b.IsNan(frb);
  Value *isNan = b.Or(nanA, nanB);
  Value *notNan = b.Xor(isNan, b.LoadConstantInt8(0x01));

  // Compute comparison results (only valid when not NaN)
  Value *lt = b.And(notNan, b.CompareSLT(fra, frb));
  Value *gt = b.And(notNan, b.CompareSGT(fra, frb));
  Value *eq = b.And(notNan, b.CompareEQ(fra, frb));

  // Build the 4-bit CR value:
  // Bit 0 (LT): set if fra < frb
  // Bit 1 (GT): set if fra > frb
  // Bit 2 (EQ): set if fra == frb
  // Bit 3 (SO/FU): set if unordered (NaN)
  Value *crValue = b.LoadZeroInt8();
  crValue = b.Or(crValue, b.Shl(lt, 3));   // LT at bit 3 (value 8)
  crValue = b.Or(crValue, b.Shl(gt, 2));   // GT at bit 2 (value 4)
  crValue = b.Or(crValue, b.Shl(eq, 1));   // EQ at bit 1 (value 2)
  crValue = b.Or(crValue, isNan);          // FU at bit 0 (value 1)

  // Store the CR field
  // CR is stored as a 32-bit value with fields packed from high to low
  // CR0 is at bits 28-31, CR1 at 24-27, etc.
  // We need to read CR, mask out the target field, and OR in the new value
  Value *cr = b.LoadCR();
  const u32 shift = (7 - crfD) * 4;
  const u32 mask = ~(0xFu << shift);
  
  // Zero extend crValue to 32 bits before shifting
  Value *crValue32 = b.ZeroExtend(crValue, INT32_TYPE);
  Value *shiftedValue = b.Shl(crValue32, static_cast<s8>(shift));
  Value *maskedCR = b.And(cr, b.LoadConstantUint32(mask));
  Value *newCR = b.Or(maskedCR, shiftedValue);
  b.StoreCR(newCR);

  // TODO: Update FPSCR FPCC and exception bits (VXSNAN, VXVC for ordered)

  return 0;
}

s32 HIRInstrEmit_fcmpu(HIRBuilder &b, const uPPCInstr &instr) {
  // Floating-pos32 Compare Unordered
  // Same as fcmpo but does not set VXVC on QNaN
  // if (FRA) is a NaN or (FRB) is a NaN then
  //   c <- 0b0001 (FU - unordered)
  // else if (FRA) < (FRB) then
  //   c <- 0b1000 (FL - less than)
  // else if (FRA) > (FRB) then
  //   c <- 0b0100 (FG - greater than)
  // else {
  //   c <- 0b0010 (FE - equal)
  // }
  // FPCC <- c
  // CR[4*BF:4*BF+3] <- c
  // if (FRA) is an SNaN or (FRB) is an SNaN then VXSNAN <- 1
  
  const u32 crfD = instr.crfd;
  Value *fra = b.LoadFPR(instr.fra);
  Value *frb = b.LoadFPR(instr.frb);

  // Check for NaN in either operand
  Value *nanA = b.IsNan(fra);
  Value *nanB = b.IsNan(frb);
  Value *isNan = b.Or(nanA, nanB);
  Value *notNan = b.Xor(isNan, b.LoadConstantInt8(0x01));

  // Compute comparison results (only valid when not NaN)
  Value *lt = b.And(notNan, b.CompareSLT(fra, frb));
  Value *gt = b.And(notNan, b.CompareSGT(fra, frb));
  Value *eq = b.And(notNan, b.CompareEQ(fra, frb));

  // Build the 4-bit CR value:
  // Bit 0 (LT): set if fra < frb
  // Bit 1 (GT): set if fra > frb
  // Bit 2 (EQ): set if fra == frb
  // Bit 3 (SO/FU): set if unordered (NaN)
  Value *crValue = b.LoadZeroInt8();
  crValue = b.Or(crValue, b.Shl(lt, 3));   // LT at bit 3 (value 8)
  crValue = b.Or(crValue, b.Shl(gt, 2));   // GT at bit 2 (value 4)
  crValue = b.Or(crValue, b.Shl(eq, 1));   // EQ at bit 1 (value 2)
  crValue = b.Or(crValue, isNan);          // FU at bit 0 (value 1)

  // Store the CR field
  Value *cr = b.LoadCR();
  const u32 shift = (7 - crfD) * 4;
  const u32 mask = ~(0xFu << shift);
  
  // Zero extend crValue to 32 bits before shifting
  Value *crValue32 = b.ZeroExtend(crValue, INT32_TYPE);
  Value *shiftedValue = b.Shl(crValue32, static_cast<s8>(shift));
  Value *maskedCR = b.And(cr, b.LoadConstantUint32(mask));
  Value *newCR = b.Or(maskedCR, shiftedValue);
  b.StoreCR(newCR);

  // TODO: Update FPSCR FPCC and exception bits (VXSNAN only for unordered)

  return 0;
}

// Floating-pos32 status and control register (FPSCR) instructions

s32 HIRInstrEmit_mcrfs(HIRBuilder &b, const uPPCInstr &instr) {
  // Move to Condition Register from FPSCR
  // CR[crfD] <- FPSCR[crfS]
  // Clears exception bits in the source FPSCR field
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_mffsx(HIRBuilder &b, const uPPCInstr &instr) {
  // Move from FPSCR
  // frD <- FPSCR (as a 64-bit value with FPSCR in low 32 bits)
  Value *fpscr = b.LoadFPSCR();
  Value *fpscr64 = b.ZeroExtend(fpscr, INT64_TYPE);
  Value *v = b.Cast(fpscr64, FLOAT64_TYPE);
  b.StoreFPR(instr.frd, v);
  if (instr.rc)
    b.UpdateCR1();
  return 0;
}

s32 HIRInstrEmit_mtfsb0x(HIRBuilder &b, const uPPCInstr &instr) {
  // Move to FPSCR Bit 0
  // FPSCR[crbD] <- 0
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_mtfsb1x(HIRBuilder &b, const uPPCInstr &instr) {
  // Move to FPSCR Bit 1
  // FPSCR[crbD] <- 1
  INSTRNOTIMPLEMENTED();
  return 1;
}

s32 HIRInstrEmit_mtfsfx(HIRBuilder &b, const uPPCInstr &instr) {
  // Move to FPSCR Fields
  // Updates FPSCR fields specified by FM mask from frB
  Value *frb = b.LoadFPR(instr.frb);
  Value *value = b.Truncate(b.Cast(frb, INT64_TYPE), INT32_TYPE);
  
  // FLM is an 8-bit field mask, each bit corresponds to a 4-bit FPSCR field
  u32 flm = instr.flm;
  u32 mask = 0;
  for (s32 i = 0; i < 8; i++) {
    if (flm & (1 << (7 - i))) {
      mask |= 0xF << (28 - i * 4);
    }
  }
  
  Value *fpscr = b.LoadFPSCR();
  Value *maskedOld = b.And(fpscr, b.LoadConstantUint32(~mask));
  Value *maskedNew = b.And(value, b.LoadConstantUint32(mask));
  Value *result = b.Or(maskedOld, maskedNew);
  b.StoreFPSCR(result);
  if(instr.rc)
    b.UpdateCR1();
  return 0;
}

s32 HIRInstrEmit_mtfsfix(HIRBuilder &b, const uPPCInstr &instr) {
  // Move to FPSCR Field Immediate
  // FPSCR[crfD] <- IMM
  INSTRNOTIMPLEMENTED();
  return 1;
}

} // namespace Xe::XCPU::HIR