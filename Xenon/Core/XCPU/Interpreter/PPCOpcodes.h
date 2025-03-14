// Copyright 2025 Xenon Emulator Project

#pragma once

#include "PPCInternal.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace PPCInterpreter {
//
// Instruction definitions
//

#define D_STUB(name) static inline void PPCInterpreter_##name(PPU_STATE *ppuState) { return PPCInterpreter_known_unimplemented(#name, ppuState); }
#define D_STUBRC(name) static inline void PPCInterpreter_##name##x(PPU_STATE *ppuState) { return PPCInterpreter_known_unimplemented(#name "x", ppuState); }

// Stubs
extern void PPCInterpreter_invalid(PPU_STATE *ppuState);
extern void PPCInterpreter_known_unimplemented(const char* name, PPU_STATE *ppuState);

D_STUBRC(addc)
D_STUBRC(addco)
D_STUBRC(addo)
D_STUBRC(addeo)
D_STUBRC(addzeo)
D_STUBRC(addme)
D_STUBRC(addmeo)
D_STUBRC(subfco)
D_STUBRC(subfeo)
D_STUBRC(subfo)
D_STUBRC(subfzeo)
D_STUBRC(subfme)
D_STUBRC(subfmeo)
D_STUBRC(divduo)
D_STUBRC(divdo)
D_STUBRC(divwo)
D_STUBRC(divwuo)
D_STUBRC(mulhd)
D_STUBRC(mulldo)
D_STUBRC(mulhw)
D_STUBRC(mullwo)
D_STUBRC(nego)
D_STUBRC(rldcl)
D_STUBRC(mcrfs)
D_STUBRC(mtfsb1)
D_STUBRC(mtfsb0)
D_STUBRC(mtfsfi)
D_STUBRC(fadd)
D_STUBRC(fsub)
D_STUBRC(fctid)
D_STUBRC(fctidz)
D_STUBRC(fcfid)
D_STUBRC(fctiw)
D_STUBRC(fctiwz)
D_STUBRC(fadds)
D_STUBRC(fsubs)
D_STUBRC(fdiv)
D_STUBRC(fdivs)
D_STUBRC(fmr)
D_STUBRC(fmadds)
D_STUBRC(fmsubs)
D_STUBRC(fmul)
D_STUBRC(fmuls)
D_STUBRC(fnmsub)
D_STUBRC(fnmsubs)
D_STUBRC(fnmadd)
D_STUBRC(fmadd)
D_STUBRC(fnmadds)
D_STUBRC(fmsub)
D_STUBRC(fneg)
D_STUBRC(fsel)
D_STUBRC(fres)
D_STUBRC(frsp)
D_STUBRC(fabs)
D_STUBRC(fnabs)
D_STUBRC(fsqrt)
D_STUBRC(fsqrts)
D_STUBRC(frsqrte)
D_STUBRC(eqv)
D_STUB(td)
D_STUB(dst)
D_STUB(dss)
D_STUB(dstst)
D_STUB(lfsx)
D_STUB(lfsu)
D_STUB(lfsux)
D_STUB(mfsrin)
D_STUB(mfsr)
D_STUB(lvsr)
D_STUB(lvsl)
D_STUB(lvx)
D_STUB(lvrx)
D_STUB(lvxl)
D_STUB(lvrxl)
D_STUB(lvlx)
D_STUB(lvlxl)
D_STUB(lwaux)
D_STUB(lswx)
D_STUB(lhaux)
D_STUB(lvewx)
D_STUB(lveb)
D_STUB(lvebx)
D_STUB(lvehx)
D_STUB(stdbrx)
D_STUB(stswx)
D_STUB(stfdu)
D_STUB(stfs)
D_STUB(stfdx)
D_STUB(stfsx)
D_STUB(stfsu)
D_STUB(stfsux)
D_STUB(stfdux)
D_STUB(stfiwx)
D_STUB(stvebx)
D_STUB(fcmpo)
D_STUB(fcmpu)
D_STUB(stvx)
D_STUB(stvxl)
D_STUB(stvrx)
D_STUB(stvlx)
D_STUB(stvrxl)
D_STUB(stvlxl)
D_STUB(stvehx)
D_STUB(stvewx)
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
D_STUB(vaddfp)
D_STUB(vmrghb)
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
D_STUB(vsel)
D_STUB(vperm)
D_STUB(vsldoi)
D_STUB(vmaddfp)
D_STUB(vnmsubfp)

D_STUB(vadduhm)
D_STUB(vmaxuh)
D_STUB(vrlh)
D_STUB(vcmpequh)
D_STUB(vcmpequh_)
D_STUB(vmulouh)
D_STUB(vsubfp)
D_STUB(vmrghh)
D_STUB(vpkuwum)
D_STUB(vadduwm)
D_STUB(vmaxuw)
D_STUB(vrlw)
D_STUB(vcmpequw)
D_STUB(vcmpequw_)
D_STUB(vmrghw)
D_STUB(vpkuhus)
D_STUB(vcmpeqfp)
D_STUB(vcmpeqfp_)
D_STUB(vpkuwus)

D_STUB(vmaxsb)
D_STUB(vslb)
D_STUB(vmulosb)
D_STUB(vrefp)
D_STUB(vmrglb)
D_STUB(vpkshus)
D_STUB(vmaxsh)
D_STUB(vslh)
D_STUB(vmulosh)
D_STUB(vrsqrtefp)
D_STUB(vmrglh)
D_STUB(vpkswus)
D_STUB(vaddcuw)
D_STUB(vmaxsw)
D_STUB(vslw)
D_STUB(vexptefp)
D_STUB(vmrglw)
D_STUB(vpkshss)
D_STUB(vsl)
D_STUB(vcmpgefp)
D_STUB(vcmpgefp_)
D_STUB(vlogefp)
D_STUB(vpkswss)
D_STUB(vaddubs)
D_STUB(vminub)
D_STUB(vsrb)
D_STUB(vcmpgtub)
D_STUB(vcmpgtub_)
D_STUB(vmuleub)
D_STUB(vrfin)
D_STUB(vspltb)
D_STUB(vupkhsb)
D_STUB(vadduhs)
D_STUB(vminuh)
D_STUB(vsrh)
D_STUB(vcmpgtuh)
D_STUB(vcmpgtuh_)
D_STUB(vmuleuh)
D_STUB(vrfiz)
D_STUB(vsplth)
D_STUB(vupkhsh)
D_STUB(vadduws)
D_STUB(vminuw)
D_STUB(vsrw)
D_STUB(vcmpgtuw)
D_STUB(vcmpgtuw_)
D_STUB(vrfip)
D_STUB(vspltw)
D_STUB(vupklsb)
D_STUB(vsr)
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
D_STUB(vcfux)
D_STUB(vspltisb)
D_STUB(vpkpx)
D_STUB(vaddshs)
D_STUB(vminsh)
D_STUB(vsrah)
D_STUB(vcmpgtsh)
D_STUB(vcmpgtsh_)
D_STUB(vmulesh)
D_STUB(vcfsx)
D_STUB(vspltish)
D_STUB(vupkhpx)
D_STUB(vaddsws)
D_STUB(vminsw)
D_STUB(vsraw)
D_STUB(vcmpgtsw)
D_STUB(vcmpgtsw_)
D_STUB(vctuxs)
D_STUB(vspltisw)
D_STUB(vcmpbfp)
D_STUB(vcmpbfp_)
D_STUB(vctsxs)
D_STUB(vupklpx)
D_STUB(vsububm)
D_STUB(vavgub)
D_STUB(vand)
D_STUB(vmaxfp)
D_STUB(vslo)
D_STUB(vsubuhm)
D_STUB(vavguh)
D_STUB(vandc)
D_STUB(vminfp)
D_STUB(vsro)
D_STUB(vsubuwm)
D_STUB(vavguw)
D_STUB(vor)
D_STUB(vxor)
D_STUB(vavgsb)
D_STUB(vnor)
D_STUB(vavgsh)
D_STUB(vsubcuw)
D_STUB(vavgsw)
D_STUB(vsububs)
D_STUB(mfvscr)
D_STUB(vsum4ubs)
D_STUB(vsubuhs)
D_STUB(mtvscr)
D_STUB(vsum4shs)
D_STUB(vsubuws)
D_STUB(vsum2sws)
D_STUB(vsubsbs)
D_STUB(vsum4sbs)
D_STUB(vsubshs)
D_STUB(vsubsws)
D_STUB(vsumsws)

// ALU
extern void PPCInterpreter_addx(PPU_STATE *ppuState);
extern void PPCInterpreter_addex(PPU_STATE *ppuState);
extern void PPCInterpreter_addi(PPU_STATE *ppuState);
extern void PPCInterpreter_addic(PPU_STATE *ppuState);
extern void PPCInterpreter_addis(PPU_STATE *ppuState);
extern void PPCInterpreter_addzex(PPU_STATE *ppuState);
extern void PPCInterpreter_andx(PPU_STATE *ppuState);
extern void PPCInterpreter_andcx(PPU_STATE *ppuState);
extern void PPCInterpreter_andi(PPU_STATE *ppuState);
extern void PPCInterpreter_andis(PPU_STATE *ppuState);
extern void PPCInterpreter_cmp(PPU_STATE *ppuState);
extern void PPCInterpreter_cmpi(PPU_STATE *ppuState);
extern void PPCInterpreter_cmpl(PPU_STATE *ppuState);
extern void PPCInterpreter_cmpli(PPU_STATE *ppuState);
extern void PPCInterpreter_cntlzdx(PPU_STATE *ppuState);
extern void PPCInterpreter_cntlzwx(PPU_STATE *ppuState);
extern void PPCInterpreter_crand(PPU_STATE *ppuState);
extern void PPCInterpreter_crandc(PPU_STATE *ppuState);
extern void PPCInterpreter_creqv(PPU_STATE *ppuState);
extern void PPCInterpreter_crnand(PPU_STATE *ppuState);
extern void PPCInterpreter_crnor(PPU_STATE *ppuState);
extern void PPCInterpreter_cror(PPU_STATE *ppuState);
extern void PPCInterpreter_crorc(PPU_STATE *ppuState);
extern void PPCInterpreter_crxor(PPU_STATE *ppuState);
extern void PPCInterpreter_divdx(PPU_STATE *ppuState);
extern void PPCInterpreter_divdux(PPU_STATE *ppuState);
extern void PPCInterpreter_divwx(PPU_STATE *ppuState);
extern void PPCInterpreter_divwux(PPU_STATE *ppuState);
extern void PPCInterpreter_isync(PPU_STATE *ppuState);
extern void PPCInterpreter_extsbx(PPU_STATE *ppuState);
extern void PPCInterpreter_extshx(PPU_STATE *ppuState);
extern void PPCInterpreter_extswx(PPU_STATE *ppuState);
extern void PPCInterpreter_mcrf(PPU_STATE *ppuState);
extern void PPCInterpreter_mfocrf(PPU_STATE *ppuState);
extern void PPCInterpreter_mftb(PPU_STATE *ppuState);
extern void PPCInterpreter_mtocrf(PPU_STATE *ppuState);
extern void PPCInterpreter_mulli(PPU_STATE *ppuState);
extern void PPCInterpreter_mulldx(PPU_STATE *ppuState);
extern void PPCInterpreter_mullwx(PPU_STATE *ppuState);
extern void PPCInterpreter_mulhwux(PPU_STATE *ppuState);
extern void PPCInterpreter_mulhdux(PPU_STATE *ppuState);
extern void PPCInterpreter_nandx(PPU_STATE *ppuState);
extern void PPCInterpreter_negx(PPU_STATE *ppuState);
extern void PPCInterpreter_norx(PPU_STATE *ppuState);
extern void PPCInterpreter_orcx(PPU_STATE *ppuState);
extern void PPCInterpreter_ori(PPU_STATE *ppuState);
extern void PPCInterpreter_oris(PPU_STATE *ppuState);
extern void PPCInterpreter_orx(PPU_STATE *ppuState);
extern void PPCInterpreter_rldicx(PPU_STATE *ppuState);
extern void PPCInterpreter_rldcrx(PPU_STATE *ppuState);
extern void PPCInterpreter_rldiclx(PPU_STATE *ppuState);
extern void PPCInterpreter_rldicrx(PPU_STATE *ppuState);
extern void PPCInterpreter_rldimix(PPU_STATE *ppuState);
extern void PPCInterpreter_rlwimix(PPU_STATE *ppuState);
extern void PPCInterpreter_rlwnmx(PPU_STATE *ppuState);
extern void PPCInterpreter_rlwinmx(PPU_STATE *ppuState);
extern void PPCInterpreter_sldx(PPU_STATE *ppuState);
extern void PPCInterpreter_slwx(PPU_STATE *ppuState);
extern void PPCInterpreter_sradx(PPU_STATE *ppuState);
extern void PPCInterpreter_sradix(PPU_STATE *ppuState);
extern void PPCInterpreter_srawx(PPU_STATE *ppuState);
extern void PPCInterpreter_srawix(PPU_STATE *ppuState);
extern void PPCInterpreter_srdx(PPU_STATE *ppuState);
extern void PPCInterpreter_srwx(PPU_STATE *ppuState);
extern void PPCInterpreter_subfcx(PPU_STATE *ppuState);
extern void PPCInterpreter_subfx(PPU_STATE *ppuState);
extern void PPCInterpreter_subfex(PPU_STATE *ppuState);
extern void PPCInterpreter_subfzex(PPU_STATE *ppuState);
extern void PPCInterpreter_subfic(PPU_STATE *ppuState);
extern void PPCInterpreter_xori(PPU_STATE *ppuState);
extern void PPCInterpreter_xoris(PPU_STATE *ppuState);
extern void PPCInterpreter_xorx(PPU_STATE *ppuState);

// Program control
extern void PPCInterpreter_b(PPU_STATE *ppuState);
extern void PPCInterpreter_bc(PPU_STATE *ppuState);
extern void PPCInterpreter_bcctr(PPU_STATE *ppuState);
extern void PPCInterpreter_bclr(PPU_STATE *ppuState);

// System instructions
extern void PPCInterpreter_eieio(PPU_STATE *ppuState);
extern void PPCInterpreter_sc(PPU_STATE *ppuState);
extern void PPCInterpreter_slbia(PPU_STATE *ppuState);
extern void PPCInterpreter_slbie(PPU_STATE *ppuState);
extern void PPCInterpreter_slbmte(PPU_STATE *ppuState);
extern void PPCInterpreter_rfid(PPU_STATE *ppuState);
extern void PPCInterpreter_tw(PPU_STATE *ppuState);
extern void PPCInterpreter_twi(PPU_STATE *ppuState);
extern void PPCInterpreter_tdi(PPU_STATE *ppuState);
extern void PPCInterpreter_tlbie(PPU_STATE *ppuState);
extern void PPCInterpreter_tlbiel(PPU_STATE *ppuState);
extern void PPCInterpreter_tlbsync(PPU_STATE *ppuState);
extern void PPCInterpreter_mfspr(PPU_STATE *ppuState);
extern void PPCInterpreter_mtspr(PPU_STATE *ppuState);
extern void PPCInterpreter_mfmsr(PPU_STATE *ppuState);
extern void PPCInterpreter_mtmsr(PPU_STATE *ppuState);
extern void PPCInterpreter_mtmsrd(PPU_STATE *ppuState);
extern void PPCInterpreter_sync(PPU_STATE *ppuState);

// Cache Management
extern void PPCInterpreter_dcbf(PPU_STATE *ppuState);
extern void PPCInterpreter_dcbi(PPU_STATE *ppuState);
extern void PPCInterpreter_dcbt(PPU_STATE *ppuState);
extern void PPCInterpreter_dcbtst(PPU_STATE *ppuState);
extern void PPCInterpreter_dcbst(PPU_STATE *ppuState);
extern void PPCInterpreter_dcbz(PPU_STATE *ppuState);
extern void PPCInterpreter_icbi(PPU_STATE *ppuState);

//
// FPU
//
extern void PPCInterpreter_mffsx(PPU_STATE *ppuState);
extern void PPCInterpreter_mtfsfx(PPU_STATE *ppuState);

//
// Load/Store
//

// Store Byte
extern void PPCInterpreter_stb(PPU_STATE *ppuState);
extern void PPCInterpreter_stbu(PPU_STATE *ppuState);
extern void PPCInterpreter_stbux(PPU_STATE *ppuState);
extern void PPCInterpreter_stbx(PPU_STATE *ppuState);

// Store Halfword
extern void PPCInterpreter_sth(PPU_STATE *ppuState);
extern void PPCInterpreter_sthbrx(PPU_STATE *ppuState);
extern void PPCInterpreter_sthu(PPU_STATE *ppuState);
extern void PPCInterpreter_sthux(PPU_STATE *ppuState);
extern void PPCInterpreter_sthx(PPU_STATE *ppuState);

// Store String Word
extern void PPCInterpreter_stswi(PPU_STATE *ppuState);

// Store Multiple Word
extern void PPCInterpreter_stmw(PPU_STATE *ppuState);

// Store Word
extern void PPCInterpreter_stw(PPU_STATE *ppuState);
extern void PPCInterpreter_stwbrx(PPU_STATE *ppuState);
extern void PPCInterpreter_stwcx(PPU_STATE *ppuState);
extern void PPCInterpreter_stwu(PPU_STATE *ppuState);
extern void PPCInterpreter_stwux(PPU_STATE *ppuState);
extern void PPCInterpreter_stwx(PPU_STATE *ppuState);

// Store Doubleword
extern void PPCInterpreter_std(PPU_STATE *ppuState);
extern void PPCInterpreter_stdcx(PPU_STATE *ppuState);
extern void PPCInterpreter_stdu(PPU_STATE *ppuState);
extern void PPCInterpreter_stdux(PPU_STATE *ppuState);
extern void PPCInterpreter_stdx(PPU_STATE *ppuState);

// Store Floating
extern void PPCInterpreter_stfd(PPU_STATE *ppuState);

// Load Byte
extern void PPCInterpreter_lbz(PPU_STATE *ppuState);
extern void PPCInterpreter_lbzu(PPU_STATE *ppuState);
extern void PPCInterpreter_lbzux(PPU_STATE *ppuState);
extern void PPCInterpreter_lbzx(PPU_STATE *ppuState);

// Load Halfword
extern void PPCInterpreter_lha(PPU_STATE *ppuState);
extern void PPCInterpreter_lhau(PPU_STATE *ppuState);
extern void PPCInterpreter_lhax(PPU_STATE *ppuState);
extern void PPCInterpreter_lhbrx(PPU_STATE *ppuState);
extern void PPCInterpreter_lhz(PPU_STATE *ppuState);
extern void PPCInterpreter_lhzu(PPU_STATE *ppuState);
extern void PPCInterpreter_lhzux(PPU_STATE *ppuState);
extern void PPCInterpreter_lhzx(PPU_STATE *ppuState);

// Load String Word
extern void PPCInterpreter_lswi(PPU_STATE *ppuState);

// Load Multiple Word
extern void PPCInterpreter_lmw(PPU_STATE *ppuState);

// Load Word
extern void PPCInterpreter_lwa(PPU_STATE *ppuState);
extern void PPCInterpreter_lwax(PPU_STATE *ppuState);
extern void PPCInterpreter_lwarx(PPU_STATE *ppuState);
extern void PPCInterpreter_lwbrx(PPU_STATE *ppuState);
extern void PPCInterpreter_lwz(PPU_STATE *ppuState);
extern void PPCInterpreter_lwzu(PPU_STATE *ppuState);
extern void PPCInterpreter_lwzux(PPU_STATE *ppuState);
extern void PPCInterpreter_lwzx(PPU_STATE *ppuState);

// Load Doubleword
extern void PPCInterpreter_ld(PPU_STATE *ppuState);
extern void PPCInterpreter_ldarx(PPU_STATE *ppuState);
extern void PPCInterpreter_ldbrx(PPU_STATE *ppuState);
extern void PPCInterpreter_ldu(PPU_STATE *ppuState);
extern void PPCInterpreter_ldux(PPU_STATE *ppuState);
extern void PPCInterpreter_ldx(PPU_STATE *ppuState);

//
// Load Floating
//

extern void PPCInterpreter_lfd(PPU_STATE *ppuState);
extern void PPCInterpreter_lfdx(PPU_STATE *ppuState);
extern void PPCInterpreter_lfdu(PPU_STATE *ppuState);
extern void PPCInterpreter_lfdux(PPU_STATE *ppuState);
extern void PPCInterpreter_lfs(PPU_STATE *ppuState);

}
