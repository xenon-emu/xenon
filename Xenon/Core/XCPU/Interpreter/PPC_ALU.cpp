// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

//
// Helper functions.
//

// Based on the work done by the rpcs3 team.

// Add/Add Carrying implementation.
template <typename T>
struct addResult {
  T result;
  bool carry;

  addResult() = default;

  // Straighforward ADD with flags.
  // The integer arithmetic instructions, always set the XER bit [CA], 
  // to reflect the carry out of bit [0] in the default 64-bit mode 
  // and out of bit[32] in 32 bit mode(of 64 bit implementations).
  addResult(T a, T b, bool sfBitMode) :
    result(a + b), carry(sfBitMode ? (result < a) :
      (static_cast<u32>(result) < static_cast<u32>(a)))
  {}

  // Straighforward ADC with flags
  addResult(T a, T b, bool c, bool sfBitMode) :
    addResult(a, b, sfBitMode)
  {
    addResult r(result, c, sfBitMode);
    result = r.result;
    carry |= r.carry;
  }
  static addResult<T> addBits(T a, T b, bool sfBitMode) {
    return { a, b, sfBitMode };
  }

  static addResult<T> addBits(T a, T b, bool c, bool sfBitMode) {
    return { a, b, c, sfBitMode };
  }
};

// Multiply High Sign/Unsigned.
inline u64 umulh64(u64 x, u64 y) {
#if defined(_MSC_VER) && defined(_WIN64)
  return __umulh(x, y);
#else
  return static_cast<u64>((u128{ x } *u128{ y }) >> 64);
#endif
}

inline s64 mulh64(s64 x, s64 y) {
#if defined(_MSC_VER) && defined(_WIN64)
  return __mulh(x, y);
#else
  return static_cast<s64>((s128{ x } *s128{ y }) >> 64);
#endif
}

// Platform agnostic 32/64 bit Rotate-left.
u32 rotl32(u32 x, u32 n)
{
#ifdef _MSC_VER
  return _rotl(x, n);
#elif defined(__clang__)
  return __builtin_rotateleft32(x, n);
#else
  return (x << (n & 31)) | (x >> (((0 - n) & 31)));
#endif
}

u64 rotl64(u64 x, u64 n)
{
#ifdef _MSC_VER
  return _rotl64(x, static_cast<int>(n));
#elif defined(__clang__)
  return __builtin_rotateleft64(x, n);
#else
  return (x << (n & 63)) | (x >> (((0 - n) & 63)));
#endif
}

// Duplicates a u32 value left, used in rotate instructions that duplicate the 
// lower 32 bits.
inline u64 duplicate32(u32 x) { return x | static_cast<u64>(x) << 32; }

// Set XER[OV] bit. Overflow enable.
inline void ppuSetXerOv(PPU_STATE *ppuState, bool inbit)
{
  XERegister xer = curThread.SPR.XER;
  // Set register as intended.
  curThread.SPR.XER.XER_Hex = 0;
  // Mantain ByteCount.
  curThread.SPR.XER.ByteCount = xer.ByteCount; 
  // Should mantain SO and CA bits.
  if (xer.CA)
    curThread.SPR.XER.CA = 1;
  if (xer.SO || inbit)
    curThread.SPR.XER.SO = 1;
  // Set OV based on input.
  curThread.SPR.XER.OV = inbit;
}

// Vali004 loves macros, and i'm getting used to them also!

// Compares and records the input value taking into account computation mode.
#define RECORD_CR0(x) if (curThread.SPR.MSR.SF) { \
                      ppuSetCR<s64>(ppuState, 0, x, 0); \
                      } else { \
                      ppuSetCR<s32>(ppuState, 0, static_cast<s32>(x), 0); \
                      }

//
// Instruction definitions.
//

void PPCInterpreter::PPCInterpreter_addx(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  GPRi(rd) = RA + RB;

  // The setting of the affected bits in the XER is mode-dependent, 
  // and reflects overflow of the 64-bit result in 64 bit mode and 
  // overflow of the low-order 32 bit result in 32 bit mode.
  if (_instr.oe) {
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == static_cast<u32>(RB) >> 31)
        && (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addox(PPU_STATE* ppuState)
{
  PPCInterpreter_addx(ppuState);
}

void PPCInterpreter::PPCInterpreter_addcx(PPU_STATE* ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>(RA, RB, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == static_cast<u32>(RB) >> 31) 
        && (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addcox(PPU_STATE* ppuState)
{
  PPCInterpreter_addcx(ppuState);
}

void PPCInterpreter::PPCInterpreter_addex(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>::addBits(RA, RB, XER_GET_CA, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == static_cast<u32>(RB) >> 31)
        && (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addeox(PPU_STATE* ppuState)
{
  PPCInterpreter_addex(ppuState);
}

void PPCInterpreter::PPCInterpreter_addi(PPU_STATE *ppuState) {
  GPRi(rd) = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
}

void PPCInterpreter::PPCInterpreter_addic(PPU_STATE *ppuState) {
  const s64 ra = GPRi(ra);
  const s64 i = _instr.simm16;

  const auto add = addResult<u64>::addBits(ra, i, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  // _rc
  if (_instr.main & 1) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addis(PPU_STATE *ppuState) {
  GPRi(rd) = _instr.ra ? GPRi(ra) + (_instr.simm16 * 65536) : (_instr.simm16 * 65536);
}

void PPCInterpreter::PPCInterpreter_addmex(PPU_STATE* ppuState)
{
  const s64 RA = GPRi(ra);

  const auto add = addResult<u64>(RA, ~0ULL, XER_GET_CA, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (static_cast<u64>(RA) >> 63 == 1) && 
        (static_cast<u64>(RA) >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == 1) &&
        (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  // _rc
  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addmeox(PPU_STATE* ppuState)
{
  PPCInterpreter_addmex(ppuState);
}

void PPCInterpreter::PPCInterpreter_addzex(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>::addBits(RA, 0, XER_GET_CA, 
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (RA >> 63 == 0) && (RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(RA) >> 31 == 0) &&
        (static_cast<u32>(RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addzeox(PPU_STATE* ppuState)
{
  PPCInterpreter_addzex(ppuState);
}

void PPCInterpreter::PPCInterpreter_andx(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) & GPRi(rb);

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_andcx(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) & ~GPRi(rb);

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_andi(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) & _instr.uimm16;

  u32 CR = CRCompS(ppuState, GPRi(ra), 0);
  ppcUpdateCR(ppuState, 0, CR);
}

void PPCInterpreter::PPCInterpreter_andis(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) & (u64{ _instr.uimm16 } << 16);

  u32 CR = CRCompS(ppuState, GPRi(ra), 0);
  ppcUpdateCR(ppuState, 0, CR);
}

void PPCInterpreter::PPCInterpreter_cmp(PPU_STATE *ppuState) {
  if (_instr.l10) {
    ppuSetCR<s64>(ppuState, _instr.crfd, GPRi(ra), GPRi(rb));
  } else {
    ppuSetCR<s32>(ppuState, _instr.crfd, static_cast<u32>(GPRi(ra)), static_cast<u32>(GPRi(rb)));
  }
}

void PPCInterpreter::PPCInterpreter_cmpi(PPU_STATE *ppuState) {
  if (_instr.l10) {
    ppuSetCR<s64>(ppuState, _instr.crfd, GPRi(ra), _instr.simm16);
  } else {
    ppuSetCR<s32>(ppuState, _instr.crfd, static_cast<u32>(GPRi(ra)), _instr.simm16);
  }
}

void PPCInterpreter::PPCInterpreter_cmpl(PPU_STATE *ppuState) {
  if (_instr.l10) {
    ppuSetCR<u64>(ppuState, _instr.crfd, GPRi(ra), GPRi(rb));
  } else {
    ppuSetCR<u32>(ppuState, _instr.crfd, static_cast<u32>(GPRi(ra)), static_cast<u32>(GPRi(rb)));
  }
}

void PPCInterpreter::PPCInterpreter_cmpli(PPU_STATE *ppuState) {
  if (_instr.l10) {
    ppuSetCR<u64>(ppuState, _instr.crfd, GPRi(ra), _instr.uimm16);
  } else {
    ppuSetCR<u32>(ppuState, _instr.crfd, static_cast<u32>(GPRi(ra)), _instr.uimm16);
  }
}

void PPCInterpreter::PPCInterpreter_cntlzdx(PPU_STATE *ppuState) {
  GPRi(ra) = std::countl_zero(GPRi(rs));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_cntlzwx(PPU_STATE *ppuState) {
  GPRi(ra) = std::countl_zero(static_cast<u32>(GPRi(rs)));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_crand(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crAnd = a & b;

  if (crAnd & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crandc(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crAndc = a & (1 ^ b);

  if (crAndc & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_creqv(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crEqv = 1 ^ (a ^ b);

  if (crEqv & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crnand(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crNand = 1 ^ (a & b);

  if (crNand & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crnor(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crNor = 1 ^ (a | b);

  if (crNor & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_cror(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;
  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crOr = a | b;

  if (crOr & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crorc(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crOrc = a | (1 ^ b);

  if (crOrc & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crxor(PPU_STATE *ppuState) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crXor = a ^ b;

  if (crXor & 1)
    BSET(curThread.CR.CR_Hex, 32, BT);
  else
    BCLR(curThread.CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_divdx(PPU_STATE *ppuState) {
  const s64 RA = GPRi(ra);
  const s64 RB = GPRi(rb);
  const bool o = RB == 0 || (RA == INT64_MIN && RB == -1);
  GPRi(rd) = o ? 0 : RA / RB;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divdux(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);
  GPRi(rd) = RB == 0 ? 0 : RA / RB;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divwx(PPU_STATE *ppuState) {
  const s32 RA = static_cast<s32>(GPRi(ra));
  const s32 RB = static_cast<s32>(GPRi(rb));
  const bool o = RB == 0 || (RA == INT32_MIN && RB == -1);
  GPRi(rd) = o ? 0 : static_cast<u32>(RA / RB);

  // If OE = 1 and RB = 0, then OV is set.
  if (_instr.oe) {
    ppuSetXerOv(ppuState, o);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divwox(PPU_STATE* ppuState)
{
  PPCInterpreter_divwx(ppuState);
}

void PPCInterpreter::PPCInterpreter_divwux(PPU_STATE *ppuState) {
  const u32 RA = static_cast<u32>(GPRi(ra));
  const u32 RB = static_cast<u32>(GPRi(rb));
  GPRi(rd) = RB == 0 ? 0 : RA / RB;

  if (_instr.oe) {
    ppuSetXerOv(ppuState, RB == 0);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divwuox(PPU_STATE* ppuState)
{
  PPCInterpreter_divwux(ppuState);
}

void PPCInterpreter::PPCInterpreter_eqvx(PPU_STATE* ppuState) {
  GPRi(ra) = ~(GPRi(rs) ^ GPRi(rb));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_extsbx(PPU_STATE *ppuState) {
  GPRi(ra) = static_cast<s8>(GPRi(rs));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_extshx(PPU_STATE *ppuState) {
  GPRi(ra) = static_cast<s16>(GPRi(rs));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_extswx(PPU_STATE *ppuState) {
  GPRi(ra) = static_cast<s32>(GPRi(rs));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mcrf(PPU_STATE *ppuState) {
  XL_FORM_BF_BFA;

  u32 CR = DGET(curThread.CR.CR_Hex, (BFA) * 4,
                (BFA) * 4 + 3);

  ppcUpdateCR(ppuState, BF, CR);
}

void PPCInterpreter::PPCInterpreter_mfocrf(PPU_STATE *ppuState) {
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
    } else { // Undefined behavior.
      GPRi(rd) = 0;
    }
  }
  else {
    // MFCR
    GPRi(rd) = curThread.CR.CR_Hex;
  }
}

void PPCInterpreter::PPCInterpreter_mftb(PPU_STATE *ppuState) {
  XFX_FORM_rD_spr; // because 5-bit fields are swapped

  switch (spr) {
  case 268:
    GPR(rD) = ppuState->SPR.TB;
    break;
  case 269:
    GPR(rD) = HIDW(ppuState->SPR.TB);
    break;

  default:
    LOG_CRITICAL(Xenon, "MFTB -> Illegal instruction form!");
    break;
  }
}

void PPCInterpreter::PPCInterpreter_mtocrf(PPU_STATE *ppuState) {
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

void PPCInterpreter::PPCInterpreter_mulli(PPU_STATE *ppuState) {
  GPRi(rd) = static_cast<s64>(GPRi(ra)) * _instr.simm16;
}

void PPCInterpreter::PPCInterpreter_mulldx(PPU_STATE *ppuState) {
  const s64 RA = GPRi(ra);
  const s64 RB = GPRi(rb);
  GPRi(rd) = RA * RB;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mullwx(PPU_STATE *ppuState) {
  GPRi(rd) = s64{ static_cast<s32>(GPRi(ra)) } *static_cast<s32>(GPRi(rb));

  if (_instr.oe) {
    ppuSetXerOv(ppuState, s64(GPRi(rd)) < INT32_MIN 
      || s64(GPRi(rd)) > INT32_MAX);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mullwox(PPU_STATE* ppuState)
{
  PPCInterpreter_mullwx(ppuState);
}

void PPCInterpreter::PPCInterpreter_mulhwx(PPU_STATE* ppuState)
{
  const s32 a = static_cast<s32>(GPRi(ra));
  const s32 b = static_cast<s32>(GPRi(rb));
  GPRi(rd) = (s64{ a } * b) >> 32;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mulhwux(PPU_STATE *ppuState) {
  const u32 a = static_cast<u32>(GPRi(ra));
  const u32 b = static_cast<u32>(GPRi(rb));
  GPRi(rd) = (u64{ a } *b) >> 32;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mulhdux(PPU_STATE *ppuState) {
  GPRi(rd) = umulh64(GPRi(ra), GPRi(rb));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_nandx(PPU_STATE *ppuState) {
  GPRi(ra) = ~(GPRi(rs) & GPRi(rb));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_negx(PPU_STATE *ppuState) {
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
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_negox(PPU_STATE* ppuState)
{
  PPCInterpreter_negx(ppuState);
}

void PPCInterpreter::PPCInterpreter_norx(PPU_STATE *ppuState) {
  GPRi(ra) = ~(GPRi(rs) | GPRi(rb));

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_orcx(PPU_STATE *ppuState)
{
  GPRi(ra) = GPRi(rs) | ~GPRi(rb);

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_ori(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) | _instr.uimm16;
}

void PPCInterpreter::PPCInterpreter_oris(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) | (u64{ _instr.uimm16 } << 16);
}

void PPCInterpreter::PPCInterpreter_orx(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) | GPRi(rb);

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

// Rotate Left Double Word Immediate then Clear (x'7800 0008')
void PPCInterpreter::PPCInterpreter_rldicx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rldcrx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rldclx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rldiclx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rldicrx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rldimix(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rlwimix(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_rlwnmx(PPU_STATE* ppuState) {
  /*
    n <- rB[59-6327-31]
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
void PPCInterpreter::PPCInterpreter_rlwinmx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_sldx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_slwx(PPU_STATE* ppuState) {
  /*
    n <- rB[59-63]
    r <- ROTL[32](rS[32–63], n)
    if rB[58] = 0 then m <- MASK(32, 63 – n)
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
void PPCInterpreter::PPCInterpreter_sradx(PPU_STATE* ppuState) {
  /*
    n <- rB[58-63]
    r <- ROTL[64](rS, 64 - n)
    if rB[57] = 0 then
      m <- MASK(n, 63)
    else m <- (64)0
    S <- rS[0]
    rA <- (r & m) | (((64)S) & ~m)
    XER[CA] <- S & ((r & ~ m) ¦ 0)
  */

  s64 RS = GPRi(rs);
  u8 shift = GPRi(rb) & 127;
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
void PPCInterpreter::PPCInterpreter_sradix(PPU_STATE* ppuState) {
  /*
    n <- sh[5] || sh[0-4]
    r <- ROTL[64](rS, 64 - n)
    m <- MASK(n, 63)
    S <- rS[0]
    rA <- (r & m) | (((64)S) & ~m)
    XER[CA] <- S & ((r & ~m) != 0)
  */

  auto sh = _instr.sh64;
  s64 RS = GPRi(rs);
  GPRi(ra) = RS >> sh;
  XER_SET_CA((RS < 0) && ((GPRi(ra) << sh) != static_cast<u64>(RS)));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Algebraic Word (x'7C00 0630')
void PPCInterpreter::PPCInterpreter_srawx(PPU_STATE* ppuState) {
  /*
    n <- rB[59-63]
    r <- ROTL[32](rS[32–63], 64 – n)
    if rB[5826] = 0 then
    m <- MASK(n + 32, 63)
    else m <- (64)0
    S <- rS[32]
    rA <- r & m | (64)S & ~m
    XER[CA] <- S & (r & ~m[32-63] != 0
  */

  s32 RS = static_cast<s32>(GPRi(rs));
  u8 shift = GPRi(rb) & 63;
  if (shift > 31)
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

// Shift Right Algebraic Word Immediate (x'7C00 0670')
void PPCInterpreter::PPCInterpreter_srawix(PPU_STATE* ppuState) {
  /*
    n <- SH
    r <- ROTL[32](rS[32-63], 64 - n)
    m<- MASK(n + 32, 63)
    S <- rS[32]
    rA <- r & m | (64)S & ~m
    XER[CA] <- S & ((r & ~m)[32-63] != 0)
  */

  s32 RS = static_cast<u32>(GPRi(rs));
  GPRi(ra) = RS >> _instr.sh32;
  XER_SET_CA((RS < 0) && (static_cast<u32>(GPRi(ra) << _instr.sh32)
    != static_cast<u32>(RS)));

  // _rc
  if (_instr.rc) {
    RECORD_CR0(GPRi(ra));
  }
}

// Shift Right Double Word (x'7C00 0436')
void PPCInterpreter::PPCInterpreter_srdx(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_srwx(PPU_STATE* ppuState) {
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

void PPCInterpreter::PPCInterpreter_subfcx(PPU_STATE *ppuState) {
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
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfcox(PPU_STATE* ppuState)
{
  PPCInterpreter_subfcx(ppuState);
}

void PPCInterpreter::PPCInterpreter_subfx(PPU_STATE *ppuState) {
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
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfox(PPU_STATE* ppuState)
{
  PPCInterpreter_subfx(ppuState);
}

void PPCInterpreter::PPCInterpreter_subfex(PPU_STATE *ppuState) {
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
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfeox(PPU_STATE* ppuState)
{
  PPCInterpreter_subfex(ppuState);
}

void PPCInterpreter::PPCInterpreter_subfmex(PPU_STATE* ppuState)
{
  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>(~RA, ~0ULL, XER_GET_CA, 
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == 1) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == 1) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfmeox(PPU_STATE* ppuState)
{
  PPCInterpreter_subfmex(ppuState);
}

void PPCInterpreter::PPCInterpreter_subfzex(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>::addBits(~RA, 0, XER_GET_CA, 
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) { // Mode dependent.
    bool ovSet = false;
    if (curThread.SPR.MSR.SF) {
      ovSet = (~RA >> 63 == 0) && (~RA >> 63 != GPRi(rd) >> 63);
    }
    else {
      ovSet = (static_cast<u32>(~RA) >> 31 == 0) &&
        (static_cast<u32>(~RA) >> 31 != static_cast<u32>(GPRi(rd)) >> 31);
    }
    ppuSetXerOv(ppuState, ovSet);
  }

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfzeox(PPU_STATE* ppuState)
{
  PPCInterpreter_subfzex(ppuState);
}

void PPCInterpreter::PPCInterpreter_subfic(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const s64 i = _instr.simm16;

  const auto add = addResult<u64>::addBits(~RA, i, 1, 
    curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);
}

void PPCInterpreter::PPCInterpreter_xorx(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) ^ GPRi(rb);

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_xori(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) ^ _instr.uimm16;
}

void PPCInterpreter::PPCInterpreter_xoris(PPU_STATE *ppuState) {
  GPRi(ra) = GPRi(rs) ^ (u64{ _instr.uimm16 } << 16);
}
