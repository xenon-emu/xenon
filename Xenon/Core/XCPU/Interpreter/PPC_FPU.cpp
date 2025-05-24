// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Arch.h"

#include "Base/Types.h"
#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

//
// Utilities & constants
//

// Excpetion/Bit masks for FPSCR. See PPC Programming Environments Manual, Page 112, Table 3-9.
enum eFPSCRExceptionBits : u32 {
  FPSCR_BIT_FX = 1U << (31 - 0), // Floating-point exception summary.
  FPSCR_BIT_FEX = 1U << (31 - 1), // Floating-point enabled exception summary.
  FPSCR_BIT_VX = 1U << (31 - 2), // Floating-point invalid operation exception summary.
  FPSCR_BIT_OX = 1U << (31 - 3), // Floating-point overflow exception.
  FPSCR_BIT_UX = 1U << (31 - 4), // Floating-point underflow exception.
  FPSCR_BIT_ZX = 1U << (31 - 5), // Floating-point zero divide exception.
  FPSCR_BIT_XX = 1U << (31 - 6), // Floating-point inexact exception.
  FPSCR_BIT_VXSNAN = 1U << (31 - 7), // Floating-point invalid operation exception for SNaN.
  FPSCR_BIT_VXISI = 1U << (31 - 8), // Floating-point invalid operation exception for Infinity – Infinity.
  FPSCR_BIT_VXIDI = 1U << (31 - 9), // Floating-point invalid operation exception for Infinity / Infinity.
  FPSCR_BIT_VXZDZ = 1U << (31 - 10), // Floating-point invalid operation exception for Zero / Zero.
  FPSCR_BIT_VXIMZ = 1U << (31 - 11), // Floating-point invalid operation exception for Infinity * Zero.
  FPSCR_BIT_VXVC = 1U << (31 - 12), // Floating-point invalid operation exception for invalid compare.
  FPSCR_BIT_VXSOFT = 1U << (31 - 21), // Floating-point invalid operation exception for software request. 
  FPSCR_BIT_VXSQRT = 1U << (31 - 22), // Floating-point invalid operation exception for invalid square root.
  FPSCR_BIT_VXCVI = 1U << (31 - 23), // Floating-point invalid operation exception for invalid integer convert.
  FPSCR_BIT_VE = 1U << (31 - 24), // Floating-point invalid operation exception enable.
  FPSCR_BIT_OE = 1U << (31 - 25), // IEEE floating-point overflow exception enable. 
  FPSCR_BIT_UE = 1U << (31 - 26), // IEEE floating-point underflow exception enable. 
  FPSCR_BIT_ZE = 1U << (31 - 27), // IEEE floating-point zero divide exception enable.
  FPSCR_BIT_XE = 1U << (31 - 28), // Floating-point inexact exception enable.

  // VX Enabled Exceptions.
  FPSCR_VX_ANY = FPSCR_BIT_VXSNAN | FPSCR_BIT_VXISI | FPSCR_BIT_VXIDI | FPSCR_BIT_VXZDZ | FPSCR_BIT_VXIMZ | FPSCR_BIT_VXVC |
  // Software Generated Exceptions.
  FPSCR_BIT_VXSOFT | FPSCR_BIT_VXSQRT | FPSCR_BIT_VXCVI,

  FPSCR_ANY_X = FPSCR_BIT_OX | FPSCR_BIT_UX | FPSCR_BIT_ZX | FPSCR_BIT_XX | FPSCR_VX_ANY,

  FPSCR_ANY_E = FPSCR_BIT_VE | FPSCR_BIT_OE | FPSCR_BIT_UE | FPSCR_BIT_ZE | FPSCR_BIT_XE,
};

// Expresions for Siganling bits on NAN's.
#define FP_DOUBLE_ZERO  0x0000000000000000ULL // Self explanatory.
#define FP_DOUBLE_QBIT  0x0008000000000000ULL // Floating point Double Quiet bit.
#define FP_DOUBLE_EXP   0x7FF0000000000000ULL // Floating point Double Exponent.
#define FP_DOUBLE_FRAC  0x000FFFFFFFFFFFFFULL // Floating point Double Fraction.
#define FP_DOUBLE_SIGN  0x8000000000000000ULL // Floating point Double Sign.
#define FP_DOUBLE_FRAC_WIDTH  52              // Floating point Double Fraction Width.

#define FP_FLOAT_EXP    0x7F800000 // Floating point Single Exponent.
#define FP_FLOAT_SIGN   0x80000000 // Floating point Single Sign.
#define FP_FLOAT_FRAC   0x007FFFFF // Floating point Single Fraction.
#define FP_FLOAT_ZERO   0x00000000 // Self explanatory.

// Floating-Point Result Flags, used to classify between result types.
// See PPC Assembly Programming Environments, v2.3, page 114, table 3-10.
enum eFPResultValueClass {
  FP_CLASS_QNAN = 0x11, // Quiet NaN
  FP_CLASS_NINF = 0x9,  // -Infinity
  FP_CLASS_NN = 0x8,    // -Normalized Number
  FP_CLASS_ND = 0x18,   // -Denormalized Number
  FP_CLASS_NZ = 0x12,   // -Zero
  FP_CLASS_PZ = 0x2,    // +Zero
  FP_CLASS_PD = 0x14,   // +Denormalized Number
  FP_CLASS_PN = 0x4,    // +Normalized Number
  FP_CLASS_PINF = 0x5,  // +Infinity
};

enum class eFPCCBits {
  FL = 8,  // <
  FG = 4,  // >
  FE = 2,  // =
  FU = 1,  // ?
};

// Updates CR1 field based on the contents of FPSCR's FX, FEX, VX and OX bits.
void PPCInterpreter::ppuSetCR1(PPU_STATE* ppuState) {
  u8 crValue = (curThread.FPSCR.FX << 3) | (curThread.FPSCR.FEX << 2) | (curThread.FPSCR.VX << 1) | curThread.FPSCR.OX;
  curThread.CR.CR1 = crValue;
}

// Checks if exceptions regarding FPU are to be generated.
inline void FPCheckExceptions(PPU_STATE* ppuState) {
  if (curThread.FPSCR.FEX && (curThread.SPR.MSR.FE0 || curThread.SPR.MSR.FE1)) {
    // Floating program exceptions are enabled and an exception is pending in 
    // Floating Point Enabled Exception Summary bit of FPSCR.
    curThread.exceptReg |= PPU_EX_PROG;
    curThread.progExceptionType = PROGRAM_EXCEPTION_TYPE_FPU;
  }
}

// Sets the FEX bit in FPSCR if any of the pending exception bits in it are to be raised.
inline void FPUpdateExceptionSummaryBit(PPU_STATE* ppuState) {
  curThread.FPSCR.VX = (curThread.FPSCR.FPSCR_Hex & FPSCR_VX_ANY) != 0;
  curThread.FPSCR.FEX = ((curThread.FPSCR.FPSCR_Hex >> 22) & (curThread.FPSCR.FPSCR_Hex & FPSCR_ANY_E)) != 0;

  FPCheckExceptions(ppuState);
}

// Sets the FX bit in FPSCR and causes said exception if allowed following logic from docs.
void FPSetException(PPU_STATE* ppuState, u32 exceptionMask) {
  // Check for the same exception already being present.
  if ((curThread.FPSCR.FPSCR_Hex & exceptionMask) != exceptionMask) {
    // Set exception summary.
    curThread.FPSCR.FX = 1;
  }

  // Set the exception bit in FPSCR.
  curThread.FPSCR.FPSCR_Hex |= exceptionMask;

  // Update FP Exception Summary bit.
  FPUpdateExceptionSummaryBit(ppuState);
}

// Checks for Signaling NaN's.
inline bool IsSignalingNAN(double inValue) {
  // Do a bit cast to u64 to check for bits.
  const u64 integer = std::bit_cast<u64>(inValue);
  return ((integer & FP_DOUBLE_EXP) == FP_DOUBLE_EXP)
    && ((integer & FP_DOUBLE_FRAC) != FP_DOUBLE_ZERO)
    && ((integer & FP_DOUBLE_QBIT) == FP_DOUBLE_ZERO);
}

inline double MakeQuiet(double d) {
  const u64 integral = std::bit_cast<u64>(d) | FP_DOUBLE_QBIT;

  return std::bit_cast<double>(integral);
}

// Checks for FP exponent, if it is zero then it turns it into signed zero.
inline float FPFlushToZero(float f) {
  u32 i = std::bit_cast<u32>(f);
  // Check for exponent and turn into signed zero.
  return std::bit_cast<float>(((i & FP_FLOAT_EXP) == 0) ? i &= FP_FLOAT_SIGN : i);
}

inline double FPFlushToZero(double d) {
  u64 i = std::bit_cast<u64>(d);
  return std::bit_cast<double>(((i & FP_DOUBLE_EXP) == 0) ? i &= FP_DOUBLE_SIGN : i);
}

// Classifies the input value based on operand type and returns the value in PPC
// FP Operand Class type.
u32 ClassifyDouble(double dvalue) {
  const u64 inValue = std::bit_cast<u64>(dvalue);
  const u64 sign = inValue & FP_DOUBLE_SIGN;
  const u64 exp = inValue & FP_DOUBLE_EXP;

  // Normalized number.
  if (exp > FP_DOUBLE_ZERO && exp < FP_DOUBLE_EXP) { return sign ? FP_CLASS_NN : FP_CLASS_PN; }

  const u64 mantissa = inValue & FP_DOUBLE_FRAC;
  if (mantissa) {
    if (exp) { return FP_CLASS_QNAN; }

    // Denormalized number.
    return sign ? FP_CLASS_ND : FP_CLASS_PD;
  }

  // Infinite.
  if (exp) { return sign ? FP_CLASS_NINF : FP_CLASS_PINF; }

  // Zero.
  return sign ? FP_CLASS_NZ : FP_CLASS_PZ;
}

u32 ClassifyFloat(float fvalue) {
  const u32 inValue = std::bit_cast<u32>(fvalue);
  const u32 sign = inValue & FP_FLOAT_SIGN;
  const u32 exp = inValue & FP_FLOAT_EXP;

  // Normalized number.
  if (exp > FP_FLOAT_ZERO && exp < FP_FLOAT_EXP) { return sign ? FP_CLASS_NN : FP_CLASS_PN; }

  const u32 mantissa = inValue & FP_FLOAT_FRAC;
  if (mantissa) {
    if (exp) { return FP_CLASS_QNAN; } // Quiet NAN

    // Denormalized number.
    return sign ? FP_CLASS_ND : FP_CLASS_PD;
  }

  // Infinite.
  if (exp) { return sign ? FP_CLASS_NINF : FP_CLASS_PINF; }

  // Zero.
  return sign ? FP_CLASS_NZ : FP_CLASS_PZ;
}

void SetFI(PPU_STATE* ppuState, u32 FI){
  if (FI != 0) { FPSetException(ppuState, FPSCR_BIT_XX); }
  curThread.FPSCR.FI = FI;
}

inline float FPForceSingle(PPU_STATE* ppuState, double value) {
  if (curThread.FPSCR.NI) {
    // Emulate a rounding quirk. If the conversion result before rounding is a subnormal single,
    // it's always flushed to zero, even if rounding would have caused it to become normal.

    constexpr u64 smallest_normal_single = 0x3810000000000000;
    const u64 value_without_sign =
      std::bit_cast<u64>(value) & (FP_DOUBLE_EXP | FP_DOUBLE_FRAC);

    if (value_without_sign < smallest_normal_single) {
      const u64 flushed_double = std::bit_cast<u64>(value) & FP_DOUBLE_SIGN;
      const u32 flushed_single = static_cast<u32>(flushed_double >> 32);
      return std::bit_cast<float>(flushed_single);
    }
  }

  // Emulate standard conversion to single precision.

  float x = static_cast<float>(value);
  if (curThread.FPSCR.NI) { x = FPFlushToZero(x); }
  return x;
}

inline double FPForceDouble(PPU_STATE* ppuState, double inValue) {
  if (curThread.FPSCR.NI) { inValue = FPFlushToZero(inValue); }
  return inValue;
}

inline double Force25Bit(double d)
{
  u64 integral = std::bit_cast<u64>(d);

  u64 exponent = integral & FP_DOUBLE_EXP;
  u64 fraction = integral & FP_DOUBLE_FRAC;

  if (exponent == 0 && fraction != 0) {
    // Subnormals get "normalized" before they're rounded
    // In the end, this practically just means that the rounding is
    // at a different bit

    s64 keep_mask = 0xFFFFFFFFF8000000LL;
    u64 round = 0x8000000;

    // Shift the mask and rounding bit to the right until
    // the fraction is "normal"
    // That is to say shifting it until the MSB of the fraction
    // would escape into the exponent
    u32 shift = std::countl_zero(fraction) - (63 - FP_DOUBLE_FRAC_WIDTH);
    keep_mask >>= shift;
    round >>= shift;

    // Round using these shifted values
    integral = (integral & keep_mask) + (integral & round);
  }
  else { integral = (integral & 0xFFFFFFFFF8000000ULL) + (integral & 0x8000000); }

  return std::bit_cast<double>(integral);
}

void PPCInterpreter::FPCompareOrdered(PPU_STATE * ppuState, double fra, double frb) {
  eFPCCBits compareResult;

  if (std::isnan(fra) || std::isnan(frb)) {
    compareResult = eFPCCBits::FU;
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb)) {
      FPSetException(ppuState, FPSCR_BIT_VXSNAN);
      if (curThread.FPSCR.VE == 0) { FPSetException(ppuState, FPSCR_BIT_VXVC); }
    }
    else { FPSetException(ppuState, FPSCR_BIT_VXVC); }
  }
  else if (fra < frb) { compareResult = eFPCCBits::FL; }
  else if (fra > frb) { compareResult = eFPCCBits::FG; }
  else { compareResult = eFPCCBits::FE; }

  const u32 compareValue = static_cast<u32>(compareResult);

  // Clear and set the FPCC bits accordingly.
  curThread.FPSCR.FPRF = (curThread.FPSCR.FPRF & ~(0xF << 12)) | compareValue;

  ppcUpdateCR(ppuState, _instr.crfd, compareValue);
}

void PPCInterpreter::FPCompareUnordered(PPU_STATE* ppuState, double fra, double frb) {
  eFPCCBits compareResult;

  if (std::isnan(fra) || std::isnan(frb)) {
    compareResult = eFPCCBits::FU;

    if (IsSignalingNAN(fra) || IsSignalingNAN(frb)) { FPSetException(ppuState, FPSCR_BIT_VXSNAN); }
  }
  else if (fra < frb) { compareResult = eFPCCBits::FL; }
  else if (fra > frb) { compareResult = eFPCCBits::FG; }
  else { compareResult = eFPCCBits::FE; }

  const u32 compareValue = static_cast<u32>(compareResult);

  // Clear and set the FPCC bits accordingly.
  curThread.FPSCR.FPRF = (curThread.FPSCR.FPRF & ~(0xF << 12)) | compareValue;

  ppcUpdateCR(ppuState, _instr.crfd, compareValue);
}

// Round a number to an integer in the same direction as the CPU rounding mode,
// without setting any CPU flags or being careful about NaNs
double RoundToIntegerMode(double number)
{
  // This value is 2^52 -- The first number in which double precision floating point
  // numbers can only store subsequent integers, and no longer any decimals
  // This keeps the sign of the unrounded value because it needs to scale it
  // upwards when added
  const double int_precision = std::copysign(4503599627370496.0, number);

  // By adding this value to the original number,
  // it will be forced to decide a integer to round to
  // This rounding will be the same as the CPU rounding mode
  return (number + int_precision) - int_precision;
}

void PPCInterpreter::ConvertToInteger(PPU_STATE* ppuState, eFPRoundMode roundingMode) {
  const double b = FPRi(frb).asDouble();
  double rounded;
  u32 value;
  bool exceptionOccurred = false;

  // To reduce complexity, this takes in a rounding mode in a switch case,
  // rather than always judging based on the emulated CPU rounding mode
  switch (roundingMode)
  {
  case eFPRoundMode::roundModeNearest:
    // On generic platforms, the rounding should be assumed to be ties to even
    // For targeted platforms this would work for any rounding mode,
    // but it's mainly just kept in to replace roundeven,
    // due to its lack in the C++17 (and possible lack for future versions)
    rounded = RoundToIntegerMode(b);
    break;
  case eFPRoundMode::roundModeTowardZero:
    rounded = std::trunc(b);
    break;
  case eFPRoundMode::roundModePlusInfinity:
    rounded = std::ceil(b);
    break;
  case eFPRoundMode::roundModeNegativeInfinity:
    rounded = std::floor(b);
    break;
  default:
    std::unreachable();
  }

  if (std::isnan(b)) {
    if (IsSignalingNAN(b)) { FPSetException(ppuState, FPSCR_BIT_VXSNAN); }

    value = 0x80000000;
    FPSetException(ppuState, FPSCR_BIT_VXCVI);
    exceptionOccurred = true;
  } else if (rounded >= static_cast<double>(0x80000000)) {
    // Positive large operand or +inf
    value = 0x7fffffff;
    FPSetException(ppuState, FPSCR_BIT_VXCVI);
    exceptionOccurred = true;
  } else if (rounded < -static_cast<double>(0x80000000)) {
    // Negative large operand or -inf
    value = 0x80000000;
    FPSetException(ppuState, FPSCR_BIT_VXCVI);
    exceptionOccurred = true;
  } else {
    s32 signed_value = static_cast<s32>(rounded);
    value = static_cast<u32>(signed_value);
    const double di = static_cast<double>(signed_value);
    if (di == b) { curThread.FPSCR.clearFIFR(); }
    else { SetFI(ppuState, 1); curThread.FPSCR.FR = fabs(di) > fabs(b); }
  }

  if (exceptionOccurred) { curThread.FPSCR.clearFIFR(); }

  if (!exceptionOccurred || curThread.FPSCR.VE == 0)
  {
    // Based on HW tests
    // FPRF is not affected
    u64 result = 0xfff8000000000000ull | value;
    if (value == 0 && std::signbit(b))
      result |= 0x100000000ull;

    FPRi(frd).setValue(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Represents a FP Operation Result, wich may set exeptions and/or check for them.
struct FPResult
{
  bool HasNoInvalidExceptions() const { return (exception & FPSCR_VX_ANY) == 0; }

  void SetException(PPU_STATE *ppuState, eFPSCRExceptionBits exceptionBits) {
    exception = exceptionBits;
    FPSetException(ppuState, exceptionBits);
  }

  // Resulting Value.
  double value = 0.0;
  eFPSCRExceptionBits exception{};
};

// Floating point addition, with exception recording.
inline FPResult FPAdd(PPU_STATE* ppuState, double fra, double frb) {
  // Calculate Result.
  FPResult result{ fra + frb };

  // Check for a NaN result.
  if (std::isnan(result.value))
  {
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb)) { result.SetException(ppuState, FPSCR_BIT_VXSNAN); }
    // Clear FIFR bits as per docs.
    curThread.FPSCR.clearFIFR();

    if (std::isnan(fra)) {
      result.value = MakeQuiet(fra);
      return result;
    }
    if (std::isnan(frb)) {
      result.value = MakeQuiet(frb);
      return result;
    }

    result.SetException(ppuState, FPSCR_BIT_VXISI);
    result.value = std::numeric_limits<double>::quiet_NaN();
    return result;
  }

  if (std::isinf(fra) || std::isinf(frb)) { curThread.FPSCR.clearFIFR(); }

  return result;
}

inline FPResult FPSub(PPU_STATE* ppuState, double fra, double frb)
{
  FPResult result{ fra - frb };

  if (std::isnan(result.value))
  {
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb)) { result.SetException(ppuState, FPSCR_BIT_VXSNAN); }

    curThread.FPSCR.clearFIFR();

    if (std::isnan(fra)) {
      result.value = MakeQuiet(fra);
      return result;
    }
    if (std::isnan(frb)) {
      result.value = MakeQuiet(frb);
      return result;
    }

    result.SetException(ppuState, FPSCR_BIT_VXISI);
    result.value = std::numeric_limits<double>::quiet_NaN();
    return result;
  }

  if (std::isinf(fra) || std::isinf(frb)) { curThread.FPSCR.clearFIFR(); }

  return result;
}


inline FPResult FPMul(PPU_STATE* ppuState, double fra, double frb)
{
  FPResult result{ fra * frb };

  if (std::isnan(result.value)) {
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb)) { result.SetException(ppuState, FPSCR_BIT_VXSNAN); }

    curThread.FPSCR.clearFIFR();

    if (std::isnan(fra)) {
      result.value = MakeQuiet(fra);
      return result;
    }
    if (std::isnan(frb)) {
      result.value = MakeQuiet(frb);
      return result;
    }

    result.value = std::numeric_limits<double>::quiet_NaN();
    result.SetException(ppuState, FPSCR_BIT_VXIMZ);
    return result;
  }

  return result;
}

inline FPResult FPDiv(PPU_STATE* ppuState, double fra, double frb) {
  FPResult result{ fra / frb };

  if (std::isinf(result.value)) {
    if (frb == 0.0) {
      result.SetException(ppuState, FPSCR_BIT_ZX);
      return result;
    }
  }
  else if (std::isnan(result.value)) {
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb)) { result.SetException(ppuState, FPSCR_BIT_VXSNAN); }

    curThread.FPSCR.clearFIFR();

    if (std::isnan(fra)) { 
      result.value = MakeQuiet(fra);
      return result;
    }
    if (std::isnan(frb)) {
      result.value = MakeQuiet(frb);
      return result;
    }

    if (frb == 0.0) { result.SetException(ppuState, FPSCR_BIT_VXZDZ); }
    else if (std::isinf(fra) && std::isinf(frb)) { result.SetException(ppuState, FPSCR_BIT_VXIDI); }

    result.value = std::numeric_limits<double>::quiet_NaN();
    return result;
  }
  return result;
}

// FMA instructions on PowerPC are weird:
// They calculate (a * c) + b, but the order in which
// inputs are checked for NaN is still a, b, c.
inline FPResult FPMadd(PPU_STATE* ppuState, double fra, double frc, double frb)
{
  FPResult result{ std::fma(fra, frc, frb) };

  if (std::isnan(result.value))
  {
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb) || IsSignalingNAN(frc)) { result.SetException(ppuState, FPSCR_BIT_VXSNAN); }

    curThread.FPSCR.clearFIFR();

    if (std::isnan(fra)) {
      result.value = MakeQuiet(fra);
      return result;
    }
    if (std::isnan(frb)) {
      result.value = MakeQuiet(frb);
      return result;
    }
    if (std::isnan(frc)) {
      result.value = MakeQuiet(frc);
      return result;
    }

    result.SetException(ppuState, std::isnan(fra * frc) ? FPSCR_BIT_VXIMZ : FPSCR_BIT_VXISI);
    result.value = std::numeric_limits<double>::quiet_NaN();
    return result;
  }

  if (std::isinf(fra) || std::isinf(frb) || std::isinf(frc)) { curThread.FPSCR.clearFIFR(); }

  return result;
}

inline FPResult FPMsub(PPU_STATE* ppuState, double fra, double frc, double frb)
{
  FPResult result{ std::fma(fra, frc, -frb) };

  if (std::isnan(result.value))
  {
    if (IsSignalingNAN(fra) || IsSignalingNAN(frb) || IsSignalingNAN(frc)) { result.SetException(ppuState, FPSCR_BIT_VXSNAN); }

    curThread.FPSCR.clearFIFR();

    if (std::isnan(fra)) {
      result.value = MakeQuiet(fra);
      return result;
    }
    if (std::isnan(frb)) {
      result.value = MakeQuiet(frb);
      return result;
    }
    if (std::isnan(frc)) {
      result.value = MakeQuiet(frc);
      return result;
    }

    result.SetException(ppuState, std::isnan(fra * frc) ? FPSCR_BIT_VXIMZ : FPSCR_BIT_VXISI);
    result.value = std::numeric_limits<double>::quiet_NaN();
    return result;
  }

  if (std::isinf(fra) || std::isinf(frb) || std::isinf(frc)) { curThread.FPSCR.clearFIFR(); }

  return result;
}


// Updates needed fields from FPSCR and CR1 bits if requested
void PPCInterpreter::ppuUpdateFPSCR(PPU_STATE *ppuState, f64 op0, f64 op1, bool updateCR, u8 CR) {
  // TODO(bitsh1ft3r): Detect NaN's

  static_assert(std::endian::native == std::endian::little, "ppcUpdateFPSCR not implemented for Big-Endian arch.");

  const auto cmpResult = op0 <=> op1;
  u32 crValue = 0;
  if (cmpResult == std::partial_ordering::less) {
    curThread.FPSCR.FL = 1;
    crValue |= 0b1000;
  }
  if (cmpResult == std::partial_ordering::greater) {
    curThread.FPSCR.FG = 1;
    crValue |= 0b0100;
  }
  if (cmpResult == std::partial_ordering::equivalent) {
    curThread.FPSCR.FE = 1;
    crValue |= 0b0010;
  }
  if (cmpResult == std::partial_ordering::unordered) {
    curThread.FPSCR.FU = 1;
    crValue |= 0b0001;
  }

  // Update CRx bits if requested.
  if (updateCR) {
    switch (CR) {
    CR_CASE(0)
    CR_CASE(1)
    CR_CASE(2)
    CR_CASE(3)
    CR_CASE(4)
    CR_CASE(5)
    CR_CASE(6)
    CR_CASE(7)
    }
  }
}

// Move to Condition Register from FPSCR (x'FC00 0080')
void PPCInterpreter::PPCInterpreter_mcrfs(PPU_STATE *ppuState) {
  /*
  * CR4 * BF + 32:4 * BF + 35 <- CR4 * crS + 32:4 * crS + 35
  * The contents of field crS (bits 4 * crS + 32-4 * crS + 35) of CR are copied to field
  * crD (bits 4 * crD + 32-4 * crD + 35) of CR.
  */
  CHECK_FPU;

  const u32 shift = 4 * (7 - _instr.crfs);
  const u32 fpFlags = (curThread.FPSCR.FPSCR_Hex >> shift) & 0xF;
  // If any exception bits were read, we clear them as per docs.
  curThread.FPSCR.FPSCR_Hex &= ~((0xF << shift) & (FPSCR_BIT_FX | FPSCR_ANY_X));

  ppcUpdateCR(ppuState, _instr.crfd, fpFlags);
}

// Floating Add (Double-Precision) (x'FC00 002A')
void PPCInterpreter::PPCInterpreter_faddx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) + (frB)
  */

  CHECK_FPU;

  const double fra = FPRi(fra).asDouble();
  const double frb = FPRi(frb).asDouble();

  const FPResult sum = FPAdd(ppuState, fra, frb);

  if (curThread.FPSCR.VE == 0 || sum.HasNoInvalidExceptions()) {
    // Flush to zero if enabled in FPSCR.
    const double result = FPForceDouble(ppuState, sum.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) ppuSetCR1(ppuState);
}

// Floating Absolute Value (x'FC00 0210')
void PPCInterpreter::PPCInterpreter_fabsx(PPU_STATE *ppuState) {
  /*
  frD <- abs(frB)
  */

  CHECK_FPU;
  
  const f64 frB = FPRi(frb).asDouble();
  FPRi(frd).setValue(std::fabs(frB));

  if (_instr.rc)
    ppuSetCR1(ppuState);
}

// Floating Add Single (x'EC00 002A')
void PPCInterpreter::PPCInterpreter_faddsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) + (frB)
  */

  CHECK_FPU;

  const double fra = FPRi(fra).asDouble();
  const double frb = FPRi(frb).asDouble();

  const FPResult sum = FPAdd(ppuState, fra, frb);

  if (curThread.FPSCR.VE == 0 || sum.HasNoInvalidExceptions()) {
    // Flush to zero if enabled in FPSCR.
    const float result = FPForceSingle(ppuState, sum.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) ppuSetCR1(ppuState);
}

// Floating Compare Unordered (x'FC00 0000')
void PPCInterpreter::PPCInterpreter_fcmpu(PPU_STATE *ppuState) {
  /*
  if (frA) is a NaN or
     (frB) is a NaN then      c <- 0b0001
  else if (frA) < (frB) then  c <- 0b1000
  else if (frA) > (frB) then  c <- 0b0100
  else                        c <- 0b0010

  FPCC <- c
  CR[4 * crfD-4 * crfD + 3] <- c
  if (frA) is an SNaN or (frB) is an SNaN then
    VXSNAN <- 1
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();

  FPCompareUnordered(ppuState, fra, frb);
}

// Floating Compare Ordered (x'FC00 0040')
void PPCInterpreter::PPCInterpreter_fcmpo(PPU_STATE* ppuState) {
  /*
  if (frA) is a NaN or
     (frB) is a NaN then c <- 0b0001
  else if (frA) < (frB) then c <- 0b1000
  else if (frA) > (frB) then c <- 0b0100
  else c <- 0b0010

  FPCC <- c
  CR[4 * crfD–4 * crfD + 3] <- c 

  if (frA) is an SNaN or
     (frB) is an SNaN then
       VXSNAN <- 1
       if VE = 0 then VXVC <- 1
  else if (frA) is a QNaN or
       (frB) is a QNaN then VXVC <- 1
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();

  FPCompareOrdered(ppuState, fra, frb);
}

// Floating Convert to Integer Word (x'FC00 001C')
void PPCInterpreter::PPCInterpreter_fctiwx(PPU_STATE* ppuState) {
  CHECK_FPU;

  ConvertToInteger(ppuState, static_cast<eFPRoundMode>(curThread.FPSCR.RN.value()));
}

// Floating Convert to Integer Double Word(x'FC00 065C')
void PPCInterpreter::PPCInterpreter_fctidx(PPU_STATE* ppuState) {
  // TODO(bitsh1ft3r): Respect rounding modes!
  // Test correct behavior.

  CHECK_FPU;

#if defined(ARCH_X86_64)
  const auto val = _mm_set_sd(FPRi(frb).asDouble());
  const auto res = _mm_xor_si128(_mm_set1_epi64x(_mm_cvtsd_si64(val)), _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(f64(1ull << 63)))));
  FPRi(frd).setValue(std::bit_cast<f64>(_mm_cvtsi128_si64(res)));
#elif defined(ARCH_X86)
  const f64 val = FPRi(frb).asDouble();
  const s64 intVal = static_cast<s64>(val);
  const f64 threshold = static_cast<f64>(1ull << 63);
  const bool flip = val >= threshold;

  s64 flippedVal = flip ? intVal ^ (1ull << 63) : intVal;
  FPRi(frd).setValue(std::bit_cast<f64>(flippedVal));
#elif defined(ARCH_AARCH64)
  #if defined(__ARM_FEATURE_FP64) && __ARM_FEATURE_FP64
    const float64x2_t val = vsetq_lane_f64(FPRi(frb).asDouble(), vdupq_n_f64(0), 0);
    const int64x2_t intVal = vcvtq_s64_f64(val);
    const float64x2_t threshold = vdupq_n_f64(static_cast<f64>(1ULL << 63));
    const uint64x2_t cmpMask = vcgeq_f64(val, threshold);
    const uint64x2_t xorResult = veorq_u64(vreinterpretq_u64_s64(intVal), cmpMask);
    u64 result = vgetq_lane_u64(xorResult, 0);
  #else
    const float64x2_t val = vsetq_lane_f64(FPRi(frb).asDouble(), vdupq_n_f64(0), 0);
    const int64x2_t intVal = vcvtq_s64_f64(val);
    const float64x2_t threshold = vdupq_n_f64(static_cast<f64>(1ULL << 63));
    const uint64x2_t cmpMask = vcgeq_f64(val, threshold);
    const uint64x2_t xorResult = veorq_u64(vreinterpretq_u64_s64(intVal), cmpMask);
    u64 result = vgetq_lane_u64(xorResult, 0);
  #endif

    FPRi(frd).setValue(std::bit_cast<f64>(result));
#else
  LOG_ERROR(Xenon, "fctidx: Unsupported arch!");
#endif

  // TODO:
  ppuUpdateFPSCR(ppuState, 0.0, 0.0, _instr.rc);
}

// Floating Convert to Integer Double Word with Round toward Zero (x'FC00 065E')
void PPCInterpreter::PPCInterpreter_fctidzx(PPU_STATE *ppuState) {
  // This was mostly taken from rpcs3's PPUInterpreter. 
  // TODO: Verify.

  CHECK_FPU;

#if defined(ARCH_X86_64)
  const auto val = _mm_set_sd(FPRi(frb).asDouble());
  const auto res = _mm_xor_si128(_mm_set1_epi64x(_mm_cvttsd_si64(val)), _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(f64(1ull << 63)))));
  FPRi(frd).setValue(std::bit_cast<f64>(_mm_cvtsi128_si64(res)));
#elif defined(ARCH_X86)
  // NOTE: This should be properly handled, but this is kind of a "placeholder" for non-SSE platforms
  const f64 val = FPRi(frb).asDouble();
  const s64 intVal = static_cast<s64>(val); // truncates toward zero
  const f64 threshold = static_cast<f64>(1ull << 63);
  const bool flip = val >= threshold;

  s64 flippedVal = flip ? intVal ^ (1ull << 63) : intVal;
  FPRi(frd).setValue(std::bit_cast<f64>(flippedVal));
#elif defined(ARCH_AARCH64)
  #if defined(__ARM_FEATURE_FP64) && __ARM_FEATURE_FP64
    const float64x2_t val = vsetq_lane_f64(FPRi(frb).asDouble(), vdupq_n_f64(0), 0);
    const s64 intVal = static_cast<s64>(vgetq_lane_f64(val, 0));
    const float64x2_t threshold = vdupq_n_f64(static_cast<f64>(1ULL << 63));
    const uint64x2_t cmpMask = vcgeq_f64(val, threshold);
    const uint64x2_t xorResult = veorq_u64(vsetq_lane_u64(static_cast<u64>(intVal), vdupq_n_u64(0), 0), cmpMask);
    u64 result = vgetq_lane_u64(xorResult, 0);
  #else
    const float64x2_t fval = vsetq_lane_f64(FPRi(frb).asDouble(), vdupq_n_f64(0), 0);
    const s64 intVal = static_cast<s64>(vgetq_lane_f64(fval, 0));
    const float64x2_t threshold = vdupq_n_f64(static_cast<f64>(1ULL << 63));
    const uint64x2_t cmpMask = vcgeq_f64(fval, threshold);
    const uint64x2_t xorResult = veorq_u64(vsetq_lane_u64(static_cast<u64>(intVal), vdupq_n_u64(0), 0), cmpMask);
    u64 result = vgetq_lane_u64(xorResult, 0);
  #endif
    FPRi(frd).setValue(std::bit_cast<f64>(result));
#else
  LOG_ERROR(Xenon, "fctidzx: Unsupported arch!");
#endif

  // TODO:
  ppuUpdateFPSCR(ppuState, 0.0, 0.0, _instr.rc);
}

// Floating Convert to Integer Word with Round toward Zero (x'FC00 001E')
void PPCInterpreter::PPCInterpreter_fctiwzx(PPU_STATE *ppuState) {
  // This was mostly taken from rpcs3's PPUInterpreter. 
  // TODO: Verify.

  CHECK_FPU;

#if defined(ARCH_X86_64)
  const auto val = _mm_set_sd(FPRi(frb).asDouble());
  const auto res = _mm_xor_si128(_mm_cvttpd_epi32(val), _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(0x80000000))));
  FPRi(frd).setValue(std::bit_cast<f64, s64>(_mm_cvtsi128_si32(res)));
#elif defined(ARCH_X86)
  // NOTE: This should be properly handled, but this is kind of a "placeholder" for non-SSE platforms
  const f64 val = FPRi(frb).asDouble();
  const s32 intVal = static_cast<s32>(val);
  const bool flip = val >= static_cast<f64>(0x80000000);

  s32 flippedVal = flip ? intVal ^ 0x80000000 : intVal;
  FPRi(frd).setValue(std::bit_cast<f64>(static_cast<s64>(flippedVal)));
#elif defined(ARCH_AARCH64)
  #if defined(__ARM_FEATURE_FP64)
    const float64x2_t val = vsetq_lane_f64(FPRi(frb).asDouble(), vdupq_n_f64(0), 0);
    const s32 intVal = static_cast<s32>(vgetq_lane_f64(val, 0));
    const float64x2_t threshold = vdupq_n_f64(static_cast<f64>(0x80000000));
    const uint32x4_t cmpMask = vreinterpretq_u32_u64(vcgeq_f64(val, threshold));
    const uint32x4_t xorResult = veorq_u32(vsetq_lane_u32(static_cast<u32>(intVal), vdupq_n_u32(0), 0), cmpMask);
    u32 uResult = vgetq_lane_u32(xorResult, 0);
    const s32 result = static_cast<s32>(uResult);
  #else
    const float64x2_t val = vsetq_lane_f64(FPRi(frb).asDouble(), vdupq_n_f64(0), 0);
    const f64 scalar = vgetq_lane_f64(val, 0);
    const float32x2_t converted = vcvt_f32_f64(val);
    const f32 result_f32 = vget_lane_f32(converted, 0);
    const s32 intVal = static_cast<s32>(result_f32);
    const bool flip = scalar >= static_cast<f64>(0x80000000);
    const s32 result = flip ? intVal ^ 0x80000000 : intVal;
  #endif

    FPRi(frd).setValue(std::bit_cast<f64>(static_cast<s64>(result)));
#else
  LOG_ERROR(Xenon, "fctiwzx: Unsupported arch!");
#endif

  // TODO:
  ppuUpdateFPSCR(ppuState, 0.0, 0.0, _instr.rc);
}

// Floating Convert from Integer Double Word (x'FC00 069C')
void PPCInterpreter::PPCInterpreter_fcfidx(PPU_STATE *ppuState) {
  /*
  frD <- signedInt64todouble(frB)
  */

  CHECK_FPU;

  FPRi(frd).setValue(static_cast<f64>(std::bit_cast<s64>(FPRi(frb).asDouble())));

  ppuUpdateFPSCR(ppuState, FPRi(frd).asDouble(), 0.0, _instr.rc);
}

// Floating Divide (Double-Precision) (x'FC00 0024')
void PPCInterpreter::PPCInterpreter_fdivx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) / (frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();

  const FPResult quotient = FPDiv(ppuState, fra, frb);
  const bool notDivideByZero = curThread.FPSCR.ZE == 0 || quotient.exception != FPSCR_BIT_ZX;
  const bool notInvalid = curThread.FPSCR.VE == 0 || quotient.HasNoInvalidExceptions();

  if (notDivideByZero && notInvalid) {
    const double result = FPForceDouble(ppuState, quotient.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Divide Single (x'EC00 0024')
void PPCInterpreter::PPCInterpreter_fdivsx(PPU_STATE *ppuState) {
  /*
  frD <- f32(frA) / (frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();

  const FPResult quotient = FPDiv(ppuState, fra, frb);
  const bool notDivideByZero = curThread.FPSCR.ZE == 0 || quotient.exception != FPSCR_BIT_ZX;
  const bool notInvalid = curThread.FPSCR.VE == 0 || quotient.HasNoInvalidExceptions();

  if (notDivideByZero && notInvalid) {
    const float result = FPForceSingle(ppuState, quotient.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Multiply-Add (Double-Precision) (x'FC00 003A')
void PPCInterpreter::PPCInterpreter_fmaddx(PPU_STATE *ppuState) {
  /*
  frD <- (frA * frC) + frB
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const FPResult product = FPMadd(ppuState, fra, frc, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const double result = FPForceDouble(ppuState, product.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Multiply-Add Single (x'EC00 003A')
void PPCInterpreter::PPCInterpreter_fmaddsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA * frC) + frB
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const double cValue = Force25Bit(frc);
  const FPResult dValue = FPMadd(ppuState, fra, cValue, frb);

  if (curThread.FPSCR.VE == 0 || dValue.HasNoInvalidExceptions()) {
    const float result = FPForceSingle(ppuState, dValue.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.clearFIFR();
    curThread.FPSCR.FI = dValue.value != result;
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Multiply (Double-Precision) (x'FC00 0032')
void PPCInterpreter::PPCInterpreter_fmulx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) * (frC)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const FPResult product = FPMul(ppuState, fra, frc);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const double result = FPForceDouble(ppuState, product.value);

    FPRi(frd).setValue(result);
    
    curThread.FPSCR.clearFIFR();
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Multiply Single (x'EC00 0032')
void PPCInterpreter::PPCInterpreter_fmulsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) * (frC)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const double cValue = Force25Bit(frc);
  const FPResult dValue = FPMul(ppuState, fra, cValue);

  if (curThread.FPSCR.VE == 0 || dValue.HasNoInvalidExceptions()) {
    const float result = FPForceSingle(ppuState, dValue.value);

    FPRi(frd).setValue(result);
    curThread.FPSCR.clearFIFR();
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Move Register (Double-Precision) (x'FC00 0090')
void PPCInterpreter::PPCInterpreter_fmrx(PPU_STATE *ppuState) {
  /*
  frD <- (frB)
  */

  CHECK_FPU;

  FPRi(frd).setValue(FPRi(frb).asU64());

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Multiply-Subtract (Double-Precision) (x'FC00 0038')
void PPCInterpreter::PPCInterpreter_fmsubx(PPU_STATE *ppuState) {
  /*
  frD <- ([frA * frC] - frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const FPResult product = FPMsub(ppuState, fra, frc, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const double result = FPForceDouble(ppuState, product.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Multiply-Subtract Single (x'EC00 0038')
void PPCInterpreter::PPCInterpreter_fmsubsx(PPU_STATE *ppuState) {
  /*
  frD <- ([frA * frC] - frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const double cValue = Force25Bit(frc);
  const FPResult product = FPMsub(ppuState, fra, cValue, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const float result = FPForceSingle(ppuState, product.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Negative Absolute Value
void PPCInterpreter::PPCInterpreter_fnabsx(PPU_STATE *ppuState) {
  /*
  frD <- - abs(frB)
  */

  CHECK_FPU;

  const f64 frB = FPRi(frb).asDouble();
  FPRi(frd).setValue(-(std::fabs(frB)));

  if (_instr.rc)
    ppuSetCR1(ppuState);
}

// Floating Negative Multiply-Add (Double-Precision) (x'FC00 003E')
void PPCInterpreter::PPCInterpreter_fnmaddx(PPU_STATE *ppuState) {
  /*
  frD <- - ([frA * frC] + frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const FPResult product = FPMadd(ppuState, fra, frc, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const float tmp = FPForceDouble(ppuState, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Negative Multiply-Add Single (x'EC00 003E')
void PPCInterpreter::PPCInterpreter_fnmaddsx(PPU_STATE *ppuState) {
  /*
  frD <- - ([frA * frC] + frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const double cValue = Force25Bit(frc);
  const FPResult product = FPMadd(ppuState, fra, cValue, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const float tmp = FPForceSingle(ppuState, product.value);
    const float result = std::isnan(tmp) ? tmp : -tmp;
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Negate (x'FC00 0050')
void PPCInterpreter::PPCInterpreter_fnegx(PPU_STATE *ppuState) {
  /*
  frD <- ~ frB[0] || frB[1-63]
  */

  CHECK_FPU;

  FPRi(frd).setValue(FPRi(frb).asU64() ^ (UINT64_C(1) << 63));

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Negative Multiply-Subtract (Double-Precision)
void PPCInterpreter::PPCInterpreter_fnmsubx(PPU_STATE *ppuState) {
  /*
  frD <- - ([frA * frC] - frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const FPResult product = FPMsub(ppuState, fra, frc, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const float tmp = FPForceDouble(ppuState, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Negative Multiply-Subtract Single (x'EC00 003C')
void PPCInterpreter::PPCInterpreter_fnmsubsx(PPU_STATE *ppuState) {
  /*
  frD <- - ([frA * frC] - frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();
  const f64 frc = FPRi(frc).asDouble();

  const double cValue = Force25Bit(frc);
  const FPResult product = FPMsub(ppuState, fra, cValue, frb);

  if (curThread.FPSCR.VE == 0 || product.HasNoInvalidExceptions()) {
    const float tmp = FPForceSingle(ppuState, product.value);
    const float result = std::isnan(tmp) ? tmp : -tmp;
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Round to Single (x'FC00 0018')
void PPCInterpreter::PPCInterpreter_frspx(PPU_STATE *ppuState) {
  /*
  frD <- Round_single( frB )
  */

  CHECK_FPU;

  const double b = FPRi(frb).asDouble();
  const float rounded = FPForceSingle(ppuState, b);

  if (std::isnan(b)) {
    const bool is_snan = IsSignalingNAN(b);

    if (is_snan) { FPSetException(ppuState, FPSCR_BIT_VXSNAN); }

    if (!is_snan || curThread.FPSCR.VE == 0) {
      FPRi(frd).setValue(rounded);
      curThread.FPSCR.FPRF = ClassifyFloat(rounded);
    }

    curThread.FPSCR.clearFIFR();
  }
  else {
    SetFI(ppuState, b != rounded);
    curThread.FPSCR.FR = fabs(rounded) > fabs(b);
    FPRi(frd).setValue(rounded);
    curThread.FPSCR.FPRF = ClassifyFloat(rounded);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Subtract (Double-Precision) (x'FC00 0028')
void PPCInterpreter::PPCInterpreter_fsubx(PPU_STATE *ppuState) {
  /*
    frD <- (frA) - (frB)
    */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();

  const FPResult difference = FPSub(ppuState, fra, frb);

  if (curThread.FPSCR.VE == 0 || difference.HasNoInvalidExceptions()) {
    const double result = FPForceDouble(ppuState, difference.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyDouble(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Subtract Single (x'EC00 0028')
void PPCInterpreter::PPCInterpreter_fsubsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) - (frB)
  */

  CHECK_FPU;

  const f64 fra = FPRi(fra).asDouble();
  const f64 frb = FPRi(frb).asDouble();

  const FPResult difference = FPSub(ppuState, fra, frb);

  if (curThread.FPSCR.VE == 0 || difference.HasNoInvalidExceptions()) {
    const double result = FPForceSingle(ppuState, difference.value);
    FPRi(frd).setValue(result);
    curThread.FPSCR.FPRF = ClassifyFloat(result);
  }

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Floating Square Root (Double-Precision) (x'FC00 002C')
void PPCInterpreter::PPCInterpreter_fsqrtx(PPU_STATE* ppuState) {
  /*
  frD <- (Square_rootfrB)
  */

  CHECK_FPU;

  FPRi(frd).setValue(std::sqrt(FPRi(frb).asDouble()));

  ppuUpdateFPSCR(ppuState, FPRi(frd).asDouble(), 0.0, _instr.rc);
}

// Floating Square Root Single (x'EC00 002C')
void PPCInterpreter::PPCInterpreter_fsqrtsx(PPU_STATE* ppuState) {
  /*
  frD <- Single(Square_rootfrB)
  */

  CHECK_FPU;

  FPRi(frd).setValue(static_cast<f32>(std::sqrt(FPRi(frb).asDouble())));

  ppuUpdateFPSCR(ppuState, FPRi(frd).asDouble(), 0.0, _instr.rc);
}

// Move from FPSCR (x'FC00 048E')
void PPCInterpreter::PPCInterpreter_mffsx(PPU_STATE *ppuState) {
  CHECK_FPU;

  FPRi(frd).setValue(static_cast<u64>(GET_FPSCR));

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Move to FPSCR Fields (x'FC00 058E')
void PPCInterpreter::PPCInterpreter_mtfsfx(PPU_STATE *ppuState) {
  CHECK_FPU;

  const u32 fm = _instr.flm;
  u32 m = 0;

  for (u32 i = 0; i < 8; i++) {
    if ((fm & (1U << i)) != 0)
      m |= (0xFU << (i * 4));
  }

  curThread.FPSCR = (curThread.FPSCR.FPSCR_Hex & ~m) | (static_cast<u32>(FPRi(frb).asU64()) & m);

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Move to FPSCR Bit 0 (x'FC00 008C')
void PPCInterpreter::PPCInterpreter_mtfsb0x(PPU_STATE* ppuState) {
  /*
  Bit crbD of the FPSCR is unset. 
  */

  CHECK_FPU;

  u32 b = 0x80000000 >> _instr.crbd;

  curThread.FPSCR.FPSCR_Hex &= ~b;

  if (_instr.rc) { ppuSetCR1(ppuState); }
}

// Move to FPSCR Bit 1 (x'FC00 004C')
void PPCInterpreter::PPCInterpreter_mtfsb1x(PPU_STATE* ppuState) {
  /*
  Bit crbD of the FPSCR is set.
  */

  CHECK_FPU;

  const u32 bit = _instr.crbd;
  const u32 b = 0x80000000 >> bit;

  if ((b & FPSCR_ANY_X) != 0)
    FPSetException(ppuState, b);
  else
    curThread.FPSCR |= b;

  if (_instr.rc) { ppuSetCR1(ppuState); }
}