/*
* Copyright 2025 Xenon Emulator Project

* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include "PPC_Instruction.h"

#include "PPCInterpreter.h"

#define D_STUB(name) void PPCInterpreter_##name(PPU_STATE *ppuState) { return PPCInterpreter_known_unimplemented(#name, ppuState); }
#define D_STUBRC(name) void PPCInterpreter_##name##x(PPU_STATE *ppuState) { return PPCInterpreter_known_unimplemented(#name "x", ppuState); }

namespace PPCInterpreter {
  void PPCInterpreter_nop(PPU_STATE *ppuState) {
    // Do nothing
  }
  void PPCInterpreter_invalid(PPU_STATE *ppuState) {
    std::string name =
      legacy_GetOpcodeName(_instr.opcode);

    LOG_CRITICAL(Xenon, "PPC Interpreter: {} found: data: {:#x}, address: {:#x}",
      name.data(),
      _instr.opcode,
      curThread.CIA);
  }

  void PPCInterpreter_known_unimplemented(const char *name, PPU_STATE *ppuState) {
    LOG_CRITICAL(Xenon, "PPC Interpreter: {} is not implemented! data: {:#x}, address: {:#x}",
      name,
      _instr.opcode,
      curThread.CIA);
  }

  D_STUBRC(addc);
  D_STUBRC(addco);
  D_STUBRC(addo);
  D_STUBRC(addeo);
  D_STUBRC(addzeo);
  D_STUBRC(addme);
  D_STUBRC(addmeo);
  D_STUBRC(subfco);
  D_STUBRC(subfeo);
  D_STUBRC(subfo);
  D_STUBRC(subfzeo);
  D_STUBRC(subfme);
  D_STUBRC(subfmeo);
  D_STUBRC(divduo);
  D_STUBRC(divdo);
  D_STUBRC(divwo);
  D_STUBRC(divwuo);
  D_STUBRC(mulhd);
  D_STUBRC(mulldo);
  D_STUBRC(mulhw);
  D_STUBRC(mullwo);
  D_STUBRC(nego);
  D_STUBRC(rldcl);
  D_STUBRC(mcrfs);
  D_STUBRC(mtfsb1);
  D_STUBRC(mtfsb0);
  D_STUBRC(mtfsfi);
  D_STUBRC(fadd);
  D_STUBRC(fsub);
  D_STUBRC(fctid);
  D_STUBRC(fctidz);
  D_STUBRC(fcfid);
  D_STUBRC(fctiw);
  D_STUBRC(fctiwz);
  D_STUBRC(fadds);
  D_STUBRC(fsubs);
  D_STUBRC(fdiv);
  D_STUBRC(fdivs);
  D_STUBRC(fmr);
  D_STUBRC(fmadds);
  D_STUBRC(fmsubs);
  D_STUBRC(fmul);
  D_STUBRC(fmuls);
  D_STUBRC(fnmsub);
  D_STUBRC(fnmsubs);
  D_STUBRC(fnmadd);
  D_STUBRC(fmadd);
  D_STUBRC(fnmadds);
  D_STUBRC(fmsub);
  D_STUBRC(fneg);
  D_STUBRC(fsel);
  D_STUBRC(fres);
  D_STUBRC(frsp);
  D_STUBRC(fabs);
  D_STUBRC(fnabs);
  D_STUBRC(fsqrt);
  D_STUBRC(fsqrts);
  D_STUBRC(frsqrte);
  D_STUBRC(eqv);
  D_STUB(td);
  D_STUB(dst);
  D_STUB(dss);
  D_STUB(dstst);
  D_STUB(lfdx);
  D_STUB(lfdu);
  D_STUB(lfdux);
  D_STUB(lfsx);
  D_STUB(lfsu);
  D_STUB(lfsux);
  D_STUB(mfsrin);
  D_STUB(mfsr);
  D_STUB(lvsr);
  D_STUB(lvsl);
  D_STUB(lvx);
  D_STUB(lvrx);
  D_STUB(lvxl);
  D_STUB(lvrxl);
  D_STUB(lvlx);
  D_STUB(lvlxl);
  D_STUB(lwaux);
  D_STUB(lswx);
  D_STUB(lhaux);
  D_STUB(lvewx);
  D_STUB(lveb);
  D_STUB(lvebx);
  D_STUB(lvehx);
  D_STUB(stdbrx);
  D_STUB(stswx);
  D_STUB(stfdu);
  D_STUB(stfs);
  D_STUB(stfdx);
  D_STUB(stfsx);
  D_STUB(stfsu);
  D_STUB(stfsux);
  D_STUB(stfdux);
  D_STUB(stfiwx);
  D_STUB(stvebx);
  D_STUB(fcmpo);
  D_STUB(fcmpu);
  D_STUB(stvx);
  D_STUB(stvxl);
  D_STUB(stvrx);
  D_STUB(stvlx);
  D_STUB(stvrxl);
  D_STUB(stvlxl);
  D_STUB(stvehx);
  D_STUB(stvewx);
  D_STUB(eciwx);
  D_STUB(ecowx);
  D_STUB(slbmfev);
  D_STUB(slbmfee);

  // Vector-instructions
  D_STUB(vaddubm);
  D_STUB(vmaxub);
  D_STUB(vrlb);
  D_STUB(vcmpequb);
  D_STUB(vcmpequb_);
  D_STUB(vmuloub);
  D_STUB(vaddfp);
  D_STUB(vmrghb);
  D_STUB(vpkuhum);

  D_STUB(vmhaddshs);
  D_STUB(vmhraddshs);
  D_STUB(vmladduhm);
  D_STUB(vmsumubm);
  D_STUB(vmsummbm);
  D_STUB(vmsumuhm);
  D_STUB(vmsumuhs);
  D_STUB(vmsumshm);
  D_STUB(vmsumshs);
  D_STUB(vsel);
  D_STUB(vperm);
  D_STUB(vsldoi);
  D_STUB(vmaddfp);
  D_STUB(vnmsubfp);

  D_STUB(vadduhm);
  D_STUB(vmaxuh);
  D_STUB(vrlh);
  D_STUB(vcmpequh);
  D_STUB(vcmpequh_);
  D_STUB(vmulouh);
  D_STUB(vsubfp);
  D_STUB(vmrghh);
  D_STUB(vpkuwum);
  D_STUB(vadduwm);
  D_STUB(vmaxuw);
  D_STUB(vrlw);
  D_STUB(vcmpequw);
  D_STUB(vcmpequw_);
  D_STUB(vmrghw);
  D_STUB(vpkuhus);
  D_STUB(vcmpeqfp);
  D_STUB(vcmpeqfp_);
  D_STUB(vpkuwus);

  D_STUB(vmaxsb);
  D_STUB(vslb);
  D_STUB(vmulosb);
  D_STUB(vrefp);
  D_STUB(vmrglb);
  D_STUB(vpkshus);
  D_STUB(vmaxsh);
  D_STUB(vslh);
  D_STUB(vmulosh);
  D_STUB(vrsqrtefp);
  D_STUB(vmrglh);
  D_STUB(vpkswus);
  D_STUB(vaddcuw);
  D_STUB(vmaxsw);
  D_STUB(vslw);
  D_STUB(vexptefp);
  D_STUB(vmrglw);
  D_STUB(vpkshss);
  D_STUB(vsl);
  D_STUB(vcmpgefp);
  D_STUB(vcmpgefp_);
  D_STUB(vlogefp);
  D_STUB(vpkswss);
  D_STUB(vaddubs);
  D_STUB(vminub);
  D_STUB(vsrb);
  D_STUB(vcmpgtub);
  D_STUB(vcmpgtub_);
  D_STUB(vmuleub);
  D_STUB(vrfin);
  D_STUB(vspltb);
  D_STUB(vupkhsb);
  D_STUB(vadduhs);
  D_STUB(vminuh);
  D_STUB(vsrh);
  D_STUB(vcmpgtuh);
  D_STUB(vcmpgtuh_);
  D_STUB(vmuleuh);
  D_STUB(vrfiz);
  D_STUB(vsplth);
  D_STUB(vupkhsh);
  D_STUB(vadduws);
  D_STUB(vminuw);
  D_STUB(vsrw);
  D_STUB(vcmpgtuw);
  D_STUB(vcmpgtuw_);
  D_STUB(vrfip);
  D_STUB(vspltw);
  D_STUB(vupklsb);
  D_STUB(vsr);
  D_STUB(vcmpgtfp);
  D_STUB(vcmpgtfp_);
  D_STUB(vrfim);
  D_STUB(vupklsh);
  D_STUB(vaddsbs);
  D_STUB(vminsb);
  D_STUB(vsrab);
  D_STUB(vcmpgtsb);
  D_STUB(vcmpgtsb_);
  D_STUB(vmulesb);
  D_STUB(vcfux);
  D_STUB(vspltisb);
  D_STUB(vpkpx);
  D_STUB(vaddshs);
  D_STUB(vminsh);
  D_STUB(vsrah);
  D_STUB(vcmpgtsh);
  D_STUB(vcmpgtsh_);
  D_STUB(vmulesh);
  D_STUB(vcfsx);
  D_STUB(vspltish);
  D_STUB(vupkhpx);
  D_STUB(vaddsws);
  D_STUB(vminsw);
  D_STUB(vsraw);
  D_STUB(vcmpgtsw);
  D_STUB(vcmpgtsw_);
  D_STUB(vctuxs);
  D_STUB(vspltisw);
  D_STUB(vcmpbfp);
  D_STUB(vcmpbfp_);
  D_STUB(vctsxs);
  D_STUB(vupklpx);
  D_STUB(vsububm);
  D_STUB(vavgub);
  D_STUB(vand);
  D_STUB(vmaxfp);
  D_STUB(vslo);
  D_STUB(vsubuhm);
  D_STUB(vavguh);
  D_STUB(vandc);
  D_STUB(vminfp);
  D_STUB(vsro);
  D_STUB(vsubuwm);
  D_STUB(vavguw);
  D_STUB(vor);
  D_STUB(vxor);
  D_STUB(vavgsb);
  D_STUB(vnor);
  D_STUB(vavgsh);
  D_STUB(vsubcuw);
  D_STUB(vavgsw);
  D_STUB(vsububs);
  D_STUB(mfvscr);
  D_STUB(vsum4ubs);
  D_STUB(vsubuhs);
  D_STUB(mtvscr);
  D_STUB(vsum4shs);
  D_STUB(vsubuws);
  D_STUB(vsum2sws);
  D_STUB(vsubsbs);
  D_STUB(vsum4sbs);
  D_STUB(vsubshs);
  D_STUB(vsubsws);
  D_STUB(vsumsws);

  PPCDecoder::PPCDecoder() {
  	#define GET_(name) &PPCInterpreter_##name
  	#define GET(name) GET_(name), GET_(name)
    #define GETRC(name) GET_(name##x), GET_(name##x)
  	for (auto& x : table) {
      x = GET(invalid);
  	}
  	// Main opcodes (field 0..5)
  	fillTable(0x00, 6, -1, {
      { 0x02, GET(tdi) },
      { 0x03, GET(twi) },
      { 0x07, GET(mulli) },
      { 0x08, GET(subfic) },
      { 0x0A, GET(cmpli) },
      { 0x0B, GET(cmpi) },
      { 0x0C, GET(addic) },
      { 0x0D, GET(addic) },
      { 0x0E, GET(addi) },
      { 0x0F, GET(addis) },
      { 0x10, GET(bc) },
      { 0x11, GET(sc) },
      { 0x12, GET(b) },
      { 0x14, GETRC(rlwimi) },
      { 0x15, GETRC(rlwinm) },
      { 0x17, GETRC(rlwnm) },
      { 0x18, GET(ori) },
      { 0x19, GET(oris) },
      { 0x1A, GET(xori) },
      { 0x1B, GET(xoris) },
      { 0x1C, GET(andi) },
      { 0x1D, GET(andis) },
      { 0x20, GET(lwz) },
      { 0x21, GET(lwzu) },
      { 0x22, GET(lbz) },
      { 0x23, GET(lbzu) },
      { 0x24, GET(stw) },
      { 0x25, GET(stwu) },
      { 0x26, GET(stb) },
      { 0x27, GET(stbu) },
      { 0x28, GET(lhz) },
      { 0x29, GET(lhzu) },
      { 0x2A, GET(lha) },
      { 0x2B, GET(lhau) },
      { 0x2C, GET(sth) },
      { 0x2D, GET(sthu) },
      { 0x2E, GET(lmw) },
      { 0x2F, GET(stmw) },
      { 0x30, GET(lfs) },
      { 0x31, GET(lfsu) },
      { 0x32, GET(lfd) },
      { 0x33, GET(lfdu) },
      { 0x34, GET(stfs) },
      { 0x35, GET(stfsu) },
      { 0x36, GET(stfd) },
      { 0x37, GET(stfdu) },
  	});
  	// Group 0x04 opcodes (field 21..31)
  	fillTable(0x04, 11, 0, {
      { 0x0, GET(vaddubm) },
      { 0x2, GET(vmaxub) },
      { 0x4, GET(vrlb) },
      { 0x006, GET(vcmpequb) },
      { 0x406, GET(vcmpequb_) },
      { 0x8, GET(vmuloub) },
      { 0xA, GET(vaddfp) },
      { 0xC, GET(vmrghb) },
      { 0xE, GET(vpkuhum) },

      { 0x20, GET(vmhaddshs), 5 },
      { 0x21, GET(vmhraddshs), 5 },
      { 0x22, GET(vmladduhm), 5 },
      { 0x24, GET(vmsumubm), 5 },
      { 0x25, GET(vmsummbm), 5 },
      { 0x26, GET(vmsumuhm), 5 },
      { 0x27, GET(vmsumuhs), 5 },
      { 0x28, GET(vmsumshm), 5 },
      { 0x29, GET(vmsumshs), 5 },
      { 0x2A, GET(vsel), 5 },
      { 0x2B, GET(vperm), 5 },
      { 0x2C, GET(vsldoi), 5 },
      { 0x2E, GET(vmaddfp), 5 },
      { 0x2F, GET(vnmsubfp), 5 },

      { 0x40, GET(vadduhm) },
      { 0x42, GET(vmaxuh) },
      { 0x44, GET(vrlh) },
      { 0x046, GET(vcmpequh) },
      { 0x446, GET(vcmpequh_) },
      { 0x48, GET(vmulouh) },
      { 0x4A, GET(vsubfp) },
      { 0x4C, GET(vmrghh) },
      { 0x4E, GET(vpkuwum) },
      { 0x80, GET(vadduwm) },
      { 0x82, GET(vmaxuw) },
      { 0x84, GET(vrlw) },
      { 0x086, GET(vcmpequw) },
      { 0x486, GET(vcmpequw_) },
      { 0x8C, GET(vmrghw) },
      { 0x8E, GET(vpkuhus) },
      { 0x0C6, GET(vcmpeqfp) },
      { 0x4C6, GET(vcmpeqfp_) },
      { 0xCE, GET(vpkuwus) },

      { 0x102, GET(vmaxsb) },
      { 0x104, GET(vslb) },
      { 0x108, GET(vmulosb) },
      { 0x10A, GET(vrefp) },
      { 0x10C, GET(vmrglb) },
      { 0x10E, GET(vpkshus) },
      { 0x142, GET(vmaxsh) },
      { 0x144, GET(vslh) },
      { 0x148, GET(vmulosh) },
      { 0x14A, GET(vrsqrtefp) },
      { 0x14C, GET(vmrglh) },
      { 0x14E, GET(vpkswus) },
      { 0x180, GET(vaddcuw) },
      { 0x182, GET(vmaxsw) },
      { 0x184, GET(vslw) },
      { 0x18A, GET(vexptefp) },
      { 0x18C, GET(vmrglw) },
      { 0x18E, GET(vpkshss) },
      { 0x1C4, GET(vsl) },
      { 0x1C6, GET(vcmpgefp) },
      { 0x5C6, GET(vcmpgefp_) },
      { 0x1CA, GET(vlogefp) },
      { 0x1CE, GET(vpkswss) },
      { 0x200, GET(vaddubs) },
      { 0x202, GET(vminub) },
      { 0x204, GET(vsrb) },
      { 0x206, GET(vcmpgtub) },
      { 0x606, GET(vcmpgtub_) },
      { 0x208, GET(vmuleub) },
      { 0x20A, GET(vrfin) },
      { 0x20C, GET(vspltb) },
      { 0x20E, GET(vupkhsb) },
      { 0x240, GET(vadduhs) },
      { 0x242, GET(vminuh) },
      { 0x244, GET(vsrh) },
      { 0x246, GET(vcmpgtuh) },
      { 0x646, GET(vcmpgtuh_) },
      { 0x248, GET(vmuleuh) },
      { 0x24A, GET(vrfiz) },
      { 0x24C, GET(vsplth) },
      { 0x24E, GET(vupkhsh) },
      { 0x280, GET(vadduws) },
      { 0x282, GET(vminuw) },
      { 0x284, GET(vsrw) },
      { 0x286, GET(vcmpgtuw) },
      { 0x686, GET(vcmpgtuw_) },
      { 0x28A, GET(vrfip) },
      { 0x28C, GET(vspltw) },
      { 0x28E, GET(vupklsb) },
      { 0x2C4, GET(vsr) },
      { 0x2C6, GET(vcmpgtfp) },
      { 0x6C6, GET(vcmpgtfp_) },
      { 0x2CA, GET(vrfim) },
      { 0x2CE, GET(vupklsh) },
      { 0x300, GET(vaddsbs) },
      { 0x302, GET(vminsb) },
      { 0x304, GET(vsrab) },
      { 0x306, GET(vcmpgtsb) },
      { 0x706, GET(vcmpgtsb_) },
      { 0x308, GET(vmulesb) },
      { 0x30A, GET(vcfux) },
      { 0x30C, GET(vspltisb) },
      { 0x30E, GET(vpkpx) },
      { 0x340, GET(vaddshs) },
      { 0x342, GET(vminsh) },
      { 0x344, GET(vsrah) },
      { 0x346, GET(vcmpgtsh) },
      { 0x746, GET(vcmpgtsh_) },
      { 0x348, GET(vmulesh) },
      { 0x34A, GET(vcfsx) },
      { 0x34C, GET(vspltish) },
      { 0x34E, GET(vupkhpx) },
      { 0x380, GET(vaddsws) },
      { 0x382, GET(vminsw) },
      { 0x384, GET(vsraw) },
      { 0x386, GET(vcmpgtsw) },
      { 0x786, GET(vcmpgtsw_) },
      { 0x38A, GET(vctuxs) },
      { 0x38C, GET(vspltisw) },
      { 0x3C6, GET(vcmpbfp) },
      { 0x7C6, GET(vcmpbfp_) },
      { 0x3CA, GET(vctsxs) },
      { 0x3CE, GET(vupklpx) },
      { 0x400, GET(vsububm) },
      { 0x402, GET(vavgub) },
      { 0x404, GET(vand) },
      { 0x40A, GET(vmaxfp) },
      { 0x40C, GET(vslo) },
      { 0x440, GET(vsubuhm) },
      { 0x442, GET(vavguh) },
      { 0x444, GET(vandc) },
      { 0x44A, GET(vminfp) },
      { 0x44C, GET(vsro) },
      { 0x480, GET(vsubuwm) },
      { 0x482, GET(vavguw) },
      { 0x484, GET(vor) },
      { 0x4C4, GET(vxor) },
      { 0x502, GET(vavgsb) },
      { 0x504, GET(vnor) },
      { 0x542, GET(vavgsh) },
      { 0x580, GET(vsubcuw) },
      { 0x582, GET(vavgsw) },
      { 0x600, GET(vsububs) },
      { 0x604, GET(mfvscr) },
      { 0x608, GET(vsum4ubs) },
      { 0x640, GET(vsubuhs) },
      { 0x644, GET(mtvscr) },
      { 0x648, GET(vsum4shs) },
      { 0x680, GET(vsubuws) },
      { 0x688, GET(vsum2sws) },
      { 0x700, GET(vsubsbs) },
      { 0x708, GET(vsum4sbs) },
      { 0x740, GET(vsubshs) },
      { 0x780, GET(vsubsws) },
      { 0x788, GET(vsumsws) },
  	});
  	// Group 0x13 opcodes (field 21..30)
  	fillTable(0x13, 10, 1, {
      { 0x000, GET(mcrf) },
      { 0x010, GET(bclr) },
      { 0x012, GET(rfid) },
      { 0x021, GET(crnor) },
      { 0x081, GET(crandc) },
      { 0x096, GET(isync) },
      { 0x0C1, GET(crxor) },
      { 0x0E1, GET(crnand) },
      { 0x101, GET(crand) },
      { 0x121, GET(creqv) },
      { 0x1A1, GET(crorc) },
      { 0x1C1, GET(cror) },
      { 0x210, GET(bcctr) },
  	});
  	// Group 0x1E opcodes (field 27..30)
  	fillTable(0x1E, 4, 1, {
      { 0x0, GETRC(rldicl) },
      { 0x1, GETRC(rldicl) },
      { 0x2, GETRC(rldicr) },
      { 0x3, GETRC(rldicr) },
      { 0x4, GETRC(rldic) },
      { 0x5, GETRC(rldic) },
      { 0x6, GETRC(rldimi) },
      { 0x7, GETRC(rldimi) },
      { 0x8, GETRC(rldcl) },
      { 0x9, GETRC(rldcr) },
  	});
  	// Group 0x1F opcodes (field 21..30)
  	fillTable(0x1F, 10, 1, {
      { 0x000, GET(cmp) },
      { 0x004, GET(tw) },
      { 0x006, GET(lvsl) },
      { 0x007, GET(lvebx) },
      { 0x008, GETRC(subfc) },
      { 0x208, GETRC(subfco) },
      { 0x009, GETRC(mulhdu) },
      { 0x00A, GETRC(addc) },
      { 0x20A, GETRC(addco) },
      { 0x00B, GETRC(mulhwu) },
      { 0x013, GET(mfocrf) },
      { 0x014, GET(lwarx) },
      { 0x015, GET(ldx) },
      { 0x017, GET(lwzx) },
      { 0x018, GETRC(slw) },
      { 0x01A, GETRC(cntlzw) },
      { 0x01B, GETRC(sld) },
      { 0x01C, GETRC(and) },
      { 0x020, GET(cmpl) },
      { 0x026, GET(lvsr) },
      { 0x027, GET(lvehx) },
      { 0x028, GETRC(subf) },
      { 0x228, GETRC(subfo) },
      { 0x035, GET(ldux) },
      { 0x036, GET(dcbst) },
      { 0x037, GET(lwzux) },
      { 0x03A, GETRC(cntlzd) },
      { 0x03C, GETRC(andc) },
      { 0x044, GET(td) },
      { 0x047, GET(lvewx) },
      { 0x049, GETRC(mulhd) },
      { 0x04B, GETRC(mulhw) },
      { 0x053, GET(mfmsr) },
      { 0x054, GET(ldarx) },
      { 0x056, GET(dcbf) },
      { 0x057, GET(lbzx) },
      { 0x067, GET(lvx) },
      { 0x068, GETRC(neg) },
      { 0x268, GETRC(nego) },
      { 0x077, GET(lbzux) },
      { 0x07C, GETRC(nor) },
      { 0x087, GET(stvebx) },
      { 0x088, GETRC(subfe) },
      { 0x288, GETRC(subfeo) },
      { 0x08A, GETRC(adde) },
      { 0x28A, GETRC(addeo) },
      { 0x090, GET(mtocrf) },
      { 0x092, GET(mtmsr) },
      { 0x095, GET(stdx) },
      { 0x096, GET(stwcx) },
      { 0x097, GET(stwx) },
      { 0x0A7, GET(stvehx) },
      { 0x0B2, GET(mtmsrd) },
      { 0x0B5, GET(stdux) },
      { 0x0B7, GET(stwux) },
      { 0x0C7, GET(stvewx) },
      { 0x0C8, GETRC(subfze) },
      { 0x2C8, GETRC(subfzeo) },
      { 0x0CA, GETRC(addze) },
      { 0x2CA, GETRC(addzeo) },
      { 0x0D6, GET(stdcx) },
      { 0x0D7, GET(stbx) },
      { 0x0E7, GET(stvx) },
      { 0x0E8, GETRC(subfme) },
      { 0x2E8, GETRC(subfmeo) },
      { 0x0E9, GETRC(mulld) },
      { 0x2E9, GETRC(mulldo) },
      { 0x0EA, GETRC(addme) },
      { 0x2EA, GETRC(addmeo) },
      { 0x0EB, GETRC(mullw) },
      { 0x2EB, GETRC(mullwo) },
      { 0x0F6, GET(dcbtst) },
      { 0x0F7, GET(stbux) },
      { 0x10A, GETRC(add) },
      { 0x30A, GETRC(addo) },
      { 0x116, GET(dcbt) },
      { 0x117, GET(lhzx) },
      { 0x11C, GETRC(eqv) },
      { 0x112, GET(tlbiel) },
      { 0x132, GET(tlbie) },
      { 0x136, GET(eciwx) },
      { 0x137, GET(lhzux) },
      { 0x13C, GETRC(xor) },
      { 0x153, GET(mfspr) },
      { 0x155, GET(lwax) },
      { 0x156, GET(dst) },
      { 0x157, GET(lhax) },
      { 0x167, GET(lvxl) },
      { 0x173, GET(mftb) },
      { 0x175, GET(lwaux) },
      { 0x176, GET(dstst) },
      { 0x177, GET(lhaux) },
      { 0x192, GET(slbmte) },
      { 0x197, GET(sthx) },
      { 0x19C, GET(orcx) },
      { 0x1B2, GET(slbie) },
      { 0x1B6, GET(ecowx) },
      { 0x1B7, GET(sthux) },
      { 0x1BC, GETRC(or) },
      { 0x1C9, GETRC(divdu) },
      { 0x3C9, GETRC(divduo) },
      { 0x1CB, GETRC(divwu) },
      { 0x3CB, GETRC(divwuo) },
      { 0x1D3, GET(mtspr) },
      { 0x1D6, GET(dcbi) },
      { 0x1DC, GETRC(nand) },
      { 0x1F2, GET(slbia) },
      { 0x1E7, GET(stvxl) },
      { 0x1E9, GETRC(divd) },
      { 0x3E9, GETRC(divdo) },
      { 0x1EB, GETRC(divw) },
      { 0x3EB, GETRC(divwo) },
      { 0x207, GET(lvlx) },
      { 0x214, GET(ldbrx) },
      { 0x215, GET(lswx) },
      { 0x216, GET(lwbrx) },
      { 0x217, GET(lfsx) },
      { 0x218, GETRC(srw) },
      { 0x21B, GETRC(srd) },
      { 0x227, GET(lvrx) },
      { 0x236, GET(tlbsync) },
      { 0x237, GET(lfsux) },
      { 0x239, GET(mfsrin) },
      { 0x253, GET(mfsr) },
      { 0x255, GET(lswi) },
      { 0x256, GET(sync) },
      { 0x257, GET(lfdx) },
      { 0x277, GET(lfdux) },
      { 0x287, GET(stvlx) },
      { 0x294, GET(stdbrx) },
      { 0x295, GET(stswx) },
      { 0x296, GET(stwbrx) },
      { 0x297, GET(stfsx) },
      { 0x2A7, GET(stvrx) },
      { 0x2B7, GET(stfsux) },
      { 0x2D5, GET(stswi) },
      { 0x2D7, GET(stfdx) },
      { 0x2F7, GET(stfdux) },
      { 0x307, GET(lvlxl) },
      { 0x316, GET(lhbrx) },
      { 0x318, GETRC(sraw) },
      { 0x31A, GETRC(srad) },
      { 0x327, GET(lvrxl) },
      { 0x336, GET(dss) },
      { 0x338, GETRC(srawi) },
      { 0x33A, GETRC(sradi) },
      { 0x33B, GETRC(sradi) },
      { 0x353, GET(slbmfev) },
      { 0x356, GET(eieio) },
      { 0x387, GET(stvlxl) },
      { 0x393, GET(slbmfee) },
      { 0x396, GET(sthbrx) },
      { 0x39A, GETRC(extsh) },
      { 0x3A7, GET(stvrxl) },
      { 0x3BA, GETRC(extsb) },
      { 0x3D7, GET(stfiwx) },
      { 0x3DA, GETRC(extsw) },
      { 0x3D6, GET(icbi) },
      { 0x3F6, GET(dcbz) },
  	});
  	// Group 0x3A opcodes (field 30..31)
  	fillTable(0x3A,2, 0, {
      { 0x0, GET(ld) },
      { 0x1, GET(ldu) },
      { 0x2, GET(lwa) },
  	});
  	// Group 0x3B opcodes (field 21..30)
  	fillTable(0x3B, 10, 1, {
      { 0x12, GETRC(fdivs), 5 },
      { 0x14, GETRC(fsubs), 5 },
      { 0x15, GETRC(fadds), 5 },
      { 0x16, GETRC(fsqrts), 5 },
      { 0x18, GETRC(fres), 5 },
      { 0x19, GETRC(fmuls), 5 },
      { 0x1C, GETRC(fmsubs), 5 },
      { 0x1D, GETRC(fmadds), 5 },
      { 0x1E, GETRC(fnmsubs), 5 },
      { 0x1F, GETRC(fnmadds), 5 },
  	});
  	// Group 0x3E opcodes (field 30..31)
  	fillTable(0x3E, 2, 0, {
      { 0x0, GET(std) },
      { 0x1, GET(stdu) },
  	});
  	// Group 0x3F opcodes (field 21..30)
  	fillTable(0x3F, 10, 1, {
      { 0x026, GETRC(mtfsb1) },
      { 0x040, GETRC(mcrfs) },
      { 0x046, GETRC(mtfsb0) },
      { 0x086, GETRC(mtfsfi) },
      { 0x247, GETRC(mffs) },
      { 0x2C7, GETRC(mtfsf) },

      { 0x000, GET(fcmpu) },
      { 0x00C, GETRC(frsp) },
      { 0x00E, GETRC(fctiw) },
      { 0x00F, GETRC(fctiwz) },

      { 0x012, GETRC(fdiv), 5 },
      { 0x014, GETRC(fsub), 5 },
      { 0x015, GETRC(fadd), 5 },
      { 0x016, GETRC(fsqrt), 5 },
      { 0x017, GETRC(fsel), 5 },
      { 0x019, GETRC(fmul), 5 },
      { 0x01A, GETRC(frsqrte), 5 },
      { 0x01C, GETRC(fmsub), 5 },
      { 0x01D, GETRC(fmadd), 5 },
      { 0x01E, GETRC(fnmsub), 5 },
      { 0x01F, GETRC(fnmadd), 5 },

      { 0x020, GET(fcmpo) },
      { 0x028, GETRC(fneg) },
      { 0x048, GETRC(fmr) },
      { 0x088, GETRC(fnabs) },
      { 0x108, GETRC(fabs) },
      { 0x32E, GETRC(fctid) },
      { 0x32F, GETRC(fctidz) },
      { 0x34E, GETRC(fcfid) },
  	});
  }	
  std::string legacy_GetOpcodeName(u32 instrData) {
    u32 OPCD = ExtractBits(instrData, 0, 5);

    u32 XO_20to30 = ExtractBits(instrData, 20, 30);
    u32 XO_27to29 = ExtractBits(instrData, 27, 29);
    u32 XO_27to30 = ExtractBits(instrData, 27, 30);
    u32 XO_21to30 = ExtractBits(instrData, 21, 30);
    u32 XO_22to30 = ExtractBits(instrData, 22, 30);
    u32 XO_21to29 = ExtractBits(instrData, 21, 29);
    u32 XO_30to31 = ExtractBits(instrData, 30, 31);

    switch (OPCD) {
    case 2: // tdi
      return "tdi";
      break;
    case 3: // twi
      return "twi";
      break;
    case 7: // mulli
      return "mulli";
      break;
    case 8: // subfic
      return "subfic";
      break;
    case 10: // cmpli
      return "cmpli";
      break;
    case 11: // cmpi
      return "cmpi";
      break;
    case 12: // addic
      return "addic";
      break;
    case 13: // addic.
      return "addicx";
      break;
    case 14: // addi
      return "addi";
      break;
    case 15: // addis
      return "addis";
      break;
    case 16: // bcx
      return "bcx";
      break;
    case 17: // sc
      return "sc";
      break;
    case 18: // bx
      return "bx";
      break;
    case 19: /* Subgroup 19 */
      switch (XO_20to30) {
      case 0: // mcrf
        return "mcrf";
        break;
      case 16: // bclrx
        return "bclrx";
        break;
      case 18: // rfid
        return "rfid";
        break;
      case 33: // crnor
        return "crnor";
        break;
      case 129: // crandc
        return "crandc";
        break;
      case 150: // isync
        return "isync";
        break;
      case 193: // crxor
        return "crxor";
        break;
      case 225: // crnand
        return "crnand";
        break;
      case 257: // crand
        return "crand";
        break;
      case 289: // creqv
        return "creqv";
        break;
      case 417: // crorc
        return "crorc";
        break;
      case 449: // cror
        return "cror";
        break;
      case 528: // bcctrx
        return "bcctrx";
        break;
      }
      return "Unknown instruction";
      break;
    case 20: // rlwimix
      return "rlwimix";
      break;
    case 21: // rlwinmx
      return "rlwinmx";
      break;
    case 23: // rlwnmx
      return "rlwnmx";
      break;
    case 24: // ori
      return "ori";
      break;
    case 25: // oris
      return "oris";
      break;
    case 26: // xori
      return "xori";
      break;
    case 27: // xoris
      return "xoris";
      break;
    case 28: // andi.
      return "andix";
      break;
    case 29: // andis.
      return "andisx";
      break;
    case 30: /* Subgroup 30 */
      switch (XO_27to29) {
      case 0: // rldiclx
        return "rldiclx";
        break;
      case 1: // rldicrx
        return "rldicrx";
        break;
      case 2: // rldicx
        return "rldicx";
        break;
      case 3: // rldimix
        return "rldimix";
        break;
      }
      switch (XO_27to30) {
      case 8: // rldclx
        return "rldclx";
        break;
      case 9: // rldcrx
        return "rldcrx";
        break;
      }
      return "Unknown instruction";
      break;
    case 31: /* Subgroup 31 */
      switch (XO_21to30) {
      case 0: // cmp
        return "cmp";
        break;
      case 4: // tw
        return "tw";
        break;
      case 19: // mfcr
        return "mfcr";
        break;
      case 20: // lwarx
        return "lwarx";
        break;
      case 21: // ldx
        return "ldx";
        break;
      case 23: // lwzx
        return "lwzx";
        break;
      case 24: // slwx
        return "slwx";
        break;
      case 26: // cntlzwx
        return "cntlzwx";
        break;
      case 27: // sldx
        return "sldx";
        break;
      case 28: // andx
        return "andx";
        break;
      case 32: // cmpl
        return "cmpl";
        break;
      case 53: // ldux
        return "ldux";
        break;
      case 54: // dcbst
        return "dcbst";
        break;
      case 55: // lwzux
        return "lwzux";
        break;
      case 58: // cntlzdx
        return "cntlzdx";
        break;
      case 60: // andcx
        return "andcx";
        break;
      case 68: // td
        return "td";
        break;
      case 83: // mfmsr
        return "mfmsr";
        break;
      case 84: // ldarx
        return "ldarx";
        break;
      case 86: // dcbf
        return "dcbf";
        break;
      case 87: // lbzx
        return "lbzx";
        break;
      case 119: // lbzux
        return "lbzux";
        break;
      case 124: // norx
        return "norx";
        break;
      case 144: // mtcrf
        return "mtcrf";
        break;
      case 146: // mtmsr
        return "mtmsr";
        break;
      case 149: // stdx
        return "stdx";
        break;
      case 150: // stwcx.
        return "stwcx";
        break;
      case 151: // stwx
        return "stwx";
        break;
      case 178: // mtmsrd
        return "mtmsrd";
        break;
      case 181: // stdux
        return "stdux";
        break;
      case 183: // stwux
        return "stwux";
        break;
      case 210: // mtsr
        return "mtsr";
        break;
      case 214: // stdcx.
        return "stdcx";
        break;
      case 215: // stbx
        return "stbx";
        break;
      case 242: // mtsrin
        return "mtsrin";
        break;
      case 247: // stbux
        return "stbux";
        break;
      case 246: // dcbt
        return "dcbt";
        break;
      case 278: // dcbt
        return "dcbt";
        break;
      case 279: // lhzx
        return "lhzx";
        break;
      case 284: // eqvx
        return "eqvx";
        break;
      case 274: // tlbiel
        return "tlbiel";
        break;
      case 306: // tlbie
        return "tlbie";
        break;
      case 310: // eciwx
        return "eciwx";
        break;
      case 311: // lhzux
        return "lhzux";
        break;
      case 316: // xorx
        return "xorx";
        break;
      case 339: // mfspr
        return "mfspr";
        break;
      case 341: // lwax
        return "lwax";
        break;
      case 343: // lhax
        return "lhax";
        break;
      case 370: // tlbia
        return "tlbia";
        break;
      case 371: // mftb
        return "mftb";
        break;
      case 373: // lwaux
        return "lwaux";
        break;
      case 375: // lhaux
        return "lhaux";
        break;
      case 407: // sthx
        return "sthx";
        break;
      case 412: // orcx
        return "orcx";
        break;
      case 434: // slbie
        return "slbie";
        break;
      case 438: // ecowx
        return "ecowx";
        break;
      case 439: // sthux
        return "sthux";
        break;
      case 444: // orx
        return "orx";
        break;
      case 467: // mtspr
        return "mtspr";
        break;
      case 476: // nandx
        return "nandx";
        break;
      case 498: // slbia
        return "slbia";
        break;
      case 533: // lswx
        return "lswx";
        break;
      case 534: // lwbrx
        return "lwbrx";
        break;
      case 535: // lfsx
        return "lfsx";
        break;
      case 536: // srwx
        return "srwx";
        break;
      case 539: // srdx
        return "srdx";
        break;
      case 566: // tlbsync
        return "tlbsync";
        break;
      case 567: // lfsux
        return "lfsux";
        break;
      case 595: // mfsr
        return "mfsr";
        break;
      case 597: // lswi
        return "lswi";
        break;
      case 598: // sync
        return "sync";
        break;
      case 599: // lfdx
        return "lfdx";
        break;
      case 631: // lfdux
        return "lfdux";
        break;
      case 569: // mfsrin
        return "mfsrin";
        break;
      case 915: // slbmfee
        return "slbmfee";
        break;
      case 851: // slbmfev
        return "slbmfev";
        break;
      case 402: // slbmte
        return "slbmte";
        break;
      case 661: // stswx
        return "stswx";
        break;
      case 662: // stwbrx
        return "stwbrx";
        break;
      case 663: // stfsx
        return "stfsx";
        break;
      case 695: // stfsux
        return "stfsux";
        break;
      case 725: // stswi
        return "stswi";
        break;
      case 727: // stfdx
        return "stfdx";
        break;
      case 759: // stfdux
        return "stfdux";
        break;
      case 790: // lhbrx
        return "lhbrx";
        break;
      case 762: // srawx
        return "srawx";
        break;
      case 794: // sradx
        return "sradx";
        break;
      case 824: // srawix
        return "srawix";
        break;
      case 854: // eieio
        return "eieio";
        break;
      case 918: // sthbrx
        return "sthbrx";
        break;
      case 922: // extshx
        return "extshx";
        break;
      case 954: // extsbx
        return "extsbx";
        break;
      case 982: // icbi
        return "icbi";
        break;
      case 983: // stfiwx
        return "stfiwx";
        break;
      case 986: // extswx
        return "extswx";
        break;
      case 1014: // dcbz
        return "dcbz";
        break;
      }
      switch (XO_22to30) {
      case 8: // subfcx
        return "subfcx";
        break;
      case 9: // mulhdux
        return "mulhdux";
        break;
      case 10: // addcx
        return "addcx";
        break;
      case 11: // mulhwux
        return "mulhwux";
        break;
      case 40: // subfx
        return "subfx";
        break;
      case 73: // mulhdx
        return "mulhdx";
        break;
      case 75: // mulhwx
        return "mulhwx";
        break;
      case 104: // negx
        return "negx";
        break;
      case 136: // subfex
        return "subfex";
        break;
      case 138: // addex
        return "addex";
        break;
      case 200: // subfzex
        return "subfzex";
        break;
      case 202: // addzex
        return "addzex";
        break;
      case 232: // subfmex
        return "subfmex";
        break;
      case 233: // mulldx
        return "mulldx";
        break;
      case 234: // addmex
        return "addmex";
        break;
      case 235: // mullwx
        return "mullwx";
        break;
      case 266: // addx
        return "addx";
        break;
      case 457: // divdux
        return "divdux";
        break;
      case 459: // divwux
        return "divwux";
        break;
      case 489: // divdx
        return "divdx";
        break;
      case 491: // divwx
        return "divwx";
        break;
      }
      switch (XO_21to29) {
      case 413: // sradix
        return "sradix";
        break;
      }
      return "Unknown instruction";
      break;
    case 32: // lwz
      return "lwz";
      break;
    case 33: // lwzu
      return "lwzu";
      break;
    case 34: // lbz
      return "lbz";
      break;
    case 35: // lbzu
      return "lbzu";
      break;
    case 36: // stw
      return "stw";
      break;
    case 37: // stwu
      return "stwu";
      break;
    case 38: // stb
      return "stb";
      break;
    case 39: // stbu
      return "stbu";
      break;
    case 40: // lhz
      return "lhz";
      break;
    case 41: // lhzu
      return "lhzu";
      break;
    case 44: // sth
      return "sth";
      break;
    case 48: // lfs
      return "lfs";
      break;
    case 58:
      switch (XO_30to31) {
      case 0: // ld
        return "ld";
        break;
      case 1: // ldu
        return "ldu";
        break;
      default:
        return "Unknown instruction";
        break;
      }
    case 62:
      switch (XO_30to31) {
      case 0: // std
        return "std";
        break;
      case 1: // stdu
        return "stdu";
        break;
      default:
        return "Unknown instruction";
        break;
      }
    default:
      return "Unknown instruction";
      break;
    }
  }
}