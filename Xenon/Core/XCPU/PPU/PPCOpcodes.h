/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "PPCInternal.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/JIT/PPU_JIT.h"

namespace PPCInterpreter {
//
// Instruction definitions
//

#define D_STUB(name) static inline void PPCInterpreter_##name(sPPEState *ppeState) { return PPCInterpreter_known_unimplemented(#name, ppeState); }
#define D_STUBRC(name) static inline void PPCInterpreter_##name##x(sPPEState *ppeState) { return PPCInterpreter_known_unimplemented(#name "x", ppeState); }

// Stubs
extern void PPCInterpreter_invalid(sPPEState *ppeState);
extern void PPCInterpreter_known_unimplemented(const char* name, sPPEState *ppeState);

D_STUBRC(mtfsfi)
D_STUBRC(fres)
D_STUB(mfsrin)
D_STUB(mfsr)
D_STUB(lvsl128)
D_STUB(stvxl128)
D_STUB(lvlxl128)
D_STUB(lvrxl128)
D_STUB(stvrxl128)
D_STUB(lvsr128)
D_STUB(lvrxl)
D_STUB(lvlxl)
D_STUB(lswx)
D_STUB(stdbrx)
D_STUB(stswx)
D_STUB(stvebx)
D_STUB(stvrxl)
D_STUB(stvlxl)
D_STUB(stvehx)
D_STUB(eciwx)
D_STUB(ecowx)
D_STUB(slbmfev)
D_STUB(slbmfee)

// Vector-instructions
D_STUB(vaddubm)
D_STUB(vmaxub)
D_STUB(vrlb)
D_STUB(vcmpequb)
D_STUB(vcmpequb_)
D_STUB(vmuloub)
D_STUB(vpkuhum)
D_STUB(vmhaddshs)
D_STUB(vmhraddshs)
D_STUB(vmladduhm)
D_STUB(vmsumubm)
D_STUB(vmsummbm)
D_STUB(vmsumuhm)
D_STUB(vmsumuhs)
D_STUB(vmsumshm)
D_STUB(vmsumshs)
D_STUB(vcmpequh)
D_STUB(vcmpequh_)
D_STUB(vmulouh)
D_STUB(vsubfp)
D_STUB(vpkuwum)
D_STUB(vadduwm)
D_STUB(vrlw)
D_STUB(vpkuhus)
D_STUB(vpkuwus)
D_STUB(vmaxsb)
D_STUB(vmulosb)
D_STUB(vpkshus)
D_STUB(vmulosh)
D_STUB(vpkswus)
D_STUB(vaddcuw)
D_STUB(vpkshss)
D_STUB(vcmpgefp)
D_STUB(vcmpgefp_)
D_STUB(vlogefp)
D_STUB(vpkswss)
D_STUB(vminub)
D_STUB(vsrb)
D_STUB(vcmpgtub)
D_STUB(vcmpgtub_)
D_STUB(vmuleub)
D_STUB(vupkhsb)
D_STUB(vadduhs)
D_STUB(vcmpgtuh)
D_STUB(vcmpgtuh_)
D_STUB(vmuleuh)
D_STUB(vrfiz)
D_STUB(vupkhsh)
D_STUB(vcmpgtuw)
D_STUB(vcmpgtuw_)
D_STUB(vrfip)
D_STUB(vupklsb)
D_STUB(vcmpgtfp)
D_STUB(vcmpgtfp_)
D_STUB(vrfim)
D_STUB(vupklsh)
D_STUB(vaddsbs)
D_STUB(vminsb)
D_STUB(vsrab)
D_STUB(vcmpgtsb)
D_STUB(vcmpgtsb_)
D_STUB(vmulesb)
D_STUB(vpkpx)
D_STUB(vcmpgtsh)
D_STUB(vcmpgtsh_)
D_STUB(vmulesh)
D_STUB(vupkhpx)
D_STUB(vaddsws)
D_STUB(vminsw)
D_STUB(vsraw)
D_STUB(vcmpgtsw)
D_STUB(vcmpgtsw_)
D_STUB(vupklpx)
D_STUB(vsububm)
D_STUB(vavgub)
D_STUB(vsubuhm)
D_STUB(vsro)
D_STUB(vsubuwm)
D_STUB(vavguw)
D_STUB(vavgsb)
D_STUB(vavgsh)
D_STUB(vsubcuw)
D_STUB(vavgsw)
D_STUB(vsububs)
D_STUB(vsum4ubs)
D_STUB(vsubuhs)
D_STUB(vsum4shs)
D_STUB(vsubuws)
D_STUB(vsum2sws)
D_STUB(vsubsbs)
D_STUB(vsum4sbs)
D_STUB(vsubshs)
D_STUB(vsubsws)
D_STUB(vsumsws)
D_STUB(vmaddfp128)
D_STUB(vpkshss128)
D_STUB(vpkshus128)
D_STUB(vpkswss128)
D_STUB(vnor128)
D_STUB(vpkswus128)
D_STUB(vpkuhum128)
D_STUB(vpkuhus128)
D_STUB(vpkuwum128)
D_STUB(vslo128)
D_STUB(vpkuwus128)
D_STUB(vsro128)
D_STUB(vpkd3d128)
D_STUB(vcfpuxws128)
D_STUB(vcuxwfp128)
D_STUB(vrfim128)
D_STUB(vrfip128)
D_STUB(vrfiz128)
D_STUB(vlogefp128)
D_STUB(vcmpgtfp128)
D_STUB(vrlw128)
D_STUB(vupkhsb128)
D_STUB(vupklsb128)

//
// ALU
//
extern void PPCInterpreter_addx(sPPEState *ppeState);
extern void PPCInterpreter_addox(sPPEState *ppeState);
extern void PPCInterpreter_addcx(sPPEState *ppeState);
extern void PPCInterpreter_addcox(sPPEState *ppeState);
extern void PPCInterpreter_addex(sPPEState *ppeState);
extern void PPCInterpreter_addeox(sPPEState *ppeState);
extern void PPCInterpreter_addi(sPPEState *ppeState);
extern void PPCInterpreter_addic(sPPEState *ppeState);
extern void PPCInterpreter_addis(sPPEState *ppeState);
extern void PPCInterpreter_addmex(sPPEState *ppeState);
extern void PPCInterpreter_addmeox(sPPEState *ppeState);
extern void PPCInterpreter_addzex(sPPEState *ppeState);
extern void PPCInterpreter_addzeox(sPPEState *ppeState);
extern void PPCInterpreter_andx(sPPEState *ppeState);
extern void PPCInterpreter_andcx(sPPEState *ppeState);
extern void PPCInterpreter_andi(sPPEState *ppeState);
extern void PPCInterpreter_andis(sPPEState *ppeState);
extern void PPCInterpreter_cmp(sPPEState *ppeState);
extern void PPCInterpreter_cmpi(sPPEState *ppeState);
extern void PPCInterpreter_cmpl(sPPEState *ppeState);
extern void PPCInterpreter_cmpli(sPPEState *ppeState);
extern void PPCInterpreter_cntlzdx(sPPEState *ppeState);
extern void PPCInterpreter_cntlzwx(sPPEState *ppeState);
extern void PPCInterpreter_crand(sPPEState *ppeState);
extern void PPCInterpreter_crandc(sPPEState *ppeState);
extern void PPCInterpreter_creqv(sPPEState *ppeState);
extern void PPCInterpreter_crnand(sPPEState *ppeState);
extern void PPCInterpreter_crnor(sPPEState *ppeState);
extern void PPCInterpreter_cror(sPPEState *ppeState);
extern void PPCInterpreter_crorc(sPPEState *ppeState);
extern void PPCInterpreter_crxor(sPPEState *ppeState);
extern void PPCInterpreter_divdx(sPPEState *ppeState);
extern void PPCInterpreter_divdux(sPPEState *ppeState);
extern void PPCInterpreter_divdox(sPPEState *ppeState);
extern void PPCInterpreter_divduox(sPPEState *ppeState);
extern void PPCInterpreter_divwx(sPPEState *ppeState);
extern void PPCInterpreter_divwox(sPPEState *ppeState);
extern void PPCInterpreter_divwux(sPPEState *ppeState);
extern void PPCInterpreter_divwuox(sPPEState *ppeState);
extern void PPCInterpreter_isync(sPPEState *ppeState);
extern void PPCInterpreter_eqvx(sPPEState *ppeState);
extern void PPCInterpreter_extsbx(sPPEState *ppeState);
extern void PPCInterpreter_extshx(sPPEState *ppeState);
extern void PPCInterpreter_extswx(sPPEState *ppeState);
extern void PPCInterpreter_mcrf(sPPEState *ppeState);
extern void PPCInterpreter_mfocrf(sPPEState *ppeState);
extern void PPCInterpreter_mftb(sPPEState *ppeState);
extern void PPCInterpreter_mtocrf(sPPEState *ppeState);
extern void PPCInterpreter_mulli(sPPEState *ppeState);
extern void PPCInterpreter_mulldx(sPPEState *ppeState);
extern void PPCInterpreter_mulldox(sPPEState *ppeState);
extern void PPCInterpreter_mullwx(sPPEState *ppeState);
extern void PPCInterpreter_mullwox(sPPEState *ppeState);
extern void PPCInterpreter_mulhdx(sPPEState *ppeState);
extern void PPCInterpreter_mulhwx(sPPEState *ppeState);
extern void PPCInterpreter_mulhwux(sPPEState *ppeState);
extern void PPCInterpreter_mulhdux(sPPEState *ppeState);
extern void PPCInterpreter_nandx(sPPEState *ppeState);
extern void PPCInterpreter_negx(sPPEState *ppeState);
extern void PPCInterpreter_negox(sPPEState *ppeState);
extern void PPCInterpreter_norx(sPPEState *ppeState);
extern void PPCInterpreter_orcx(sPPEState *ppeState);
extern void PPCInterpreter_ori(sPPEState *ppeState);
extern void PPCInterpreter_oris(sPPEState *ppeState);
extern void PPCInterpreter_orx(sPPEState *ppeState);
extern void PPCInterpreter_rldicx(sPPEState *ppeState);
extern void PPCInterpreter_rldclx(sPPEState *ppeState);
extern void PPCInterpreter_rldcrx(sPPEState *ppeState);
extern void PPCInterpreter_rldiclx(sPPEState *ppeState);
extern void PPCInterpreter_rldicrx(sPPEState *ppeState);
extern void PPCInterpreter_rldimix(sPPEState *ppeState);
extern void PPCInterpreter_rlwimix(sPPEState *ppeState);
extern void PPCInterpreter_rlwnmx(sPPEState *ppeState);
extern void PPCInterpreter_rlwinmx(sPPEState *ppeState);
extern void PPCInterpreter_sldx(sPPEState *ppeState);
extern void PPCInterpreter_slwx(sPPEState *ppeState);
extern void PPCInterpreter_sradx(sPPEState *ppeState);
extern void PPCInterpreter_sradix(sPPEState *ppeState);
extern void PPCInterpreter_srawx(sPPEState *ppeState);
extern void PPCInterpreter_srawix(sPPEState *ppeState);
extern void PPCInterpreter_srdx(sPPEState *ppeState);
extern void PPCInterpreter_srwx(sPPEState *ppeState);
extern void PPCInterpreter_subfcx(sPPEState *ppeState);
extern void PPCInterpreter_subfcox(sPPEState *ppeState);
extern void PPCInterpreter_subfx(sPPEState *ppeState);
extern void PPCInterpreter_subfox(sPPEState *ppeState);
extern void PPCInterpreter_subfex(sPPEState *ppeState);
extern void PPCInterpreter_subfeox(sPPEState *ppeState);
extern void PPCInterpreter_subfmex(sPPEState *ppeState);
extern void PPCInterpreter_subfmeox(sPPEState *ppeState);
extern void PPCInterpreter_subfzex(sPPEState *ppeState);
extern void PPCInterpreter_subfzeox(sPPEState *ppeState);
extern void PPCInterpreter_subfic(sPPEState *ppeState);
extern void PPCInterpreter_xori(sPPEState *ppeState);
extern void PPCInterpreter_xoris(sPPEState *ppeState);
extern void PPCInterpreter_xorx(sPPEState *ppeState);

// Program control
extern void PPCInterpreter_b(sPPEState *ppeState);
extern void PPCInterpreter_bc(sPPEState *ppeState);
extern void PPCInterpreter_bcctr(sPPEState *ppeState);
extern void PPCInterpreter_bclr(sPPEState *ppeState);

// System instructions
extern void PPCInterpreter_eieio(sPPEState *ppeState);
extern void PPCInterpreter_sc(sPPEState *ppeState);
extern void PPCInterpreter_slbia(sPPEState *ppeState);
extern void PPCInterpreter_slbie(sPPEState *ppeState);
extern void PPCInterpreter_slbmte(sPPEState *ppeState);
extern void PPCInterpreter_rfid(sPPEState *ppeState);
extern void PPCInterpreter_tw(sPPEState *ppeState);
extern void PPCInterpreter_twi(sPPEState *ppeState);
extern void PPCInterpreter_td(sPPEState *ppeState);
extern void PPCInterpreter_tdi(sPPEState *ppeState);
extern void PPCInterpreter_tlbie(sPPEState *ppeState);
extern void PPCInterpreter_tlbiel(sPPEState *ppeState);
extern void PPCInterpreter_tlbsync(sPPEState *ppeState);
extern void PPCInterpreter_mfspr(sPPEState *ppeState);
extern void PPCInterpreter_mtspr(sPPEState *ppeState);
extern void PPCInterpreter_mfmsr(sPPEState *ppeState);
extern void PPCInterpreter_mtmsr(sPPEState *ppeState);
extern void PPCInterpreter_mtmsrd(sPPEState *ppeState);
extern void PPCInterpreter_sync(sPPEState *ppeState);

// Cache Management
extern void PPCInterpreter_dcbf(sPPEState *ppeState);
extern void PPCInterpreter_dcbi(sPPEState *ppeState);
extern void PPCInterpreter_dcbt(sPPEState *ppeState);
extern void PPCInterpreter_dcbtst(sPPEState *ppeState);
extern void PPCInterpreter_dcbst(sPPEState *ppeState);
extern void PPCInterpreter_dcbz(sPPEState *ppeState);
extern void PPCInterpreter_icbi(sPPEState *ppeState);

//
// FPU
//
extern void PPCInterpreter_mcrfs(sPPEState *ppeState);
extern void PPCInterpreter_faddx(sPPEState *ppeState);
extern void PPCInterpreter_fabsx(sPPEState *ppeState);
extern void PPCInterpreter_faddsx(sPPEState *ppeState);
extern void PPCInterpreter_fcmpu(sPPEState *ppeState);
extern void PPCInterpreter_fcmpo(sPPEState *ppeState);
extern void PPCInterpreter_fctiwx(sPPEState *ppeState);
extern void PPCInterpreter_fctidx(sPPEState *ppeState);
extern void PPCInterpreter_fctidzx(sPPEState *ppeState);
extern void PPCInterpreter_fctiwzx(sPPEState *ppeState);
extern void PPCInterpreter_fcfidx(sPPEState *ppeState);
extern void PPCInterpreter_fdivx(sPPEState *ppeState);
extern void PPCInterpreter_fdivsx(sPPEState *ppeState);
extern void PPCInterpreter_fmaddx(sPPEState *ppeState);
extern void PPCInterpreter_fmaddsx(sPPEState *ppeState);
extern void PPCInterpreter_fmulx(sPPEState *ppeState);
extern void PPCInterpreter_fmulsx(sPPEState *ppeState);
extern void PPCInterpreter_fmrx(sPPEState *ppeState);
extern void PPCInterpreter_fmsubx(sPPEState *ppeState);
extern void PPCInterpreter_fmsubsx(sPPEState *ppeState);
extern void PPCInterpreter_fnabsx(sPPEState *ppeState);
extern void PPCInterpreter_fnmaddx(sPPEState *ppeState);
extern void PPCInterpreter_fnmaddsx(sPPEState *ppeState);
extern void PPCInterpreter_fnegx(sPPEState *ppeState);
extern void PPCInterpreter_fnmsubx(sPPEState *ppeState);
extern void PPCInterpreter_fnmsubsx(sPPEState *ppeState);
extern void PPCInterpreter_frspx(sPPEState *ppeState);
extern void PPCInterpreter_fsubx(sPPEState *ppeState);
extern void PPCInterpreter_fselx(sPPEState *ppeState);
extern void PPCInterpreter_fsubsx(sPPEState *ppeState);
extern void PPCInterpreter_fsqrtx(sPPEState *ppeState);
extern void PPCInterpreter_fsqrtsx(sPPEState *ppeState);
extern void PPCInterpreter_frsqrtex(sPPEState *ppeState);
extern void PPCInterpreter_mffsx(sPPEState *ppeState);
extern void PPCInterpreter_mtfsfx(sPPEState *ppeState);
extern void PPCInterpreter_mtfsb0x(sPPEState *ppeState);
extern void PPCInterpreter_mtfsb1x(sPPEState *ppeState);

//
// VXU
//
extern void PPCInterpreter_dss(sPPEState *ppeState);
extern void PPCInterpreter_dst(sPPEState *ppeState);
extern void PPCInterpreter_dstst(sPPEState *ppeState);
extern void PPCInterpreter_mfvscr(sPPEState *ppeState);
extern void PPCInterpreter_mtvscr(sPPEState *ppeState);
extern void PPCInterpreter_vaddfp(sPPEState *ppeState);
extern void PPCInterpreter_vaddfp128(sPPEState *ppeState);
extern void PPCInterpreter_vaddubs(sPPEState *ppeState);
extern void PPCInterpreter_vadduhm(sPPEState *ppeState);
extern void PPCInterpreter_vadduws(sPPEState *ppeState);
extern void PPCInterpreter_vaddshs(sPPEState *ppeState);
extern void PPCInterpreter_vavguh(sPPEState *ppeState);
extern void PPCInterpreter_vand(sPPEState *ppeState);
extern void PPCInterpreter_vand128(sPPEState *ppeState);
extern void PPCInterpreter_vandc(sPPEState *ppeState);
extern void PPCInterpreter_vandc128(sPPEState *ppeState);
extern void PPCInterpreter_vctsxs(sPPEState *ppeState);
extern void PPCInterpreter_vctuxs(sPPEState *ppeState);
extern void PPCInterpreter_vcfsx(sPPEState *ppeState);
extern void PPCInterpreter_vcfux(sPPEState *ppeState);
extern void PPCInterpreter_vcmpbfp(sPPEState *ppeState);
extern void PPCInterpreter_vcmpbfp128(sPPEState *ppeState);
extern void PPCInterpreter_vcmpeqfp(sPPEState *ppeState);
extern void PPCInterpreter_vcmpeqfp128(sPPEState *ppeState);
extern void PPCInterpreter_vcmpequwx(sPPEState *ppeState);
extern void PPCInterpreter_vcmpequw128(sPPEState *ppeState);
extern void PPCInterpreter_vcmpgefp128(sPPEState *ppeState);
extern void PPCInterpreter_vcsxwfp128(sPPEState *ppeState);
extern void PPCInterpreter_vcfpsxws128(sPPEState *ppeState);
extern void PPCInterpreter_vexptefp(sPPEState *ppeState);
extern void PPCInterpreter_vexptefp128(sPPEState *ppeState);
extern void PPCInterpreter_vnmsubfp(sPPEState *ppeState);
extern void PPCInterpreter_vnor(sPPEState *ppeState);
extern void PPCInterpreter_vor(sPPEState *ppeState);
extern void PPCInterpreter_vor128(sPPEState *ppeState);
extern void PPCInterpreter_vspltw(sPPEState *ppeState);
extern void PPCInterpreter_vmaxuw(sPPEState *ppeState);
extern void PPCInterpreter_vmaxsh(sPPEState* ppeState);
extern void PPCInterpreter_vmaxuh(sPPEState* ppeState);
extern void PPCInterpreter_vmaxsw(sPPEState* ppeState);
extern void PPCInterpreter_vminsh(sPPEState *ppeState);
extern void PPCInterpreter_vminuh(sPPEState *ppeState);
extern void PPCInterpreter_vminuw(sPPEState *ppeState);
extern void PPCInterpreter_vmaxfp(sPPEState *ppeState);
extern void PPCInterpreter_vmaxfp128(sPPEState *ppeState);
extern void PPCInterpreter_vminfp(sPPEState *ppeState);
extern void PPCInterpreter_vminfp128(sPPEState *ppeState);
extern void PPCInterpreter_vmaddfp(sPPEState *ppeState);
extern void PPCInterpreter_vmulfp128(sPPEState *ppeState);
extern void PPCInterpreter_vmaddcfp128(sPPEState* ppeState);
extern void PPCInterpreter_vmrghb(sPPEState *ppeState);
extern void PPCInterpreter_vmrghh(sPPEState *ppeState);
extern void PPCInterpreter_vmrghw(sPPEState *ppeState);
extern void PPCInterpreter_vmrghw128(sPPEState* ppeState);
extern void PPCInterpreter_vmrglb(sPPEState *ppeState);
extern void PPCInterpreter_vmrglh(sPPEState *ppeState);
extern void PPCInterpreter_vmrglw(sPPEState *ppeState);
extern void PPCInterpreter_vmrglw128(sPPEState *ppeState);
extern void PPCInterpreter_vnmsubfp128(sPPEState* ppeState);
extern void PPCInterpreter_vperm(sPPEState *ppeState);
extern void PPCInterpreter_vperm128(sPPEState *ppeState);
extern void PPCInterpreter_vpermwi128(sPPEState *ppeState);
extern void PPCInterpreter_vrlh(sPPEState *ppeState);
extern void PPCInterpreter_vrlimi128(sPPEState *ppeState);
extern void PPCInterpreter_vrfin(sPPEState *ppeState);
extern void PPCInterpreter_vrfin128(sPPEState *ppeState);
extern void PPCInterpreter_vrefp(sPPEState *ppeState);
extern void PPCInterpreter_vrefp128(sPPEState *ppeState);
extern void PPCInterpreter_vrsqrtefp(sPPEState *ppeState);
extern void PPCInterpreter_vrsqrtefp128(sPPEState *ppeState);
extern void PPCInterpreter_vsel(sPPEState *ppeState);
extern void PPCInterpreter_vsel128(sPPEState *ppeState);
extern void PPCInterpreter_vsl(sPPEState *ppeState);
extern void PPCInterpreter_vslo(sPPEState *ppeState);
extern void PPCInterpreter_vslb(sPPEState *ppeState);
extern void PPCInterpreter_vslh(sPPEState *ppeState);
extern void PPCInterpreter_vslw(sPPEState *ppeState);
extern void PPCInterpreter_vslw128(sPPEState *ppeState);
extern void PPCInterpreter_vsr(sPPEState *ppeState);
extern void PPCInterpreter_vsrh(sPPEState *ppeState);
extern void PPCInterpreter_vsrah(sPPEState *ppeState);
extern void PPCInterpreter_vsrw(sPPEState *ppeState);
extern void PPCInterpreter_vsrw128(sPPEState *ppeState);
extern void PPCInterpreter_vsraw128(sPPEState *ppeState);
extern void PPCInterpreter_vsldoi(sPPEState *ppeState);
extern void PPCInterpreter_vsldoi128(sPPEState *ppeState);
extern void PPCInterpreter_vspltb(sPPEState *ppeState);
extern void PPCInterpreter_vsplth(sPPEState *ppeState);
extern void PPCInterpreter_vspltisb(sPPEState *ppeState);
extern void PPCInterpreter_vspltish(sPPEState* ppeState);
extern void PPCInterpreter_vspltisw(sPPEState* ppeState);
extern void PPCInterpreter_vspltisw128(sPPEState *ppeState);
extern void PPCInterpreter_vsubfp128(sPPEState *ppeState);
extern void PPCInterpreter_vspltw128(sPPEState *ppeState);
extern void PPCInterpreter_vmsum3fp128(sPPEState *ppeState);
extern void PPCInterpreter_vmsum4fp128(sPPEState *ppeState);
extern void PPCInterpreter_vupkd3d128(sPPEState *ppeState);
extern void PPCInterpreter_vxor(sPPEState *ppeState);
extern void PPCInterpreter_vxor128(sPPEState *ppeState);

//
// Load/Store
//

// Store Byte
extern void PPCInterpreter_stb(sPPEState *ppeState);
extern void PPCInterpreter_stbu(sPPEState *ppeState);
extern void PPCInterpreter_stbux(sPPEState *ppeState);
extern void PPCInterpreter_stbx(sPPEState *ppeState);

// Store Halfword
extern void PPCInterpreter_sth(sPPEState *ppeState);
extern void PPCInterpreter_sthbrx(sPPEState *ppeState);
extern void PPCInterpreter_sthu(sPPEState *ppeState);
extern void PPCInterpreter_sthux(sPPEState *ppeState);
extern void PPCInterpreter_sthx(sPPEState *ppeState);

// Store String Word
extern void PPCInterpreter_stswi(sPPEState *ppeState);

// Store Multiple Word
extern void PPCInterpreter_stmw(sPPEState *ppeState);

// Store Word
extern void PPCInterpreter_stw(sPPEState *ppeState);
extern void PPCInterpreter_stwbrx(sPPEState *ppeState);
extern void PPCInterpreter_stwcx(sPPEState *ppeState);
extern void PPCInterpreter_stwu(sPPEState *ppeState);
extern void PPCInterpreter_stwux(sPPEState *ppeState);
extern void PPCInterpreter_stwx(sPPEState *ppeState);

// Store Doubleword
extern void PPCInterpreter_std(sPPEState *ppeState);
extern void PPCInterpreter_stdcx(sPPEState *ppeState);
extern void PPCInterpreter_stdu(sPPEState *ppeState);
extern void PPCInterpreter_stdux(sPPEState *ppeState);
extern void PPCInterpreter_stdx(sPPEState *ppeState);

// Store Floating
extern void PPCInterpreter_stfs(sPPEState *ppeState);
extern void PPCInterpreter_stfsu(sPPEState *ppeState);
extern void PPCInterpreter_stfsux(sPPEState *ppeState);
extern void PPCInterpreter_stfsx(sPPEState *ppeState);
extern void PPCInterpreter_stfd(sPPEState *ppeState);
extern void PPCInterpreter_stfdx(sPPEState *ppeState);
extern void PPCInterpreter_stfdu(sPPEState *ppeState);
extern void PPCInterpreter_stfdux(sPPEState *ppeState);
extern void PPCInterpreter_stfiwx(sPPEState *ppeState);

// Store Vector
extern void PPCInterpreter_stvx(sPPEState *ppeState);
extern void PPCInterpreter_stvx128(sPPEState *ppeState);
extern void PPCInterpreter_stvewx(sPPEState *ppeState);
extern void PPCInterpreter_stvewx128(sPPEState *ppeState);
extern void PPCInterpreter_stvrx(sPPEState *ppeState);
extern void PPCInterpreter_stvrx128(sPPEState *ppeState);
extern void PPCInterpreter_stvlx(sPPEState *ppeState);
extern void PPCInterpreter_stvxl(sPPEState *ppeState);
extern void PPCInterpreter_stvlx128(sPPEState *ppeState);
extern void PPCInterpreter_stvlxl128(sPPEState *ppeState);

// Load Byte
extern void PPCInterpreter_lbz(sPPEState *ppeState);
extern void PPCInterpreter_lbzu(sPPEState *ppeState);
extern void PPCInterpreter_lbzux(sPPEState *ppeState);
extern void PPCInterpreter_lbzx(sPPEState *ppeState);

// Load Halfword
extern void PPCInterpreter_lha(sPPEState *ppeState);
extern void PPCInterpreter_lhau(sPPEState *ppeState);
extern void PPCInterpreter_lhaux(sPPEState *ppeState);
extern void PPCInterpreter_lhax(sPPEState *ppeState);
extern void PPCInterpreter_lhbrx(sPPEState *ppeState);
extern void PPCInterpreter_lhz(sPPEState *ppeState);
extern void PPCInterpreter_lhzu(sPPEState *ppeState);
extern void PPCInterpreter_lhzux(sPPEState *ppeState);
extern void PPCInterpreter_lhzx(sPPEState *ppeState);

// Load String Word
extern void PPCInterpreter_lswi(sPPEState *ppeState);

// Load Multiple Word
extern void PPCInterpreter_lmw(sPPEState *ppeState);

// Load Word
extern void PPCInterpreter_lwa(sPPEState *ppeState);
extern void PPCInterpreter_lwax(sPPEState *ppeState);
extern void PPCInterpreter_lwaux(sPPEState *ppeState);
extern void PPCInterpreter_lwarx(sPPEState *ppeState);
extern void PPCInterpreter_lwbrx(sPPEState *ppeState);
extern void PPCInterpreter_lwz(sPPEState *ppeState);
extern void PPCInterpreter_lwzu(sPPEState *ppeState);
extern void PPCInterpreter_lwzux(sPPEState *ppeState);
extern void PPCInterpreter_lwzx(sPPEState *ppeState);

// Load Doubleword
extern void PPCInterpreter_ld(sPPEState *ppeState);
extern void PPCInterpreter_ldarx(sPPEState *ppeState);
extern void PPCInterpreter_ldbrx(sPPEState *ppeState);
extern void PPCInterpreter_ldu(sPPEState *ppeState);
extern void PPCInterpreter_ldux(sPPEState *ppeState);
extern void PPCInterpreter_ldx(sPPEState *ppeState);

// Load Floating
extern void PPCInterpreter_lfsx(sPPEState *ppeState);
extern void PPCInterpreter_lfsux(sPPEState *ppeState);
extern void PPCInterpreter_lfd(sPPEState *ppeState);
extern void PPCInterpreter_lfdx(sPPEState *ppeState);
extern void PPCInterpreter_lfdu(sPPEState *ppeState);
extern void PPCInterpreter_lfdux(sPPEState *ppeState);
extern void PPCInterpreter_lfs(sPPEState *ppeState);
extern void PPCInterpreter_lfsu(sPPEState *ppeState);

// Load Vector
extern void PPCInterpreter_lvebx(sPPEState *ppeState);
extern void PPCInterpreter_lvehx(sPPEState *ppeState);
extern void PPCInterpreter_lvewx(sPPEState *ppeState);
extern void PPCInterpreter_lvewx128(sPPEState *ppeState);
extern void PPCInterpreter_lvx(sPPEState *ppeState);
extern void PPCInterpreter_lvx128(sPPEState *ppeState);
extern void PPCInterpreter_lvxl128(sPPEState *ppeState);
extern void PPCInterpreter_lvxl(sPPEState *ppeState);
extern void PPCInterpreter_lvlx(sPPEState *ppeState);
extern void PPCInterpreter_lvlx128(sPPEState *ppeState);
extern void PPCInterpreter_lvrx(sPPEState *ppeState);
extern void PPCInterpreter_lvrx128(sPPEState *ppeState);
extern void PPCInterpreter_lvsl(sPPEState *ppeState);
extern void PPCInterpreter_lvsr(sPPEState *ppeState);


// @Aleblbl probably better to move it into a separate file for the JIT emitters
//
//	JIT emitters
//
extern void PPCInterpreterJIT_addx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_addic(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_addcx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_addex(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_addi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_addis(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_andi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_andis(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_andx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_andcx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_b(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_cmp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_cmpi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_cmpl(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_cmpli(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_cntlzdx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_mfspr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_mfocrf(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_mulldx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_nandx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_norx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_orx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_orcx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_ori(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_oris(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rldclx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rldcrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rldicx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rldiclx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rldicrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rldimix(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rlwimix(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rlwnmx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_rlwinmx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_sldx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_slwx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_srdx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_srwx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_sradix(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_xorx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_xori(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_xoris(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
extern void PPCInterpreterJIT_invalid(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);

}
