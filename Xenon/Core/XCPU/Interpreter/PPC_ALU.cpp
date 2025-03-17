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
#ifdef _MSC_VER
  return __umulh(x, y);
#else
  return static_cast<u64>((u128{ x } *u128{ y }) >> 64);
#endif
}

inline s64 mulh64(s64 x, s64 y) {
#ifdef _MSC_VER
  return __mulh(x, y);
#else
  return static_cast<s64>(s128{ x } *s128{ y }) >> 64;
#endif
}

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

//
// Instruction definitions.
//

void PPCInterpreter::PPCInterpreter_addx(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  GPRi(rd) = RA + RB;

  if (_instr.oe) {
    ppuSetXerOv(ppuState, (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63));
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

  if (_instr.oe) {
    ppuSetXerOv(ppuState, (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63));
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

  if (_instr.oe) {
    ppuSetXerOv(ppuState, (RA >> 63 == RB >> 63) && (RA >> 63 != GPRi(rd) >> 63));
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

  const auto add = addResult<u64>(RA, ~0ull, XER_GET_CA, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) {
    ppuSetXerOv(ppuState, (u64(RA) >> 63 == 1) && (u64(RA) >> 63 != GPRi(rd) >> 63));
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
  const u64 ra = GPRi(ra);

  const auto add = addResult<u64>::addBits(ra, 0, XER_GET_CA, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.oe) {
    ppuSetXerOv(ppuState, (ra >> 63 == 0) && (ra >> 63 != GPRi(rd) >> 63));
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
  X_FORM_BF_L_rA_rB;

  u32 CR;

  if (L) {
    CR = CRCompS64(ppuState, GPR(rA), GPR(rB));
  } else {
    CR = CRCompS32(ppuState, static_cast<s32>(GPR(rA)), static_cast<s32>(GPR(rB)));
  }

  ppcUpdateCR(ppuState, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cmpi(PPU_STATE *ppuState) {
  D_FORM_BF_L_rA_SI;
  SI = EXTS(SI, 16);

  u32 CR;

  if (L) {
    CR = CRCompS64(ppuState, GPR(rA), SI);
  } else {
    CR = CRCompS32(ppuState, static_cast<s32>(GPR(rA)), static_cast<s32>(SI));
  }

  ppcUpdateCR(ppuState, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cmpl(PPU_STATE *ppuState) {
  X_FORM_BF_L_rA_rB;

  u32 CR;

  if (L) {
    CR = CRCompU(ppuState, GPR(rA), GPR(rB));
  } else {
    CR = CRCompU(ppuState, static_cast<u32>(GPR(rA)), static_cast<u32>(GPR(rB)));
  }

  ppcUpdateCR(ppuState, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cmpli(PPU_STATE *ppuState) {
  D_FORM_BF_L_rA_UI;

  u32 CR;

  if (L) {
    CR = CRCompU(ppuState, GPR(rA), UI);
  } else {
    CR = CRCompU(ppuState, static_cast<u32>(GPR(rA)), UI);
  }

  ppcUpdateCR(ppuState, BF, CR);
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

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divwux(PPU_STATE *ppuState) {
  const u32 RA = static_cast<u32>(GPRi(ra));
  const u32 RB = static_cast<u32>(GPRi(rb));
  GPRi(rd) = RB == 0 ? 0 : RA / RB;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
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
  XFX_FORM_rD;

  GPR(rD) = curThread.CR.CR_Hex;
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
  XFX_FORM_rS_FXM;
  u32 Mask = 0;
  u32 b = 0x80;

  for (; b; b >>= 1) {
    Mask <<= 4;

    if (FXM & b) {
      Mask |= 0xF;
    }
  }
  curThread.CR.CR_Hex = (static_cast<u32>(GPR(rS)) & Mask) | (curThread.CR.CR_Hex & ~Mask);
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

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mulhwx(PPU_STATE* ppuState)
{
  s32 a = static_cast<s32>(GPRi(ra));
  s32 b = static_cast<s32>(GPRi(rb));
  GPRi(rd) = (s64{ a } * b) >> 32;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mulhwux(PPU_STATE *ppuState) {
  u32 a = static_cast<u32>(GPRi(ra));
  u32 b = static_cast<u32>(GPRi(rb));
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

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
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
    u32 CR = CRCompU(ppuState, GPRi(ra), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldicx(PPU_STATE *ppuState) {
  MD_FORM_rS_rA_sh_mb_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u32 e = 63 - sh;
  u64 m = QMASK(mb, e);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldcrx(PPU_STATE *ppuState) {
  MDS_FORM_rS_rA_rB_me_RC;

  u64 qwRb = GPR(rB);
  u32 n = static_cast<u32>(QGET(qwRb, 58, 63));
  u64 r = std::rotl<u64>(GPR(rS), n);
  u64 m = QMASK(0, me);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldiclx(PPU_STATE *ppuState) {
  MD_FORM_rS_rA_sh_mb_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u64 m = QMASK(mb, 63);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldicrx(PPU_STATE *ppuState) {
  MD_FORM_rS_rA_sh_me_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u64 m = QMASK(0, me);
  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldimix(PPU_STATE *ppuState) {
  MD_FORM_rS_rA_sh_mb_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u32 e = 63 - sh;
  u64 m = QMASK(mb, e);

  GPR(rA) = (r & m) | (GPR(rA) & ~m);

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rlwimix(PPU_STATE *ppuState) {
  M_FORM_rS_rA_SH_MB_ME_RC;

  u32 r = std::rotl<u32>(static_cast<u32>(GPR(rS)), SH);
  u32 m = (MB <= ME) ? DMASK(MB, ME) : (DMASK(0, ME) | DMASK(MB, 31));

  GPR(rA) = (r & m) | (static_cast<u32>(GPR(rA)) & ~m);

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rlwnmx(PPU_STATE *ppuState) {
  M_FORM_rS_rA_rB_MB_ME_RC;

  u32 m = (MB <= ME) ? DMASK(MB, ME) : (DMASK(0, ME) | DMASK(MB, 31));

  GPR(rA) = std::rotl<u32>(static_cast<u32>(GPR(rS)), (static_cast<u32>(GPR(rB))) & 31) & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rlwinmx(PPU_STATE *ppuState) {
  M_FORM_rS_rA_SH_MB_ME_RC;

  u32 m = (MB <= ME) ? DMASK(MB, ME) : (DMASK(0, ME) | DMASK(MB, 31));

  GPR(rA) = std::rotl<u32>(static_cast<u32>(GPR(rS)), SH) & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_sldx(PPU_STATE *ppuState) {
  X_FORM_rS_rA_rB_RC;

  u64 regB = GPR(rB);
  u32 n = static_cast<u32>(QGET(regB, 58, 63));
  u64 r = std::rotl<u64>(GPR(rS), n);
  u64 m = QGET(regB, 57, 57) ? 0 : QMASK(0, 63 - n);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_slwx(PPU_STATE *ppuState) {
  X_FORM_rS_rA_rB_RC;

  u32 n = static_cast<u32>(GPR(rB)) & 63;

  GPR(rA) = (n < 32) ? (static_cast<u32>(GPR(rS)) << n) : 0;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_sradx(PPU_STATE *ppuState) {
  X_FORM_rS_rA_rB_RC;

  u64 regRS = GPR(rS);
  u32 n = static_cast<u32>(GPR(rB)) & 127;
  u64 r = std::rotl<u64>(regRS, 64 - (n & 63));
  u64 m = (n & 0x40) ? 0 : QMASK(n, 63);
  u64 s = BGET(regRS, 64, 0) ? QMASK(0, 63) : 0;

  GPR(rA) = (r & m) | (s & ~m);

  if (s && ((r & ~m) != 0))
    curThread.SPR.XER.CA = 1;
  else
    curThread.SPR.XER.CA = 0;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_sradix(PPU_STATE *ppuState) {
  X_FORM_rS_rA_SH_XO_RC;

  SH |= (XO & 1) << 5;

  if (SH == 0) {
    GPR(rA) = GPR(rS);
    curThread.SPR.XER.CA = 0;
  } else {
    u64 r = std::rotl<u64>(GPR(rS), 64 - SH);
    u64 m = QMASK(SH, 63);
    u64 s = BGET(GPR(rS), 64, 0);

    GPR(rA) = (r & m) | ((static_cast<u64>(-static_cast<s64>(s))) & ~m);

    if (s && (r & ~m) != 0)
      curThread.SPR.XER.CA = 1;
    else
      curThread.SPR.XER.CA = 0;
  }

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srawx(PPU_STATE *ppuState) {
  X_FORM_rS_rA_rB_RC;

  u64 regRs = GPR(rS);
  u64 n = static_cast<u32>(GPR(rB)) & 63;
  u64 r = std::rotl<u32>(static_cast<u32>(regRs), 64 - (n & 31));
  u64 m = (n & 0x20) ? 0 : QMASK(n + 32, 63);
  u64 s = BGET(regRs, 32, 0) ? QMASK(0, 63) : 0;
  GPR(rA) = (r & m) | (s & ~m);

  if (s && ((static_cast<u32>(r & ~m)) != 0))
    XER_SET_CA(1);
  else
    XER_SET_CA(0);

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srawix(PPU_STATE *ppuState) {
  X_FORM_rS_rA_SH_RC;

  u64 rSReg = GPR(rS);
  u64 r = std::rotl<u32>(static_cast<u32>(rSReg), 64 - SH);
  u64 m = QMASK(SH + 32, 63);
  u64 s = BGET(rSReg, 32, 0) ? QMASK(0, 63) : 0;

  GPR(rA) = (r & m) | (s & ~m);

  if (s && ((static_cast<u32>(r & ~m)) != 0))
    XER_SET_CA(1);
  else
    XER_SET_CA(0);

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srdx(PPU_STATE *ppuState) {
  X_FORM_rS_rA_rB_RC;

  u64 regS = GPR(rS);
  u32 n = static_cast<u32>(GPR(rB)) & 127;
  u64 r = std::rotl<u64>(regS, 64 - (n & 63));
  u64 m = (n & 0x40) ? 0 : QMASK(n, 63);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srwx(PPU_STATE *ppuState) {
  X_FORM_rS_rA_rB_RC;

  u32 n = static_cast<u32>(GPR(rB)) & 63;

  GPR(rA) = (n < 32) ? (GPR(rS) >> n) : 0;

  if (RC) {
    u32 CR = CRCompS(ppuState, GPR(rA), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfcx(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>::addBits(~RA, RB, 1, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfx(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  GPRi(rd) = RB - RA;

  if (_instr.rc) {
    u32 CR = CRCompS(ppuState, GPRi(rd), 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfex(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const u64 RB = GPRi(rb);

  const auto add = addResult<u64>::addBits(~RA, RB, XER_GET_CA, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.ra) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfzex(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);

  const auto add = addResult<u64>::addBits(~RA, 0, XER_GET_CA, curThread.SPR.MSR.SF);
  GPRi(rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.ra) {
    u32 CR = CRCompS(ppuState, add.result, 0);
    ppcUpdateCR(ppuState, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfic(PPU_STATE *ppuState) {
  const u64 RA = GPRi(ra);
  const s64 i = _instr.simm16;

  const auto add = addResult<u64>::addBits(~RA, i, 1, curThread.SPR.MSR.SF);
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
