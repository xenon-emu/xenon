/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Arch.h"
#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

//
// Helper functions.
//

// Based on the work done by the rpcs3 team

// Add/Add Carrying implementation
template <typename T>
struct addResult {
  T result;
  bool carry;

  addResult() = default;

  // Straightforward ADD with flags
  // The integer arithmetic instructions, always set the XER bit [CA],
  // to reflect the carry out of bit [0] in the default 64-bit mode
  // and out of bit[32] in 32 bit mode(of 64 bit implementations)
  addResult(T a, T b, bool sfBitMode) :
    result(a + b), carry(sfBitMode ? (result < a) :
      (static_cast<u32>(result) < static_cast<u32>(a))) {}

  // Straightforward ADC with flags
  addResult(T a, T b, bool c, bool sfBitMode) :
    addResult(a, b, sfBitMode) {
    addResult r(result, c, sfBitMode);
    result = r.result;
    carry |= r.carry;
  }
  static constexpr addResult<T> addBits(T a, T b, bool sfBitMode) {
    return { a, b, sfBitMode };
  }

  static constexpr addResult<T> addBits(T a, T b, bool c, bool sfBitMode) {
    return { a, b, c, sfBitMode };
  }
};

// Multiply High Sign/Unsigned.
#ifdef ARCH_X86
inline u64 umulh64(u64 x, u64 y) {
#if defined(_MSC_VER) || defined(__MINGW32__)
  const u32 x_lo = static_cast<u32>(x);
  const u32 x_hi = static_cast<u32>(x >> 32);
  const u32 y_lo = static_cast<u32>(y);
  const u32 y_hi = static_cast<u32>(y >> 32);

  const u64 lo_lo = static_cast<u64>(x_lo) * y_lo;
  const u64 hi_lo = static_cast<u64>(x_hi) * y_lo;
  const u64 lo_hi = static_cast<u64>(x_lo) * y_hi;
  const u64 hi_hi = static_cast<u64>(x_hi) * y_hi;

  const u64 cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFFull) + (lo_hi & 0xFFFFFFFFull);
  return hi_hi + (hi_lo >> 32) + (lo_hi >> 32) + (cross >> 32);
#else
  return static_cast<u64>((u128{ x } *u128{ y }) >> 64);
#endif
}
inline s64 mulh64(s64 x, s64 y) {
#if defined(_MSC_VER) || defined(__MINGW32__)
  const bool negate = (x < 0) ^ (y < 0);
  const u64 ux = static_cast<u64>(x < 0 ? -x : x);
  const u64 uy = static_cast<u64>(y < 0 ? -y : y);
  const u64 high = umulh64(ux, uy);
  if (negate) {
    // Perform two's complement negation for high part.
    if (ux * uy == 0) return -static_cast<s64>(high);
    return ~static_cast<s64>(high) + ((ux * uy & 0xFFFFFFFFFFFFFFFFull) == 0 ? 1 : 0);
  } else {
    return static_cast<s64>(high);
  }
#else
  return static_cast<s64>((s128{ x } *s128{ y }) >> 64);
#endif
}
#else
inline u64 umulh64(u64 x, u64 y) {
#if defined(_MSC_VER)
  return __umulh(x, y);
#else
  return static_cast<u64>((u128{ x } *u128{ y }) >> 64);
#endif
}
inline s64 mulh64(s64 x, s64 y) {
#if defined(_MSC_VER)
  return __mulh(x, y);
#else
  return static_cast<s64>((s128{ x } *s128{ y }) >> 64);
#endif
}
#endif

// Platform agnostic 32/64 bit Rotate-left.
u32 rotl32(u32 x, u32 n) {
#ifdef _MSC_VER
  return _rotl(x, n);
#elif defined(__clang__)
  return __builtin_rotateleft32(x, n);
#else
  return (x << (n & 31)) | (x >> (((0 - n) & 31)));
#endif
}

u64 rotl64(u64 x, u64 n) {
#ifdef _MSC_VER
  return _rotl64(x, static_cast<int>(n));
#elif defined(__clang__)
  return __builtin_rotateleft64(x, n);
#else
  return (x << (n & 63)) | (x >> (((0 - n) & 63)));
#endif
}

// Duplicates a u32 value left, used in rotate instructions that duplicate the
// lower 32 bits
inline constexpr u64 duplicate32(u32 x) { return x | static_cast<u64>(x) << 32; }

// Set XER[OV] bit. Overflow enable
inline void ppuSetXerOv(sPPEState *ppeState, bool inbit) {
  uXER xer = curThread.SPR.XER;
  // Set register as intended
  curThread.SPR.XER.hexValue = 0;
  // Maintain ByteCount
  curThread.SPR.XER.ByteCount = xer.ByteCount;
  // Should maintain SO and CA bits
  if (xer.CA)
    curThread.SPR.XER.CA = 1;
  if (xer.SO || inbit)
    curThread.SPR.XER.SO = 1;
  // Set OV based on input.
  curThread.SPR.XER.OV = inbit;
}

// Compares and records the input value taking into account computation mode.
#define RECORD_CR0(x) \
  if (curThread.SPR.MSR.SF) { \
    ppuSetCR<s64>(ppeState, 0, x, 0); \
  } else { \
    ppuSetCR<s32>(ppeState, 0, static_cast<s32>(x), 0); \
  }

//
// Instruction definitions.
//

// Add (x'7C00 0214')
void PPCInterpreter::PPCInterpreter_addx(sPPEState *ppeState) {
  /*
    rD <- (rA) + (rB)
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  GPRi(rd) = RA + RB;

  // The setting of the affected bits in the XER is mode-dependent,
  // and reflects overflow of the 64-bit result in 64 bit mode and
  // overflow of the low-order 32 bit result in 32 bit mode
  if (_instr.oe) {
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == static_cast<u32>(RB) >> 31)
        && (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // So basically after doing hardware tests it appears that at least in 32bit
  // mode of operation the setting of CR is mode-dependant
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Add + OE
void PPCInterpreter::PPCInterpreter_addox(sPPEState *ppeState) {
  PPCInterpreter_addx(ppeState);
}

// Add Carrying (x'7C00 0014')
void PPCInterpreter::PPCInterpreter_addcx(sPPEState *ppeState) {
  /*
    rD <- (rA) + (rB)
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>(RA, RB, curThread.SPR.MSR.SF);

  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // _oe
  if (_instr.oe) { // Mode dependent
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == static_cast<u32>(RB) >> 31)
        && (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Add Carrying + OE
void PPCInterpreter::PPCInterpreter_addcox(sPPEState *ppeState) {
  PPCInterpreter_addcx(ppeState);
}

// Add Extended (x'7C00 0114')
void PPCInterpreter::PPCInterpreter_addex(sPPEState *ppeState) {
  /*
    rD <- (rA) + (rB) + XER[CA]
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>::addBits(RA, RB, XER_GET_CA, curThread.SPR.MSR.SF);

  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // _oe
  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == static_cast<u32>(RB) >> 31)
        && (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(add.result);
  }
}

// Add Extended + OE
void PPCInterpreter::PPCInterpreter_addeox(sPPEState *ppeState) {
  PPCInterpreter_addex(ppeState);
}

// Add Immediate (x'3800 0000')
void PPCInterpreter::PPCInterpreter_addi(sPPEState *ppeState) {
  /*
    if rA = 0 then rD <- EXTS(SIMM)
    else rD <- (rA) + EXTS(SIMM)
  */

  GPRi(rd) = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
}

// Add Immediate Carrying (x'3000 0000')
void PPCInterpreter::PPCInterpreter_addic(sPPEState *ppeState) {
  /*
    rD <- (rA) + EXTS(SIMM)
  */

  const s64 ra = GPRi(ra);
  const s64 i = _instr.simm16;

  const auto add = addResult<u64>::addBits(ra, i, curThread.SPR.MSR.SF);

  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // _rc
  if (_instr.main & 1) {
    RECORD_CR0(add.result);
  }
}

// Add Immediate Shifted (x'3C00 0000')
void PPCInterpreter::PPCInterpreter_addis(sPPEState *ppeState) {
  /*
    if rA = 0 then rD <- EXTS(SIMM || (16)0)
    else rD <- (rA) + EXTS(SIMM || (16)0)
  */

  GPRi(rd) = _instr.ra ? GPRi(ra) + (_instr.simm16 * 65536) : (_instr.simm16 * 65536);
}

// Add to Minus One Extended (x'7C00 01D4')
void PPCInterpreter::PPCInterpreter_addmex(sPPEState *ppeState) {
  /*
    rD <- (rA) + XER[CA] - 1
  */

  const s64 RA = GPRi(ra);

  const auto add = addResult<u64>(RA, ~0ull, XER_GET_CA, curThread.SPR.MSR.SF);

  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) {
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (static_cast<u64>(RA) >> 63 == 1) &&
        (static_cast<u64>(RA) >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == 1) &&
        (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  if (_instr.rc) {
    RECORD_CR0(add.result);
  }
}

// Add to Minus One Extended + OE
void PPCInterpreter::PPCInterpreter_addmeox(sPPEState *ppeState) {
  PPCInterpreter_addmex(ppeState);
}

// Add to Zero Extended (x'7C00 0194')
void PPCInterpreter::PPCInterpreter_addzex(sPPEState *ppeState) {
  /*
    rD <- (rA) + XER[CA]
  */

  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>::addBits(RA, 0, XER_GET_CA,
    curThread.SPR.MSR.SF);

  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // Mode dependent
  if (_instr.oe) {
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == 0) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == 0) &&
        (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(add.result);
  }
}

// Add to Zero Extended + OE
void PPCInterpreter::PPCInterpreter_addzeox(sPPEState *ppeState) {
  PPCInterpreter_addzex(ppeState);
}

// And (x'7C00 0038')
void PPCInterpreter::PPCInterpreter_andx(sPPEState *ppeState) {
  /*
    rA <- (rS) & (rB)
  */

  GPRi(ra) = GPRi(rs) & GPRi(rb);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// AND with Complement (x'7C00 0078')
void PPCInterpreter::PPCInterpreter_andcx(sPPEState *ppeState) {
  /*
    rA <- (rS) + ~(rB)
  */

  GPRi(ra) = GPRi(rs) & ~GPRi(rb);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// And Immediate (x'7000 0000')
void PPCInterpreter::PPCInterpreter_andi(sPPEState *ppeState) {
  /*
    rA <- (rS) & ((48)0 || UIMM)
  */

  GPRi(ra) = GPRi(rs) & _instr.uimm16;

  RECORD_CR0(GPRi(ra));
}

// And Immediate Shifted (x'7400 0000')
void PPCInterpreter::PPCInterpreter_andis(sPPEState *ppeState) {
  /*
    rA <- (rS) + ((32)0 || UIMM || (16)0)
  */

  GPRi(ra) = GPRi(rs) & (u64{ _instr.uimm16 } << 16);

  RECORD_CR0(GPRi(ra))
}

// Compare
void PPCInterpreter::PPCInterpreter_cmp(sPPEState *ppeState) {
  if (_instr.l10) {
    ppuSetCR<s64>(ppeState, _instr.crfd, GPRi(ra), GPRi(rb));
  }
  else {
    ppuSetCR<s32>(ppeState, _instr.crfd, static_cast<u32>(GPRi(ra)), static_cast<u32>(GPRi(rb)));
  }
}

// Compare Immediate
void PPCInterpreter::PPCInterpreter_cmpi(sPPEState *ppeState) {
  if (_instr.l10) {
    ppuSetCR<s64>(ppeState, _instr.crfd, GPRi(ra), _instr.simm16);
  }
  else {
    ppuSetCR<s32>(ppeState, _instr.crfd, static_cast<u32>(GPRi(ra)), _instr.simm16);
  }
}

// Compare Logical
void PPCInterpreter::PPCInterpreter_cmpl(sPPEState *ppeState) {
  if (_instr.l10) {
    ppuSetCR<u64>(ppeState, _instr.crfd, GPRi(ra), GPRi(rb));
  }
  else {
    ppuSetCR<u32>(ppeState, _instr.crfd, static_cast<u32>(GPRi(ra)), static_cast<u32>(GPRi(rb)));
  }
}

// Compare Logical Immediate
void PPCInterpreter::PPCInterpreter_cmpli(sPPEState *ppeState) {
  if (_instr.l10) {
    ppuSetCR<u64>(ppeState, _instr.crfd, GPRi(ra), _instr.uimm16);
  }
  else {
    ppuSetCR<u32>(ppeState, _instr.crfd, static_cast<u32>(GPRi(ra)), _instr.uimm16);
  }
}

// Count Leading Zeros Double Word (x'7C00 0074')
void PPCInterpreter::PPCInterpreter_cntlzdx(sPPEState *ppeState) {
  /*
    n <- 0
    do while n < 64
      if rS[n] = 1 then leave
      n <- n + 1
    rA <- n
  */

  GPRi(ra) = std::countl_zero(GPRi(rs));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Count Leading Zeros Word (x'7C00 0034')
void PPCInterpreter::PPCInterpreter_cntlzwx(sPPEState *ppeState) {
  /*
    n <- 32
    do while n < 64
      if rS[n] = 1 then leave
      n <- n + 1
    rA <- n - 32
  */

  GPRi(ra) = std::countl_zero(static_cast<u32>(GPRi(rs)));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Condition Register AND
void PPCInterpreter::PPCInterpreter_crand(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crAnd = a & b;

  if (crAnd & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register AND with Complement
void PPCInterpreter::PPCInterpreter_crandc(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crAndc = a & (1 ^ b);

  if (crAndc & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register Equivalent
void PPCInterpreter::PPCInterpreter_creqv(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crEqv = 1 ^ (a ^ b);

  if (crEqv & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register NAND
void PPCInterpreter::PPCInterpreter_crnand(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crNand = 1 ^ (a & b);

  if (crNand & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register NOR
void PPCInterpreter::PPCInterpreter_crnor(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crNor = 1 ^ (a | b);

  if (crNor & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register OR
void PPCInterpreter::PPCInterpreter_cror(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crOr = a | b;

  if (crOr & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register OR with Complement
void PPCInterpreter::PPCInterpreter_crorc(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crOrc = a | (1 ^ b);

  if (crOrc & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Condition Register XOR
void PPCInterpreter::PPCInterpreter_crxor(sPPEState *ppeState) {
  const u32 a = CR_GET(_instr.crba);
  const u32 b = CR_GET(_instr.crbb);

  const u32 crXor = a ^ b;

  if (crXor & 1)
    BSET(curThread.CR.CR_Hex, 32, _instr.crbd);
  else
    BCLR(curThread.CR.CR_Hex, 32, _instr.crbd);
}

// Divide Double Word (x'7C00 03D2')
void PPCInterpreter::PPCInterpreter_divdx(sPPEState *ppeState) {
  /*
    dividend[0-63] <- (rA)
    divisor[0-63] <- (rB)
    rD <- dividend + divisor
  */

  const s64 RA = GPRi(ra);
  const s64 RB = GPRi(rb);
  const bool o = RB == 0 || (RA == INT64_MIN && RB == -1);
  GPRi(rd) = o ? 0 : RA / RB;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Divide Double Word Unsigned (x'7C00 0392')
void PPCInterpreter::PPCInterpreter_divdux(sPPEState *ppeState) {
  /*
    dividend[0-63] <- (rA)
    divisor[0-63] <- (rB)
    rD <- dividend / divisor
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);
  GPRi(rd) = RB == 0 ? 0 : RA / RB;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Divide Double Word (Overflow-Enabled) (x'7C00 03D2')
void PPCInterpreter::PPCInterpreter_divdox(sPPEState *ppeState) {
  /*
    dividend[0-63] <- (rA)
    divisor[0-63] <- (rB)
    rD <- dividend + divisor
  */

  const s64 RA = GPRi(ra);
  const s64 RB = GPRi(rb);
  const bool o = RB == 0 || (RA == INT64_MIN && RB == -1);
  GPRi(rd) = o ? 0 : RA / RB;

  // _oe
  if (_instr.oe) {
    ppuSetXerOv(ppeState, RB == 0);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Divide Double Word Unsigned (Overflow-Enabled) (x'7C00 0392')
void PPCInterpreter::PPCInterpreter_divduox(sPPEState *ppeState) {
  /*
    dividend[0-63] <- (rA)
    divisor[0-63] <- (rB)
    rD <- dividend / divisor
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);
  GPRi(rd) = RB == 0 ? 0 : RA / RB;

  // _oe
  if (_instr.oe) {
    ppuSetXerOv(ppeState, RB == 0);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Divide Word (x'7C00 03D6')
void PPCInterpreter::PPCInterpreter_divwx(sPPEState *ppeState) {
  /*
    dividend[0-63] <- EXTS(rA[32-63])
    divisor[0-63] <- EXTS(rB[32-63])
    rD[32-63] <- dividend / divisor
    rD[0-31] <- undefined
  */

  const s32 RA = static_cast<s32>(GPRi(ra));
  const s32 RB = static_cast<s32>(GPRi(rb));
  const bool o = RB == 0 || (RA == INT32_MIN && RB == -1);
  GPRi(rd) = o ? 0 : static_cast<u32>(RA / RB);

  // If OE = 1 and RB = 0, then OV is set.
  if (_instr.oe) {
    ppuSetXerOv(ppeState, o);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Divide Word + OE
void PPCInterpreter::PPCInterpreter_divwox(sPPEState *ppeState) {
  PPCInterpreter_divwx(ppeState);
}

// Divide Word Unsigned (x'7C00 0396')
void PPCInterpreter::PPCInterpreter_divwux(sPPEState *ppeState) {
  /*
    dividend[0-63] <- (32)0 || (rA)[32-63]
    divisor[0-63] <- (32)0 || (rB)[32-63]
    rD[32-63] <- dividend / divisor
    rD[0-31] <- undefined
  */

  const u32 RA = static_cast<u32>(GPRi(ra));
  const u32 RB = static_cast<u32>(GPRi(rb));
  GPRi(rd) = RB == 0 ? 0 : RA / RB;

  if (_instr.oe) {
    ppuSetXerOv(ppeState, RB == 0);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Divide Word Unsigned + OE
void PPCInterpreter::PPCInterpreter_divwuox(sPPEState *ppeState) {
  PPCInterpreter_divwux(ppeState);
}

// Equivalent (x'7C00 0238')
void PPCInterpreter::PPCInterpreter_eqvx(sPPEState *ppeState) {
  /*
    rA <- 1 ^ ((rS) ^ (rB))
  */

  GPRi(ra) = ~(GPRi(rs) ^ GPRi(rb));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Extend Sign Byte (x'7C00 0774')
void PPCInterpreter::PPCInterpreter_extsbx(sPPEState *ppeState) {
  /*
    S <- rS[56]
    rA[56-63] <- rS[56-63]
    rA[0-55] <- (56)S
  */

  GPRi(ra) = static_cast<s8>(GPRi(rs));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Extend Sign Half Word (x'7C00 0734')
void PPCInterpreter::PPCInterpreter_extshx(sPPEState *ppeState) {
  /*
    S <- rS[48]
    rA[48-63] <- rS[48-63]
    rA[0-47] <- (48)S
  */

  GPRi(ra) = static_cast<s16>(GPRi(rs));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Extend Sign Word (x'7C00 07B4')
void PPCInterpreter::PPCInterpreter_extswx(sPPEState *ppeState) {
  /*
    S <- rS[32]
    rA[32-63] <- rS[32-63]
    rA[0-31] <- (32)S
  */

  GPRi(ra) = static_cast<s32>(GPRi(rs));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Move Condition Register Field
void PPCInterpreter::PPCInterpreter_mcrf(sPPEState *ppeState) {
  /*
  * CR4 * BF + 32:4 * BF + 35 <- CR4 * crS + 32:4 * crS + 35
  * The contents of field crS (bits 4 * crS + 32-4 * crS + 35) of CR are copied to field 
  * crD (bits 4 * crD + 32-4 * crD + 35) of CR.
  */
  const u32 BF = _instr.crfd;
  const u32 BFA = _instr.crfs;

  const u32 CR = DGET(curThread.CR.CR_Hex, (BFA) * 4, (BFA) * 4 + 3);

  ppcUpdateCR(ppeState, BF, CR);
}

// Move from Time Base (x'7C00 02E6')
void PPCInterpreter::PPCInterpreter_mftb(sPPEState *ppeState) {
  /*
    n <- tbr[5-9] || tbr[0-4]
    if n = 268 then
      if (64-bit implementation) then
        rD <- TB
      else
        rD <- TBL
    else if n = 269 then
      if (64-bit implementation) then
        rD <- (32)0 || TBU
      else
        rD <- TBU
  */

  const u32 spr = (_instr.spr >> 5) | ((_instr.spr & 0x1f) << 5);
  if (spr == TBLRO) {
    GPRi(rd) = ppeState->SPR.TB.TBL;
  } else { GPRi(rd) = ppeState->SPR.TB.TBU; }
}

// Move From One Condition Register Field
void PPCInterpreter::PPCInterpreter_mfocrf(sPPEState *ppeState) {
  if (_instr.l11) {
    // MFOCRF
    u32 crMask = 0;
    u32 bit = 0x80;
    u32 count = 0;

    for (; bit; bit >>= 1) {
      crMask <<= 4;
      if (_instr.crm & bit) {
        crMask |= 0xF;
        count++;
      }
    }

    if (count == 1) {
      GPRi(rd) = curThread.CR.CR_Hex & crMask;
    }
    else { // Undefined behavior.
      GPRi(rd) = 0;
    }
  }
  else {
    // MFCR
    GPRi(rd) = curThread.CR.CR_Hex;
  }
}

// Move To One Condition Register Field
void PPCInterpreter::PPCInterpreter_mtocrf(sPPEState *ppeState) {
  // MTOCRF
  u32 crMask = 0;
  u32 bit = 0x80;

  for (; bit; bit >>= 1) {
    crMask <<= 4;
    if (_instr.crm & bit) {
      crMask |= 0xF;
    }
  }
  curThread.CR.CR_Hex = (static_cast<u32>(GPRi(rs)) & crMask) | (curThread.CR.CR_Hex & ~crMask);
}

// Multiply Low Immediate (x'1C00 0000')
void PPCInterpreter::PPCInterpreter_mulli(sPPEState *ppeState) {
  /*
    prod[0-127] <- (rA) * EXTS(SIMM)
    rD <- prod[64-127]
  */

  GPRi(rd) = static_cast<s64>(GPRi(ra)) * _instr.simm16;
}

// Multiply Low Double Word (x'7C00 01D2')
void PPCInterpreter::PPCInterpreter_mulldx(sPPEState *ppeState) {
  /*
    prod[0-127] <- (rA) * (rB)
    rD <- prod[64-127]
  */

  const s64 RA = GPRi(ra);
  const s64 RB = GPRi(rb);
  GPRi(rd) = RA * RB;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Multiply Low Double Word (Overflow-enabled) (x'7C00 01D2')
void PPCInterpreter::PPCInterpreter_mulldox(sPPEState *ppeState) {
  /*
    prod[0-127] <- (rA) * (rB)
    rD <- prod[64-127]
  */

  const s64 RA = GPRi(ra);
  const s64 RB = GPRi(rb);
  GPRi(rd) = RA * RB;

  // _oe
  if (_instr.oe) [[unlikely]] {
    const u64 high = mulh64(RA, RB);
    ppuSetXerOv(ppeState, high != static_cast<s64>(GPRi(rd)) >> 63);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Multiply Low Word (x'7C00 01D6')
void PPCInterpreter::PPCInterpreter_mullwx(sPPEState *ppeState) {
  /*
    rD <- rA[32-63] * rB[32-63]
  */

  GPRi(rd) = s64{ static_cast<s32>(GPRi(ra)) } *static_cast<s32>(GPRi(rb));

  if (_instr.oe) {
    ppuSetXerOv(ppeState, s64(GPRi(rd)) < INT32_MIN ||
                          s64(GPRi(rd)) > INT32_MAX);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Multiply Low Word + OE
void PPCInterpreter::PPCInterpreter_mullwox(sPPEState *ppeState) {
  PPCInterpreter_mullwx(ppeState);
}

// Multiply High Word (x'7C00 0096')
void PPCInterpreter::PPCInterpreter_mulhwx(sPPEState *ppeState) {
  /*
    prod[0-63] <- rA[32-63] * rB[32-63]
    rD[32-63] <- prod[0-31]
    rD[0-31] <- undefined
  */

  const s32 a = static_cast<s32>(GPRi(ra));
  const s32 b = static_cast<s32>(GPRi(rb));
  GPRi(rd) = (s64{ a } *b) >> 32;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Multiply High Word Unsigned (x'7C00 0016')
void PPCInterpreter::PPCInterpreter_mulhwux(sPPEState *ppeState) {
  /*
    prod[0-63] <- rA[32-63] * rB[32-63]
    rD[32-63] <- prod[0-31]
    rD[0-31] <- undefined
  */

  const u32 a = static_cast<u32>(GPRi(ra));
  const u32 b = static_cast<u32>(GPRi(rb));
  GPRi(rd) = (u64{ a } *b) >> 32;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Multiply High Double Word (x'7C00 0092')
void PPCInterpreter::PPCInterpreter_mulhdx(sPPEState *ppeState) {
  /*
    prod[0-127] <- (rA) * (rB)
    rD <- prod[0-63]
  */

  GPRi(rd) = mulh64(GPRi(ra), GPRi(rb));

  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Multiply High Double Word Unsigned (x'7C00 0012')
void PPCInterpreter::PPCInterpreter_mulhdux(sPPEState *ppeState) {
  /*
    prod[0-127] <- (rA) * (rB)
    rD <- prod[0-63]
  */

  GPRi(rd) = umulh64(GPRi(ra), GPRi(rb));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// NAND
void PPCInterpreter::PPCInterpreter_nandx(sPPEState *ppeState) {
  /*
    rA <- ~((rS) & (rB))
  */

  GPRi(ra) = ~(GPRi(rs) & GPRi(rb));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Negate
void PPCInterpreter::PPCInterpreter_negx(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + 1
  */

  const u64 RA = GPRi(ra);
  GPRi(rd) = 0 - RA;

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = RA == (1ULL << 63);
    }
    else {
      ovSet = static_cast<u32>(RA) == (1ULL << 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Negate + OE
void PPCInterpreter::PPCInterpreter_negox(sPPEState *ppeState) {
  PPCInterpreter_negx(ppeState);
}

// NOR (x'7C00 00F8')
void PPCInterpreter::PPCInterpreter_norx(sPPEState *ppeState) {
  /*
    rA <- ~((rS) | (rB))
  */

  GPRi(ra) = ~(GPRi(rs) | GPRi(rb));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// OR with Complement (x'7C00 0338')
void PPCInterpreter::PPCInterpreter_orcx(sPPEState *ppeState) {
  /*
    rA <- (rS) | ~(rB)
  */

  GPRi(ra) = GPRi(rs) | ~GPRi(rb);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// OR Immediate (x'6000 0000')
void PPCInterpreter::PPCInterpreter_ori(sPPEState *ppeState) {
  /*
    rA <- (rS) | ((4816)0 || UIMM)
  */

  GPRi(ra) = GPRi(rs) | _instr.uimm16;
}

// OR Immediate Shifted (x'6400 0000')
void PPCInterpreter::PPCInterpreter_oris(sPPEState *ppeState) {
  /*
    rA <- (rS) | ((32)0 || UIMM || (16)0)
  */

  GPRi(ra) = GPRi(rs) | (u64{ _instr.uimm16 } << 16);
}

// OR (x'7C00 0378')
void PPCInterpreter::PPCInterpreter_orx(sPPEState *ppeState) {
  /*
    rA <- (rS) | (rB)
  */

  GPRi(ra) = GPRi(rs) | GPRi(rb);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Double Word Immediate then Clear (x'7800 0008')
void PPCInterpreter::PPCInterpreter_rldicx(sPPEState *ppeState) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, ~ n)
    rA <- r & m
  */

  GPRi(ra) = rotl64(GPRi(rs), _instr.sh64) & PPCRotateMask(_instr.mbe64, _instr.sh64 ^ 63);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Double Word then Clear Right (x'7800 0012')
void PPCInterpreter::PPCInterpreter_rldcrx(sPPEState *ppeState) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, n)
    e <- me[5] || me[0-4]
    m <- MASK(0, e)
    rA <- r & m
  */

  GPRi(ra) = rotl64(GPRi(rs), GPRi(rb) & 0x3f) & (~0ull << (_instr.mbe64 ^ 63));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Double Word then Clear Left (x'7800 0010')
void PPCInterpreter::PPCInterpreter_rldclx(sPPEState *ppeState) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, 63)
    rA <- r & m
  */

  GPRi(ra) = rotl64(GPRi(rs), GPRi(rb) & 0x3f) & (~0ull >> _instr.mbe64);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Double Word Immediate then Clear Left (x'7800 0000')
void PPCInterpreter::PPCInterpreter_rldiclx(sPPEState *ppeState) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, 63)
    rA <- r & m
  */

  GPRi(ra) = rotl64(GPRi(rs), _instr.sh64) & (~0ull >> _instr.mbe64);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Double Word Immediate then Clear Right (x'7800 0004')
void PPCInterpreter::PPCInterpreter_rldicrx(sPPEState *ppeState) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    e <- me[5] || me[0-4]
    m <- MASK(0, e)
    rA <- r & m
  */

  GPRi(ra) = rotl64(GPRi(rs), _instr.sh64) & (~0ull << (_instr.mbe64 ^ 63));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Double Word Immediate then Mask Insert (x'7800 000C')
void PPCInterpreter::PPCInterpreter_rldimix(sPPEState *ppeState) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, n)
    b <- mb[5] || mb[0-4]
    m <- MASK(b, ~n)
    rA <- (r & m) | (rA & ~m)
  */

  const u64 mask = PPCRotateMask(_instr.mbe64, _instr.sh64 ^ 63);
  GPRi(ra) = (GPRi(ra) & ~mask) | (rotl64(GPRi(rs), _instr.sh64) & mask);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Word Immediate then Mask Insert (x'5000 0000')
void PPCInterpreter::PPCInterpreter_rlwimix(sPPEState *ppeState) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- (r & m) | (rA & ~m)
  */

  const u64 mask = PPCRotateMask(32 + _instr.mb32, 32 + _instr.me32);
  GPRi(ra) = (GPRi(ra) & ~mask) | (duplicate32(rotl32(static_cast<u32>(GPRi(rs)), _instr.sh32)) & mask);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Word then AND with Mask (x'5C00 0000')
void PPCInterpreter::PPCInterpreter_rlwnmx(sPPEState *ppeState) {
  /*
    n <- rB[59-63]27-31
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- r & m
  */

  GPRi(ra) = duplicate32(rotl32(static_cast<u32>(GPRi(rs)), GPRi(rb) & 0x1f))
    & PPCRotateMask(32 + _instr.mb32, 32 + _instr.me32);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Rotate Left Word Immediate then AND with Mask (x'5400 0000')
void PPCInterpreter::PPCInterpreter_rlwinmx(sPPEState *ppeState) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], n)
    m <- MASK(MB + 32, ME + 32)
    rA <- (r & m)
  */

  GPRi(ra) = duplicate32(rotl32(static_cast<u32>(GPRi(rs)), _instr.sh32))
    & PPCRotateMask(32 + _instr.mb32, 32 + _instr.me32);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Left Double Word (x'7C00 0036')
void PPCInterpreter::PPCInterpreter_sldx(sPPEState *ppeState) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, n)
    if rB[57] = 0 then
      m <- MASK(0, 63 - n)
    else m <- (64)0
    rA <- r & m
  */

  const u32 n = GPRi(rb) & 0x7f;
  GPRi(ra) = n & 0x40 ? 0 : GPRi(rs) << n;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Left Word (x'7C00 0030')
void PPCInterpreter::PPCInterpreter_slwx(sPPEState *ppeState) {
  /*
    n <- rB[59-63]
    r <- ROTL[32](rS[32-63], n)
    if rB[58] = 0 then m <- MASK(32, 63 - n)
    else m <- (64)0
    rA <- r & m
  */

  GPRi(ra) = static_cast<u32>(GPRi(rs) << (GPRi(rb) & 0x3f));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Algebraic Double Word (x'7C00 0634')
void PPCInterpreter::PPCInterpreter_sradx(sPPEState *ppeState) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, 64 - n)
    if rB[57] = 0 then
      m <- MASK(n, 63)
    else m <- (64)0
    S <- rS[0]
    rA <- (r & m) | (((64)S) & ~m)
    XER[CA] <- S & ((r & ~ m) | 0)
  */

  const s64 RS = GPRi(rs);
  const u8 shift = GPRi(rb) & 127;
  if (shift > 63)
  {
    GPRi(ra) = 0 - (RS < 0);
    XER_SET_CA((RS < 0));
  }
  else
  {
    GPRi(ra) = RS >> shift;
    XER_SET_CA((RS < 0) && ((GPRi(ra) << shift) != static_cast<u64>(RS)));
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Algebraic Double Word Immediate (x'7C00 0674')
void PPCInterpreter::PPCInterpreter_sradix(sPPEState *ppeState) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, 64 - n)
    m <- MASK(n, 63)
    S <- rS[0]
    rA <- (r & m) | (((64)S) & ~m)
    XER[CA] <- S & ((r & ~m) != 0)
  */

  const auto sh = _instr.sh64;
  const s64 RS = GPRi(rs);
  GPRi(ra) = RS >> sh;
  XER_SET_CA((RS < 0) && ((GPRi(ra) << sh) != static_cast<u64>(RS)));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Algebraic Word (x'7C00 0630')
void PPCInterpreter::PPCInterpreter_srawx(sPPEState *ppeState) {
  /*
    n <- rB[59-63]
    r <- ROTL[32](rS[32-63], 64 - n)
    if rB[5826] = 0 then
    m <- MASK(n + 32, 63)
    else m <- (64)0
    S <- rS[32]
    rA <- r & m | (64)S & ~m
    XER[CA] <- S & (r & ~m[32-63] != 0
  */

  const s32 RS = static_cast<s32>(GPRi(rs));
  const u8 shift = GPRi(rb) & 63;
  if (shift > 31) {
    GPRi(ra) = 0 - (RS < 0);
    XER_SET_CA((RS < 0));
  }
  else {
    GPRi(ra) = RS >> shift;
    XER_SET_CA((RS < 0) && ((GPRi(ra) << shift) != static_cast<u64>(RS)));
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Algebraic Word Immediate (x'7C00 0670')
void PPCInterpreter::PPCInterpreter_srawix(sPPEState *ppeState) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], 64 - n)
    m<- MASK(n + 32, 63)
    S <- rS[32]
    rA <- r & m | (64)S & ~m
    XER[CA] <- S & ((r & ~m)[32-63] != 0)
  */

  const s32 RS = static_cast<u32>(GPRi(rs));
  GPRi(ra) = RS >> _instr.sh32;
  XER_SET_CA((RS < 0) && (static_cast<u32>(GPRi(ra) << _instr.sh32)
    != static_cast<u32>(RS)));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Double Word (x'7C00 0436')
void PPCInterpreter::PPCInterpreter_srdx(sPPEState *ppeState) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, 64 - n)
    if rB[57] = 0 then
      m <- MASK(n, 63)
    else m <- (64)0
    rA <- r & m
  */

  const u32 n = GPRi(rb) & 0x7f;
  GPRi(ra) = n & 0x40 ? 0 : GPRi(rs) >> n;

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Word (x'7C00 0430')
void PPCInterpreter::PPCInterpreter_srwx(sPPEState *ppeState) {
  /*
    n <- rB[58-63]
    r <- ROTL[32](rS[32-63], 64 - n)
    if rB[58] = 0 then
      m <- MASK(n + 32, 63)
    else m <- (64)0
    rA <- r & m
  */

  GPRi(ra) = (GPRi(rs) & 0xffffffff) >> (GPRi(rb) & 0x3f);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Subtract from Carrying (x'7C00 0010')
void PPCInterpreter::PPCInterpreter_subfcx(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + (rB) + 1
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>::addBits(~RA, RB, 1,
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == RB >> 63) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == static_cast<u32>(RB) >> 31) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Subtract from Carrying + OE
void PPCInterpreter::PPCInterpreter_subfcox(sPPEState *ppeState) {
  PPCInterpreter_subfcx(ppeState);
}

// Subtract From (x'7C00 0050')
void PPCInterpreter::PPCInterpreter_subfx(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + (rB) + 1
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  GPRi(rd) = RB - RA;

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == RB >> 63) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == static_cast<u32>(RB) >> 31) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(rd));
  }
}

// Subtract From + OE
void PPCInterpreter::PPCInterpreter_subfox(sPPEState *ppeState) {
  PPCInterpreter_subfx(ppeState);
}

// Subtract from Extended (x'7C00 0110')
void PPCInterpreter::PPCInterpreter_subfex(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + (rB) + XER[CA]
  */

  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>::addBits(~RA, RB, XER_GET_CA,
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == RB >> 63) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == static_cast<u32>(RB) >> 31) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(add.result);
  }
}

// Subtract from Extended + OE
void PPCInterpreter::PPCInterpreter_subfeox(sPPEState *ppeState) {
  PPCInterpreter_subfex(ppeState);
}

// Subtract from Minus One Extended (x'7C00 01D0')
void PPCInterpreter::PPCInterpreter_subfmex(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + XER[CA] - 1
  */
  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>(~RA, ~0ULL, XER_GET_CA,
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // Mode dependent
  if (_instr.oe) {
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == 1) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == 1) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(add.result);
  }
}

// Subtract from Minus One Extended + OE
void PPCInterpreter::PPCInterpreter_subfmeox(sPPEState *ppeState) {
  PPCInterpreter_subfmex(ppeState);
}

// Subtract from Zero Extended (x'7C00 0190')
void PPCInterpreter::PPCInterpreter_subfzex(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + XER[CA]
  */

  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>::addBits(~RA, 0, XER_GET_CA,
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // Mode dependent
  if (_instr.oe) {
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == 0) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == 0) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppeState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    RECORD_CR0(add.result);
  }
}

// Subtract from Zero Extended + OE
void PPCInterpreter::PPCInterpreter_subfzeox(sPPEState *ppeState) {
  PPCInterpreter_subfzex(ppeState);
}

// Subtract from Immediate Carrying (x'2000 0000')
void PPCInterpreter::PPCInterpreter_subfic(sPPEState *ppeState) {
  /*
    rD <- ~(rA) + EXTS(SIMM) + 1
  */

  const u64 RA = GPRi(ra);
  const s64 i = _instr.simm16;

  const auto add = addResult<u64>::addBits(~RA, i, 1,
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);
}

// XOR (x'7C00 0278')
void PPCInterpreter::PPCInterpreter_xorx(sPPEState *ppeState) {
  /*
    rA <- (rS) ^ (rB)
  */

  GPRi(ra) = GPRi(rs) ^ GPRi(rb);

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// XOR Immediate (x'6800 0000')
void PPCInterpreter::PPCInterpreter_xori(sPPEState *ppeState) {
  /*
    rA <- (rS) ^ ((4816)0 || UIMM)
  */

  GPRi(ra) = GPRi(rs) ^ _instr.uimm16;
}

// XOR Immediate Shifted (x'6C00 0000')
void PPCInterpreter::PPCInterpreter_xoris(sPPEState *ppeState) {
  /*
    rA <- (rS) ^ ((32)0 || UIMM || (16)0)
  */

  GPRi(ra) = GPRi(rs) ^ (u64{ _instr.uimm16 } << 16);
}
