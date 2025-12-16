/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

//
// Floating Point Register Pointer Helper
//

#define FPRPtr(x) b->threadCtx->array(&sPPUThread::FPR).Ptr(x)

//
// Allocates a new XMM register for floating-point operations
//

#define newXMM() b->compiler->newXmm()

//
// FPSCR Pointer Helper
//

#define FPSCRPtr() b->threadCtx->scalar(&sPPUThread::FPSCR)

//
// FPSCR bit definitions (in little-endian bit positions)
// PowerPC FPSCR is big-endian, so bit 0 in BE = bit 31 in LE
//
// FX (FP Exception Summary) - BE bit 0 = LE bit 31
// FEX (FP Enabled Exception Summary) - BE bit 1 = LE bit 30
// VX (FP Invalid Operation Exception Summary) - BE bit 2 = LE bit 29
// OX (FP Overflow Exception) - BE bit 3 = LE bit 28
// VXSNAN (FP Invalid Operation Exception for SNaN) - BE bit 7 = LE bit 24
// VXISI (FP Invalid Operation Exception for Inf - Inf) - BE bit 8 = LE bit 23
//

#define FPSCR_FX_BIT     (1u << 31)
#define FPSCR_FEX_BIT    (1u << 30)
#define FPSCR_VX_BIT     (1u << 29)
#define FPSCR_OX_BIT     (1u << 28)
#define FPSCR_VXSNAN_BIT (1u << 24)
#define FPSCR_VXISI_BIT  (1u << 23)
#define FPSCR_VXIMZ_BIT  (1u << 20)  // Inf * 0 invalid operation

// Mask for all VX sub-exception bits that affect VX summary
#define FPSCR_VX_ALL_BITS (FPSCR_VXSNAN_BIT | FPSCR_VXISI_BIT | (1u << 22) | (1u << 21) | FPSCR_VXIMZ_BIT | (1u << 19) | (1u << 10) | (1u << 9) | (1u << 8))

// PowerPC default QNaN (positive)
#define PPC_DEFAULT_QNAN 0x7FF8000000000000ull

//
// Helpers
//

// Checks for FPU enabled bit of MSR and raises an exception if not set
inline void J_checkFPUEnabled(JITBlockBuilder* b) {
  x86::Gp msrReg = newGP64();
  x86::Gp exceptionReg = newGP16();

  Label fpEnabledLabel = b->compiler->newLabel();

  COMP->mov(msrReg, SPRPtr(MSR));
  COMP->bt(msrReg, 13);
  COMP->jc(fpEnabledLabel);
  COMP->mov(exceptionReg, EXPtr());
  COMP->or_(exceptionReg, ppuFPUnavailableEx);
  COMP->mov(EXPtr(), exceptionReg);
  COMP->ret();
  COMP->bind(fpEnabledLabel);
}

// Helper to reset FPSCR exception bits before an FPU operation
inline void J_resetFPSCRExceptionBits(JITBlockBuilder* b) {
  x86::Gp fpscr = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  // Clear FX, VX, OX and all VX sub-bits
  COMP->and_(fpscr, ~(FPSCR_FX_BIT | FPSCR_VX_BIT | FPSCR_OX_BIT | FPSCR_VX_ALL_BITS));
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
}

// Helper to check if a value is an SNaN and set FPSCR exception bits if so
inline void J_checkAndSetSNaN(JITBlockBuilder* b, x86::Xmm value) {
  x86::Gp valueBits = newGP64();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();
  x86::Gp fpscr = newGP32();

  Label notSNaN = b->compiler->newLabel();

  COMP->vmovq(valueBits, value);
  COMP->mov(expBits, valueBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notSNaN);

  COMP->mov(fracBits, valueBits);
  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jz(notSNaN);

  COMP->bt(valueBits, 51);
  COMP->jc(notSNaN);

  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXSNAN_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  COMP->bind(notSNaN);
}

// Helper to check if a value is an SNaN and return its QNaN conversion in snanQNaN if found
// Sets snanFlag to 1 if SNaN found, 0 otherwise
// snanQNaN will contain the SNaN converted to QNaN (bit 51 set)
inline void J_checkSNaNAndGetQNaN(JITBlockBuilder* b, x86::Xmm value, x86::Gp snanFlag, x86::Gp snanQNaN) {
  x86::Gp valueBits = newGP64();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();

  Label notSNaN = b->compiler->newLabel();
  Label done = b->compiler->newLabel();

  COMP->vmovq(valueBits, value);
  COMP->mov(expBits, valueBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notSNaN);

  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, valueBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jz(notSNaN);

  // Check if bit 51 is clear (SNaN has bit 51 = 0)
  COMP->bt(valueBits, 51);
  COMP->jc(notSNaN);

  // It's an SNaN - convert to QNaN by setting bit 51
  COMP->mov(snanFlag, 1);
  COMP->mov(snanQNaN, valueBits);
  x86::Gp bit51Mask = newGP64();
  COMP->mov(bit51Mask, 0x0008000000000000ull);
  COMP->or_(snanQNaN, bit51Mask);
  COMP->jmp(done);

  COMP->bind(notSNaN);
  // Not an SNaN - don't modify flags (caller initializes them)

  COMP->bind(done);
}

// Helper to check if a value is a QNaN (quiet NaN) and return it
// A QNaN has exp=0x7FF, frac!=0, and bit 51 set
// Sets qnanFlag to 1 if QNaN found, 0 otherwise
// qnanValue will contain the QNaN value
inline void J_checkQNaNAndGetValue(JITBlockBuilder* b, x86::Xmm value, x86::Gp qnanFlag, x86::Gp qnanValue) {
  x86::Gp valueBits = newGP64();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();

  Label notQNaN = b->compiler->newLabel();
  Label done = b->compiler->newLabel();

  COMP->vmovq(valueBits, value);
  COMP->mov(expBits, valueBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notQNaN);

  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, valueBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jz(notQNaN);

  // Check if bit 51 is set (QNaN has bit 51 = 1)
  COMP->bt(valueBits, 51);
  COMP->jnc(notQNaN);

  // It's a QNaN - return it as-is
  COMP->mov(qnanFlag, 1);
  COMP->mov(qnanValue, valueBits);
  COMP->jmp(done);

  COMP->bind(notQNaN);
  // Not a QNaN - don't modify flags (caller initializes them)

  COMP->bind(done);
}

// Helper to check if a value is infinity (positive or negative)
// Sets infFlag to 1 if infinity, 0 otherwise
inline void J_checkInfinity(JITBlockBuilder* b, x86::Xmm value, x86::Gp infFlag) {
  x86::Gp valueBits = newGP64();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();

  Label notInf = b->compiler->newLabel();
  Label done = b->compiler->newLabel();

  COMP->vmovq(valueBits, value);

  // Extract exponent (bits 52-62)
  COMP->mov(expBits, valueBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);

  // If exponent != 0x7FF, not infinity
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notInf);

  // Check if fraction is zero (infinity has exp=0x7FF and frac=0)
  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, valueBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jnz(notInf);

  // It's infinity
  COMP->mov(infFlag, 1);
  COMP->jmp(done);

  COMP->bind(notInf);
  // Not infinity - don't modify flag (caller initializes it)

  COMP->bind(done);
}

// Helper to check if a value is a double-precision denormal (for single-precision ops)
// A denormal has exponent = 0 and fraction != 0
// Returns 1 in denormFlag if either input is denormal, 0 otherwise
inline void J_checkDenormal(JITBlockBuilder* b, x86::Xmm value, x86::Gp denormFlag) {
  x86::Gp valueBits = newGP64();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();

  Label notDenorm = b->compiler->newLabel();
  Label isDenorm = b->compiler->newLabel();

  COMP->vmovq(valueBits, value);

  // Extract exponent (bits 52-62)
  COMP->mov(expBits, valueBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);

  // If exponent != 0, not a denormal
  COMP->test(expBits.r32(), expBits.r32());
  COMP->jnz(notDenorm);

  // Exponent is 0, check if fraction is non-zero
  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, valueBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jz(notDenorm);

  // It's a denormal
  COMP->bind(isDenorm);
  COMP->mov(denormFlag, 1);
  Label done = b->compiler->newLabel();
  COMP->jmp(done);

  COMP->bind(notDenorm);
  // Don't modify flag here - let caller initialize it

  COMP->bind(done);
}

// Helper to check for Inf + (-Inf) or Inf - Inf invalid operation
inline void J_checkInfSubInf(JITBlockBuilder* b, x86::Xmm fra, x86::Xmm frb, x86::Gp vxisiFlag) {
  x86::Gp aBits = newGP64();
  x86::Gp bBits = newGP64();
  x86::Gp aExp = newGP64();
  x86::Gp bExp = newGP64();
  x86::Gp aFrac = newGP64();
  x86::Gp bFrac = newGP64();
  x86::Gp aSign = newGP64();
  x86::Gp bSign = newGP64();
  x86::Gp fpscr = newGP32();

  Label notInfSubInf = b->compiler->newLabel();

  COMP->xor_(vxisiFlag, vxisiFlag);

  COMP->vmovq(aBits, fra);
  COMP->vmovq(bBits, frb);

  COMP->mov(aExp, aBits);
  COMP->shr(aExp, 52);
  COMP->and_(aExp, 0x7FF);
  COMP->cmp(aExp.r32(), 0x7FF);
  COMP->jne(notInfSubInf);

  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(aFrac, aBits);
  COMP->and_(aFrac, fracMask);
  COMP->test(aFrac, aFrac);
  COMP->jnz(notInfSubInf);

  COMP->mov(bExp, bBits);
  COMP->shr(bExp, 52);
  COMP->and_(bExp, 0x7FF);
  COMP->cmp(bExp.r32(), 0x7FF);
  COMP->jne(notInfSubInf);

  COMP->mov(bFrac, bBits);
  COMP->and_(bFrac, fracMask);
  COMP->test(bFrac, bFrac);
  COMP->jnz(notInfSubInf);

  COMP->mov(aSign, aBits);
  COMP->shr(aSign, 63);
  COMP->mov(bSign, bBits);
  COMP->shr(bSign, 63);
  COMP->cmp(aSign.r32(), bSign.r32());
  COMP->je(notInfSubInf);

  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXISI_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  COMP->mov(vxisiFlag, 1);

  COMP->bind(notInfSubInf);
}

// Helper to check for Inf - Inf (same sign subtraction) invalid operation
inline void J_checkInfMinusInf(JITBlockBuilder* b, x86::Xmm fra, x86::Xmm frb, x86::Gp vxisiFlag) {
  x86::Gp aBits = newGP64();
  x86::Gp bBits = newGP64();
  x86::Gp aExp = newGP64();
  x86::Gp bExp = newGP64();
  x86::Gp aFrac = newGP64();
  x86::Gp bFrac = newGP64();
  x86::Gp aSign = newGP64();
  x86::Gp bSign = newGP64();
  x86::Gp fpscr = newGP32();

  Label notInfSubInf = b->compiler->newLabel();

  COMP->xor_(vxisiFlag, vxisiFlag);

  COMP->vmovq(aBits, fra);
  COMP->vmovq(bBits, frb);

  COMP->mov(aExp, aBits);
  COMP->shr(aExp, 52);
  COMP->and_(aExp, 0x7FF);
  COMP->cmp(aExp.r32(), 0x7FF);
  COMP->jne(notInfSubInf);

  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(aFrac, aBits);
  COMP->and_(aFrac, fracMask);
  COMP->test(aFrac, aFrac);
  COMP->jnz(notInfSubInf);

  COMP->mov(bExp, bBits);
  COMP->shr(bExp, 52);
  COMP->and_(bExp, 0x7FF);
  COMP->cmp(bExp.r32(), 0x7FF);
  COMP->jne(notInfSubInf);

  COMP->mov(bFrac, bBits);
  COMP->and_(bFrac, fracMask);
  COMP->test(bFrac, bFrac);
  COMP->jnz(notInfSubInf);

  // For subtraction, invalid if signs are SAME (Inf - Inf)
  COMP->mov(aSign, aBits);
  COMP->shr(aSign, 63);
  COMP->mov(bSign, bBits);
  COMP->shr(bSign, 63);
  COMP->cmp(aSign.r32(), bSign.r32());
  COMP->jne(notInfSubInf);

  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXISI_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  COMP->mov(vxisiFlag, 1);

  COMP->bind(notInfSubInf);
}

// Helper to check for Inf * 0 invalid operation
inline void J_checkInfMulZero(JITBlockBuilder* b, x86::Xmm fra, x86::Xmm frc, x86::Gp vximzFlag) {
  x86::Gp aBits = newGP64();
  x86::Gp cBits = newGP64();
  x86::Gp aExp = newGP64();
  x86::Gp cExp = newGP64();
  x86::Gp aFrac = newGP64();
  x86::Gp cFrac = newGP64();
  x86::Gp fpscr = newGP32();

  Label notInfMulZero = b->compiler->newLabel();
  Label checkCInfAZero = b->compiler->newLabel();

  COMP->xor_(vximzFlag, vximzFlag);

  COMP->vmovq(aBits, fra);
  COMP->vmovq(cBits, frc);

  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);

  // Check if A is Inf
  COMP->mov(aExp, aBits);
  COMP->shr(aExp, 52);
  COMP->and_(aExp, 0x7FF);
  COMP->cmp(aExp.r32(), 0x7FF);
  COMP->jne(checkCInfAZero);

  COMP->mov(aFrac, aBits);
  COMP->and_(aFrac, fracMask);
  COMP->test(aFrac, aFrac);
  COMP->jnz(checkCInfAZero);

  // A is Inf, check if C is zero (exp=0 AND frac=0)
  COMP->mov(cExp, cBits);
  COMP->shr(cExp, 52);
  COMP->and_(cExp, 0x7FF);
  COMP->test(cExp.r32(), cExp.r32());
  COMP->jnz(checkCInfAZero);  // C has non-zero exponent, not zero

  COMP->mov(cFrac, cBits);
  COMP->and_(cFrac, fracMask);
  COMP->test(cFrac, cFrac);
  COMP->jnz(checkCInfAZero);  // C has non-zero fraction (denormal), not zero

  // A is Inf and C is zero - invalid
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXIMZ_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  COMP->mov(vximzFlag, 1);
  COMP->jmp(notInfMulZero);

  // Check if C is Inf and A is zero
  COMP->bind(checkCInfAZero);
  COMP->mov(cExp, cBits);
  COMP->shr(cExp, 52);
  COMP->and_(cExp, 0x7FF);
  COMP->cmp(cExp.r32(), 0x7FF);
  COMP->jne(notInfMulZero);

  COMP->mov(cFrac, cBits);
  COMP->and_(cFrac, fracMask);
  COMP->test(cFrac, cFrac);
  COMP->jnz(notInfMulZero);

  // C is Inf, check if A is zero (exp=0 AND frac=0)
  COMP->mov(aExp, aBits);
  COMP->shr(aExp, 52);
  COMP->and_(aExp, 0x7FF);
  COMP->test(aExp.r32(), aExp.r32());
  COMP->jnz(notInfMulZero);  // A has non-zero exponent, not zero

  COMP->mov(aFrac, aBits);
  COMP->and_(aFrac, fracMask);
  COMP->test(aFrac, aFrac);
  COMP->jnz(notInfMulZero);  // A has non-zero fraction (denormal), not zero

  // C is Inf and A is zero - invalid
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXIMZ_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  COMP->mov(vximzFlag, 1);

  COMP->bind(notInfMulZero);
}

// Helper to set CR1 based on FPSCR (FX, FEX, VX, OX bits)
inline void J_ppuSetCR1(JITBlockBuilder* b) {
  x86::Gp fpscr = newGP32();
  x86::Gp cr1Value = newGP32();
  x86::Gp crReg = newGP32();
  x86::Gp tmp = newGP32();

  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->xor_(cr1Value, cr1Value);
  COMP->mov(tmp, fpscr);
  COMP->shr(tmp, 28);
  COMP->and_(tmp, 0xF);
  COMP->mov(cr1Value, tmp);
  COMP->mov(crReg, CRValPtr());
  COMP->and_(crReg, 0xF0FFFFFF);
  COMP->shl(cr1Value, 24);
  COMP->or_(crReg, cr1Value);
  COMP->mov(CRValPtr(), crReg);
}

// Helper to classify a double-precision floating point value and set FPRF
inline void J_classifyAndSetFPRF(JITBlockBuilder* b, x86::Xmm result) {
  x86::Gp resultBits = newGP64();
  x86::Gp fprf = newGP32();
  x86::Gp fpscr = newGP32();
  x86::Gp signBit = newGP64();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();

  Label isNaNOrInf = b->compiler->newLabel();
  Label isNaN = b->compiler->newLabel();
  Label isInfPos = b->compiler->newLabel();
  Label isInfNeg = b->compiler->newLabel();
  Label isZeroOrDenorm = b->compiler->newLabel();
  Label isZeroPos = b->compiler->newLabel();
  Label isZeroNeg = b->compiler->newLabel();
  Label isDenormPos = b->compiler->newLabel();
  Label isDenormNeg = b->compiler->newLabel();
  Label isNormPos = b->compiler->newLabel();
  Label isNormNeg = b->compiler->newLabel();
  Label done = b->compiler->newLabel();

  COMP->vmovq(resultBits, result);

  COMP->mov(signBit, resultBits);
  COMP->shr(signBit, 63);

  COMP->mov(expBits, resultBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);

  COMP->mov(fracBits, resultBits);
  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->and_(fracBits, fracMask);

  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->je(isNaNOrInf);

  COMP->test(expBits.r32(), expBits.r32());
  COMP->jz(isZeroOrDenorm);

  COMP->test(signBit.r32(), signBit.r32());
  COMP->jnz(isNormNeg);
  COMP->jmp(isNormPos);

  COMP->bind(isNaNOrInf);
  COMP->test(fracBits, fracBits);
  COMP->jnz(isNaN);
  COMP->test(signBit.r32(), signBit.r32());
  COMP->jnz(isInfNeg);
  COMP->jmp(isInfPos);

  COMP->bind(isNaN);
  COMP->mov(fprf, 0x11);
  COMP->jmp(done);

  COMP->bind(isInfPos);
  COMP->mov(fprf, 0x05);
  COMP->jmp(done);

  COMP->bind(isInfNeg);
  COMP->mov(fprf, 0x09);
  COMP->jmp(done);

  COMP->bind(isZeroOrDenorm);
  COMP->test(fracBits, fracBits);
  COMP->jnz(isDenormPos);
  COMP->test(signBit.r32(), signBit.r32());
  COMP->jnz(isZeroNeg);
  COMP->jmp(isZeroPos);

  COMP->bind(isZeroPos);
  COMP->mov(fprf, 0x02);
  COMP->jmp(done);

  COMP->bind(isZeroNeg);
  COMP->mov(fprf, 0x12);
  COMP->jmp(done);

  COMP->bind(isDenormPos);
  COMP->test(signBit.r32(), signBit.r32());
  COMP->jnz(isDenormNeg);
  COMP->mov(fprf, 0x14);
  COMP->jmp(done);

  COMP->bind(isDenormNeg);
  COMP->mov(fprf, 0x18);
  COMP->jmp(done);

  COMP->bind(isNormPos);
  COMP->mov(fprf, 0x04);
  COMP->jmp(done);

  COMP->bind(isNormNeg);
  COMP->mov(fprf, 0x08);
  COMP->jmp(done);

  COMP->bind(done);
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->and_(fpscr, ~(0x1F << 12));
  COMP->shl(fprf, 12);
  COMP->or_(fpscr, fprf);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
}

// Helper to convert double to single and store back as double (for single-precision operations)
inline void J_roundToSingle(JITBlockBuilder* b, x86::Xmm frd) {
  COMP->vcvtsd2ss(frd, frd, frd);
  COMP->vcvtss2sd(frd, frd, frd);
}

// Floating Add (Double-Precision) (x'FC00 002A')
// frD <- (frA) + (frB)
void PPCInterpreter::PPCInterpreterJIT_faddx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  // Load frA (64-bit double from FPR[fra])
  // FPR is stored as u64, which represents the double bit pattern
  COMP->vmovsd(fra, FPRPtr(instr.fra));

  // Load frB (64-bit double from FPR[frb])
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  // Reset FPSCR exception bits for this operation
  J_resetFPSCRExceptionBits(b);

  // Check for SNaN inputs and set FPSCR exception bits if found
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for Inf + (-Inf) invalid operation, get flag indicating if it occurred
  x86::Gp vxisiFlag = newGP32();
  J_checkInfSubInf(b, fra, frb, vxisiFlag);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point addition
  COMP->vaddsd(frd, fra, frb);

  // If VXISI occurred (Inf + (-Inf)), replace result with PowerPC default QNaN
  Label noVxisiFixup = b->compiler->newLabel();
  COMP->test(vxisiFlag, vxisiFlag);
  COMP->jz(noVxisiFixup);

  // Load PowerPC default positive QNaN into frd
  x86::Gp defaultQNaN = newGP64();
  COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
  COMP->vmovq(frd, defaultQNaN);

  COMP->bind(noVxisiFixup);

  // Check MXCSR for inexact result (Precision Exception - bit 5)
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);

  Label notInexact = b->compiler->newLabel();
  COMP->bt(mxcsrMem, 5);
  COMP->jnc(notInexact);

  // Inexact result detected - set FX bit in FPSCR
  x86::Gp fpscr = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  COMP->bind(notInexact);

  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Add Single (x'EC00 002A')
// frD <- (frA) + (frB) [single precision]
// Note: Single-precision operations treat double-precision denormals as invalid
// EXCEPT when the other operand is infinity or NaN (infinity/NaN dominates)
// NaN priority: fra NaN (SNaN->QNaN or QNaN) > frb NaN (SNaN->QNaN or QNaN) > VXISI > denorm
void PPCInterpreter::PPCInterpreterJIT_faddsx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  // Load frA (64-bit double from FPR[fra])
  COMP->vmovsd(fra, FPRPtr(instr.fra));

  // Load frB (64-bit double from FPR[frb])
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  // Reset FPSCR exception bits for this operation
  J_resetFPSCRExceptionBits(b);

  // Check for SNaN inputs and set FPSCR exception bits if found
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in fra and get converted QNaN value
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  // Check for QNaN in fra
  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for infinity inputs - infinity dominates over denormals
  x86::Gp infFlagA = newGP32();
  COMP->xor_(infFlagA, infFlagA);
  J_checkInfinity(b, fra, infFlagA);

  x86::Gp infFlagB = newGP32();
  COMP->xor_(infFlagB, infFlagB);
  J_checkInfinity(b, frb, infFlagB);

  // Check for denormal inputs - single-precision ops treat them as invalid
  // But only if neither operand is infinity or NaN
  x86::Gp denormFlag = newGP32();
  COMP->xor_(denormFlag, denormFlag);
  J_checkDenormal(b, fra, denormFlag);
  J_checkDenormal(b, frb, denormFlag);

  // Check for Inf + (-Inf) invalid operation
  x86::Gp vxisiFlag = newGP32();
  J_checkInfSubInf(b, fra, frb, vxisiFlag);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point addition
  COMP->vaddsd(frd, fra, frb);

  // Determine the correct result:
  // Priority: fra NaN (any) > frb NaN (any) > VXISI > denorm (only if no infinity/NaN)
  // For fra: SNaN converted to QNaN takes precedence, then QNaN
  // For frb: SNaN converted to QNaN takes precedence, then QNaN
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkVxisi = b->compiler->newLabel();
  Label checkDenorm = b->compiler->newLabel();
  Label doRounding = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If fra is SNaN, use its converted QNaN (fra has priority)
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // If fra is QNaN, propagate it (fra has priority)
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // If frb is SNaN, use its converted QNaN
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frb is QNaN, propagate it
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkVxisi);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // If VXISI (Inf + (-Inf)), use default QNaN
  COMP->bind(checkVxisi);
  COMP->test(vxisiFlag, vxisiFlag);
  COMP->jz(checkDenorm);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Check for denormal - but only produce QNaN if no operand is infinity
  COMP->bind(checkDenorm);
  COMP->test(denormFlag, denormFlag);
  COMP->jz(doRounding);

  // Has denormal - check if either operand is infinity (NaN already handled above)
  x86::Gp hasInf = newGP32();
  COMP->mov(hasInf, infFlagA);
  COMP->or_(hasInf, infFlagB);
  COMP->test(hasInf, hasInf);
  COMP->jnz(doRounding);  // If infinity present, just round (infinity dominates)

  // Denormal with no infinity/NaN - produce default QNaN
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Normal case - round to single precision
  COMP->bind(doRounding);
  // Save the double value before rounding to compare later
  x86::Gp beforeRound = newGP64();
  COMP->vmovq(beforeRound, frd);

  // Round to single precision
  J_roundToSingle(b, frd);

  // Check if rounding caused precision loss by comparing before/after
  // Also check MXCSR for inexact from the addition itself
  {
    x86::Gp afterRound = newGP64();
    x86::Gp fpscr = newGP32();
    Label notInexact = b->compiler->newLabel();

    COMP->vmovq(afterRound, frd);
    COMP->cmp(beforeRound, afterRound);
    COMP->je(notInexact);

    // Rounding changed the value - set FX bit in FPSCR
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  COMP->bind(storeResult);
  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Subtract (Double-Precision) (x'FC00 0028')
// frD <- (frA) - (frB)
void PPCInterpreter::PPCInterpreterJIT_fsubx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  // Load frA (64-bit double from FPR[fra])
  // FPR is stored as u64, which represents the double bit pattern
  COMP->vmovsd(fra, FPRPtr(instr.fra));

  // Load frB (64-bit double from FPR[frb])
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  // Reset FPSCR exception bits for this operation
  J_resetFPSCRExceptionBits(b);

  // Check for SNaN inputs and set FPSCR exception bits if found
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for Inf - Inf invalid operation, get flag indicating if it occurred
  x86::Gp vxisiFlag = newGP32();
  J_checkInfMinusInf(b, fra, frb, vxisiFlag);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point subtraction
  COMP->vsubsd(frd, fra, frb);

  // If VXISI occurred (Inf - Inf), replace result with PowerPC default QNaN
  Label noVxisiFixup = b->compiler->newLabel();
  COMP->test(vxisiFlag, vxisiFlag);
  COMP->jz(noVxisiFixup);

  // Load PowerPC default positive QNaN into frd
  x86::Gp defaultQNaN = newGP64();
  COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
  COMP->vmovq(frd, defaultQNaN);

  COMP->bind(noVxisiFixup);

  // Check MXCSR for inexact result (Precision Exception - bit 5)
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);

  Label notInexact = b->compiler->newLabel();
  COMP->bt(mxcsrMem, 5);
  COMP->jnc(notInexact);

  // Inexact result detected - set FX bit in FPSCR
  x86::Gp fpscr = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  COMP->bind(notInexact);

  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Subtract Single (x'EC00 0028')
// frD <- (frA) - (frB) [single precision]
// NaN priority: fra NaN > frb NaN > VXISI > denorm
void PPCInterpreter::PPCInterpreterJIT_fsubsx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in fra and get converted QNaN value
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  // Check for QNaN in fra
  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for infinity inputs - infinity dominates over denormals
  x86::Gp infFlagA = newGP32();
  COMP->xor_(infFlagA, infFlagA);
  J_checkInfinity(b, fra, infFlagA);

  x86::Gp infFlagB = newGP32();
  COMP->xor_(infFlagB, infFlagB);
  J_checkInfinity(b, frb, infFlagB);

  // Check for denormal inputs - single-precision ops treat them as invalid
  // But only if neither operand is infinity or NaN
  x86::Gp denormFlag = newGP32();
  COMP->xor_(denormFlag, denormFlag);
  J_checkDenormal(b, fra, denormFlag);
  J_checkDenormal(b, frb, denormFlag);

  // Check for Inf - Inf invalid operation
  x86::Gp vxisiFlag = newGP32();
  J_checkInfMinusInf(b, fra, frb, vxisiFlag);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point subtraction
  COMP->vsubsd(frd, fra, frb);

  // Determine the correct result:
  // Priority: fra NaN > frb NaN > VXISI > denorm
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkVxisi = b->compiler->newLabel();
  Label checkDenorm = b->compiler->newLabel();
  Label doRounding = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If fra is SNaN, use its converted QNaN (fra has priority)
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // If fra is QNaN, propagate it (fra has priority)
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // If frb is SNaN, use its converted QNaN
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frb is QNaN, propagate it
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkVxisi);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // VXISI (Inf - Inf)
  COMP->bind(checkVxisi);
  COMP->test(vxisiFlag, vxisiFlag);
  COMP->jz(checkDenorm);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Denormal check
  COMP->bind(checkDenorm);
  COMP->test(denormFlag, denormFlag);
  COMP->jz(doRounding);

  // Has denormal - check if either operand is infinity (NaN already handled above)
  x86::Gp hasInf = newGP32();
  COMP->mov(hasInf, infFlagA);
  COMP->or_(hasInf, infFlagB);
  COMP->test(hasInf, hasInf);
  COMP->jnz(doRounding);

  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Normal case - round to single precision
  COMP->bind(doRounding);
  // Save the double value before rounding to compare later
  x86::Gp beforeRound = newGP64();
  COMP->vmovq(beforeRound, frd);

  // Round to single precision
  J_roundToSingle(b, frd);

  // Check if rounding caused precision loss by comparing before/after
  // Also check MXCSR for inexact from the addition itself
  {
    x86::Gp afterRound = newGP64();
    x86::Gp fpscr = newGP32();
    Label notInexact = b->compiler->newLabel();

    COMP->vmovq(afterRound, frd);
    COMP->cmp(beforeRound, afterRound);
    COMP->je(notInexact);

    // Rounding changed the value - set FX bit in FPSCR
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  COMP->bind(storeResult);
  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Multiply (Double-Precision) (x'FC00 0032')
void PPCInterpreter::PPCInterpreterJIT_fmulx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frc));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in fra and get converted QNaN value
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  // Check for QNaN in fra
  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for Inf * 0 invalid operation
  x86::Gp vximzFlag = newGP32();
  J_checkInfMulZero(b, fra, frb, vximzFlag);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point multiplication
  COMP->vmulsd(frd, fra, frb);

  // Determine result priority:
  // fra SNaN -> fra QNaN
  // fra QNaN -> propagate
  // frb SNaN -> frb QNaN
  // frb QNaN -> propagate
  // VXIMZ -> default QNaN
  // otherwise use computed product
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkVximz = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkVximz);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  COMP->bind(checkVximz);
  COMP->test(vximzFlag, vximzFlag);
  COMP->jz(storeResult); // use computed product
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }

  COMP->bind(storeResult);

  // Check MXCSR for inexact result (Precision Exception - bit 5)
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);

  {
    Label notInexact = b->compiler->newLabel();
    COMP->bt(mxcsrMem, 5);
    COMP->jnc(notInexact);

    // Inexact result detected - set FX bit in FPSCR
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Multiply Single (x'EC00 0032')
void PPCInterpreter::PPCInterpreterJIT_fmulsx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frc = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frc, FPRPtr(instr.frc));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frc);

  // Check for SNaN in fra and get converted QNaN value
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  // Check for QNaN in fra
  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN in frc and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frc, snanFlagB, snanQNaNB);

  // Check for QNaN in frc
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frc, qnanFlagB, qnanValueB);

  // Check for infinity inputs - infinity dominates over denormals
  x86::Gp infFlagA = newGP32();
  COMP->xor_(infFlagA, infFlagA);
  J_checkInfinity(b, fra, infFlagA);

  x86::Gp infFlagB = newGP32();
  COMP->xor_(infFlagB, infFlagB);
  J_checkInfinity(b, frc, infFlagB);

  // Check for denormal inputs - single-precision ops treat them as invalid
  // But only if neither operand is infinity or NaN
  x86::Gp denormFlag = newGP32();
  COMP->xor_(denormFlag, denormFlag);
  J_checkDenormal(b, fra, denormFlag);
  J_checkDenormal(b, frc, denormFlag);

  // Check for Inf * 0 invalid operation
  x86::Gp vximzFlag = newGP32();
  J_checkInfMulZero(b, fra, frc, vximzFlag);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision multiply (we will round to single after)
  COMP->vmulsd(frd, fra, frc);

  // Determine the correct result:
  // Priority: fra NaN (any) > frc NaN (any) > VXIMZ > denorm
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkVximz = b->compiler->newLabel();
  Label checkDenorm = b->compiler->newLabel();
  Label doRounding = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If fra is SNaN, use its converted QNaN (fra has priority)
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // If fra is QNaN, propagate it (fra has priority)
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // If frc is SNaN, use its converted QNaN
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frc is QNaN, propagate it
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkVximz);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // VXIMZ (Inf * 0)
  COMP->bind(checkVximz);
  COMP->test(vximzFlag, vximzFlag);
  COMP->jz(checkDenorm);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Denormal check
  COMP->bind(checkDenorm);
  COMP->test(denormFlag, denormFlag);
  COMP->jz(doRounding);

  // Has denormal - check if either operand is infinity (NaN already handled above)
  x86::Gp hasInf = newGP32();
  COMP->mov(hasInf, infFlagA);
  COMP->or_(hasInf, infFlagB);
  COMP->test(hasInf, hasInf);
  COMP->jnz(doRounding);  // If infinity present, just round (infinity dominates)

  // Denormal with no infinity/NaN - produce default QNaN
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Normal case - round to single precision
  COMP->bind(doRounding);
  // Save the double value before rounding to compare later
  x86::Gp beforeRound = newGP64();
  COMP->vmovq(beforeRound, frd);

  // Round to single precision
  J_roundToSingle(b, frd);

  // Check if rounding caused precision loss by comparing before/after
  // Also check MXCSR for inexact from the multiplication itself
  {
    x86::Gp afterRound = newGP64();
    x86::Gp fpscr = newGP32();
    Label notInexact = b->compiler->newLabel();

    COMP->vmovq(afterRound, frd);
    COMP->cmp(beforeRound, afterRound);
    COMP->je(notInexact);

    // Rounding changed the value - set FX bit in FPSCR
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  COMP->bind(storeResult);
  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Divide (Double-Precision) (x'FC00 0024')
void PPCInterpreter::PPCInterpreterJIT_fdivx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in fra and get converted QNaN value
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  // Check for QNaN in fra
  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for infinity in fra and frb
  x86::Gp infFlagA = newGP32();
  COMP->xor_(infFlagA, infFlagA);
  J_checkInfinity(b, fra, infFlagA);

  x86::Gp infFlagB = newGP32();
  COMP->xor_(infFlagB, infFlagB);
  J_checkInfinity(b, frb, infFlagB);

  // Check if frb is zero (for divide by zero and 0/0 detection)
  x86::Gp bBits = newGP64();
  x86::Gp bIsZero = newGP32();
  x86::Gp fracMaskTmp = newGP64();
  COMP->xor_(bIsZero, bIsZero);
  COMP->vmovq(bBits, frb);
  COMP->mov(fracMaskTmp, 0x7FFFFFFFFFFFFFFFull);  // Mask out sign bit
  COMP->and_(bBits, fracMaskTmp);
  COMP->test(bBits, bBits);
  Label bNotZero = b->compiler->newLabel();
  COMP->jnz(bNotZero);
  COMP->mov(bIsZero, 1);
  COMP->bind(bNotZero);

  // Check if fra is zero
  x86::Gp aBits = newGP64();
  x86::Gp aIsZero = newGP32();
  COMP->xor_(aIsZero, aIsZero);
  COMP->vmovq(aBits, fra);
  COMP->mov(fracMaskTmp, 0x7FFFFFFFFFFFFFFFull);
  COMP->and_(aBits, fracMaskTmp);
  COMP->test(aBits, aBits);
  Label aNotZero = b->compiler->newLabel();
  COMP->jnz(aNotZero);
  COMP->mov(aIsZero, 1);
  COMP->bind(aNotZero);

  // Clear MXCSR exception flags before the operation
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point division
  COMP->vdivsd(frd, fra, frb);

  // Determine the correct result:
  // Priority: fra NaN > frb NaN > Inf/Inf (VXIDI) > 0/0 (VXZDZ) > x/0 (ZX) > normal result
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkInfDivInf = b->compiler->newLabel();
  Label checkZeroDivZero = b->compiler->newLabel();
  Label checkDivByZero = b->compiler->newLabel();
  Label checkOverflow = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If fra is SNaN, use its converted QNaN
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // If fra is QNaN, propagate it
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // If frb is SNaN, use its converted QNaN
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frb is QNaN, propagate it
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkInfDivInf);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // Check for Inf / Inf (VXIDI)
  COMP->bind(checkInfDivInf);
  {
    x86::Gp bothInf = newGP32();
    COMP->mov(bothInf, infFlagA);
    COMP->and_(bothInf, infFlagB);
    COMP->test(bothInf, bothInf);
    COMP->jz(checkZeroDivZero);

    // Inf / Inf - set VXIDI exception and return default QNaN
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, (1u << 22) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXIDI bit
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
    COMP->jmp(storeResult);
  }

  // Check for 0 / 0 (VXZDZ)
  COMP->bind(checkZeroDivZero);
  {
    x86::Gp bothZero = newGP32();
    COMP->mov(bothZero, aIsZero);
    COMP->and_(bothZero, bIsZero);
    COMP->test(bothZero, bothZero);
    COMP->jz(checkDivByZero);

    // 0 / 0 - set VXZDZ exception and return default QNaN
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, (1u << 21) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXZDZ bit
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
    COMP->jmp(storeResult);
  }

  // Check for x / 0 where x != 0 (ZX - divide by zero)
  // But NOT if fra is infinity (Inf / 0 = Inf is a valid result, no exception)
  COMP->bind(checkDivByZero);
  {
    COMP->test(bIsZero, bIsZero);
    COMP->jz(checkOverflow);  // Not dividing by zero, check overflow

    // If fra is infinity, Inf / 0 = Inf is valid - no ZX exception
    COMP->test(infFlagA, infFlagA);
    COMP->jnz(storeResult);  // fra is Inf, result is Inf, no exception

    // x / 0 where x is finite and non-zero - set ZX exception
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, (1u << 26) | FPSCR_FX_BIT);  // ZX bit
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
    COMP->jmp(storeResult);
  }

  // Check for overflow: result is infinity but fra was not infinity
  // This happens when dividing by a very small number (like denormals)
  COMP->bind(checkOverflow);
  {
    // Skip overflow check if fra was already infinity
    COMP->test(infFlagA, infFlagA);
    COMP->jnz(storeResult);

    // Check if result is infinity (exp == 0x7FF and frac == 0)
    x86::Gp resultBits = newGP64();
    x86::Gp resultExp = newGP64();
    x86::Gp resultFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->vmovq(resultBits, frd);

    // Extract exponent
    COMP->mov(resultExp, resultBits);
    COMP->shr(resultExp, 52);
    COMP->and_(resultExp, 0x7FF);

    // If exponent is not 0x7FF, not infinity - no overflow
    COMP->cmp(resultExp.r32(), 0x7FF);
    COMP->jne(storeResult);

    // Check if fraction is zero (infinity) vs non-zero (NaN)
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(resultFrac, resultBits);
    COMP->and_(resultFrac, fracMask);
    COMP->test(resultFrac, resultFrac);
    COMP->jnz(storeResult);  // Result is NaN, not overflow

    // Result is infinity from non-infinity fra - this is overflow
    // Set OX bit (bit 3 in BE = bit 28 in LE) and FX
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_OX_BIT | FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  }

  COMP->bind(storeResult);

  // Check MXCSR for inexact result
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  {
    Label notInexact = b->compiler->newLabel();
    COMP->bt(mxcsrMem, 5);
    COMP->jnc(notInexact);

    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Divide Single (x'EC00 0024')
void PPCInterpreter::PPCInterpreterJIT_fdivsx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in fra and get converted QNaN value
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  // Check for QNaN in fra
  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for infinity in fra and frb
  x86::Gp infFlagA = newGP32();
  COMP->xor_(infFlagA, infFlagA);
  J_checkInfinity(b, fra, infFlagA);

  x86::Gp infFlagB = newGP32();
  COMP->xor_(infFlagB, infFlagB);
  J_checkInfinity(b, frb, infFlagB);

  // Check for denormal in fra (exponent == 0 and fraction != 0)
  x86::Gp denormFlagA = newGP32();
  COMP->xor_(denormFlagA, denormFlagA);
  J_checkDenormal(b, fra, denormFlagA);

  // Check for denormal in frb
  x86::Gp denormFlagB = newGP32();
  COMP->xor_(denormFlagB, denormFlagB);
  J_checkDenormal(b, frb, denormFlagB);

  // Check if frb is zero
  x86::Gp bBits = newGP64();
  x86::Gp bIsZero = newGP32();
  x86::Gp absMask = newGP64();
  COMP->xor_(bIsZero, bIsZero);
  COMP->vmovq(bBits, frb);
  COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
  COMP->and_(bBits, absMask);
  COMP->test(bBits, bBits);
  Label bNotZero = b->compiler->newLabel();
  COMP->jnz(bNotZero);
  COMP->mov(bIsZero, 1);
  COMP->bind(bNotZero);

  // Check if fra is zero
  x86::Gp aBits = newGP64();
  x86::Gp aIsZero = newGP32();
  COMP->xor_(aIsZero, aIsZero);
  COMP->vmovq(aBits, fra);
  COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
  COMP->and_(aBits, absMask);
  COMP->test(aBits, aBits);
  Label aNotZero = b->compiler->newLabel();
  COMP->jnz(aNotZero);
  COMP->mov(aIsZero, 1);
  COMP->bind(aNotZero);

  // Clear MXCSR exception flags before the operation
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision floating-point division
  COMP->vdivsd(frd, fra, frb);

  // Determine the correct result:
  // Priority: fra NaN > frb NaN > Inf/Inf > 0/0 > x/0 (finite x only) > normal result
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkInfDivInf = b->compiler->newLabel();
  Label checkZeroDivZero = b->compiler->newLabel();
  Label checkDivByZero = b->compiler->newLabel();
  Label checkRounding = b->compiler->newLabel();
  Label checkInexactAndOverflow = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If fra is SNaN, use its converted QNaN
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // If fra is QNaN, propagate it
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // If frb is SNaN, use its converted QNaN
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frb is QNaN, propagate it
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkInfDivInf);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // Check for Inf / Inf (VXIDI)
  COMP->bind(checkInfDivInf);
  {
    x86::Gp bothInf = newGP32();
    COMP->mov(bothInf, infFlagA);
    COMP->and_(bothInf, infFlagB);
    COMP->test(bothInf, bothInf);
    COMP->jz(checkZeroDivZero);

    // Inf / Inf - set VXIDI exception and return default QNaN
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, (1u << 22) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXIDI bit
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
    COMP->jmp(storeResult);
  }

  // Check for 0 / 0 (VXZDZ)
  COMP->bind(checkZeroDivZero);
  {
    x86::Gp bothZero = newGP32();
    COMP->mov(bothZero, aIsZero);
    COMP->and_(bothZero, bIsZero);
    COMP->test(bothZero, bothZero);
    COMP->jz(checkDivByZero);

    // 0 / 0 - set VXZDZ exception and return default QNaN
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, (1u << 21) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXZDZ bit
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
    COMP->jmp(storeResult);
  }

  // Check for x / 0 where x != 0 and x is finite (ZX - divide by zero)
  // Note: Inf / 0 = Inf (no exception), only finite / 0 raises ZX
  COMP->bind(checkDivByZero);
  {
    COMP->test(bIsZero, bIsZero);
    COMP->jz(checkRounding);  // Not dividing by zero, check rounding

    // frb is zero - check if fra is infinity (Inf / 0 = Inf, no exception)
    COMP->test(infFlagA, infFlagA);
    COMP->jnz(checkInexactAndOverflow);  // fra is Inf, result is Inf, no ZX exception

    // fra is finite and non-zero, frb is zero - set ZX exception
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, (1u << 26) | FPSCR_FX_BIT);  // ZX bit
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
    COMP->jmp(storeResult);  // Result is already infinity from vdivsd
  }

  // Check if we need to round to single precision
  // fdivs rounds to single ONLY when BOTH operands are normal numbers (not denormals, not zero)
  // When either operand is a denormal, the full double precision result is preserved
  COMP->bind(checkRounding);
  {
    // Check if either operand is a denormal - if so, skip rounding but check overflow/inexact
    x86::Gp hasDenorm = newGP32();
    COMP->mov(hasDenorm, denormFlagA);
    COMP->or_(hasDenorm, denormFlagB);
    COMP->test(hasDenorm, hasDenorm);
    COMP->jnz(checkInexactAndOverflow);  // Has denormal input - preserve full precision but check overflow

    // Check if either operand is zero - if so, skip rounding (result is already correct)
    x86::Gp hasZero = newGP32();
    COMP->mov(hasZero, aIsZero);
    COMP->or_(hasZero, bIsZero);
    COMP->test(hasZero, hasZero);
    COMP->jnz(checkInexactAndOverflow);  // Has zero input - preserve result but check overflow

    // Check if result is denormal or zero (exponent == 0) - don't round these
    x86::Gp resultBits = newGP64();
    x86::Gp expBits = newGP64();
    COMP->vmovq(resultBits, frd);
    COMP->mov(expBits, resultBits);
    COMP->shr(expBits, 52);
    COMP->and_(expBits, 0x7FF);
    COMP->test(expBits.r32(), expBits.r32());
    COMP->jz(checkInexactAndOverflow);  // Result is denormal or zero - preserve but check

    // Check if result is infinity or NaN (exponent == 0x7FF) - don't round these
    COMP->cmp(expBits.r32(), 0x7FF);
    COMP->je(checkInexactAndOverflow);  // Result is Inf/NaN - preserve but check overflow

    // Both operands are normal, result is normal - round to single precision
    J_roundToSingle(b, frd);
  }

  // Check MXCSR for inexact and overflow from the division
  COMP->bind(checkInexactAndOverflow);
  {
    COMP->stmxcsr(mxcsrSlot);
    COMP->mov(mxcsrMem, mxcsrSlot);

    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());

    // Check if result is infinity when fra was NOT infinity - this indicates overflow
    // This catches cases like normal/denormal_min = Inf (overflow)
    // But normal/denormal_max = large_number (NOT overflow)
    Label checkInexactOnly = b->compiler->newLabel();

    // First check if fra was already infinity - if so, no overflow possible
    COMP->test(infFlagA, infFlagA);
    COMP->jnz(checkInexactOnly);  // fra was infinity, result infinity is not overflow

    // Check if result is infinity (exp == 0x7FF and frac == 0)
    x86::Gp resultBits = newGP64();
    x86::Gp resultExp = newGP64();
    x86::Gp resultFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->vmovq(resultBits, frd);
    COMP->mov(resultExp, resultBits);
    COMP->shr(resultExp, 52);
    COMP->and_(resultExp, 0x7FF);

    // If exponent is not 0x7FF, result is not infinity - no overflow
    COMP->cmp(resultExp.r32(), 0x7FF);
    COMP->jne(checkInexactOnly);  // Result is not Inf/NaN, no overflow

    // Exponent is 0x7FF - check if fraction is zero (infinity) vs non-zero (NaN)
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(resultFrac, resultBits);
    COMP->and_(resultFrac, fracMask);
    COMP->test(resultFrac, resultFrac);
    COMP->jnz(checkInexactOnly);  // Result is NaN, not overflow

    // Result is infinity from non-infinity fra - this is overflow
    // Set OX bit (bit 3 in BE = bit 28 in LE) and FX
    COMP->or_(fpscr, FPSCR_OX_BIT | FPSCR_FX_BIT);

    COMP->bind(checkInexactOnly);
    // Check for inexact (bit 5 in MXCSR)
    Label notInexact = b->compiler->newLabel();
    COMP->bt(mxcsrMem, 5);
    COMP->jnc(notInexact);

    // Inexact result detected - set FX bit in FPSCR
    COMP->or_(fpscr, FPSCR_FX_BIT);

    COMP->bind(notInexact);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  }

  COMP->bind(storeResult);

  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Square Root (Double-Precision) (x'FC00 002C')
void PPCInterpreter::PPCInterpreterJIT_fsqrtx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  Label setVxsqrt = b->compiler->newLabel();

  COMP->vmovsd(frb, FPRPtr(instr.frb));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check if frb is negative (and not -0 or NaN) - VXSQRT exception
  x86::Gp bBits = newGP64();
  x86::Gp isNegative = newGP32();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();

  COMP->xor_(isNegative, isNegative);
  COMP->vmovq(bBits, frb);

  // Check sign bit
  COMP->bt(bBits, 63);
  Label notNegative = b->compiler->newLabel();
  COMP->jnc(notNegative);

  // Sign bit is set - check if it's -0 (which is valid for sqrt)
  // -0 has sign=1, exp=0, frac=0
  x86::Gp absMask = newGP64();
  x86::Gp absValue = newGP64();
  COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
  COMP->mov(absValue, bBits);
  COMP->and_(absValue, absMask);
  COMP->test(absValue, absValue);
  COMP->jz(notNegative);  // It's -0, which is valid

  // Check if it's a negative NaN (NaN has exp=0x7FF, frac!=0)
  COMP->mov(expBits, bBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notNegative);  // Not exp=0x7FF, will be handled as negative number

  x86::Gp fracMask = newGP64();
  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, bBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jnz(notNegative);  // It's a negative NaN, handled by NaN logic

  // It's -Inf, which will produce NaN (handled by vsqrtsd)
  COMP->jmp(notNegative);

  COMP->bind(notNegative);

  // Check for negative non-zero, non-NaN value to set VXSQRT
  Label checkNaN = b->compiler->newLabel();
  {
    x86::Gp checkNeg = newGP64();
    COMP->vmovq(checkNeg, frb);

    // Skip if sign bit is not set
    Label noVxsqrt = b->compiler->newLabel();
    COMP->bt(checkNeg, 63);
    COMP->jnc(noVxsqrt);

    // Check if -0
    x86::Gp absVal = newGP64();
    COMP->mov(absVal, checkNeg);
    COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
    COMP->and_(absVal, absMask);
    COMP->test(absVal, absVal);
    COMP->jz(noVxsqrt);  // -0 is valid

    // Check if NaN (exp=0x7FF and frac!=0)
    x86::Gp exp = newGP64();
    COMP->mov(exp, checkNeg);
    COMP->shr(exp, 52);
    COMP->and_(exp, 0x7FF);
    COMP->cmp(exp.r32(), 0x7FF);
    COMP->jne(setVxsqrt);  // Not Inf/NaN, it's negative - set VXSQRT

    // exp is 0x7FF, check fraction
    x86::Gp frac = newGP64();
    COMP->mov(frac, checkNeg);
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->and_(frac, fracMask);
    COMP->test(frac, frac);
    COMP->jnz(noVxsqrt);  // It's NaN, no VXSQRT

    // It's -Inf - set VXSQRT
    COMP->bind(setVxsqrt);
    {
      x86::Gp fpscr = newGP32();
      COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
      COMP->or_(fpscr, (1u << 9) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXSQRT bit (BE bit 22 = LE bit 9)
      COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
      COMP->mov(isNegative, 1);
    }

    COMP->bind(noVxsqrt);
  }

  // Clear MXCSR exception flags before the operation
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  // Perform double-precision square root
  COMP->vsqrtsd(frd, frb, frb);

  // Determine the correct result:
  // Priority: SNaN -> QNaN > QNaN > negative -> default QNaN > normal result
  Label checkQNaNB = b->compiler->newLabel();
  Label checkNegative = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If frb is SNaN, use its converted QNaN
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frb is QNaN, propagate it
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkNegative);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // If frb was negative (VXSQRT), use default QNaN
  COMP->bind(checkNegative);
  COMP->test(isNegative, isNegative);
  COMP->jz(storeResult);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }

  COMP->bind(storeResult);

  // Check MXCSR for inexact result
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  {
    Label notInexact = b->compiler->newLabel();
    COMP->bt(mxcsrMem, 5);
    COMP->jnc(notInexact);

    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  // Store result to frD
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  // Classify result and set FPRF in FPSCR
  J_classifyAndSetFPRF(b, frd);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Compare Unordered (x'FC00 0000')
void PPCInterpreter::PPCInterpreterJIT_fcmpu(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  // Check for SNaN in both operands and set VXSNAN if found
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);

  // Get raw bits for manual comparison (to handle denormals correctly)
  x86::Gp aBits = newGP64();
  x86::Gp bBits = newGP64();
  COMP->vmovq(aBits, fra);
  COMP->vmovq(bBits, frb);

  // Check for NaN in fra (exp=0x7FF and frac!=0)
  x86::Gp aIsNaN = newGP32();
  COMP->xor_(aIsNaN, aIsNaN);
  {
    x86::Gp aExp = newGP64();
    x86::Gp aFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->mov(aExp, aBits);
    COMP->shr(aExp, 52);
    COMP->and_(aExp, 0x7FF);
    COMP->cmp(aExp.r32(), 0x7FF);
    Label aNotNaN = b->compiler->newLabel();
    COMP->jne(aNotNaN);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(aFrac, aBits);
    COMP->and_(aFrac, fracMask);
    COMP->test(aFrac, aFrac);
    COMP->jz(aNotNaN);
    COMP->mov(aIsNaN, 1);
    COMP->bind(aNotNaN);
  }

  // Check for NaN in frb
  x86::Gp bIsNaN = newGP32();
  COMP->xor_(bIsNaN, bIsNaN);
  {
    x86::Gp bExp = newGP64();
    x86::Gp bFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->mov(bExp, bBits);
    COMP->shr(bExp, 52);
    COMP->and_(bExp, 0x7FF);
    COMP->cmp(bExp.r32(), 0x7FF);
    Label bNotNaN = b->compiler->newLabel();
    COMP->jne(bNotNaN);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(bFrac, bBits);
    COMP->and_(bFrac, fracMask);
    COMP->test(bFrac, bFrac);
    COMP->jz(bNotNaN);
    COMP->mov(bIsNaN, 1);
    COMP->bind(bNotNaN);
  }

  // Build comparison result
  x86::Gp compareResult = newGP32();
  COMP->xor_(compareResult, compareResult);

  Label checkNaNB = b->compiler->newLabel();
  Label doCompare = b->compiler->newLabel();
  Label setUnordered = b->compiler->newLabel();
  Label setResult = b->compiler->newLabel();

  // If either is NaN, result is unordered
  COMP->test(aIsNaN, aIsNaN);
  COMP->jnz(setUnordered);
  COMP->test(bIsNaN, bIsNaN);
  COMP->jnz(setUnordered);
  COMP->jmp(doCompare);

  COMP->bind(setUnordered);
  COMP->mov(compareResult, 0x1);  // FU bit
  COMP->jmp(setResult);

  COMP->bind(doCompare);
  {
    // Manual IEEE 754 double comparison that handles denormals
    // Extract signs
    x86::Gp aSign = newGP64();
    x86::Gp bSign = newGP64();
    COMP->mov(aSign, aBits);
    COMP->shr(aSign, 63);
    COMP->mov(bSign, bBits);
    COMP->shr(bSign, 63);

    // Get absolute values (clear sign bit)
    x86::Gp aAbs = newGP64();
    x86::Gp bAbs = newGP64();
    x86::Gp absMask = newGP64();
    COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
    COMP->mov(aAbs, aBits);
    COMP->and_(aAbs, absMask);
    COMP->mov(bAbs, bBits);
    COMP->and_(bAbs, absMask);

    // Check for +0 == -0 case (both absolute values are 0)
    Label notBothZero = b->compiler->newLabel();
    Label setEqual = b->compiler->newLabel();
    Label setLess = b->compiler->newLabel();
    Label setGreater = b->compiler->newLabel();

    x86::Gp bothZeroCheck = newGP64();
    COMP->mov(bothZeroCheck, aAbs);
    COMP->or_(bothZeroCheck, bAbs);
    COMP->test(bothZeroCheck, bothZeroCheck);
    COMP->jnz(notBothZero);
    // Both are zero (regardless of sign)
    COMP->jmp(setEqual);

    COMP->bind(notBothZero);

    // Different signs: negative < positive (unless both zero, handled above)
    Label sameSign = b->compiler->newLabel();
    COMP->cmp(aSign.r32(), bSign.r32());
    COMP->je(sameSign);

    // Different signs: if aSign=1 (negative), a < b; else a > b
    COMP->test(aSign.r32(), aSign.r32());
    COMP->jnz(setLess);
    COMP->jmp(setGreater);

    COMP->bind(sameSign);
    // Same sign - compare magnitudes
    // For positive numbers: larger magnitude = larger value
    // For negative numbers: larger magnitude = smaller value

    Label aAbsGreater = b->compiler->newLabel();
    Label aAbsLess = b->compiler->newLabel();
    Label aAbsEqual = b->compiler->newLabel();

    COMP->cmp(aAbs, bAbs);
    COMP->ja(aAbsGreater);
    COMP->jb(aAbsLess);
    COMP->jmp(setEqual);  // aAbs == bAbs means equal

    COMP->bind(aAbsGreater);
    // |a| > |b|: if positive, a > b; if negative, a < b
    COMP->test(aSign.r32(), aSign.r32());
    COMP->jnz(setLess);
    COMP->jmp(setGreater);

    COMP->bind(aAbsLess);
    // |a| < |b|: if positive, a < b; if negative, a > b
    COMP->test(aSign.r32(), aSign.r32());
    COMP->jnz(setGreater);
    COMP->jmp(setLess);

    COMP->bind(setLess);
    COMP->mov(compareResult, 0x8);  // FL bit
    COMP->jmp(setResult);

    COMP->bind(setGreater);
    COMP->mov(compareResult, 0x4);  // FG bit
    COMP->jmp(setResult);

    COMP->bind(setEqual);
    COMP->mov(compareResult, 0x2);  // FE bit
  }

  COMP->bind(setResult);

  // Update FPSCR FPRF field (FPCC is bits 12-15)
  x86::Gp fpscr = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->and_(fpscr, ~(0xF << 12));
  x86::Gp fprfShifted = newGP32();
  COMP->mov(fprfShifted, compareResult);
  COMP->shl(fprfShifted, 12);
  COMP->or_(fpscr, fprfShifted);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  // Update CR field specified by crfD
  x86::Gp crReg = newGP32();
  COMP->mov(crReg, CRValPtr());

  u32 crField = instr.crfd;
  u32 shiftAmount = (7 - crField) * 4;
  u32 mask = ~(0xF << shiftAmount);

  COMP->and_(crReg, mask);
  x86::Gp crBits = newGP32();
  COMP->mov(crBits, compareResult);
  if (shiftAmount > 0) {
    COMP->shl(crBits, shiftAmount);
  }
  COMP->or_(crReg, crBits);
  COMP->mov(CRValPtr(), crReg);
}

// Floating Compare Ordered (x'FC00 0040')
void PPCInterpreter::PPCInterpreterJIT_fcmpo(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));

  // Get raw bits for manual comparison (to handle denormals correctly)
  x86::Gp aBits = newGP64();
  x86::Gp bBits = newGP64();
  COMP->vmovq(aBits, fra);
  COMP->vmovq(bBits, frb);

  // Check for SNaN in fra (exp=0x7FF, frac!=0, bit51=0)
  x86::Gp aIsSNaN = newGP32();
  COMP->xor_(aIsSNaN, aIsSNaN);
  {
    x86::Gp aExp = newGP64();
    x86::Gp aFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->mov(aExp, aBits);
    COMP->shr(aExp, 52);
    COMP->and_(aExp, 0x7FF);
    COMP->cmp(aExp.r32(), 0x7FF);
    Label aNotSNaN = b->compiler->newLabel();
    COMP->jne(aNotSNaN);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(aFrac, aBits);
    COMP->and_(aFrac, fracMask);
    COMP->test(aFrac, aFrac);
    COMP->jz(aNotSNaN);

    // Check bit 51 is clear (SNaN)
    COMP->bt(aBits, 51);
    COMP->jc(aNotSNaN);

    COMP->mov(aIsSNaN, 1);
    COMP->bind(aNotSNaN);
  }

  // Check for SNaN in frb
  x86::Gp bIsSNaN = newGP32();
  COMP->xor_(bIsSNaN, bIsSNaN);
  {
    x86::Gp bExp = newGP64();
    x86::Gp bFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->mov(bExp, bBits);
    COMP->shr(bExp, 52);
    COMP->and_(bExp, 0x7FF);
    COMP->cmp(bExp.r32(), 0x7FF);
    Label bNotSNaN = b->compiler->newLabel();
    COMP->jne(bNotSNaN);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(bFrac, bBits);
    COMP->and_(bFrac, fracMask);
    COMP->test(bFrac, bFrac);
    COMP->jz(bNotSNaN);

    COMP->bt(bBits, 51);
    COMP->jc(bNotSNaN);

    COMP->mov(bIsSNaN, 1);
    COMP->bind(bNotSNaN);
  }

  // Check for QNaN in fra (exp=0x7FF, frac!=0, bit51=1)
  x86::Gp aIsQNaN = newGP32();
  COMP->xor_(aIsQNaN, aIsQNaN);
  {
    x86::Gp aExp = newGP64();
    x86::Gp aFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->mov(aExp, aBits);
    COMP->shr(aExp, 52);
    COMP->and_(aExp, 0x7FF);
    COMP->cmp(aExp.r32(), 0x7FF);
    Label aNotQNaN = b->compiler->newLabel();
    COMP->jne(aNotQNaN);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(aFrac, aBits);
    COMP->and_(aFrac, fracMask);
    COMP->test(aFrac, aFrac);
    COMP->jz(aNotQNaN);

    // Check bit 51 is set (QNaN)
    COMP->bt(aBits, 51);
    COMP->jnc(aNotQNaN);

    COMP->mov(aIsQNaN, 1);
    COMP->bind(aNotQNaN);
  }

  // Check for QNaN in frb
  x86::Gp bIsQNaN = newGP32();
  COMP->xor_(bIsQNaN, bIsQNaN);
  {
    x86::Gp bExp = newGP64();
    x86::Gp bFrac = newGP64();
    x86::Gp fracMask = newGP64();

    COMP->mov(bExp, bBits);
    COMP->shr(bExp, 52);
    COMP->and_(bExp, 0x7FF);
    COMP->cmp(bExp.r32(), 0x7FF);
    Label bNotQNaN = b->compiler->newLabel();
    COMP->jne(bNotQNaN);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(bFrac, bBits);
    COMP->and_(bFrac, fracMask);
    COMP->test(bFrac, bFrac);
    COMP->jz(bNotQNaN);

    COMP->bt(bBits, 51);
    COMP->jnc(bNotQNaN);

    COMP->mov(bIsQNaN, 1);
    COMP->bind(bNotQNaN);
  }

  // Check if either is any NaN (SNaN or QNaN)
  x86::Gp aIsNaN = newGP32();
  COMP->mov(aIsNaN, aIsSNaN);
  COMP->or_(aIsNaN, aIsQNaN);

  x86::Gp bIsNaN = newGP32();
  COMP->mov(bIsNaN, bIsSNaN);
  COMP->or_(bIsNaN, bIsQNaN);

  // Build comparison result
  x86::Gp compareResult = newGP32();
  COMP->xor_(compareResult, compareResult);

  Label doCompare = b->compiler->newLabel();
  Label setUnordered = b->compiler->newLabel();
  Label setResult = b->compiler->newLabel();

  // If either is NaN, result is unordered
  COMP->test(aIsNaN, aIsNaN);
  COMP->jnz(setUnordered);
  COMP->test(bIsNaN, bIsNaN);
  COMP->jnz(setUnordered);
  COMP->jmp(doCompare);

  COMP->bind(setUnordered);
  COMP->mov(compareResult, 0x1);  // FU bit
  COMP->jmp(setResult);

  COMP->bind(doCompare);
  {
    // Manual IEEE 754 double comparison that handles denormals
    // Extract signs
    x86::Gp aSign = newGP64();
    x86::Gp bSign = newGP64();
    COMP->mov(aSign, aBits);
    COMP->shr(aSign, 63);
    COMP->mov(bSign, bBits);
    COMP->shr(bSign, 63);

    // Get absolute values (clear sign bit)
    x86::Gp aAbs = newGP64();
    x86::Gp bAbs = newGP64();
    x86::Gp absMask = newGP64();
    COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
    COMP->mov(aAbs, aBits);
    COMP->and_(aAbs, absMask);
    COMP->mov(bAbs, bBits);
    COMP->and_(bAbs, absMask);

    // Check for +0 == -0 case (both absolute values are 0)
    Label notBothZero = b->compiler->newLabel();
    Label setEqual = b->compiler->newLabel();
    Label setLess = b->compiler->newLabel();
    Label setGreater = b->compiler->newLabel();

    x86::Gp bothZeroCheck = newGP64();
    COMP->mov(bothZeroCheck, aAbs);
    COMP->or_(bothZeroCheck, bAbs);
    COMP->test(bothZeroCheck, bothZeroCheck);
    COMP->jnz(notBothZero);
    COMP->jmp(setEqual);

    COMP->bind(notBothZero);

    // Different signs: negative < positive
    Label sameSign = b->compiler->newLabel();
    COMP->cmp(aSign.r32(), bSign.r32());
    COMP->je(sameSign);

    // Different signs: if aSign=1 (negative), a < b; else a > b
    COMP->test(aSign.r32(), aSign.r32());
    COMP->jnz(setLess);
    COMP->jmp(setGreater);

    COMP->bind(sameSign);
    // Same sign - compare magnitudes
    COMP->cmp(aAbs, bAbs);
    Label aAbsGreater = b->compiler->newLabel();
    Label aAbsLess = b->compiler->newLabel();
    COMP->ja(aAbsGreater);
    COMP->jb(aAbsLess);
    COMP->jmp(setEqual);

    COMP->bind(aAbsGreater);
    // |a| > |b|: if positive, a > b; if negative, a < b
    COMP->test(aSign.r32(), aSign.r32());
    COMP->jnz(setLess);
    COMP->jmp(setGreater);

    COMP->bind(aAbsLess);
    // |a| < |b|: if positive, a < b; if negative, a > b
    COMP->test(aSign.r32(), aSign.r32());
    COMP->jnz(setGreater);
    COMP->jmp(setLess);

    COMP->bind(setLess);
    COMP->mov(compareResult, 0x8);  // FL bit
    COMP->jmp(setResult);

    COMP->bind(setGreater);
    COMP->mov(compareResult, 0x4);  // FG bit
    COMP->jmp(setResult);

    COMP->bind(setEqual);
    COMP->mov(compareResult, 0x2);  // FE bit
  }

  COMP->bind(setResult);

  // Handle FPSCR exceptions for fcmpo:
  // - If SNaN: set VXSNAN, and if VE=0, also set VXVC
  // - If QNaN (but not SNaN): set VXVC
  x86::Gp hasSNaN = newGP32();
  COMP->mov(hasSNaN, aIsSNaN);
  COMP->or_(hasSNaN, bIsSNaN);

  x86::Gp hasQNaN = newGP32();
  COMP->mov(hasQNaN, aIsQNaN);
  COMP->or_(hasQNaN, bIsQNaN);

  x86::Gp fpscr = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());

  Label noSNaN = b->compiler->newLabel();
  Label afterExceptions = b->compiler->newLabel();

  // If SNaN present, set VXSNAN
  COMP->test(hasSNaN, hasSNaN);
  COMP->jz(noSNaN);
  COMP->or_(fpscr, FPSCR_VXSNAN_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);

  // If VE=0, also set VXVC (VE is bit 7 in LE representation)
  COMP->bt(fpscr, 7);
  COMP->jc(afterExceptions);  // VE=1, skip VXVC
  COMP->or_(fpscr, (1u << 19) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXVC bit
  COMP->jmp(afterExceptions);

  COMP->bind(noSNaN);
  // If only QNaN (no SNaN), set VXVC
  COMP->test(hasQNaN, hasQNaN);
  COMP->jz(afterExceptions);
  COMP->or_(fpscr, (1u << 19) | FPSCR_VX_BIT | FPSCR_FX_BIT);  // VXVC bit

  COMP->bind(afterExceptions);

  // Update FPSCR FPRF field (FPCC is bits 12-15)
  COMP->and_(fpscr, ~(0xF << 12));
  x86::Gp fprfShifted = newGP32();
  COMP->mov(fprfShifted, compareResult);
  COMP->shl(fprfShifted, 12);
  COMP->or_(fpscr, fprfShifted);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

  // Update CR field specified by crfD
  x86::Gp crReg = newGP32();
  COMP->mov(crReg, CRValPtr());

  u32 crField = instr.crfd;
  u32 shiftAmount = (7 - crField) * 4;
  u32 mask = ~(0xF << shiftAmount);

  COMP->and_(crReg, mask);
  x86::Gp crBits = newGP32();
  COMP->mov(crBits, compareResult);
  if (shiftAmount > 0) {
    COMP->shl(crBits, shiftAmount);
  }
  COMP->or_(crReg, crBits);
  COMP->mov(CRValPtr(), crReg);
}

// Floating Negate (x'FC00 0050')
void PPCInterpreter::PPCInterpreterJIT_fnegx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Gp frb = newGP64();

  // Load frB as integer
  COMP->mov(frb, FPRPtr(instr.frb));

  // XOR with sign bit mask to flip the sign
  x86::Gp signMask = newGP64();
  COMP->mov(signMask, 0x8000000000000000ull);
  COMP->xor_(frb, signMask);

  // Store result to frD
  COMP->mov(FPRPtr(instr.frd), frb);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Move Register (x'FC00 0090')
void PPCInterpreter::PPCInterpreterJIT_fmrx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  // Simple 64-bit move from frB to frD
  x86::Gp frb = newGP64();

  COMP->mov(frb, FPRPtr(instr.frb));
  COMP->mov(FPRPtr(instr.frd), frb);

  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Select (x'FC00 002E')
// Note: fsel does NOT check for NaN - it only checks the sign bit and treats -0.0 as >= 0.0
// The comparison is: if frA >= +0.0 (including -0.0 which equals +0.0) then frC else frB
void PPCInterpreter::PPCInterpreterJIT_fselx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Gp fraBits = newGP64();
  x86::Gp frbBits = newGP64();
  x86::Gp frcBits = newGP64();
  x86::Gp frdBits = newGP64();

  // Load all operands as raw bits
  COMP->mov(fraBits, FPRPtr(instr.fra));
  COMP->mov(frbBits, FPRPtr(instr.frb));
  COMP->mov(frcBits, FPRPtr(instr.frc));

  // fsel comparison: frA >= 0.0
  // In IEEE 754, a value is >= 0.0 if:
  // - Sign bit is 0 (positive), OR
  // - The value is -0.0 (sign=1, exp=0, frac=0)
  // 
  // Simplified: frA >= 0.0 is equivalent to checking if the value
  // interpreted as a signed 64-bit integer is >= 0x8000000000000000 when
  // the sign bit is 0, OR the value is exactly -0.0
  //
  // Even simpler approach: check if NOT (negative and not -0.0)
  // frA < 0.0 only if sign bit is set AND (exp != 0 OR frac != 0)

  Label selectFrB = b->compiler->newLabel();
  Label done = b->compiler->newLabel();

  // Check sign bit first
  COMP->bt(fraBits, 63);
  COMP->jnc(done);  // Sign bit clear -> frA >= 0.0, use frC (default)

  // Sign bit is set - check if it's -0.0
  // -0.0 has bits 0x8000000000000000 (sign=1, exp=0, frac=0)
  x86::Gp absMask = newGP64();
  x86::Gp absValue = newGP64();
  COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
  COMP->mov(absValue, fraBits);
  COMP->and_(absValue, absMask);
  COMP->test(absValue, absValue);
  COMP->jz(done);  // It's -0.0, which is >= 0.0, use frC (default)

  // frA is negative and not -0.0, so frA < 0.0 -> select frB
  COMP->bind(selectFrB);
  COMP->mov(frdBits, frbBits);
  COMP->mov(FPRPtr(instr.frd), frdBits);
  COMP->jmp(done);

  // Default: frA >= 0.0, select frC
  COMP->bind(done);
  // Only store frC if we didn't jump to selectFrB
  // We need to restructure this slightly

  // Actually, let's restructure for clarity
  Label storeResult = b->compiler->newLabel();

  // Reset and redo
  COMP->mov(frdBits, frcBits);  // Default: select frC

  // Check sign bit
  COMP->bt(fraBits, 63);
  COMP->jnc(storeResult);  // Sign bit clear -> frA >= 0.0, keep frC

  // Sign bit is set - check if it's -0.0
  COMP->mov(absValue, fraBits);
  COMP->and_(absValue, absMask);
  COMP->test(absValue, absValue);
  COMP->jz(storeResult);  // It's -0.0, keep frC

  // frA < 0.0, select frB
  COMP->mov(frdBits, frbBits);

  COMP->bind(storeResult);
  COMP->mov(FPRPtr(instr.frd), frdBits);

  // fsel does NOT update FPSCR or set FPRF
  // If Rc bit is set, update CR1
  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Round to Single (x'FC00 0018')
void PPCInterpreter::PPCInterpreterJIT_frspx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(frb, FPRPtr(instr.frb));

  // Check for SNaN and set VXSNAN if found
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Save original value for comparison
  x86::Gp beforeRound = newGP64();
  COMP->vmovq(beforeRound, frb);

  // Convert double to single and back to double
  COMP->vcvtsd2ss(frd, frb, frb);
  COMP->vcvtss2sd(frd, frd, frd);

  // Handle NaN cases
  Label checkQNaN = b->compiler->newLabel();
  Label doFPRF = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // If frb is SNaN, use converted QNaN
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaN);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // If frb is QNaN, propagate it
  COMP->bind(checkQNaN);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(doFPRF);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  COMP->bind(doFPRF);
  // For non-NaN results, check if rounding changed the value (FI bit)
  // and set FR if result magnitude increased
  {
    x86::Gp afterRound = newGP64();
    x86::Gp fpscr = newGP32();

    COMP->vmovq(afterRound, frd);
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());

    // Clear FI and FR bits first (bits 14 and 13 in LE = bits 17 and 18 in BE)
    COMP->and_(fpscr, ~((1u << 14) | (1u << 13)));

    // Check if value changed (FI = inexact)
    Label notInexact = b->compiler->newLabel();
    COMP->cmp(beforeRound, afterRound);
    COMP->je(notInexact);

    // Set FI bit and FX bit
    COMP->or_(fpscr, (1u << 14) | FPSCR_FX_BIT);

    // Check if result magnitude is greater than original (FR bit)
    // Compare absolute values
    x86::Gp absBefore = newGP64();
    x86::Gp absAfter = newGP64();
    x86::Gp absMask = newGP64();

    COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
    COMP->mov(absBefore, beforeRound);
    COMP->and_(absBefore, absMask);
    COMP->mov(absAfter, afterRound);
    COMP->and_(absAfter, absMask);

    COMP->cmp(absAfter, absBefore);
    COMP->jbe(notInexact);

    // Set FR bit
    COMP->or_(fpscr, (1u << 13));

    COMP->bind(notInexact);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  }

  // Classify and set FPRF
  J_classifyAndSetFPRF(b, frd);

  COMP->bind(storeResult);
  COMP->vmovsd(FPRPtr(instr.frd), frd);

  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Convert to Integer Word (x'FC00 001C')
// Convert frB to 32-bit signed integer using FPSCR[RN] rounding mode
void PPCInterpreter::PPCInterpreterJIT_fctiwx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Xmm frb = newXMM();
  x86::Gp result = newGP64();
  x86::Gp intResult = newGP32();
  x86::Gp frbBits = newGP64();

  COMP->vmovsd(frb, FPRPtr(instr.frb));
  COMP->vmovq(frbBits, frb);

  // Check for SNaN and set VXSNAN if found
  J_checkAndSetSNaN(b, frb);

  // Check if NaN (exp=0x7FF and frac!=0)
  x86::Gp isNaN = newGP32();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();
  x86::Gp fracMask = newGP64();

  COMP->xor_(isNaN, isNaN);
  COMP->mov(expBits, frbBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);

  Label notNaN = b->compiler->newLabel();
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notNaN);

  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, frbBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jz(notNaN);
  COMP->mov(isNaN, 1);

  COMP->bind(notNaN);

  // Check if Infinity (exp=0x7FF and frac==0)
  x86::Gp isInf = newGP32();
  COMP->xor_(isInf, isInf);
  {
    Label notInf = b->compiler->newLabel();
    x86::Gp expTmp = newGP64();
    x86::Gp fracTmp = newGP64();

    COMP->mov(expTmp, frbBits);
    COMP->shr(expTmp, 52);
    COMP->and_(expTmp, 0x7FF);
    COMP->cmp(expTmp.r32(), 0x7FF);
    COMP->jne(notInf);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(fracTmp, frbBits);
    COMP->and_(fracTmp, fracMask);
    COMP->test(fracTmp, fracTmp);
    COMP->jnz(notInf);

    COMP->mov(isInf, 1);
    COMP->bind(notInf);
  }

  // Labels
  Label handleNaN = b->compiler->newLabel();
  Label handleOverflowPos = b->compiler->newLabel();
  Label handleOverflowNeg = b->compiler->newLabel();
  Label doConversion = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // Save and setup MXCSR
  x86::Gp mxcsrOrig = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrOrig, mxcsrSlot);

  // Check for NaN first
  COMP->test(isNaN, isNaN);
  COMP->jnz(handleNaN);

  // Check for Infinity - treat as overflow
  COMP->test(isInf, isInf);
  COMP->jnz(handleOverflowPos);  // Will check sign below

  // Get FPSCR rounding mode (bits 0-1)
  x86::Gp fpscr = newGP32();
  x86::Gp roundMode = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->mov(roundMode, fpscr);
  COMP->and_(roundMode, 0x3);

  // Map PPC rounding mode to x86 MXCSR
  x86::Gp mxcsrNew = newGP32();
  COMP->mov(mxcsrNew, mxcsrOrig);
  COMP->and_(mxcsrNew, ~(0x3 << 13));

  Label rmNearest = b->compiler->newLabel();
  Label rmTowardZero = b->compiler->newLabel();
  Label rmPlusInf = b->compiler->newLabel();
  Label rmMinusInf = b->compiler->newLabel();
  Label rmDone = b->compiler->newLabel();

  COMP->test(roundMode, roundMode);
  COMP->jz(rmNearest);
  COMP->cmp(roundMode, 1);
  COMP->je(rmTowardZero);
  COMP->cmp(roundMode, 2);
  COMP->je(rmPlusInf);
  COMP->jmp(rmMinusInf);

  COMP->bind(rmNearest);
  COMP->jmp(rmDone);

  COMP->bind(rmTowardZero);
  COMP->or_(mxcsrNew, 0x3 << 13);
  COMP->jmp(rmDone);

  COMP->bind(rmPlusInf);
  COMP->or_(mxcsrNew, 0x2 << 13);
  COMP->jmp(rmDone);

  COMP->bind(rmMinusInf);
  COMP->or_(mxcsrNew, 0x1 << 13);

  COMP->bind(rmDone);
  COMP->mov(mxcsrSlot, mxcsrNew);
  COMP->ldmxcsr(mxcsrSlot);

  // Check bounds
  x86::Xmm maxVal = newXMM();
  x86::Xmm minVal = newXMM();
  x86::Gp maxBits = newGP64();
  x86::Gp minBits = newGP64();

  COMP->mov(maxBits, 0x41DFFFFFFFC00000ull);  // 2147483647.0
  COMP->vmovq(maxVal, maxBits);
  COMP->mov(minBits, 0xC1E0000000000000ull);  // -2147483648.0
  COMP->vmovq(minVal, minBits);

  COMP->vucomisd(frb, maxVal);
  COMP->ja(handleOverflowPos);

  COMP->vucomisd(minVal, frb);
  COMP->ja(handleOverflowNeg);

  COMP->jmp(doConversion);

  // NaN case - result = 0x80000000, sign-extended
  COMP->bind(handleNaN);
  {
    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscrTmp, (1u << 8) | FPSCR_VX_BIT | FPSCR_FX_BIT);
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }
  COMP->mov(intResult, 0x80000000u);
  COMP->movsxd(result, intResult);
  COMP->jmp(storeResult);

  // Overflow positive - check sign for +Inf vs large positive
  COMP->bind(handleOverflowPos);
  {
    // Check if it's actually negative infinity
    COMP->bt(frbBits, 63);
    COMP->jc(handleOverflowNeg);

    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscrTmp, (1u << 8) | FPSCR_VX_BIT | FPSCR_FX_BIT);
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }
  COMP->mov(intResult, 0x7FFFFFFFu);
  COMP->mov(result.r32(), intResult);
  COMP->mov(mxcsrSlot, mxcsrOrig);
  COMP->ldmxcsr(mxcsrSlot);
  COMP->jmp(storeResult);

  // Overflow negative
  COMP->bind(handleOverflowNeg);
  {
    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscrTmp, (1u << 8) | FPSCR_VX_BIT | FPSCR_FX_BIT);
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }
  COMP->mov(intResult, 0x80000000u);
  COMP->movsxd(result, intResult);
  COMP->mov(mxcsrSlot, mxcsrOrig);
  COMP->ldmxcsr(mxcsrSlot);
  COMP->jmp(storeResult);

  // Normal conversion
  COMP->bind(doConversion);
  COMP->vcvtsd2si(intResult, frb);
  COMP->mov(mxcsrSlot, mxcsrOrig);
  COMP->ldmxcsr(mxcsrSlot);

  // Sign-extend the 32-bit result to 64 bits
  COMP->movsxd(result, intResult);

  // Check for inexact result - compare numerically (handles -0.0 == +0.0)
  {
    x86::Xmm converted = newXMM();
    x86::Gp signedResult = newGP32();
    COMP->mov(signedResult, intResult);
    COMP->vcvtsi2sd(converted, converted, signedResult);

    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));  // Clear FI, FR

    Label notInexact = b->compiler->newLabel();

    // Numeric compare: if equal -> no inexact
    COMP->vucomisd(frb, converted);
    COMP->je(notInexact);

    // Inexact - set FI bit AND FX bit (exception summary)
    COMP->or_(fpscrTmp, (1u << 14) | FPSCR_FX_BIT);

    // Check FR (result magnitude increased): compare absolute bitpatterns
    x86::Gp absBefore = newGP64();
    x86::Gp absAfter = newGP64();
    x86::Gp absMaskTmp = newGP64();
    COMP->mov(absMaskTmp, 0x7FFFFFFFFFFFFFFFull);
    COMP->vmovq(absBefore, frb);
    COMP->and_(absBefore, absMaskTmp);
    COMP->vmovq(absAfter, converted);
    COMP->and_(absAfter, absMaskTmp);

    Label noFR = b->compiler->newLabel();
    COMP->cmp(absAfter, absBefore);
    COMP->jbe(noFR);
    COMP->or_(fpscrTmp, (1u << 13));

    COMP->bind(noFR);
    COMP->bind(notInexact);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }

  COMP->bind(storeResult);
  COMP->mov(FPRPtr(instr.frd), result);

  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Convert to Integer Word with Round toward Zero (x'FC00 001E')
void PPCInterpreter::PPCInterpreterJIT_fctiwzx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  J_checkFPUEnabled(b);

  x86::Xmm frb = newXMM();
  x86::Gp result = newGP64();
  x86::Gp intResult = newGP32();
  x86::Gp frbBits = newGP64();

  COMP->vmovsd(frb, FPRPtr(instr.frb));
  COMP->vmovq(frbBits, frb);

  // Check for SNaN
  J_checkAndSetSNaN(b, frb);

  // Check if NaN
  x86::Gp isNaN = newGP32();
  x86::Gp expBits = newGP64();
  x86::Gp fracBits = newGP64();
  x86::Gp fracMask = newGP64();

  COMP->xor_(isNaN, isNaN);
  COMP->mov(expBits, frbBits);
  COMP->shr(expBits, 52);
  COMP->and_(expBits, 0x7FF);

  Label notNaN = b->compiler->newLabel();
  COMP->cmp(expBits.r32(), 0x7FF);
  COMP->jne(notNaN);

  COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
  COMP->mov(fracBits, frbBits);
  COMP->and_(fracBits, fracMask);
  COMP->test(fracBits, fracBits);
  COMP->jz(notNaN);
  COMP->mov(isNaN, 1);

  COMP->bind(notNaN);

  // Check if Infinity
  x86::Gp isInf = newGP32();
  COMP->xor_(isInf, isInf);
  {
    Label notInf = b->compiler->newLabel();
    x86::Gp expTmp = newGP64();
    x86::Gp fracTmp = newGP64();

    COMP->mov(expTmp, frbBits);
    COMP->shr(expTmp, 52);
    COMP->and_(expTmp, 0x7FF);
    COMP->cmp(expTmp.r32(), 0x7FF);
    COMP->jne(notInf);

    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(fracTmp, frbBits);
    COMP->and_(fracTmp, fracMask);
    COMP->test(fracTmp, fracTmp);
    COMP->jnz(notInf);

    COMP->mov(isInf, 1);
    COMP->bind(notInf);
  }

  Label handleNaN = b->compiler->newLabel();
  Label handleOverflowPos = b->compiler->newLabel();
  Label handleOverflowNeg = b->compiler->newLabel();
  Label doConversion = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  COMP->test(isNaN, isNaN);
  COMP->jnz(handleNaN);

  // Check for Infinity
  COMP->test(isInf, isInf);
  COMP->jnz(handleOverflowPos);

  // Check bounds
  x86::Xmm maxVal = newXMM();
  x86::Xmm minVal = newXMM();
  x86::Gp maxBits = newGP64();
  x86::Gp minBits = newGP64();

  COMP->mov(maxBits, 0x41DFFFFFFFC00000ull);
  COMP->vmovq(maxVal, maxBits);
  COMP->mov(minBits, 0xC1E0000000000000ull);
  COMP->vmovq(minVal, minBits);

  COMP->vucomisd(frb, maxVal);
  COMP->ja(handleOverflowPos);

  COMP->vucomisd(minVal, frb);
  COMP->ja(handleOverflowNeg);

  COMP->jmp(doConversion);

  // NaN case
  COMP->bind(handleNaN);
  {
    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscrTmp, (1u << 8) | FPSCR_VX_BIT | FPSCR_FX_BIT);
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }
  COMP->mov(intResult, 0x80000000u);
  COMP->movsxd(result, intResult);
  COMP->jmp(storeResult);

  // Overflow positive
  COMP->bind(handleOverflowPos);
  {
    // Check if negative
    COMP->bt(frbBits, 63);
    COMP->jc(handleOverflowNeg);

    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscrTmp, (1u << 8) | FPSCR_VX_BIT | FPSCR_FX_BIT);
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }
  COMP->mov(intResult, 0x7FFFFFFFu);
  COMP->mov(result.r32(), intResult);
  COMP->jmp(storeResult);

  // Overflow negative
  COMP->bind(handleOverflowNeg);
  {
    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscrTmp, (1u << 8) | FPSCR_VX_BIT | FPSCR_FX_BIT);
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }
  COMP->mov(intResult, 0x80000000u);
  COMP->movsxd(result, intResult);
  COMP->jmp(storeResult);

  // Normal conversion with truncation
  COMP->bind(doConversion);
  COMP->vcvttsd2si(intResult, frb);

  // Sign-extend
  COMP->movsxd(result, intResult);

  // Check for inexact (numeric compare)
  {
    x86::Xmm converted = newXMM();
    x86::Gp signedResult = newGP32();
    COMP->mov(signedResult, intResult);
    COMP->vcvtsi2sd(converted, converted, signedResult);

    x86::Gp fpscrTmp = newGP32();
    COMP->mov(fpscrTmp, FPSCRPtr().Ptr<u32>());
    COMP->and_(fpscrTmp, ~((1u << 14) | (1u << 13)));

    Label notInexact = b->compiler->newLabel();

    COMP->vucomisd(frb, converted);
    COMP->je(notInexact);

    COMP->or_(fpscrTmp, (1u << 14) | FPSCR_FX_BIT);

    x86::Gp absBefore = newGP64();
    x86::Gp absAfter = newGP64();
    x86::Gp absMaskTmp = newGP64();
    COMP->mov(absMaskTmp, 0x7FFFFFFFFFFFFFFFull);
    COMP->vmovq(absBefore, frb);
    COMP->and_(absBefore, absMaskTmp);
    COMP->vmovq(absAfter, converted);
    COMP->and_(absAfter, absMaskTmp);

    Label noFR = b->compiler->newLabel();
    COMP->cmp(absAfter, absBefore);
    COMP->jbe(noFR);
    COMP->or_(fpscrTmp, (1u << 13));

    COMP->bind(noFR);
    COMP->bind(notInexact);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscrTmp);
  }

  COMP->bind(storeResult);
  COMP->mov(FPRPtr(instr.frd), result);

  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

//
// Bugged instructions, mostly rounding and CR errors, but still
// NOTE: Most of these are far more superior here than on the interpreter in terms of accuracy, 
// so keeping them on for the moment
//

// Floating Negative Multiply-Add (Double-Precision) (x'FC00 003E')
/* HAS ISSUES -> 56 Failed tests */
void PPCInterpreter::PPCInterpreterJIT_fnmaddx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frc = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));
  COMP->vmovsd(frc, FPRPtr(instr.frc));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);
  J_checkAndSetSNaN(b, frc);

  // Check for SNaN/QNaN in fra (highest priority)
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN/QNaN in frb (second priority - operand order a, b, c)
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for SNaN/QNaN in frc (lowest priority)
  x86::Gp snanFlagC = newGP32();
  x86::Gp snanQNaNC = newGP64();
  COMP->xor_(snanFlagC, snanFlagC);
  J_checkSNaNAndGetQNaN(b, frc, snanFlagC, snanQNaNC);

  x86::Gp qnanFlagC = newGP32();
  x86::Gp qnanValueC = newGP64();
  COMP->xor_(qnanFlagC, qnanFlagC);
  J_checkQNaNAndGetValue(b, frc, qnanFlagC, qnanValueC);

  // Check for Inf * 0 invalid operation
  x86::Gp vximzFlag = newGP32();
  J_checkInfMulZero(b, fra, frc, vximzFlag);

  // NaN priority check: fra (any NaN) > frb (any NaN) > frc (any NaN)
  // For each operand, check SNaN first (convert to QNaN), then QNaN
  // This ensures we return the first NaN in operand order a, b, c
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkNaNC = b->compiler->newLabel();
  Label checkQNaNC = b->compiler->newLabel();
  Label checkVximz = b->compiler->newLabel();
  Label computeFMA = b->compiler->newLabel();
  Label doNegate = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // fra SNaN -> QNaN (highest priority)
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // fra QNaN (still fra priority - check before moving to frb)
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // frb SNaN -> QNaN (second priority)
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // frb QNaN (still frb priority - check before moving to frc)
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkNaNC);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // frc SNaN -> QNaN (lowest priority)
  COMP->bind(checkNaNC);
  COMP->test(snanFlagC, snanFlagC);
  COMP->jz(checkQNaNC);
  COMP->vmovq(frd, snanQNaNC);
  COMP->jmp(storeResult);

  // frc QNaN
  COMP->bind(checkQNaNC);
  COMP->test(qnanFlagC, qnanFlagC);
  COMP->jz(checkVximz);
  COMP->vmovq(frd, qnanValueC);
  COMP->jmp(storeResult);

  // VXIMZ (Inf * 0) - checked after all operand NaNs
  COMP->bind(checkVximz);
  COMP->test(vximzFlag, vximzFlag);
  COMP->jz(computeFMA);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // No NaN operands - compute FMA: frd = (fra * frc) + frb
  COMP->bind(computeFMA);

  // Clear MXCSR exception flags before the operation to detect inexact results
  x86::Gp mxcsrMem = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);
  COMP->and_(mxcsrMem, ~0x3F);
  COMP->mov(mxcsrSlot, mxcsrMem);
  COMP->ldmxcsr(mxcsrSlot);

  COMP->vmovaps(frd, fra);
  COMP->vfmadd213sd(frd, frc, frb);

  // Check MXCSR for inexact result (Precision Exception - bit 5)
  // This handles cases like operations with denormals
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrMem, mxcsrSlot);

  {
    Label notInexact = b->compiler->newLabel();
    COMP->bt(mxcsrMem, 5);
    COMP->jnc(notInexact);

    // Inexact result detected - set FX bit in FPSCR
    x86::Gp fpscr = newGP32();
    COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
    COMP->or_(fpscr, FPSCR_FX_BIT);
    COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);

    COMP->bind(notInexact);
  }

  // Now negate the result (unless it's NaN from the FMA itself, e.g., inf - inf)
  COMP->bind(doNegate);
  {
    x86::Gp resultBits = newGP64();
    x86::Gp expBits = newGP64();
    x86::Gp fracBits = newGP64();
    Label notNaN = b->compiler->newLabel();

    COMP->vmovq(resultBits, frd);
    COMP->mov(expBits, resultBits);
    COMP->shr(expBits, 52);
    COMP->and_(expBits, 0x7FF);
    COMP->cmp(expBits.r32(), 0x7FF);
    COMP->jne(notNaN);  // Not exp=0x7FF, so not NaN/Inf - safe to negate

    x86::Gp fracMask = newGP64();
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(fracBits, resultBits);
    COMP->and_(fracBits, fracMask);
    COMP->test(fracBits, fracBits);
    COMP->jnz(storeResult);  // It's a NaN from FMA (inf*0 or inf-inf), don't negate

    // It's infinity or normal number - negate it
    COMP->bind(notNaN);
    x86::Gp signMask = newGP64();
    COMP->mov(signMask, 0x8000000000000000ull);
    COMP->xor_(resultBits, signMask);
    COMP->vmovq(frd, resultBits);
  }

  COMP->bind(storeResult);
  COMP->vmovsd(FPRPtr(instr.frd), frd);
  J_classifyAndSetFPRF(b, frd);

  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Negative Multiply-Add Single (x'EC00 003E')
/* HAS ISSUES -> 128 Failed tests */
void PPCInterpreter::PPCInterpreterJIT_fnmaddsx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm fra = newXMM();
  x86::Xmm frb = newXMM();
  x86::Xmm frc = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(fra, FPRPtr(instr.fra));
  COMP->vmovsd(frb, FPRPtr(instr.frb));
  COMP->vmovsd(frc, FPRPtr(instr.frc));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, fra);
  J_checkAndSetSNaN(b, frb);
  J_checkAndSetSNaN(b, frc);

  // Check for SNaN/QNaN in fra (highest priority)
  x86::Gp snanFlagA = newGP32();
  x86::Gp snanQNaNA = newGP64();
  COMP->xor_(snanFlagA, snanFlagA);
  J_checkSNaNAndGetQNaN(b, fra, snanFlagA, snanQNaNA);

  x86::Gp qnanFlagA = newGP32();
  x86::Gp qnanValueA = newGP64();
  COMP->xor_(qnanFlagA, qnanFlagA);
  J_checkQNaNAndGetValue(b, fra, qnanFlagA, qnanValueA);

  // Check for SNaN/QNaN in frb (second priority - operand order a, b, c)
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check for SNaN/QNaN in frc (lowest priority)
  x86::Gp snanFlagC = newGP32();
  x86::Gp snanQNaNC = newGP64();
  COMP->xor_(snanFlagC, snanFlagC);
  J_checkSNaNAndGetQNaN(b, frc, snanFlagC, snanQNaNC);

  x86::Gp qnanFlagC = newGP32();
  x86::Gp qnanValueC = newGP64();
  COMP->xor_(qnanFlagC, qnanFlagC);
  J_checkQNaNAndGetValue(b, frc, qnanFlagC, qnanValueC);

  // Check for infinity inputs
  x86::Gp infFlagA = newGP32();
  COMP->xor_(infFlagA, infFlagA);
  J_checkInfinity(b, fra, infFlagA);

  x86::Gp infFlagB = newGP32();
  COMP->xor_(infFlagB, infFlagB);
  J_checkInfinity(b, frb, infFlagB);

  x86::Gp infFlagC = newGP32();
  COMP->xor_(infFlagC, infFlagC);
  J_checkInfinity(b, frc, infFlagC);

  // Check for denormal inputs
  x86::Gp denormFlag = newGP32();
  COMP->xor_(denormFlag, denormFlag);
  J_checkDenormal(b, fra, denormFlag);
  J_checkDenormal(b, frb, denormFlag);
  J_checkDenormal(b, frc, denormFlag);

  // Check for Inf * 0 invalid operation
  x86::Gp vximzFlag = newGP32();
  J_checkInfMulZero(b, fra, frc, vximzFlag);

  // NaN priority check: fra (any NaN) > frb (any NaN) > frc (any NaN)
  // For each operand, check SNaN first (convert to QNaN), then QNaN
  // This ensures we return the first NaN in operand order a, b, c
  Label checkQNaNA = b->compiler->newLabel();
  Label checkNaNB = b->compiler->newLabel();
  Label checkQNaNB = b->compiler->newLabel();
  Label checkNaNC = b->compiler->newLabel();
  Label checkQNaNC = b->compiler->newLabel();
  Label checkVximz = b->compiler->newLabel();
  Label checkDenorm = b->compiler->newLabel();
  Label computeFMA = b->compiler->newLabel();
  Label doRoundAndNegate = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  // fra SNaN -> QNaN (highest priority)
  COMP->test(snanFlagA, snanFlagA);
  COMP->jz(checkQNaNA);
  COMP->vmovq(frd, snanQNaNA);
  COMP->jmp(storeResult);

  // fra QNaN (still fra priority - check before moving to frb)
  COMP->bind(checkQNaNA);
  COMP->test(qnanFlagA, qnanFlagA);
  COMP->jz(checkNaNB);
  COMP->vmovq(frd, qnanValueA);
  COMP->jmp(storeResult);

  // frb SNaN -> QNaN (second priority)
  COMP->bind(checkNaNB);
  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  // frb QNaN (still frb priority - check before moving to frc)
  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkNaNC);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  // frc SNaN -> QNaN (lowest priority)
  COMP->bind(checkNaNC);
  COMP->test(snanFlagC, snanFlagC);
  COMP->jz(checkQNaNC);
  COMP->vmovq(frd, snanQNaNC);
  COMP->jmp(storeResult);

  // frc QNaN
  COMP->bind(checkQNaNC);
  COMP->test(qnanFlagC, qnanFlagC);
  COMP->jz(checkVximz);
  COMP->vmovq(frd, qnanValueC);
  COMP->jmp(storeResult);

  // VXIMZ (Inf * 0) - checked after all operand NaNs
  COMP->bind(checkVximz);
  COMP->test(vximzFlag, vximzFlag);
  COMP->jz(checkDenorm);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Denormal check
  COMP->bind(checkDenorm);
  COMP->test(denormFlag, denormFlag);
  COMP->jz(computeFMA);

  // Has denormal - check if any operand is infinity
  x86::Gp hasInf = newGP32();
  COMP->mov(hasInf, infFlagA);
  COMP->or_(hasInf, infFlagB);
  COMP->or_(hasInf, infFlagC);
  COMP->test(hasInf, hasInf);
  COMP->jnz(computeFMA);  // Infinity dominates denormal

  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // No NaN operands - compute FMA: frd = (fra * frc) + frb
  COMP->bind(computeFMA);
  COMP->vmovaps(frd, fra);
  COMP->vfmadd213sd(frd, frc, frb);

  // Round to single and negate
  COMP->bind(doRoundAndNegate);
  J_roundToSingle(b, frd);

  // Negate (result is not a propagated NaN here, but could be from inf-inf)
  {
    x86::Gp resultBits = newGP64();
    x86::Gp expBits = newGP64();
    x86::Gp fracBits = newGP64();
    Label notNaN = b->compiler->newLabel();

    COMP->vmovq(resultBits, frd);
    COMP->mov(expBits, resultBits);
    COMP->shr(expBits, 52);
    COMP->and_(expBits, 0x7FF);
    COMP->cmp(expBits.r32(), 0x7FF);
    COMP->jne(notNaN);

    x86::Gp fracMask = newGP64();
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(fracBits, resultBits);
    COMP->and_(fracBits, fracMask);
    COMP->test(fracBits, fracBits);
    COMP->jnz(storeResult);  // It's a NaN from FMA, don't negate

    COMP->bind(notNaN);
    x86::Gp signMask = newGP64();
    COMP->mov(signMask, 0x8000000000000000ull);
    COMP->xor_(resultBits, signMask);
    COMP->vmovq(frd, resultBits);
  }

  COMP->bind(storeResult);
  COMP->vmovsd(FPRPtr(instr.frd), frd);
  J_classifyAndSetFPRF(b, frd);

  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

// Floating Square Root Single (x'EC00 002C')
/* HAS ISSUES -> 5 Failed tests */
void PPCInterpreter::PPCInterpreterJIT_fsqrtsx(sPPEState* ppeState, JITBlockBuilder* b, uPPCInstr instr) {
  // Ensure FPU is enabled
  J_checkFPUEnabled(b);

  x86::Xmm frb = newXMM();
  x86::Xmm frd = newXMM();

  COMP->vmovsd(frb, FPRPtr(instr.frb));

  J_resetFPSCRExceptionBits(b);
  J_checkAndSetSNaN(b, frb);

  // Check for SNaN in frb and get converted QNaN value
  x86::Gp snanFlagB = newGP32();
  x86::Gp snanQNaNB = newGP64();
  COMP->xor_(snanFlagB, snanFlagB);
  J_checkSNaNAndGetQNaN(b, frb, snanFlagB, snanQNaNB);

  // Check for QNaN in frb
  x86::Gp qnanFlagB = newGP32();
  x86::Gp qnanValueB = newGP64();
  COMP->xor_(qnanFlagB, qnanFlagB);
  J_checkQNaNAndGetValue(b, frb, qnanFlagB, qnanValueB);

  // Check if frb is negative (and not -0 or NaN) - VXSQRT exception
  x86::Gp isNegative = newGP32();
  COMP->xor_(isNegative, isNegative);

  {
    x86::Gp bBits = newGP64();
    COMP->vmovq(bBits, frb);

    Label noVxsqrt = b->compiler->newLabel();
    Label setVxsqrt = b->compiler->newLabel();

    COMP->bt(bBits, 63);
    COMP->jnc(noVxsqrt);

    x86::Gp absMask = newGP64();
    x86::Gp absVal = newGP64();
    COMP->mov(absMask, 0x7FFFFFFFFFFFFFFFull);
    COMP->mov(absVal, bBits);
    COMP->and_(absVal, absMask);
    COMP->test(absVal, absVal);
    COMP->jz(noVxsqrt);

    x86::Gp exp = newGP64();
    COMP->mov(exp, bBits);
    COMP->shr(exp, 52);
    COMP->and_(exp, 0x7FF);
    COMP->cmp(exp.r32(), 0x7FF);
    COMP->jne(setVxsqrt);

    x86::Gp frac = newGP64();
    x86::Gp fracMask = newGP64();
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(frac, bBits);
    COMP->and_(frac, fracMask);
    COMP->test(frac, frac);
    COMP->jnz(noVxsqrt);

    COMP->bind(setVxsqrt);
    {
      x86::Gp fpscr = newGP32();
      COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
      COMP->or_(fpscr, (1u << 9) | FPSCR_VX_BIT | FPSCR_FX_BIT);
      COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
      COMP->mov(isNegative, 1);
    }

    COMP->bind(noVxsqrt);
  }

  // Check if input is a denormal (exp=0, frac!=0)
  x86::Gp isDenormal = newGP32();
  COMP->xor_(isDenormal, isDenormal);

  {
    x86::Gp bBits = newGP64();
    COMP->vmovq(bBits, frb);

    Label notDenormal = b->compiler->newLabel();

    x86::Gp exp = newGP64();
    COMP->mov(exp, bBits);
    COMP->shr(exp, 52);
    COMP->and_(exp, 0x7FF);

    COMP->test(exp.r32(), exp.r32());
    COMP->jnz(notDenormal);

    x86::Gp frac = newGP64();
    x86::Gp fracMask = newGP64();
    COMP->mov(fracMask, 0x000FFFFFFFFFFFFFull);
    COMP->mov(frac, bBits);
    COMP->and_(frac, fracMask);
    COMP->test(frac, frac);
    COMP->jz(notDenormal);

    COMP->mov(isDenormal, 1);

    COMP->bind(notDenormal);
  }

  // Save and configure MXCSR - disable DAZ/FTZ for proper denormal handling
  x86::Gp mxcsrOrig = newGP32();
  x86::Mem mxcsrSlot = b->compiler->newStack(4, 4);
  COMP->stmxcsr(mxcsrSlot);
  COMP->mov(mxcsrOrig, mxcsrSlot);

  x86::Gp mxcsrNew = newGP32();
  COMP->mov(mxcsrNew, mxcsrOrig);
  COMP->and_(mxcsrNew, ~(0x3F | (1 << 6) | (1 << 15)));
  COMP->mov(mxcsrSlot, mxcsrNew);
  COMP->ldmxcsr(mxcsrSlot);

  // For denormals: use scaling to bring value into normal range
  // We multiply by 2^1022 in two steps, compute sqrt, then divide by 2^511
  Label skipDenormHandling = b->compiler->newLabel();
  Label afterSqrt = b->compiler->newLabel();

  COMP->test(isDenormal, isDenormal);
  COMP->jz(skipDenormHandling);

  {
    x86::Gp scaleFactor = newGP64();
    x86::Xmm scaleXmm = newXMM();
    x86::Xmm tempXmm = newXMM();

    // Step 1: multiply by 2^512 = 0x5FF0000000000000
    COMP->mov(scaleFactor, 0x5FF0000000000000ull);
    COMP->vmovq(scaleXmm, scaleFactor);
    COMP->vmulsd(tempXmm, frb, scaleXmm);

    // Step 2: multiply by 2^510 = 0x5FD0000000000000
    // Total scale: 2^1022
    COMP->mov(scaleFactor, 0x5FD0000000000000ull);
    COMP->vmovq(scaleXmm, scaleFactor);
    COMP->vmulsd(frd, tempXmm, scaleXmm);

    // Compute sqrt of normalized value
    COMP->vsqrtsd(frd, frd, frd);

    // Divide by 2^511 (sqrt of 2^1022)
    // 2^511 = 0x5FE0000000000000
    COMP->mov(scaleFactor, 0x5FE0000000000000ull);
    COMP->vmovq(scaleXmm, scaleFactor);
    COMP->vdivsd(frd, frd, scaleXmm);

    COMP->jmp(afterSqrt);
  }

  COMP->bind(skipDenormHandling);
  COMP->vsqrtsd(frd, frb, frb);

  COMP->bind(afterSqrt);

  // Restore original MXCSR
  COMP->mov(mxcsrSlot, mxcsrOrig);
  COMP->ldmxcsr(mxcsrSlot);

  // Determine the correct result
  Label checkQNaNB = b->compiler->newLabel();
  Label checkNegative = b->compiler->newLabel();
  Label doRounding = b->compiler->newLabel();
  Label storeResult = b->compiler->newLabel();

  COMP->test(snanFlagB, snanFlagB);
  COMP->jz(checkQNaNB);
  COMP->vmovq(frd, snanQNaNB);
  COMP->jmp(storeResult);

  COMP->bind(checkQNaNB);
  COMP->test(qnanFlagB, qnanFlagB);
  COMP->jz(checkNegative);
  COMP->vmovq(frd, qnanValueB);
  COMP->jmp(storeResult);

  COMP->bind(checkNegative);
  COMP->test(isNegative, isNegative);
  COMP->jz(doRounding);
  {
    x86::Gp defaultQNaN = newGP64();
    COMP->mov(defaultQNaN, PPC_DEFAULT_QNAN);
    COMP->vmovq(frd, defaultQNaN);
  }
  COMP->jmp(storeResult);

  // Round to single precision - BUT skip rounding for denormal results
  // Check if result has exponent 0 (denormal) - if so, don't round
  COMP->bind(doRounding);
  {
    x86::Gp resultBits = newGP64();
    x86::Gp resultExp = newGP64();
    Label skipRounding = b->compiler->newLabel();

    COMP->vmovq(resultBits, frd);
    COMP->mov(resultExp, resultBits);
    COMP->shr(resultExp, 52);
    COMP->and_(resultExp, 0x7FF);

    // If result exponent is 0 (denormal result), skip single-precision rounding
    // This preserves full precision for denormal results like the interpreter does
    COMP->test(resultExp.r32(), resultExp.r32());
    COMP->jz(storeResult);

    // Normal result - round to single precision
    COMP->vcvtsd2ss(frd, frd, frd);
    COMP->vcvtss2sd(frd, frd, frd);
  }

  COMP->bind(storeResult);

  COMP->vmovsd(FPRPtr(instr.frd), frd);
  J_classifyAndSetFPRF(b, frd);

  if (instr.rc) {
    J_ppuSetCR1(b);
  }
}

#endif
