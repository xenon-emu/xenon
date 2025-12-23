/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <array>
#include <initializer_list>

#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "PPCTranslator.h"

namespace Xe::XCPU::JIT {

  //=============================================================================
  // Instruction Handler Type Definition
  //=============================================================================

  // Type alias for IR translation handler functions
  using IRTranslatorHandler = bool (*)(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);

  //=============================================================================
  // Stub Handler - Used for unimplemented instructions
  //=============================================================================

  inline bool IRTranslate_invalid(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {
    LOG_WARNING(JIT, "IR Translator: Unimplemented instruction at {:#x}, opcode={:#x}",
      ctx.currentAddress, instr.opcode);
    return false;
  }

  // NOP Handler

  inline bool IRTranslate_nop(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {
    return true;
  }

  // Instruction Translation Stubs

  // bool IRTranslate_tdi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_twi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulli(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfic(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cmpli(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cmpi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addic(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addis(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_bc(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sc(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_b(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rlwimix(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rlwinmx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rlwnmx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ori(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_oris(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_xori(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_xoris(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_andi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_andis(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwz(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwzu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lbz(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lbzu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stw(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stwu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stb(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stbu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhz(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhzu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lha(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhau(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sth(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sthu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lmw(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stmw(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfs(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfsu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfd(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfdu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfs(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfsu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfd(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfdu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mcrf(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  bool IRTranslate_bclr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rfid(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_crnor(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_crandc(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_isync(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_crxor(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_crnand(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_crand(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_creqv(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_crorc(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cror(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_bcctr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rldiclx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rldicrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rldicx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rldimix(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rldclx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_rldcrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cmp(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_tw(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvsl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvebx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfcx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfcox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulhdux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addcx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addcox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulhwux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mfocrf(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwarx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ldx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwzx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_slwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cntlzwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sldx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_andx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cmpl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvsr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvehx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ldux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dcbst(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwzux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_cntlzdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_andcx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_td(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvewx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulhdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulhwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mfmsr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ldarx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dcbf(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lbzx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_negx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_negox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lbzux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_norx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvebx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfeox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addeox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtocrf(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtmsr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stwcx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvehx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtmsrd(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stdux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stwux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvewx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfzex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfzeox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addzex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addzeox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stdcx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stbx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfmex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_subfmeox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulldx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mulldox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addmex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addmeox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mullwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mullwox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dcbtst(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stbux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  bool IRTranslate_addx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_addox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dcbt(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhzx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_eqvx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_tlbiel(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_tlbie(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_eciwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhzux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_xorx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mfspr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwax(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dst(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhax(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvxl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mftb(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwaux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dstst(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhaux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_slbmte(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sthx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_orcx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_slbie(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ecowx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sthux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_orx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divdux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divduox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divwux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divwuox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtspr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dcbi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_nandx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_slbia(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvxl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divdox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_divwox(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvlx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ldbrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lswx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwbrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_srwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_srdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_tlbsync(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfsux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mfsrin(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mfsr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lswi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sync(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lfdux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvlx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stdbrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stswx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stwbrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfsux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stswi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfdx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfdux(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvlxl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lhbrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_srawx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sradx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lvrxl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dss(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_srawix(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sradix(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_slbmfev(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_eieio(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvlxl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_slbmfee(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_sthbrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_extshx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stvrxl(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_extsbx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stfiwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_extswx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_icbi(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_dcbz(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ld(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_ldu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_lwa(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fdivsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fsubsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_faddsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fsqrtsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fresx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmulsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmsubsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmaddsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fnmsubsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fnmaddsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_std(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_stdu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtfsb1x(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mcrfs(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtfsb0x(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtfsfiq(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mffsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_mtfsfx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fcmpu(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_frspx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fctiwx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fctiwzx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fdivx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fsubx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_faddx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fsqrtx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fselx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmulx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_frsqrtex(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmsubx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmaddx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fnmsubx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fnmaddx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fcmpo(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fnegx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fmrx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fnabsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fabsx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fctidx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fctidzx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);
  // bool IRTranslate_fcfidx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr);

}