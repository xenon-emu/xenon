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
inline void J_checkFPUEnabled(JITBlockBuilder *b) {
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
inline void J_resetFPSCRExceptionBits(JITBlockBuilder *b) {
  x86::Gp fpscr = newGP32();
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->and_(fpscr, ~(FPSCR_FX_BIT | FPSCR_VX_BIT | FPSCR_VX_ALL_BITS));
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
}

// Helper to check if a value is an SNaN and set FPSCR exception bits if so
inline void J_checkAndSetSNaN(JITBlockBuilder *b, x86::Xmm value) {
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
inline void J_checkSNaNAndGetQNaN(JITBlockBuilder *b, x86::Xmm value, x86::Gp snanFlag, x86::Gp snanQNaN) {
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
inline void J_checkQNaNAndGetValue(JITBlockBuilder *b, x86::Xmm value, x86::Gp qnanFlag, x86::Gp qnanValue) {
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
inline void J_checkInfinity(JITBlockBuilder *b, x86::Xmm value, x86::Gp infFlag) {
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
inline void J_checkDenormal(JITBlockBuilder *b, x86::Xmm value, x86::Gp denormFlag) {
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
inline void J_checkInfSubInf(JITBlockBuilder *b, x86::Xmm fra, x86::Xmm frb, x86::Gp vxisiFlag) {
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
inline void J_checkInfMinusInf(JITBlockBuilder *b, x86::Xmm fra, x86::Xmm frb, x86::Gp vxisiFlag) {
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
inline void J_checkInfMulZero(JITBlockBuilder *b, x86::Xmm fra, x86::Xmm frc, x86::Gp vximzFlag) {
  x86::Gp aBits = newGP64();
  x86::Gp cBits = newGP64();
  x86::Gp aExp = newGP64();
  x86::Gp cExp = newGP64();
  x86::Gp aFrac = newGP64();
  x86::Gp cFrac = newGP64();
  x86::Gp fpscr = newGP32();

  Label notInfMulZero = b->compiler->newLabel();
  Label checkCZero = b->compiler->newLabel();

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
  COMP->jne(checkCZero);

  COMP->mov(aFrac, aBits);
  COMP->and_(aFrac, fracMask);
  COMP->test(aFrac, aFrac);
  COMP->jnz(checkCZero);

  // A is Inf, check if C is zero
  COMP->mov(cExp, cBits);
  COMP->shr(cExp, 52);
  COMP->and_(cExp, 0x7FF);
  COMP->test(cExp.r32(), cExp.r32());
  COMP->jnz(checkCZero);

  COMP->mov(cFrac, cBits);
  COMP->and_(cFrac, fracMask);
  COMP->test(cFrac, cFrac);
  COMP->jnz(checkCZero);

  // A is Inf and C is zero - invalid
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXIMZ_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  COMP->mov(vximzFlag, 1);
  COMP->jmp(notInfMulZero);

  // Check if C is Inf and A is zero
  COMP->bind(checkCZero);
  COMP->mov(cExp, cBits);
  COMP->shr(cExp, 52);
  COMP->and_(cExp, 0x7FF);
  COMP->cmp(cExp.r32(), 0x7FF);
  COMP->jne(notInfMulZero);

  COMP->mov(cFrac, cBits);
  COMP->and_(cFrac, fracMask);
  COMP->test(cFrac, cFrac);
  COMP->jnz(notInfMulZero);

  // C is Inf, check if A is zero
  COMP->mov(aExp, aBits);
  COMP->shr(aExp, 52);
  COMP->and_(aExp, 0x7FF);
  COMP->test(aExp.r32(), aExp.r32());
  COMP->jnz(notInfMulZero);

  COMP->mov(aFrac, aBits);
  COMP->and_(aFrac, fracMask);
  COMP->test(cFrac, cFrac);
  COMP->jnz(notInfMulZero);

  // C is Inf and A is zero - invalid
  COMP->mov(fpscr, FPSCRPtr().Ptr<u32>());
  COMP->or_(fpscr, FPSCR_VXIMZ_BIT | FPSCR_VX_BIT | FPSCR_FX_BIT);
  COMP->mov(FPSCRPtr().Ptr<u32>(), fpscr);
  COMP->mov(vximzFlag, 1);

  COMP->bind(notInfMulZero);
}

// Helper to set CR1 based on FPSCR (FX, FEX, VX, OX bits)
inline void J_ppuSetCR1(JITBlockBuilder *b) {
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
inline void J_classifyAndSetFPRF(JITBlockBuilder *b, x86::Xmm result) {
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
inline void J_roundToSingle(JITBlockBuilder *b, x86::Xmm frd) {
  COMP->vcvtsd2ss(frd, frd, frd);
  COMP->vcvtss2sd(frd, frd, frd);
}

// Floating Add (Double-Precision) (x'FC00 002A')
// frD <- (frA) + (frB)
void PPCInterpreter::PPCInterpreterJIT_faddx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
void PPCInterpreter::PPCInterpreterJIT_faddsx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
void PPCInterpreter::PPCInterpreterJIT_fsubx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
void PPCInterpreter::PPCInterpreterJIT_fsubsx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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

// Floating Negative Multiply-Add (Double-Precision) (x'FC00 003E')
// frD <- -((frA * frC) + frB)
// NaN priority: fra > frb > frc (operands checked in order a, b, c)
// For each operand, ANY NaN (SNaN or QNaN) takes priority before checking the next operand
void PPCInterpreter::PPCInterpreterJIT_fnmaddx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
// frD <- -((frA * frC) + frB) [single precision]
// NaN priority: fra > frb > frc (operands checked in order a, b, c)
void PPCInterpreter::PPCInterpreterJIT_fnmaddsx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
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
#endif
