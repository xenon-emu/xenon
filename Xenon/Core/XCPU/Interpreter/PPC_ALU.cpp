// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

#define GPR(x)        hCore->ppuThread[hCore->currentThread].GPR[x]
#define XER_SET_CA(v) hCore->ppuThread[hCore->currentThread].SPR.XER.CA = v
#define XER_GET_CA    hCore->ppuThread[hCore->currentThread].SPR.XER.CA
#define _instr        hCore->ppuThread[hCore->currentThread].CI

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

  // Straighforward ADD with flags
  addResult(T a, T b) :
    result(a + b), carry(result < a)
  {}

  // Straighforward ADC with flags
  addResult(T a, T b, bool c) :
    addResult(a, b)
  {
    addResult r(result, c);
    result = r.result;
    carry |= r.carry;
  }
};

static addResult<u64> add64Bits(u64 a, u64 b) {
  return { a, b };
}

static addResult<u64> add64Bits(u64 a, u64 b, bool c) {
  return { a, b, c };
}

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
  return (s128{ x } *s128{ y }) >> 64;
#endif
}

//
// Instruction definitions.
//

void PPCInterpreter::PPCInterpreter_addx(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const u64 RB = GPR(_instr.rb);

  GPR(_instr.rd) = RA + RB;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addex(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const u64 RB = GPR(_instr.rb);

  const auto add = add64Bits(RA, RB, XER_GET_CA);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, add.result, 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addi(PPU_STATE *hCore) {
  GPR(_instr.rd) = _instr.ra ? GPR(_instr.ra) + _instr.simm16 : _instr.simm16;
}

void PPCInterpreter::PPCInterpreter_addic(PPU_STATE *hCore) {
  const s64 ra = GPR(_instr.ra);
  const s64 i = _instr.simm16;

  const auto add = add64Bits(ra, i);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);

  // _rc
  if (hCore->ppuThread[hCore->currentThread].CI.main & 1) {
    u32 CR = CRCompS(hCore, add.result, 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_addis(PPU_STATE *hCore) {
  GPR(_instr.rd) = _instr.ra ? GPR(_instr.ra) + (_instr.simm16 * 65536) : (_instr.simm16 * 65536);
}

void PPCInterpreter::PPCInterpreter_addzex(PPU_STATE *hCore) {
  const u64 ra = GPR(_instr.ra);

  const auto add = add64Bits(ra, 0, XER_GET_CA);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, add.result, 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_andx(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) & GPR(_instr.rb);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_andcx(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) & ~GPR(_instr.rb);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_andi(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) & _instr.uimm16;

  u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
  ppcUpdateCR(hCore, 0, CR);
}

void PPCInterpreter::PPCInterpreter_andis(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) & (u64{ _instr.uimm16 } << 16);

  u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
  ppcUpdateCR(hCore, 0, CR);
}

void PPCInterpreter::PPCInterpreter_cmp(PPU_STATE *hCore) {
  X_FORM_BF_L_rA_rB;

  u32 CR;

  if (L) {
    CR = CRCompS64(hCore, GPR(rA), GPR(rB));
  } else {
    CR = CRCompS32(hCore, static_cast<s32>(GPR(rA)), static_cast<s32>(GPR(rB)));
  }

  ppcUpdateCR(hCore, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cmpi(PPU_STATE *hCore) {
  D_FORM_BF_L_rA_SI;
  SI = EXTS(SI, 16);

  u32 CR;

  if (L) {
    CR = CRCompS64(hCore, GPR(rA), SI);
  } else {
    CR = CRCompS32(hCore, (s32)GPR(rA), (s32)SI);
  }

  ppcUpdateCR(hCore, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cmpl(PPU_STATE *hCore) {
  X_FORM_BF_L_rA_rB;

  u32 CR;

  if (L) {
    CR = CRCompU(hCore, GPR(rA), GPR(rB));
  } else {
    CR = CRCompU(hCore, (u32)GPR(rA), (u32)GPR(rB));
  }

  ppcUpdateCR(hCore, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cmpli(PPU_STATE *hCore) {
  D_FORM_BF_L_rA_UI;

  u32 CR;

  if (L) {
    CR = CRCompU(hCore, GPR(rA), UI);
  } else {
    CR = CRCompU(hCore, (u32)GPR(rA), UI);
  }

  ppcUpdateCR(hCore, BF, CR);
}

void PPCInterpreter::PPCInterpreter_cntlzdx(PPU_STATE *hCore) {
  GPR(_instr.ra) = std::countl_zero(GPR(_instr.rs));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_cntlzwx(PPU_STATE *hCore) {
  GPR(_instr.ra) = std::countl_zero(static_cast<u32>(GPR(_instr.rs)));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_crand(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crAnd = a & b;

  if (crAnd & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crandc(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crAndc = a & (1 ^ b);

  if (crAndc & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_creqv(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crEqv = 1 ^ (a ^ b);

  if (crEqv & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crnand(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crNand = 1 ^ (a & b);

  if (crNand & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crnor(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crNor = 1 ^ (a | b);

  if (crNor & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_cror(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;
  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crOr = a | b;

  if (crOr & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crorc(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crOrc = a | (1 ^ b);

  if (crOrc & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_crxor(PPU_STATE *hCore) {
  XL_FORM_BT_BA_BB;

  const u32 a = CR_GET(BA);
  const u32 b = CR_GET(BB);

  const u32 crXor = a ^ b;

  if (crXor & 1)
    BSET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
  else
    BCLR(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, 32, BT);
}

void PPCInterpreter::PPCInterpreter_divdx(PPU_STATE *hCore) {
  const s64 RA = GPR(_instr.ra);
  const s64 RB = GPR(_instr.rb);
  const bool o = RB == 0 || (RA == INT64_MIN && RB == -1);
  GPR(_instr.rd) = o ? 0 : RA / RB;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divdux(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const u64 RB = GPR(_instr.rb);
  GPR(_instr.rd) = RB == 0 ? 0 : RA / RB;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divwx(PPU_STATE *hCore) {
  const s32 RA = static_cast<s32>(GPR(_instr.ra));
  const s32 RB = static_cast<s32>(GPR(_instr.rb));
  const bool o = RB == 0 || (RA == INT32_MIN && RB == -1);
  GPR(_instr.rd) = o ? 0 : static_cast<u32>(RA / RB);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_divwux(PPU_STATE *hCore) {
  const u32 RA = static_cast<u32>(GPR(_instr.ra));
  const u32 RB = static_cast<u32>(GPR(_instr.rb));
  GPR(_instr.rd) = RB == 0 ? 0 : RA / RB;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_extsbx(PPU_STATE *hCore) {
  GPR(_instr.ra) = static_cast<s8>(GPR(_instr.rs));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_extshx(PPU_STATE *hCore) {
  GPR(_instr.ra) = static_cast<s16>(GPR(_instr.rs));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_extswx(PPU_STATE *hCore) {
  GPR(_instr.ra) = static_cast<s32>(GPR(_instr.rs));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mcrf(PPU_STATE *hCore) {
  XL_FORM_BF_BFA;

  u32 CR = DGET(hCore->ppuThread[hCore->currentThread].CR.CR_Hex, (BFA) * 4,
                (BFA) * 4 + 3);

  ppcUpdateCR(hCore, BF, CR);
}

void PPCInterpreter::PPCInterpreter_mfocrf(PPU_STATE *hCore) {
  XFX_FORM_rD;

  GPR(rD) = hCore->ppuThread[hCore->currentThread].CR.CR_Hex;
}

void PPCInterpreter::PPCInterpreter_mftb(PPU_STATE *hCore) {
  XFX_FORM_rD_spr; // because 5-bit fields are swapped

  switch (spr) {
  case 268:
    GPR(rD) = hCore->SPR.TB;
    break;
  case 269:
    GPR(rD) = HIDW(hCore->SPR.TB);
    break;

  default:
    LOG_CRITICAL(Xenon, "MFTB -> Illegal instruction form!");
    break;
  }
}

void PPCInterpreter::PPCInterpreter_mtocrf(PPU_STATE *hCore) {
  XFX_FORM_rS_FXM;
  u32 Mask = 0;
  u32 b = 0x80;

  for (; b; b >>= 1) {
    Mask <<= 4;

    if (FXM & b) {
      Mask |= 0xF;
    }
  }
  hCore->ppuThread[hCore->currentThread].CR.CR_Hex =
      ((u32)GPR(rS) & Mask) |
      (hCore->ppuThread[hCore->currentThread].CR.CR_Hex & ~Mask);
}

void PPCInterpreter::PPCInterpreter_mulli(PPU_STATE *hCore) {
  GPR(_instr.rd) = static_cast<s64>(GPR(_instr.ra)) * _instr.simm16;
}

void PPCInterpreter::PPCInterpreter_mulldx(PPU_STATE *hCore) {
  const s64 RA = GPR(_instr.ra);
  const s64 RB = GPR(_instr.rb);
  GPR(_instr.rd) = RA * RB;

  if (_instr.rc) {
      u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
      ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mullwx(PPU_STATE *hCore) {
  GPR(_instr.rd) = s64{ static_cast<s32>(GPR(_instr.ra)) } *static_cast<s32>(GPR(_instr.rb));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mulhwux(PPU_STATE *hCore) {
  u32 a = static_cast<u32>(GPR(_instr.ra));
  u32 b = static_cast<u32>(GPR(_instr.rb));
  GPR(_instr.rd) = (u64{ a } *b) >> 32;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_mulhdux(PPU_STATE *hCore) {
  GPR(_instr.rd) = umulh64(GPR(_instr.ra), GPR(_instr.rb));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_nandx(PPU_STATE *hCore) {
  GPR(_instr.ra) = ~(GPR(_instr.rs) & GPR(_instr.rb));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_negx(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  GPR(_instr.rd) = 0 - RA;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_norx(PPU_STATE *hCore) {
  GPR(_instr.ra) = ~(GPR(_instr.rs) | GPR(_instr.rb));

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_orcx(PPU_STATE* hCore)
{
  GPR(_instr.ra) = GPR(_instr.rs) | ~GPR(_instr.rb);

  if (_instr.rc) {
        u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
        ppcUpdateCR(hCore, 0, CR);
    }
}

void PPCInterpreter::PPCInterpreter_ori(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) | _instr.uimm16;
}

void PPCInterpreter::PPCInterpreter_oris(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) | (u64{ _instr.uimm16 } << 16);
}

void PPCInterpreter::PPCInterpreter_orx(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) | GPR(_instr.rb);

  if (_instr.rc) {
    u32 CR = CRCompU(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldicx(PPU_STATE *hCore) {
  MD_FORM_rS_rA_sh_mb_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u32 e = 63 - sh;
  u64 m = QMASK(mb, e);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldcrx(PPU_STATE *hCore) {
  MDS_FORM_rS_rA_rB_me_RC;

  u64 qwRb = GPR(rB);
  u32 n = (u32)QGET(qwRb, 58, 63);
  u64 r = std::rotl<u64>(GPR(rS), n);
  u64 m = QMASK(0, me);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldiclx(PPU_STATE *hCore) {
  MD_FORM_rS_rA_sh_mb_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u64 m = QMASK(mb, 63);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldicrx(PPU_STATE *hCore) {
  MD_FORM_rS_rA_sh_me_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u64 m = QMASK(0, me);
  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rldimix(PPU_STATE *hCore) {
  MD_FORM_rS_rA_sh_mb_RC;

  u64 r = std::rotl<u64>(GPR(rS), sh);
  u32 e = 63 - sh;
  u64 m = QMASK(mb, e);

  GPR(rA) = (r & m) | (GPR(rA) & ~m);

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rlwimix(PPU_STATE *hCore) {
  M_FORM_rS_rA_SH_MB_ME_RC;

  u32 r = std::rotl<u32>((u32)GPR(rS), SH);
  u32 m = (MB <= ME) ? DMASK(MB, ME) : (DMASK(0, ME) | DMASK(MB, 31));

  GPR(rA) = (r & m) | ((u32)GPR(rA) & ~m);

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rlwnmx(PPU_STATE *hCore) {
  M_FORM_rS_rA_rB_MB_ME_RC;

  u32 m = (MB <= ME) ? DMASK(MB, ME) : (DMASK(0, ME) | DMASK(MB, 31));

  GPR(rA) = std::rotl<u32>((u32)GPR(rS), ((u32)GPR(rB)) & 31) & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_rlwinmx(PPU_STATE *hCore) {
  M_FORM_rS_rA_SH_MB_ME_RC;

  u32 m = (MB <= ME) ? DMASK(MB, ME) : (DMASK(0, ME) | DMASK(MB, 31));

  GPR(rA) = std::rotl<u32>((u32)GPR(rS), SH) & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_sldx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB_RC;

  u64 regB = GPR(rB);
  u32 n = (u32)QGET(regB, 58, 63);
  u64 r = std::rotl<u64>(GPR(rS), n);
  u64 m = QGET(regB, 57, 57) ? 0 : QMASK(0, 63 - n);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_slwx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB_RC;

  u32 n = (u32)(GPR(rB)) & 63;

  GPR(rA) = (n < 32) ? ((u32)(GPR(rS)) << n) : 0;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_sradx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB_RC;

  u64 regRS = GPR(rS);
  u32 n = (u32)GPR(rB) & 127;
  u64 r = std::rotl<u64>(regRS, 64 - (n & 63));
  u64 m = (n & 0x40) ? 0 : QMASK(n, 63);
  u64 s = BGET(regRS, 64, 0) ? QMASK(0, 63) : 0;

  GPR(rA) = (r & m) | (s & ~m);

  if (s && ((r & ~m) != 0))
    hCore->ppuThread[hCore->currentThread].SPR.XER.CA = 1;
  else
    hCore->ppuThread[hCore->currentThread].SPR.XER.CA = 0;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_sradix(PPU_STATE *hCore) {
  X_FORM_rS_rA_SH_XO_RC;

  SH |= (XO & 1) << 5;

  if (SH == 0) {
    GPR(rA) = GPR(rS);
    hCore->ppuThread[hCore->currentThread].SPR.XER.CA = 0;
  } else {
    u64 r = std::rotl<u64>(GPR(rS), 64 - SH);
    u64 m = QMASK(SH, 63);
    u64 s = BGET(GPR(rS), 64, 0);

    GPR(rA) = (r & m) | (((u64)(-(long long)s)) & ~m);

    if (s && (r & ~m) != 0)
      hCore->ppuThread[hCore->currentThread].SPR.XER.CA = 1;
    else
      hCore->ppuThread[hCore->currentThread].SPR.XER.CA = 0;
  }

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srawx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB_RC;

  u64 regRs = GPR(rS);
  u64 n = (u32)GPR(rB) & 63;
  u64 r = std::rotl<u32>(static_cast<u32>(regRs), 64 - (n & 31));
  u64 m = (n & 0x20) ? 0 : QMASK(n + 32, 63);
  u64 s = BGET(regRs, 32, 0) ? QMASK(0, 63) : 0;
  GPR(rA) = (r & m) | (s & ~m);

  if (s && (((u32)(r & ~m)) != 0))
    XER_SET_CA(1);
  else
    XER_SET_CA(0);

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srawix(PPU_STATE *hCore) {
  X_FORM_rS_rA_SH_RC;

  u64 rSReg = GPR(rS);
  u64 r = std::rotl<u32>(static_cast<u32>(rSReg), 64 - SH);
  u64 m = QMASK(SH + 32, 63);
  u64 s = BGET(rSReg, 32, 0) ? QMASK(0, 63) : 0;

  GPR(rA) = (r & m) | (s & ~m);

  if (s && (((u32)(r & ~m)) != 0))
    XER_SET_CA(1);
  else
    XER_SET_CA(0);

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srdx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB_RC;

  u64 regS = GPR(rS);
  u32 n = (u32)GPR(rB) & 127;
  u64 r = std::rotl<u64>(regS, 64 - (n & 63));
  u64 m = (n & 0x40) ? 0 : QMASK(n, 63);

  GPR(rA) = r & m;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_srwx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB_RC;

  u32 n = (u32)GPR(rB) & 63;

  GPR(rA) = (n < 32) ? (GPR(rS) >> n) : 0;

  if (RC) {
    u32 CR = CRCompS(hCore, GPR(rA), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfcx(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const u64 RB = GPR(_instr.rb);

  const auto add = add64Bits(~RA, RB, 1);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfx(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const u64 RB = GPR(_instr.rb);

  GPR(_instr.rd) = RB - RA;

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.rd), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfex(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const u64 RB = GPR(_instr.rb);

  const auto add = add64Bits(~RA, RB, XER_GET_CA);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.ra) {
    u32 CR = CRCompS(hCore, add.result, 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfzex(PPU_STATE* hCore) {
  const u64 RA = GPR(_instr.ra);

  const auto add = add64Bits(~RA, 0, XER_GET_CA);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);

  if (_instr.ra) {
    u32 CR = CRCompS(hCore, add.result, 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_subfic(PPU_STATE *hCore) {
  const u64 RA = GPR(_instr.ra);
  const s64 i = _instr.simm16;

  const auto add = add64Bits(~RA, i, 1);
  GPR(_instr.rd) = add.result;
  XER_SET_CA(add.carry);
}

void PPCInterpreter::PPCInterpreter_xorx(PPU_STATE* hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) ^ GPR(_instr.rb);

  if (_instr.rc) {
    u32 CR = CRCompS(hCore, GPR(_instr.ra), 0);
    ppcUpdateCR(hCore, 0, CR);
  }
}

void PPCInterpreter::PPCInterpreter_xori(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) ^ _instr.uimm16;
}

void PPCInterpreter::PPCInterpreter_xoris(PPU_STATE *hCore) {
  GPR(_instr.ra) = GPR(_instr.rs) ^ (u64{ _instr.uimm16 } << 16);
}
