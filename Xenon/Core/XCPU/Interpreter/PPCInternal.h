// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include <immintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#elif defined(ARCH_AARCH64)
#include <arm_neon.h>
#endif

#define PPC_OPC_RC 1
#define PPC_OPC_LK 1
#define PPC_OPC_AA (1 << 1)

#define HIDW(data) (static_cast<u64>(data) >> 32)
#define LODW(data) (static_cast<u32>(data))

#define PPC_OPC_TEMPL_A(opc, rD, rA, rB, RC) {                                 \
  rD = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  rB = ((opc) >> 11) & 0x1F;                                                   \
  RC = ((opc) >> 6) & 0x1F;                                                    \
}
#define PPC_OPC_TEMPL_B(opc, BO, BI, BD) {                                     \
  BO = ((opc) >> 21) & 0x1F;                                                   \
  BI = ((opc) >> 16) & 0x1F;                                                   \
  BD = (u32)(s32)(s16)((opc) & 0xfffc);                                        \
}
#define PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm) {                               \
  rD = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  imm = (u32)(s32)(s16)((opc) & 0xffff);                                       \
}
#define PPC_OPC_TEMPL_D_UImm(opc, rD, rA, imm) {                               \
  rD = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  imm = (opc) & 0xffff;                                                        \
}
#define PPC_OPC_TEMPL_D_Shift16(opc, rD, rA, imm) {                            \
  rD = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  imm = (opc) << 16;                                                           \
}
#define PPC_OPC_TEMPL_I(opc, LI) {                                             \
  LI = (opc) & 0x3fffffc;                                                      \
  if (LI & 0x02000000)                                                         \
    LI |= 0xfc000000;                                                          \
}
#define PPC_OPC_TEMPL_M(opc, rS, rA, SH, MB, ME) {                             \
  rS = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  SH = ((opc) >> 11) & 0x1F;                                                   \
  MB = ((opc) >> 6) & 0x1F;                                                    \
  ME = ((opc) >> 1) & 0x1F;                                                    \
}
#define PPC_OPC_TEMPL_X(opc, rS, rA, rB) {                                     \
  rS = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  rB = ((opc) >> 11) & 0x1F;                                                   \
}
#define PPC_OPC_TEMPL_XFX(opc, rS, CRM) {                                      \
  rS = ((opc) >> 21) & 0x1F;                                                   \
  CRM = ((opc) >> 12) & 0xff;                                                  \
}
#define PPC_OPC_TEMPL_XO(opc, rS, rA, rB) {                                    \
  rS = ((opc) >> 21) & 0x1F;                                                   \
  rA = ((opc) >> 16) & 0x1F;                                                   \
  rB = ((opc) >> 11) & 0x1F;                                                   \
}
#define PPC_OPC_TEMPL_XL(opc, BO, BI, BD) {                                    \
  BO = ((opc) >> 21) & 0x1F;                                                   \
  BI = ((opc) >> 16) & 0x1F;                                                   \
  BD = ((opc) >> 11) & 0x1F;                                                   \
}
#define PPC_OPC_TEMPL_XFL(opc, rB, FM) {                                       \
  rB = ((opc) >> 11) & 0x1F;                                                   \
  FM = ((opc) >> 17) & 0xff;                                                   \
}

#define PPC_OPC_TEMPL3_XO()                                                    \
  s32 rD, rA, rB;                                                              \
  rD = ((hCore->CI) >> 21) & 0x1F;                                             \
  rA = ((hCore->CI) >> 16) & 0x1F;                                             \
  rB = ((hCore->CI) >> 11) & 0x1F
#define PPC_OPC_TEMPL_X_CR()                                                   \
  s32 crD, crA, crB;                                                           \
  crD = ((hCore->CI) >> 21) & 0x1F;                                            \
  crA = ((hCore->CI) >> 16) & 0x1F;                                            \
  crB = ((hCore->CI) >> 11) & 0x1F

static constexpr u64 GetBits64(u64 data, s8 begin, s8 end) {
  const u64 mask = (0xFFFFFFFFFFFFFFFF << (63 - end));
  return data & mask;
}

static constexpr u32 ExtractBits(u32 input, u32 begin, u32 end) {
  return (input >> (32 - 1 - end)) & ((1 << (end - begin + 1)) - 1);
}

static constexpr u64 ExtractBits64(u32 input, u32 begin, u32 end) {
  return (input >> (64 - 1 - end)) & ((1 << (end - begin + 1)) - 1);
}

#define QMASK(b, e) ((0xFFFFFFFFFFFFFFFF << ((63 + (b)) - (e))) >> (b))
#define QGET(qw, b, e) ((static_cast<u64>(qw) & QMASK((b), (e))) >> (63 - (e)))
#define QSET(qw, b, e, qwSet)                                                  \
  (qw) &= ~QMASK(b, e);                                                        \
  (qw) |= (static_cast<u64>(qwSet) << (63 - (e))) & QMASK(b, e)

#define DMASK(b, e) (((0xFFFFFFFF << ((31 + (b)) - (e))) >> (b)))
#define DGET(dw, b, e) (((dw) & DMASK((b), (e))) >> (31 - (e)))
#define DSET(dw, b, e, dwSet)                                                  \
  (dw) &= ~DMASK(b, e);                                                        \
  (dw) |= ((dwSet) << (31 - (e))) & DMASK(b, e);

#define IFIELD(v, b, e)                                                        \
  const u32 v = DGET(_instr.opcode, b, e);
#define IFIELDQ(v, b, e)                                                       \
  const u64 v = DGET(_instr.opcode, b, e);

#define I_FORM_LI_AA_LK                                                        \
  IFIELD(LI, 6, 29);                                                           \
  IFIELD(AA, 30, 30);                                                          \
  IFIELD(LK, 31, 31);

#define B_FORM_BO_BI_BD_AA_LK                                                  \
  IFIELD(BO, 6, 10);                                                           \
  IFIELD(BI, 11, 15);                                                          \
  IFIELD(BD, 16, 29);                                                          \
  IFIELD(AA, 30, 30);                                                          \
  IFIELD(LK, 31, 31);

#define SC_FORM_LEV IFIELD(LEV, 20, 26);

#define D_FORM(a, b, c)                                                        \
  IFIELD(a, 6, 10);                                                            \
  IFIELD(b, 11, 15);                                                           \
  IFIELDQ(c, 16, 31);
#define D_FORM_rD_rA_D D_FORM(rD, rA, D)
#define D_FORM_rD_rA_SI D_FORM(rD, rA, SI)
#define D_FORM_rS_rA_D D_FORM(rS, rA, D)
#define D_FORM_FrS_rA_D D_FORM(FrS, rA, D)
#define D_FORM_rS_rA_UI D_FORM(rS, rA, UI)
#define D_FORM_BF_L_rA_SI                                                      \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(L, 10, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELDQ(SI, 16, 31);
#define D_FORM_BF_L_rA_UI                                                      \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(L, 10, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELDQ(UI, 16, 31);
#define D_FORM_TO_rA_SI D_FORM(TO, rA, SI)
#define D_FORM_FrD_rA_D D_FORM(FrD, rA, D)

#define DS_FORM(a, b, c)                                                       \
  IFIELD(a, 6, 10);                                                            \
  IFIELD(b, 11, 15);                                                           \
  IFIELDQ(c, 16, 29);
#define DS_FORM_rD_rA_DS DS_FORM(rD, rA, DS)
#define DS_FORM_rS_rA_DS DS_FORM(rS, rA, DS)

#define X_FORM(a, b, c)                                                        \
  IFIELD(a, 6, 10);                                                            \
  IFIELD(b, 11, 15);                                                           \
  IFIELD(c, 16, 20);
#define X_FORM_XO(a, b, c, d)                                                  \
  X_FORM(a, b, c);                                                             \
  IFIELD(d, 21, 30);
#define X_FORM_rD_rA_rB X_FORM(rD, rA, rB)
#define X_FORM_rD_rA_NB_XO X_FORM_XO(rD, rA, NB, XO)
#define X_FORM_rD_SR_XO                                                        \
  IFIELD(rD, 6, 10);                                                           \
  IFIELD(SR, 12, 15);                                                          \
  IFIELD(XO, 21, 30);
#define X_FORM_rD_rB_XO                                                        \
  IFIELD(rD, 6, 10);                                                           \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(XO, 21, 30);
#define X_FORM_rD IFIELD(rD, 6, 10);
#define X_FORM_rS_L                                                            \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(L, 15, 15);
#define X_FORM_rS_rA_rB X_FORM(rS, rA, rB);
#define X_FORM_rS_rA_rB_RC                                                     \
  X_FORM(rS, rA, rB);                                                          \
  IFIELD(RC, 31, 31);
#define X_FORM_rS_rA_NB_XO X_FORM_XO(rS, rA, NB, XO)
#define X_FORM_rS_rA_SH_RC                                                     \
  X_FORM(rS, rA, SH);                                                          \
  IFIELD(RC, 31, 31);
#define X_FORM_rS_rA_SH_XO_RC                                                  \
  X_FORM_XO(rS, rA, SH, XO);                                                   \
  IFIELD(RC, 31, 31);
#define X_FORM_rS_rA_RC                                                        \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(RC, 31, 31);
#define X_FORM_rS_SR                                                           \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(SR, 12, 15);
#define X_FORM_rS_rB                                                           \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rB, 16, 20);
#define X_FORM_rS IFIELD(rS, 6, 10);
#define X_FORM_BF_L_rA_rB                                                      \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(L, 10, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);
#define X_FORM_BF_FrA_FrB                                                      \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(FrA, 11, 15);                                                         \
  IFIELD(FrB, 16, 20);
#define X_FORM_BF_BFA_XO                                                       \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(BFA, 11, 13);                                                         \
  IFIELD(XO, 21, 30);
#define X_FORM_BF_U_XO_RC                                                      \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(U, 16, 19);                                                           \
  IFIELD(XO, 21, 30);                                                          \
  IFIELD(RC, 31, 31);
#define X_FORM_BF_XO                                                           \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(XO, 21, 30);
#define X_FORM_TH_rA_rB_XO                                                     \
  IFIELD(TH, 9, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(XO, 21, 30);
#define X_FORM_L_rB                                                            \
  IFIELD(L, 10, 10);                                                           \
  IFIELD(rB, 16, 20);
#define X_FORM_L IFIELD(L, 10, 10);
#define X_FORM_TO_rA_rB X_FORM(TO, rA, rB)
#define X_FORM_FrD_rA_rB_XO X_FORM_XO(FrD, rA, rB, XO)
#define X_FORM_FrD_FrB_RC                                                      \
  IFIELD(FrD, 6, 10);                                                          \
  IFIELD(FrB, 16, 20);                                                         \
  IFIELD(RC, 31, 31);
#define X_FORM_FrD_RC                                                          \
  IFIELD(FrD, 6, 10);                                                          \
  IFIELD(RC, 31, 31);
#define X_FORM_FrS_rA_rB_XO X_FORM_XO(FrS, rA, rB, XO)
#define X_FORM_BT_XO_RC                                                        \
  IFIELD(BT, 6, 10);                                                           \
  IFIELD(XO, 21, 30);                                                          \
  IFIELD(RC, 31, 31);
#define X_FORM_rA_rB                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);
#define X_FORM_rB IFIELD(rB, 16, 20);

#define XL_FORM_BT_BA_BB                                                       \
  IFIELD(BT, 6, 10);                                                           \
  IFIELD(BA, 11, 15);                                                          \
  IFIELD(BB, 16, 20);
#define XL_FORM_BO_BI_BH_LK                                                    \
  IFIELD(BO, 6, 10);                                                           \
  IFIELD(BI, 11, 15);                                                          \
  IFIELD(BH, 19, 20);                                                          \
  IFIELD(LK, 31, 31);
#define XL_FORM_BF_BFA                                                         \
  IFIELD(BF, 6, 8);                                                            \
  IFIELD(BFA, 11, 13);

#define XFX_FORM(a, b)                                                         \
  IFIELD(a, 6, 10);                                                            \
  IFIELD(b, 11, 20);
#define XFX_FORM_rD_spr                                                        \
  XFX_FORM(rD, spr_1);                                                         \
  const u32 spr = (spr_1 >> 5) | ((spr_1 << 5) & 0x3FF);
#define XFX_FORM_rD IFIELD(rD, 6, 10);
#define XFX_FORM_rD_FXM_XO                                                     \
  IFIELD(rD, 6, 10);                                                           \
  IFIELD(FXM, 12, 19);                                                         \
  IFIELD(XO, 21, 30);
#define XFX_FORM_rS_FXM                                                        \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(FXM, 12, 19);
#define XFX_FORM_rS_spr_XO                                                     \
  XFX_FORM(rS, spr_1);                                                         \
  const u32 spr = (spr_1 >> 5) | ((spr_1 << 5) & 0x3FF);

#define XFL_FORM_FLM_FrB_RC                                                    \
  IFIELD(FLM, 7, 14);                                                          \
  IFIELD(FrB, 16, 20);                                                         \
  IFIELD(RC, 31, 31);

#define XS_FORM_rS_rA_sh_XO_RC                                                 \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(sh_1, 16, 20);                                                        \
  IFIELD(XO, 21, 29);                                                          \
  IFIELD(sh_2, 30, 30);                                                        \
  IFIELD(RC, 31, 31);                                                          \
  const u32 sh = (sh_2 << 5) | sh_1;

#define XO_FORM_rD_rA_rB_RC                                                    \
  IFIELD(rD, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(RC, 31, 31);
#define XO_FORM_rD_rA_rB_RC                                                    \
  IFIELD(rD, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(RC, 31, 31);
#define XO_FORM_rD_rA_RC                                                       \
  IFIELD(rD, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(RC, 31, 31);

#define A_FORM_FrD_FrA_FrB_FRC_XO_RC                                           \
  IFIELD(FrD, 6, 10);                                                          \
  IFIELD(FrA, 11, 15);                                                         \
  IFIELD(FrB, 16, 20);                                                         \
  IFIELD(FRC, 21, 25);                                                         \
  IFIELD(XO, 26, 30);                                                          \
  IFIELD(RC, 31, 31);
#define A_FORM_FrD_FrA_FrB_RC                                                  \
  IFIELD(FrD, 6, 10);                                                          \
  IFIELD(FrA, 11, 15);                                                         \
  IFIELD(FrB, 16, 20);                                                         \
  IFIELD(RC, 31, 31);
#define A_FORM_FrD_FrA_FRC_XO_RC                                               \
  IFIELD(FrD, 6, 10);                                                          \
  IFIELD(FrA, 11, 15);                                                         \
  IFIELD(FRC, 21, 25);                                                         \
  IFIELD(XO, 26, 30);                                                          \
  IFIELD(RC, 31, 31);
#define A_FORM_FrD_FrB_XO_RC                                                   \
  IFIELD(FrD, 6, 10);                                                          \
  IFIELD(FrB, 16, 20);                                                         \
  IFIELD(XO, 26, 30);                                                          \
  IFIELD(RC, 31, 31);

#define M_FORM_rS_rA_rB_MB_ME_RC                                               \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(MB, 21, 25);                                                          \
  IFIELD(ME, 26, 30);                                                          \
  IFIELD(RC, 31, 31);
#define M_FORM_rS_rA_SH_MB_ME_RC                                               \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(SH, 16, 20);                                                          \
  IFIELD(MB, 21, 25);                                                          \
  IFIELD(ME, 26, 30);                                                          \
  IFIELD(RC, 31, 31);

#define MD_FORM_rS_rA_sh_mb_RC                                                 \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(sh_1, 16, 20);                                                        \
  IFIELD(mb_1, 21, 25);                                                        \
  IFIELD(mb_2, 26, 26);                                                        \
  IFIELD(sh_2, 30, 30);                                                        \
  IFIELD(RC, 31, 31);                                                          \
  const u32 sh = (sh_2 << 5) | sh_1;                                           \
  const u32 mb = (mb_2 << 5) | mb_1;
#define MD_FORM_rS_rA_sh_me_RC                                                 \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(sh_1, 16, 20);                                                        \
  IFIELD(me_1, 21, 25);                                                        \
  IFIELD(me_2, 26, 26);                                                        \
  IFIELD(sh_2, 30, 30);                                                        \
  IFIELD(RC, 31, 31);                                                          \
  const u32 sh = (sh_2 << 5) | sh_1;                                           \
  const u32 me = (me_2 << 5) | me_1;

#define MDS_FORM_rS_rA_rB_mb_RC                                                \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(mb_1, 21, 25);                                                        \
  IFIELD(mb_2, 26, 26);                                                        \
  IFIELD(RC, 31, 31);                                                          \
  const u32 mb = (mb_2 << 5) | mb_1;
#define MDS_FORM_rS_rA_rB_me_RC                                                \
  IFIELD(rS, 6, 10);                                                           \
  IFIELD(rA, 11, 15);                                                          \
  IFIELD(rB, 16, 20);                                                          \
  IFIELD(me_1, 21, 25);                                                        \
  IFIELD(me_2, 26, 26);                                                        \
  IFIELD(RC, 31, 31);                                                          \
  const u32 me = (me_2 << 5) | me_1;

#define VX_FORM(a, b, c)                                                       \
  IFIELD(a, 6, 10);                                                            \
  IFIELD(b, 11, 15);                                                           \
  IFIELD(c, 16, 20);
#define VX_FORM_rD_rA_rB VX_FORM(rD, rA, rB);
#define VX_FORM_rD IFIELD(rD, 6, 10);
#define VX_FORM_rB IFIELD(rB, 16, 20);

#define EXTS(qw, ib)                                                           \
  (((static_cast<u64>(qw)) & ((static_cast<u64>(1)) << ((ib) - 1)))            \
       ? ((static_cast<u64>(qw)) | QMASK(0, 63 - (ib)))                        \
       : (static_cast<u64>(qw)))

#define BMSK(w, i) ((static_cast<u64>(1)) << ((w) - (i) - (1)))

#define BGET(dw, w, i) (((dw) & BMSK(w, i)) ? 1 : 0)
#define BSET(dw, w, i) (dw) |= BMSK(w, i)
#define BCLR(dw, w, i) (dw) &= ~BMSK(w, i)

#define BO_GET(i) BGET(BO, 5, i)

#define CR_GET(i) BGET(curThread.CR.CR_Hex, 32, i)
#define CR_GETi(i) BGET(curThread.CR.CR_Hex, 32, _instr.cr##i)
#define CR_SET(i) BSET(curThread.CR.CR_Hex, 32, i)
#define CR_CLR(i) BCLR(curThread.CR.CR_Hex, 32, i)

#define CR_BIT_LT 0
#define CR_BIT_GT 1
#define CR_BIT_EQ 2
#define CR_BIT_SO 3

// VMX Bitfields.
// Sources:
// https://github.com/kakaroto/ps3ida/blob/master/plugins/PPCAltivec/src/main.cpp
// http://biallas.net/doc/vmx128/vmx128.txt

#define VMX128_VD128   (_instr.VMX128.VD128l | (_instr.VMX128.VD128h << 5))
#define VMX128_VA128   (_instr.VMX128.VA128l | (_instr.VMX128.VA128h << 5) | (_instr.VMX128.VA128H << 6))
#define VMX128_VB128   (_instr.VMX128.VB128l | (_instr.VMX128.VB128h << 5))

#define VMX128_1_VD128 (_instr.VMX128_1.VD128l | (_instr.VMX128_1.VD128h << 5))

#define VMX128_2_VD128 (_instr.VMX128_2.VD128l | (_instr.VMX128_2.VD128h << 5))
#define VMX128_2_VA128 (_instr.VMX128_2.VA128l | (_instr.VMX128_2.VA128h << 5) | (_instr.VMX128_2.VA128H << 6))
#define VMX128_2_VB128 (_instr.VMX128_2.VB128l | (_instr.VMX128_2.VB128h << 5))
#define VMX128_2_VC    (_instr.VMX128_2.VC)

#define VMX128_3_VD128 (_instr.VMX128_3.VD128l | (_instr.VMX128_3.VD128h << 5))
#define VMX128_3_VB128 (_instr.VMX128_3.VB128l | (_instr.VMX128_3.VB128h << 5))
#define VMX128_3_IMM   (_instr.VMX128_3.IMM)

#define VMX128_5_VD128 (_instr.VMX128_5.VD128l | (_instr.VMX128_5.VD128h << 5))
#define VMX128_5_VA128 (_instr.VMX128_5.VA128l | (_instr.VMX128_5.VA128h << 5)) | (_instr.VMX128_5.VA128H << 6)
#define VMX128_5_VB128 (_instr.VMX128_5.VB128l | (_instr.VMX128_5.VB128h << 5))
#define VMX128_5_SH    (_instr.VMX128_5.SH)

#define VMX128_R_VD128 (_instr.VMX128_R.VD128l | (_instr.VMX128_R.VD128h << 5))
#define VMX128_R_VA128 (_instr.VMX128_R.VA128l | (_instr.VMX128_R.VA128h << 5) | (_instr.VMX128_R.VA128H << 6))
#define VMX128_R_VB128 (_instr.VMX128_R.VB128l | (_instr.VMX128_R.VB128h << 5))
