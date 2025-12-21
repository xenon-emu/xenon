/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#pragma once

#include <array>
#include <string>

#include "Base/Hash.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/PPU/PPCOpcodes.h"
#include "Core/XCPU/JIT/PPU_JIT.h"

constexpr u64 PPCRotateMask(u32 mb, u32 me) {
  const u64 mask = ~0ULL << (~(me - mb) & 63);
  return (mask >> (mb & 63)) | (mask << ((64 - mb) & 63));
}

constexpr u32 PPCDecode(u32 instr) {
  return ((instr >> 26) | (instr << 6)) & 0x1FFFF; // Rotate + mask
}

namespace PPCInterpreter {
  // Define a type alias for function pointers
  using instructionHandler = fptr<void(sPPEState *ppeState)>;
  using instructionHandlerJIT = fptr<void(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr)>;
  extern void PPCInterpreter_nop(sPPEState *ppeState);
  extern void PPCInterpreter_invalid(sPPEState *ppeState);
  extern void PPCInterpreter_known_unimplemented(const char *name, sPPEState *ppeState);
  extern const std::string PPCInterpreter_getFullName(u32 instr);
  extern void PPCInterpreterJIT_invalid(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr);
  class PPCDecoder {
    template <typename T>
    class InstrInfo {
    public:
      constexpr InstrInfo(u32 v, T p, T pRc, u32 m = 0) :
        value(v), ptr0(p), ptrRc(pRc), magn(m)
      {}

      constexpr InstrInfo(u32 v, const T* p, const T* pRc, u32 m = 0) :
        value(v), ptr0(*p), ptrRc(*pRc), magn(m)
      {}

      T ptr0;
      T ptrRc;
      u32 value;
      u32 magn; // Non-zero for "columns" (effectively, number of most significant bits "eaten")
      u32 mask;
    };
  public:
    void fillTables();
    void fillJITTables();
    void fillNameTables() {
#define GET_(name) std::string(#name)
#define GET(name) GET_(name), GET_(name)
#define GETRC(name) GET_(name##x), GET_(name##x)
      for (auto& x : nameTable) {
        x = GET(invalid);
      }
      // Main opcodes (field 0..5)
      fillTable<std::string>(nameTable, 0x00, 6, -1, {
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
      // Group 0x13 opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x13, 10, 1, {
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
      fillTable<std::string>(nameTable, 0x1E, 4, 1, {
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
      fillTable<std::string>(nameTable, 0x1F, 10, 1, {
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
      fillTable<std::string>(nameTable, 0x3A, 2, 0, {
        { 0x0, GET(ld) },
        { 0x1, GET(ldu) },
        { 0x2, GET(lwa) },
        });
      // Group 0x3B opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x3B, 10, 1, {
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
      fillTable<std::string>(nameTable, 0x3E, 2, 0, {
        { 0x0, GET(std) },
        { 0x1, GET(stdu) },
        });
      // Group 0x3F opcodes (field 21..30)
      fillTable<std::string>(nameTable, 0x3F, 10, 1, {
        { 0x026, GETRC(mtfsb1) },
        { 0x040, GET(mcrfs) },
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
    PPCDecoder();
    ~PPCDecoder() = default;
    const std::array<instructionHandler, 0x20000> &getTable() const noexcept {
      return table;
    }
    const std::array<instructionHandlerJIT, 0x20000> &getJITTable() const noexcept {
      return jitTable;
    }
    instructionHandler decode(u32 instr) const noexcept {
      if (instr == 0x60000000) {
        return &PPCInterpreter_nop;
      }

      instructionHandler handler = getTable()[PPCDecode(instr)];
      if (handler != PPCInterpreter_invalid) {
        return handler;
      } else {
#define GET_HANDLER(name) &PPCInterpreter_##name
        // VMX128 Lookup.
        switch (ExtractBits(instr, 0, 5)) {
        case 4:
          switch ((ExtractBits(instr, 21, 27) << 4) | (ExtractBits(instr, 30, 31) << 0)) {
          case 0b00000000011: handler = GET_HANDLER(lvsl128); break;
          case 0b00001000011: handler = GET_HANDLER(lvsr128); break;
          case 0b00010000011: handler = GET_HANDLER(lvewx128); break;
          case 0b00011000011: handler = GET_HANDLER(lvx128); break;
          case 0b00110000011: handler = GET_HANDLER(stvewx128); break;
          case 0b00111000011: handler = GET_HANDLER(stvx128); break;
          case 0b01011000011: handler = GET_HANDLER(lvxl128); break;
          case 0b01111000011: handler = GET_HANDLER(stvxl128); break;
          case 0b10000000011: handler = GET_HANDLER(lvlx128); break;
          case 0b10001000011: handler = GET_HANDLER(lvrx128); break;
          case 0b10100000011: handler = GET_HANDLER(stvlx128); break;
          case 0b10101000011: handler = GET_HANDLER(stvrx128); break;
          case 0b11000000011: handler = GET_HANDLER(lvlxl128); break;
          case 0b11001000011: handler = GET_HANDLER(lvrxl128); break;
          case 0b11100000011: handler = GET_HANDLER(stvlxl128); break;
          case 0b11101000011: handler = GET_HANDLER(stvrxl128); break;
          }
          switch ((ExtractBits(instr, 21, 31) << 0)) {
          case 0b00000000000: handler = GET_HANDLER(vaddubm); break;
          case 0b00000000010: handler = GET_HANDLER(vmaxub); break;
          case 0b00000000100: handler = GET_HANDLER(vrlb); break;
          case 0b00000001000: handler = GET_HANDLER(vmuloub); break;
          case 0b00000001010: handler = GET_HANDLER(vaddfp); break;
          case 0b00000001100: handler = GET_HANDLER(vmrghb); break;
          case 0b00000001110: handler = GET_HANDLER(vpkuhum); break;
          case 0b00001000000: handler = GET_HANDLER(vadduhm); break;
          case 0b00001000010: handler = GET_HANDLER(vmaxuh); break;
          case 0b00001000100: handler = GET_HANDLER(vrlh); break;
          case 0b00001001000: handler = GET_HANDLER(vmulouh); break;
          case 0b00001001010: handler = GET_HANDLER(vsubfp); break;
          case 0b00001001100: handler = GET_HANDLER(vmrghh); break;
          case 0b00001001110: handler = GET_HANDLER(vpkuwum); break;
          case 0b00010000000: handler = GET_HANDLER(vadduwm); break;
          case 0b00010000010: handler = GET_HANDLER(vmaxuw); break;
          case 0b00010000100: handler = GET_HANDLER(vrlw); break;
          case 0b00010001100: handler = GET_HANDLER(vmrghw); break;
          case 0b00010001110: handler = GET_HANDLER(vpkuhus); break;
          case 0b00011001110: handler = GET_HANDLER(vpkuwus); break;
          case 0b00100000010: handler = GET_HANDLER(vmaxsb); break;
          case 0b00100000100: handler = GET_HANDLER(vslb); break;
          case 0b00100001000: handler = GET_HANDLER(vmulosb); break;
          case 0b00100001010: handler = GET_HANDLER(vrefp); break;
          case 0b00100001100: handler = GET_HANDLER(vmrglb); break;
          case 0b00100001110: handler = GET_HANDLER(vpkshus); break;
          case 0b00101000010: handler = GET_HANDLER(vmaxsh); break;
          case 0b00101000100: handler = GET_HANDLER(vslh); break;
          case 0b00101001000: handler = GET_HANDLER(vmulosh); break;
          case 0b00101001010: handler = GET_HANDLER(vrsqrtefp); break;
          case 0b00101001100: handler = GET_HANDLER(vmrglh); break;
          case 0b00101001110: handler = GET_HANDLER(vpkswus); break;
          case 0b00110000000: handler = GET_HANDLER(vaddcuw); break;
          case 0b00110000010: handler = GET_HANDLER(vmaxsw); break;
          case 0b00110000100: handler = GET_HANDLER(vslw); break;
          case 0b00110001010: handler = GET_HANDLER(vexptefp); break;
          case 0b00110001100: handler = GET_HANDLER(vmrglw); break;
          case 0b00110001110: handler = GET_HANDLER(vpkshss); break;
          case 0b00111000100: handler = GET_HANDLER(vsl); break;
          case 0b00111001010: handler = GET_HANDLER(vlogefp); break;
          case 0b00111001110: handler = GET_HANDLER(vpkswss); break;
          case 0b01000000000: handler = GET_HANDLER(vaddubs); break;
          case 0b01000000010: handler = GET_HANDLER(vminub); break;
          case 0b01000000100: handler = GET_HANDLER(vsrb); break;
          case 0b01000001000: handler = GET_HANDLER(vmuleub); break;
          case 0b01000001010: handler = GET_HANDLER(vrfin); break;
          case 0b01000001100: handler = GET_HANDLER(vspltb); break;
          case 0b01000001110: handler = GET_HANDLER(vupkhsb); break;
          case 0b01001000000: handler = GET_HANDLER(vadduhs); break;
          case 0b01001000010: handler = GET_HANDLER(vminuh); break;
          case 0b01001000100: handler = GET_HANDLER(vsrh); break;
          case 0b01001001000: handler = GET_HANDLER(vmuleuh); break;
          case 0b01001001010: handler = GET_HANDLER(vrfiz); break;
          case 0b01001001100: handler = GET_HANDLER(vsplth); break;
          case 0b01001001110: handler = GET_HANDLER(vupkhsh); break;
          case 0b01010000000: handler = GET_HANDLER(vadduws); break;
          case 0b01010000010: handler = GET_HANDLER(vminuw); break;
          case 0b01010000100: handler = GET_HANDLER(vsrw); break;
          case 0b01010001010: handler = GET_HANDLER(vrfip); break;
          case 0b01010001100: handler = GET_HANDLER(vspltw); break;
          case 0b01010001110: handler = GET_HANDLER(vupklsb); break;
          case 0b01011000100: handler = GET_HANDLER(vsr); break;
          case 0b01011001010: handler = GET_HANDLER(vrfim); break;
          case 0b01011001110: handler = GET_HANDLER(vupklsh); break;
          case 0b01100000000: handler = GET_HANDLER(vaddsbs); break;
          case 0b01100000010: handler = GET_HANDLER(vminsb); break;
          case 0b01100000100: handler = GET_HANDLER(vsrab); break;
          case 0b01100001000: handler = GET_HANDLER(vmulesb); break;
          case 0b01100001010: handler = GET_HANDLER(vcfux); break;
          case 0b01100001100: handler = GET_HANDLER(vspltisb); break;
          case 0b01100001110: handler = GET_HANDLER(vpkpx); break;
          case 0b01101000000: handler = GET_HANDLER(vaddshs); break;
          case 0b01101000010: handler = GET_HANDLER(vminsh); break;
          case 0b01101000100: handler = GET_HANDLER(vsrah); break;
          case 0b01101001000: handler = GET_HANDLER(vmulesh); break;
          case 0b01101001010: handler = GET_HANDLER(vcfsx); break;
          case 0b01101001100: handler = GET_HANDLER(vspltish); break;
          case 0b01101001110: handler = GET_HANDLER(vupkhpx); break;
          case 0b01110000000: handler = GET_HANDLER(vaddsws); break;
          case 0b01110000010: handler = GET_HANDLER(vminsw); break;
          case 0b01110000100: handler = GET_HANDLER(vsraw); break;
          case 0b01110001010: handler = GET_HANDLER(vctuxs); break;
          case 0b01110001100: handler = GET_HANDLER(vspltisw); break;
          case 0b01111001010: handler = GET_HANDLER(vctsxs); break;
          case 0b01111001110: handler = GET_HANDLER(vupklpx); break;
          case 0b10000000000: handler = GET_HANDLER(vsububm); break;
          case 0b10000000010: handler = GET_HANDLER(vavgub); break;
          case 0b10000000100: handler = GET_HANDLER(vand); break;
          case 0b10000001010: handler = GET_HANDLER(vmaxfp); break;
          case 0b10000001100: handler = GET_HANDLER(vslo); break;
          case 0b10001000000: handler = GET_HANDLER(vsubuhm); break;
          case 0b10001000010: handler = GET_HANDLER(vavguh); break;
          case 0b10001000100: handler = GET_HANDLER(vandc); break;
          case 0b10001001010: handler = GET_HANDLER(vminfp); break;
          case 0b10001001100: handler = GET_HANDLER(vsro); break;
          case 0b10010000000: handler = GET_HANDLER(vsubuwm); break;
          case 0b10010000010: handler = GET_HANDLER(vavguw); break;
          case 0b10010000100: handler = GET_HANDLER(vor); break;
          case 0b10011000100: handler = GET_HANDLER(vxor); break;
          case 0b10100000010: handler = GET_HANDLER(vavgsb); break;
          case 0b10100000100: handler = GET_HANDLER(vnor); break;
          case 0b10101000010: handler = GET_HANDLER(vavgsh); break;
          case 0b10110000000: handler = GET_HANDLER(vsubcuw); break;
          case 0b10110000010: handler = GET_HANDLER(vavgsw); break;
          case 0b11000000000: handler = GET_HANDLER(vsububs); break;
          case 0b11000000100: handler = GET_HANDLER(mfvscr); break;
          case 0b11000001000: handler = GET_HANDLER(vsum4ubs); break;
          case 0b11001000000: handler = GET_HANDLER(vsubuhs); break;
          case 0b11001000100: handler = GET_HANDLER(mtvscr); break;
          case 0b11001001000: handler = GET_HANDLER(vsum4shs); break;
          case 0b11010000000: handler = GET_HANDLER(vsubuws); break;
          case 0b11010001000: handler = GET_HANDLER(vsum2sws); break;
          case 0b11100000000: handler = GET_HANDLER(vsubsbs); break;
          case 0b11100001000: handler = GET_HANDLER(vsum4sbs); break;
          case 0b11101000000: handler = GET_HANDLER(vsubshs); break;
          case 0b11110000000: handler = GET_HANDLER(vsubsws); break;
          case 0b11110001000: handler = GET_HANDLER(vsumsws); break;
          }
          switch ((ExtractBits(instr, 22, 31) << 0)) {
          case 0b0000000110: handler = GET_HANDLER(vcmpequb); break;
          case 0b0001000110: handler = GET_HANDLER(vcmpequh); break;
          case 0b0010000110: handler = GET_HANDLER(vcmpequwx); break;
          case 0b0011000110: handler = GET_HANDLER(vcmpeqfp); break;
          case 0b0111000110: handler = GET_HANDLER(vcmpgefp); break;
          case 0b1000000110: handler = GET_HANDLER(vcmpgtub); break;
          case 0b1001000110: handler = GET_HANDLER(vcmpgtuh); break;
          case 0b1010000110: handler = GET_HANDLER(vcmpgtuw); break;
          case 0b1011000110: handler = GET_HANDLER(vcmpgtfp); break;
          case 0b1100000110: handler = GET_HANDLER(vcmpgtsb); break;
          case 0b1101000110: handler = GET_HANDLER(vcmpgtsh); break;
          case 0b1110000110: handler = GET_HANDLER(vcmpgtsw); break;
          case 0b1111000110: handler = GET_HANDLER(vcmpbfp); break;
          }
          switch ((ExtractBits(instr, 26, 31) << 0)) {
          case 0b100000: handler = GET_HANDLER(vmhaddshs); break;
          case 0b100001: handler = GET_HANDLER(vmhraddshs); break;
          case 0b100010: handler = GET_HANDLER(vmladduhm); break;
          case 0b100100: handler = GET_HANDLER(vmsumubm); break;
          case 0b100101: handler = GET_HANDLER(vmsummbm); break;
          case 0b100110: handler = GET_HANDLER(vmsumuhm); break;
          case 0b100111: handler = GET_HANDLER(vmsumuhs); break;
          case 0b101000: handler = GET_HANDLER(vmsumshm); break;
          case 0b101001: handler = GET_HANDLER(vmsumshs); break;
          case 0b101010: handler = GET_HANDLER(vsel); break;
          case 0b101011: handler = GET_HANDLER(vperm); break;
          case 0b101100: handler = GET_HANDLER(vsldoi); break;
          case 0b101110: handler = GET_HANDLER(vmaddfp); break;
          case 0b101111: handler = GET_HANDLER(vnmsubfp); break;
          }
          switch ((ExtractBits(instr, 27, 27) << 0)) {
          case 0b1: handler = GET_HANDLER(vsldoi128); break;
          }
          break;
        case 5:
          switch ((ExtractBits(instr, 22, 22) << 5) | (ExtractBits(instr, 27, 27) << 0)) {
          case 0b000000: handler = GET_HANDLER(vperm128); break;
          }
          switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {
          case 0b000001: handler = GET_HANDLER(vaddfp128); break;
          case 0b000101: handler = GET_HANDLER(vsubfp128); break;
          case 0b001001: handler = GET_HANDLER(vmulfp128); break;
          case 0b001101: handler = GET_HANDLER(vmaddfp128); break;
          case 0b010001: handler = GET_HANDLER(vmaddcfp128); break;
          case 0b010101: handler = GET_HANDLER(vnmsubfp128); break;
          case 0b011001: handler = GET_HANDLER(vmsum3fp128); break;
          case 0b011101: handler = GET_HANDLER(vmsum4fp128); break;
          case 0b100000: handler = GET_HANDLER(vpkshss128); break;
          case 0b100001: handler = GET_HANDLER(vand128); break;
          case 0b100100: handler = GET_HANDLER(vpkshus128); break;
          case 0b100101: handler = GET_HANDLER(vandc128); break;
          case 0b101000: handler = GET_HANDLER(vpkswss128); break;
          case 0b101001: handler = GET_HANDLER(vnor128); break;
          case 0b101100: handler = GET_HANDLER(vpkswus128); break;
          case 0b101101: handler = GET_HANDLER(vor128); break;
          case 0b110000: handler = GET_HANDLER(vpkuhum128); break;
          case 0b110001: handler = GET_HANDLER(vxor128); break;
          case 0b110100: handler = GET_HANDLER(vpkuhus128); break;
          case 0b110101: handler = GET_HANDLER(vsel128); break;
          case 0b111000: handler = GET_HANDLER(vpkuwum128); break;
          case 0b111001: handler = GET_HANDLER(vslo128); break;
          case 0b111100: handler = GET_HANDLER(vpkuwus128); break;
          case 0b111101: handler = GET_HANDLER(vsro128); break;
          }
          break;
        case 6:
          switch ((ExtractBits(instr, 21, 22) << 5) | (ExtractBits(instr, 26, 27) << 0)) {
          case 0b0100001: handler = GET_HANDLER(vpermwi128); break;
          }
          switch ((ExtractBits(instr, 21, 23) << 4) | (ExtractBits(instr, 26, 27) << 0)) {
          case 0b1100001: handler = GET_HANDLER(vpkd3d128); break;
          case 0b1110001: handler = GET_HANDLER(vrlimi128); break;
          }
          switch ((ExtractBits(instr, 21, 27) << 0)) {
          case 0b0100011: handler = GET_HANDLER(vcfpsxws128); break;
          case 0b0100111: handler = GET_HANDLER(vcfpuxws128); break;
          case 0b0101011: handler = GET_HANDLER(vcsxwfp128); break;
          case 0b0101111: handler = GET_HANDLER(vcuxwfp128); break;
          case 0b0110011: handler = GET_HANDLER(vrfim128); break;
          case 0b0110111: handler = GET_HANDLER(vrfin128); break;
          case 0b0111011: handler = GET_HANDLER(vrfip128); break;
          case 0b0111111: handler = GET_HANDLER(vrfiz128); break;
          case 0b1100011: handler = GET_HANDLER(vrefp128); break;
          case 0b1100111: handler = GET_HANDLER(vrsqrtefp128); break;
          case 0b1101011: handler = GET_HANDLER(vexptefp128); break;
          case 0b1101111: handler = GET_HANDLER(vlogefp128); break;
          case 0b1110011: handler = GET_HANDLER(vspltw128); break;
          case 0b1110111: handler = GET_HANDLER(vspltisw128); break;
          case 0b1111111: handler = GET_HANDLER(vupkd3d128); break;
          }
          switch ((ExtractBits(instr, 22, 24) << 3) | (ExtractBits(instr, 27, 27) << 0)) {
          case 0b000000: handler = GET_HANDLER(vcmpeqfp128); break;
          case 0b001000: handler = GET_HANDLER(vcmpgefp128); break;
          case 0b010000: handler = GET_HANDLER(vcmpgtfp128); break;
          case 0b011000: handler = GET_HANDLER(vcmpbfp128); break;
          case 0b100000: handler = GET_HANDLER(vcmpequw128); break;
          }
          switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {
          case 0b000101: handler = GET_HANDLER(vrlw128); break;
          case 0b001101: handler = GET_HANDLER(vslw128); break;
          case 0b010101: handler = GET_HANDLER(vsraw128); break;
          case 0b011101: handler = GET_HANDLER(vsrw128); break;
          case 0b101000: handler = GET_HANDLER(vmaxfp128); break;
          case 0b101100: handler = GET_HANDLER(vminfp128); break;
          case 0b110000: handler = GET_HANDLER(vmrghw128); break;
          case 0b110100: handler = GET_HANDLER(vmrglw128); break;
          case 0b111000: handler = GET_HANDLER(vupkhsb128); break;
          case 0b111100: handler = GET_HANDLER(vupklsb128); break;
          }
          break;
        }
#undef GET_HANDLER
        return handler;
      }
    }
    instructionHandlerJIT decodeJIT(u32 instr) const noexcept {
      instructionHandlerJIT handler = getJITTable()[PPCDecode(instr)];
#define GET_HANDLER(name) &PPCInterpreterJIT_##name
      // VMX128 Lookup.
      switch (ExtractBits(instr, 0, 5)) {
      case 4:
        switch ((ExtractBits(instr, 21, 27) << 4) | (ExtractBits(instr, 30, 31) << 0)) {
        //case 0b00000000011: handler = GET_HANDLER(lvsl128); break;
        //case 0b00001000011: handler = GET_HANDLER(lvsr128); break;
        //case 0b00010000011: handler = GET_HANDLER(lvewx128); break;
        //case 0b00011000011: handler = GET_HANDLER(lvx128); break;
        //case 0b00110000011: handler = GET_HANDLER(stvewx128); break;
        //case 0b00111000011: handler = GET_HANDLER(stvx128); break;
        //case 0b01011000011: handler = GET_HANDLER(lvxl128); break;
        //case 0b01111000011: handler = GET_HANDLER(stvxl128); break;
        //case 0b10000000011: handler = GET_HANDLER(lvlx128); break;
        //case 0b10001000011: handler = GET_HANDLER(lvrx128); break;
        //case 0b10100000011: handler = GET_HANDLER(stvlx128); break;
        //case 0b10101000011: handler = GET_HANDLER(stvrx128); break;
        //case 0b11000000011: handler = GET_HANDLER(lvlxl128); break;
        //case 0b11001000011: handler = GET_HANDLER(lvrxl128); break;
        //case 0b11100000011: handler = GET_HANDLER(stvlxl128); break;
        //case 0b11101000011: handler = GET_HANDLER(stvrxl128); break;
        }
        switch ((ExtractBits(instr, 21, 31) << 0)) {
        //case 0b00000000000: handler = GET_HANDLER(vaddubm); break;
        //case 0b00000000010: handler = GET_HANDLER(vmaxub); break;
        //case 0b00000000100: handler = GET_HANDLER(vrlb); break;
        //case 0b00000001000: handler = GET_HANDLER(vmuloub); break;
        case 0b00000001010: handler = GET_HANDLER(vaddfp); break;
        //case 0b00000001100: handler = GET_HANDLER(vmrghb); break;
        //case 0b00000001110: handler = GET_HANDLER(vpkuhum); break;
        //case 0b00001000000: handler = GET_HANDLER(vadduhm); break;
        //case 0b00001000010: handler = GET_HANDLER(vmaxuh); break;
        //case 0b00001000100: handler = GET_HANDLER(vrlh); break;
        //case 0b00001001000: handler = GET_HANDLER(vmulouh); break;
        case 0b00001001010: handler = GET_HANDLER(vsubfp); break;
        //case 0b00001001100: handler = GET_HANDLER(vmrghh); break;
        //case 0b00001001110: handler = GET_HANDLER(vpkuwum); break;
        //case 0b00010000000: handler = GET_HANDLER(vadduwm); break;
        //case 0b00010000010: handler = GET_HANDLER(vmaxuw); break;
        //case 0b00010000100: handler = GET_HANDLER(vrlw); break;
        //case 0b00010001100: handler = GET_HANDLER(vmrghw); break;
        //case 0b00010001110: handler = GET_HANDLER(vpkuhus); break;
        //case 0b00011001110: handler = GET_HANDLER(vpkuwus); break;
        //case 0b00100000010: handler = GET_HANDLER(vmaxsb); break;
        case 0b00100000100: handler = GET_HANDLER(vslb); break;
        //case 0b00100001000: handler = GET_HANDLER(vmulosb); break;
        //case 0b00100001010: handler = GET_HANDLER(vrefp); break;
        //case 0b00100001100: handler = GET_HANDLER(vmrglb); break;
        //case 0b00100001110: handler = GET_HANDLER(vpkshus); break;
        //case 0b00101000010: handler = GET_HANDLER(vmaxsh); break;
        case 0b00101000100: handler = GET_HANDLER(vslh); break;
        //case 0b00101001000: handler = GET_HANDLER(vmulosh); break;
        //case 0b00101001010: handler = GET_HANDLER(vrsqrtefp); break;
        //case 0b00101001100: handler = GET_HANDLER(vmrglh); break;
        //case 0b00101001110: handler = GET_HANDLER(vpkswus); break;
        case 0b00110000000: handler = GET_HANDLER(vaddcuw); break;
        //case 0b00110000010: handler = GET_HANDLER(vmaxsw); break;
        case 0b00110000100: handler = GET_HANDLER(vslw); break;
        //case 0b00110001010: handler = GET_HANDLER(vexptefp); break;
        //case 0b00110001100: handler = GET_HANDLER(vmrglw); break;
        //case 0b00110001110: handler = GET_HANDLER(vpkshss); break;
        //case 0b00111000100: handler = GET_HANDLER(vsl); break;
        //case 0b00111001010: handler = GET_HANDLER(vlogefp); break;
        //case 0b00111001110: handler = GET_HANDLER(vpkswss); break;
        //case 0b01000000000: handler = GET_HANDLER(vaddubs); break;
        //case 0b01000000010: handler = GET_HANDLER(vminub); break;
        //case 0b01000000100: handler = GET_HANDLER(vsrb); break;
        //case 0b01000001000: handler = GET_HANDLER(vmuleub); break;
        case 0b01000001010: handler = GET_HANDLER(vrfin); break;
        case 0b01000001100: handler = GET_HANDLER(vspltb); break;
        //case 0b01000001110: handler = GET_HANDLER(vupkhsb); break;
        //case 0b01001000000: handler = GET_HANDLER(vadduhs); break;
        //case 0b01001000010: handler = GET_HANDLER(vminuh); break;
        //case 0b01001000100: handler = GET_HANDLER(vsrh); break;
        //case 0b01001001000: handler = GET_HANDLER(vmuleuh); break;
        case 0b01001001010: handler = GET_HANDLER(vrfiz); break;
        case 0b01001001100: handler = GET_HANDLER(vsplth); break;
        //case 0b01001001110: handler = GET_HANDLER(vupkhsh); break;
        //case 0b01010000000: handler = GET_HANDLER(vadduws); break;
        //case 0b01010000010: handler = GET_HANDLER(vminuw); break;
        case 0b01010000100: handler = GET_HANDLER(vsrw); break;
        case 0b01010001010: handler = GET_HANDLER(vrfip); break;
        case 0b01010001100: handler = GET_HANDLER(vspltw); break;
        //case 0b01010001110: handler = GET_HANDLER(vupklsb); break;
        //case 0b01011000100: handler = GET_HANDLER(vsr); break;
        case 0b01011001010: handler = GET_HANDLER(vrfim); break;
        //case 0b01011001110: handler = GET_HANDLER(vupklsh); break;
        //case 0b01100000000: handler = GET_HANDLER(vaddsbs); break;
        //case 0b01100000010: handler = GET_HANDLER(vminsb); break;
        //case 0b01100000100: handler = GET_HANDLER(vsrab); break;
        //case 0b01100001000: handler = GET_HANDLER(vmulesb); break;
        //case 0b01100001010: handler = GET_HANDLER(vcfux); break;
        case 0b01100001100: handler = GET_HANDLER(vspltisb); break;
        //case 0b01100001110: handler = GET_HANDLER(vpkpx); break;
        //case 0b01101000000: handler = GET_HANDLER(vaddshs); break;
        //case 0b01101000010: handler = GET_HANDLER(vminsh); break;
        //case 0b01101000100: handler = GET_HANDLER(vsrah); break;
        //case 0b01101001000: handler = GET_HANDLER(vmulesh); break;
        //case 0b01101001010: handler = GET_HANDLER(vcfsx); break;
        case 0b01101001100: handler = GET_HANDLER(vspltish); break;
        //case 0b01101001110: handler = GET_HANDLER(vupkhpx); break;
        //case 0b01110000000: handler = GET_HANDLER(vaddsws); break;
        //case 0b01110000010: handler = GET_HANDLER(vminsw); break;
        //case 0b01110000100: handler = GET_HANDLER(vsraw); break;
        //case 0b01110001010: handler = GET_HANDLER(vctuxs); break;
        case 0b01110001100: handler = GET_HANDLER(vspltisw); break;
        //case 0b01111001010: handler = GET_HANDLER(vctsxs); break;
        //case 0b01111001110: handler = GET_HANDLER(vupklpx); break;
        //case 0b10000000000: handler = GET_HANDLER(vsububm); break;
        //case 0b10000000010: handler = GET_HANDLER(vavgub); break;
        case 0b10000000100: handler = GET_HANDLER(vand); break;
        case 0b10000001010: handler = GET_HANDLER(vmaxfp); break;
        //case 0b10000001100: handler = GET_HANDLER(vslo); break;
        //case 0b10001000000: handler = GET_HANDLER(vsubuhm); break;
        //case 0b10001000010: handler = GET_HANDLER(vavguh); break;
        case 0b10001000100: handler = GET_HANDLER(vandc); break;
        case 0b10001001010: handler = GET_HANDLER(vminfp); break;
        //case 0b10001001100: handler = GET_HANDLER(vsro); break;
        //case 0b10010000000: handler = GET_HANDLER(vsubuwm); break;
        //case 0b10010000010: handler = GET_HANDLER(vavguw); break;
        case 0b10010000100: handler = GET_HANDLER(vor); break;
        case 0b10011000100: handler = GET_HANDLER(vxor); break;
        //case 0b10100000010: handler = GET_HANDLER(vavgsb); break;
        case 0b10100000100: handler = GET_HANDLER(vnor); break;
        //case 0b10101000010: handler = GET_HANDLER(vavgsh); break;
        //case 0b10110000000: handler = GET_HANDLER(vsubcuw); break;
        //case 0b10110000010: handler = GET_HANDLER(vavgsw); break;
        //case 0b11000000000: handler = GET_HANDLER(vsububs); break;
        //case 0b11000000100: handler = GET_HANDLER(mfvscr); break;
        //case 0b11000001000: handler = GET_HANDLER(vsum4ubs); break;
        //case 0b11001000000: handler = GET_HANDLER(vsubuhs); break;
        //case 0b11001000100: handler = GET_HANDLER(mtvscr); break;
        //case 0b11001001000: handler = GET_HANDLER(vsum4shs); break;
        //case 0b11010000000: handler = GET_HANDLER(vsubuws); break;
        //case 0b11010001000: handler = GET_HANDLER(vsum2sws); break;
        //case 0b11100000000: handler = GET_HANDLER(vsubsbs); break;
        //case 0b11100001000: handler = GET_HANDLER(vsum4sbs); break;
        //case 0b11101000000: handler = GET_HANDLER(vsubshs); break;
        //case 0b11110000000: handler = GET_HANDLER(vsubsws); break;
        //case 0b11110001000: handler = GET_HANDLER(vsumsws); break;
        }
        switch ((ExtractBits(instr, 22, 31) << 0)) {
        //case 0b0000000110: handler = GET_HANDLER(vcmpequb); break;
        //case 0b0001000110: handler = GET_HANDLER(vcmpequh); break;
        //case 0b0010000110: handler = GET_HANDLER(vcmpequwx); break;
        //case 0b0011000110: handler = GET_HANDLER(vcmpeqfp); break;
        //case 0b0111000110: handler = GET_HANDLER(vcmpgefp); break;
        //case 0b1000000110: handler = GET_HANDLER(vcmpgtub); break;
        //case 0b1001000110: handler = GET_HANDLER(vcmpgtuh); break;
        //case 0b1010000110: handler = GET_HANDLER(vcmpgtuw); break;
        //case 0b1011000110: handler = GET_HANDLER(vcmpgtfp); break;
        //case 0b1100000110: handler = GET_HANDLER(vcmpgtsb); break;
        //case 0b1101000110: handler = GET_HANDLER(vcmpgtsh); break;
        //case 0b1110000110: handler = GET_HANDLER(vcmpgtsw); break;
        //case 0b1111000110: handler = GET_HANDLER(vcmpbfp); break;
        }
        switch ((ExtractBits(instr, 26, 31) << 0)) {
        //case 0b100000: handler = GET_HANDLER(vmhaddshs); break;
        //case 0b100001: handler = GET_HANDLER(vmhraddshs); break;
        //case 0b100010: handler = GET_HANDLER(vmladduhm); break;
        //case 0b100100: handler = GET_HANDLER(vmsumubm); break;
        //case 0b100101: handler = GET_HANDLER(vmsummbm); break;
        //case 0b100110: handler = GET_HANDLER(vmsumuhm); break;
        //case 0b100111: handler = GET_HANDLER(vmsumuhs); break;
        //case 0b101000: handler = GET_HANDLER(vmsumshm); break;
        //case 0b101001: handler = GET_HANDLER(vmsumshs); break;
        case 0b101010: handler = GET_HANDLER(vsel); break;
        //case 0b101011: handler = GET_HANDLER(vperm); break;
        //case 0b101100: handler = GET_HANDLER(vsldoi); break;
        //case 0b101110: handler = GET_HANDLER(vmaddfp); break;
        //case 0b101111: handler = GET_HANDLER(vnmsubfp); break;
        }
        switch ((ExtractBits(instr, 27, 27) << 0)) {
        //case 0b1: handler = GET_HANDLER(vsldoi128); break;
        }
        break;
      case 5:
        switch ((ExtractBits(instr, 22, 22) << 5) | (ExtractBits(instr, 27, 27) << 0)) {
        //case 0b000000: handler = GET_HANDLER(vperm128); break;
        }
        switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {
        case 0b000001: handler = GET_HANDLER(vaddfp128); break;
        case 0b000101: handler = GET_HANDLER(vsubfp128); break;
        //case 0b001001: handler = GET_HANDLER(vmulfp128); break;
        //case 0b001101: handler = GET_HANDLER(vmaddfp128); break;
        //case 0b010001: handler = GET_HANDLER(vmaddcfp128); break;
        //case 0b010101: handler = GET_HANDLER(vnmsubfp128); break;
        //case 0b011001: handler = GET_HANDLER(vmsum3fp128); break;
        //case 0b011101: handler = GET_HANDLER(vmsum4fp128); break;
        //case 0b100000: handler = GET_HANDLER(vpkshss128); break;
        case 0b100001: handler = GET_HANDLER(vand128); break;
        //case 0b100100: handler = GET_HANDLER(vpkshus128); break;
        case 0b100101: handler = GET_HANDLER(vandc128); break;
        //case 0b101000: handler = GET_HANDLER(vpkswss128); break;
        case 0b101001: handler = GET_HANDLER(vnor128); break;
        //case 0b101100: handler = GET_HANDLER(vpkswus128); break;
        case 0b101101: handler = GET_HANDLER(vor128); break;
        //case 0b110000: handler = GET_HANDLER(vpkuhum128); break;
        case 0b110001: handler = GET_HANDLER(vxor128); break;
        //case 0b110100: handler = GET_HANDLER(vpkuhus128); break;
        case 0b110101: handler = GET_HANDLER(vsel128); break;
        //case 0b111000: handler = GET_HANDLER(vpkuwum128); break;
        //case 0b111001: handler = GET_HANDLER(vslo128); break;
        //case 0b111100: handler = GET_HANDLER(vpkuwus128); break;
        //case 0b111101: handler = GET_HANDLER(vsro128); break;
        }
        break;
      case 6:
        switch ((ExtractBits(instr, 21, 22) << 5) | (ExtractBits(instr, 26, 27) << 0)) {
        //case 0b0100001: handler = GET_HANDLER(vpermwi128); break;
        }
        switch ((ExtractBits(instr, 21, 23) << 4) | (ExtractBits(instr, 26, 27) << 0)) {
        //case 0b1100001: handler = GET_HANDLER(vpkd3d128); break;
        //case 0b1110001: handler = GET_HANDLER(vrlimi128); break;
        }
        switch ((ExtractBits(instr, 21, 27) << 0)) {
        //case 0b0100011: handler = GET_HANDLER(vcfpsxws128); break;
        //case 0b0100111: handler = GET_HANDLER(vcfpuxws128); break;
        //case 0b0101011: handler = GET_HANDLER(vcsxwfp128); break;
        //case 0b0101111: handler = GET_HANDLER(vcuxwfp128); break;
        case 0b0110011: handler = GET_HANDLER(vrfim128); break;
        case 0b0110111: handler = GET_HANDLER(vrfin128); break;
        case 0b0111011: handler = GET_HANDLER(vrfip128); break;
        case 0b0111111: handler = GET_HANDLER(vrfiz128); break;
        //case 0b1100011: handler = GET_HANDLER(vrefp128); break;
        //case 0b1100111: handler = GET_HANDLER(vrsqrtefp128); break;
        //case 0b1101011: handler = GET_HANDLER(vexptefp128); break;
        //case 0b1101111: handler = GET_HANDLER(vlogefp128); break;
        case 0b1110011: handler = GET_HANDLER(vspltw128); break;
        case 0b1110111: handler = GET_HANDLER(vspltisw128); break;
        //case 0b1111111: handler = GET_HANDLER(vupkd3d128); break;
        }
        switch ((ExtractBits(instr, 22, 24) << 3) | (ExtractBits(instr, 27, 27) << 0)) {
        //case 0b000000: handler = GET_HANDLER(vcmpeqfp128); break;
        //case 0b001000: handler = GET_HANDLER(vcmpgefp128); break;
        //case 0b010000: handler = GET_HANDLER(vcmpgtfp128); break;
        //case 0b011000: handler = GET_HANDLER(vcmpbfp128); break;
        //case 0b100000: handler = GET_HANDLER(vcmpequw128); break;
        }
        switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {
        //case 0b000101: handler = GET_HANDLER(vrlw128); break;
        case 0b001101: handler = GET_HANDLER(vslw128); break;
        //case 0b010101: handler = GET_HANDLER(vsraw128); break;
        case 0b011101: handler = GET_HANDLER(vsrw128); break;
        case 0b101000: handler = GET_HANDLER(vmaxfp128); break;
        case 0b101100: handler = GET_HANDLER(vminfp128); break;
        //case 0b110000: handler = GET_HANDLER(vmrghw128); break;
        //case 0b110100: handler = GET_HANDLER(vmrglw128); break;
        //case 0b111000: handler = GET_HANDLER(vupkhsb128); break;
        //case 0b111100: handler = GET_HANDLER(vupklsb128); break;
        }
        break;
      }
#undef GET_HANDLER
      return handler;
    }
    const std::array<std::string, 0x20000> &getNameTable() const noexcept {
      return nameTable;
    }
    std::string decodeName(u32 instr) const noexcept {
      std::string name = "";

      // VMX128 Lookup.
      switch (ExtractBits(instr, 0, 5)) {
      case 4:
        switch ((ExtractBits(instr, 21, 27) << 4) | (ExtractBits(instr, 30, 31) << 0)) {
        case 0b00000000011: name = GET_(lvsl128); break;
        case 0b00001000011: name = GET_(lvsr128); break;
        case 0b00010000011: name = GET_(lvewx128); break;
        case 0b00011000011: name = GET_(lvx128); break;
        case 0b00110000011: name = GET_(stvewx128); break;
        case 0b00111000011: name = GET_(stvx128); break;
        case 0b01011000011: name = GET_(lvxl128); break;
        case 0b01111000011: name = GET_(stvxl128); break;
        case 0b10000000011: name = GET_(lvlx128); break;
        case 0b10001000011: name = GET_(lvrx128); break;
        case 0b10100000011: name = GET_(stvlx128); break;
        case 0b10101000011: name = GET_(stvrx128); break;
        case 0b11000000011: name = GET_(lvlxl128); break;
        case 0b11001000011: name = GET_(lvrxl128); break;
        case 0b11100000011: name = GET_(stvlxl128); break;
        case 0b11101000011: name = GET_(stvrxl128); break;
        }
        switch ((ExtractBits(instr, 21, 31) << 0)) {
        case 0b00000000000: name = GET_(vaddubm); break;
        case 0b00000000010: name = GET_(vmaxub); break;
        case 0b00000000100: name = GET_(vrlb); break;
        case 0b00000001000: name = GET_(vmuloub); break;
        case 0b00000001010: name = GET_(vaddfp); break;
        case 0b00000001100: name = GET_(vmrghb); break;
        case 0b00000001110: name = GET_(vpkuhum); break;
        case 0b00001000000: name = GET_(vadduhm); break;
        case 0b00001000010: name = GET_(vmaxuh); break;
        case 0b00001000100: name = GET_(vrlh); break;
        case 0b00001001000: name = GET_(vmulouh); break;
        case 0b00001001010: name = GET_(vsubfp); break;
        case 0b00001001100: name = GET_(vmrghh); break;
        case 0b00001001110: name = GET_(vpkuwum); break;
        case 0b00010000000: name = GET_(vadduwm); break;
        case 0b00010000010: name = GET_(vmaxuw); break;
        case 0b00010000100: name = GET_(vrlw); break;
        case 0b00010001100: name = GET_(vmrghw); break;
        case 0b00010001110: name = GET_(vpkuhus); break;
        case 0b00011001110: name = GET_(vpkuwus); break;
        case 0b00100000010: name = GET_(vmaxsb); break;
        case 0b00100000100: name = GET_(vslb); break;
        case 0b00100001000: name = GET_(vmulosb); break;
        case 0b00100001010: name = GET_(vrefp); break;
        case 0b00100001100: name = GET_(vmrglb); break;
        case 0b00100001110: name = GET_(vpkshus); break;
        case 0b00101000010: name = GET_(vmaxsh); break;
        case 0b00101000100: name = GET_(vslh); break;
        case 0b00101001000: name = GET_(vmulosh); break;
        case 0b00101001010: name = GET_(vrsqrtefp); break;
        case 0b00101001100: name = GET_(vmrglh); break;
        case 0b00101001110: name = GET_(vpkswus); break;
        case 0b00110000000: name = GET_(vaddcuw); break;
        case 0b00110000010: name = GET_(vmaxsw); break;
        case 0b00110000100: name = GET_(vslw); break;
        case 0b00110001010: name = GET_(vexptefp); break;
        case 0b00110001100: name = GET_(vmrglw); break;
        case 0b00110001110: name = GET_(vpkshss); break;
        case 0b00111000100: name = GET_(vsl); break;
        case 0b00111001010: name = GET_(vlogefp); break;
        case 0b00111001110: name = GET_(vpkswss); break;
        case 0b01000000000: name = GET_(vaddubs); break;
        case 0b01000000010: name = GET_(vminub); break;
        case 0b01000000100: name = GET_(vsrb); break;
        case 0b01000001000: name = GET_(vmuleub); break;
        case 0b01000001010: name = GET_(vrfin); break;
        case 0b01000001100: name = GET_(vspltb); break;
        case 0b01000001110: name = GET_(vupkhsb); break;
        case 0b01001000000: name = GET_(vadduhs); break;
        case 0b01001000010: name = GET_(vminuh); break;
        case 0b01001000100: name = GET_(vsrh); break;
        case 0b01001001000: name = GET_(vmuleuh); break;
        case 0b01001001010: name = GET_(vrfiz); break;
        case 0b01001001100: name = GET_(vsplth); break;
        case 0b01001001110: name = GET_(vupkhsh); break;
        case 0b01010000000: name = GET_(vadduws); break;
        case 0b01010000010: name = GET_(vminuw); break;
        case 0b01010000100: name = GET_(vsrw); break;
        case 0b01010001010: name = GET_(vrfip); break;
        case 0b01010001100: name = GET_(vspltw); break;
        case 0b01010001110: name = GET_(vupklsb); break;
        case 0b01011000100: name = GET_(vsr); break;
        case 0b01011001010: name = GET_(vrfim); break;
        case 0b01011001110: name = GET_(vupklsh); break;
        case 0b01100000000: name = GET_(vaddsbs); break;
        case 0b01100000010: name = GET_(vminsb); break;
        case 0b01100000100: name = GET_(vsrab); break;
        case 0b01100001000: name = GET_(vmulesb); break;
        case 0b01100001010: name = GET_(vcfux); break;
        case 0b01100001100: name = GET_(vspltisb); break;
        case 0b01100001110: name = GET_(vpkpx); break;
        case 0b01101000000: name = GET_(vaddshs); break;
        case 0b01101000010: name = GET_(vminsh); break;
        case 0b01101000100: name = GET_(vsrah); break;
        case 0b01101001000: name = GET_(vmulesh); break;
        case 0b01101001010: name = GET_(vcfsx); break;
        case 0b01101001100: name = GET_(vspltish); break;
        case 0b01101001110: name = GET_(vupkhpx); break;
        case 0b01110000000: name = GET_(vaddsws); break;
        case 0b01110000010: name = GET_(vminsw); break;
        case 0b01110000100: name = GET_(vsraw); break;
        case 0b01110001010: name = GET_(vctuxs); break;
        case 0b01110001100: name = GET_(vspltisw); break;
        case 0b01111001010: name = GET_(vctsxs); break;
        case 0b01111001110: name = GET_(vupklpx); break;
        case 0b10000000000: name = GET_(vsububm); break;
        case 0b10000000010: name = GET_(vavgub); break;
        case 0b10000000100: name = GET_(vand); break;
        case 0b10000001010: name = GET_(vmaxfp); break;
        case 0b10000001100: name = GET_(vslo); break;
        case 0b10001000000: name = GET_(vsubuhm); break;
        case 0b10001000010: name = GET_(vavguh); break;
        case 0b10001000100: name = GET_(vandc); break;
        case 0b10001001010: name = GET_(vminfp); break;
        case 0b10001001100: name = GET_(vsro); break;
        case 0b10010000000: name = GET_(vsubuwm); break;
        case 0b10010000010: name = GET_(vavguw); break;
        case 0b10010000100: name = GET_(vor); break;
        case 0b10011000100: name = GET_(vxor); break;
        case 0b10100000010: name = GET_(vavgsb); break;
        case 0b10100000100: name = GET_(vnor); break;
        case 0b10101000010: name = GET_(vavgsh); break;
        case 0b10110000000: name = GET_(vsubcuw); break;
        case 0b10110000010: name = GET_(vavgsw); break;
        case 0b11000000000: name = GET_(vsububs); break;
        case 0b11000000100: name = GET_(mfvscr); break;
        case 0b11000001000: name = GET_(vsum4ubs); break;
        case 0b11001000000: name = GET_(vsubuhs); break;
        case 0b11001000100: name = GET_(mtvscr); break;
        case 0b11001001000: name = GET_(vsum4shs); break;
        case 0b11010000000: name = GET_(vsubuws); break;
        case 0b11010001000: name = GET_(vsum2sws); break;
        case 0b11100000000: name = GET_(vsubsbs); break;
        case 0b11100001000: name = GET_(vsum4sbs); break;
        case 0b11101000000: name = GET_(vsubshs); break;
        case 0b11110000000: name = GET_(vsubsws); break;
        case 0b11110001000: name = GET_(vsumsws); break;
        }
        switch ((ExtractBits(instr, 22, 31) << 0)) {
        case 0b0000000110: name = GET_(vcmpequb); break;
        case 0b0001000110: name = GET_(vcmpequh); break;
        case 0b0010000110: name = GET_(vcmpequwx); break;
        case 0b0011000110: name = GET_(vcmpeqfp); break;
        case 0b0111000110: name = GET_(vcmpgefp); break;
        case 0b1000000110: name = GET_(vcmpgtub); break;
        case 0b1001000110: name = GET_(vcmpgtuh); break;
        case 0b1010000110: name = GET_(vcmpgtuw); break;
        case 0b1011000110: name = GET_(vcmpgtfp); break;
        case 0b1100000110: name = GET_(vcmpgtsb); break;
        case 0b1101000110: name = GET_(vcmpgtsh); break;
        case 0b1110000110: name = GET_(vcmpgtsw); break;
        case 0b1111000110: name = GET_(vcmpbfp); break;
        }
        switch ((ExtractBits(instr, 26, 31) << 0)) {
        case 0b100000: name = GET_(vmhaddshs); break;
        case 0b100001: name = GET_(vmhraddshs); break;
        case 0b100010: name = GET_(vmladduhm); break;
        case 0b100100: name = GET_(vmsumubm); break;
        case 0b100101: name = GET_(vmsummbm); break;
        case 0b100110: name = GET_(vmsumuhm); break;
        case 0b100111: name = GET_(vmsumuhs); break;
        case 0b101000: name = GET_(vmsumshm); break;
        case 0b101001: name = GET_(vmsumshs); break;
        case 0b101010: name = GET_(vsel); break;
        case 0b101011: name = GET_(vperm); break;
        case 0b101100: name = GET_(vsldoi); break;
        case 0b101110: name = GET_(vmaddfp); break;
        case 0b101111: name = GET_(vnmsubfp); break;
        }
        switch ((ExtractBits(instr, 27, 27) << 0)) {
        case 0b1: name = GET_(vsldoi128); break;
        }
        break;
      case 5:
        switch ((ExtractBits(instr, 22, 22) << 5) | (ExtractBits(instr, 27, 27) << 0)) {
        case 0b000000: name = GET_(vperm128); break;
        }
        switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {
        case 0b000001: name = GET_(vaddfp128); break;
        case 0b000101: name = GET_(vsubfp128); break;
        case 0b001001: name = GET_(vmulfp128); break;
        case 0b001101: name = GET_(vmaddfp128); break;
        case 0b010001: name = GET_(vmaddcfp128); break;
        case 0b010101: name = GET_(vnmsubfp128); break;
        case 0b011001: name = GET_(vmsum3fp128); break;
        case 0b011101: name = GET_(vmsum4fp128); break;
        case 0b100000: name = GET_(vpkshss128); break;
        case 0b100001: name = GET_(vand128); break;
        case 0b100100: name = GET_(vpkshus128); break;
        case 0b100101: name = GET_(vandc128); break;
        case 0b101000: name = GET_(vpkswss128); break;
        case 0b101001: name = GET_(vnor128); break;
        case 0b101100: name = GET_(vpkswus128); break;
        case 0b101101: name = GET_(vor128); break;
        case 0b110000: name = GET_(vpkuhum128); break;
        case 0b110001: name = GET_(vxor128); break;
        case 0b110100: name = GET_(vpkuhus128); break;
        case 0b110101: name = GET_(vsel128); break;
        case 0b111000: name = GET_(vpkuwum128); break;
        case 0b111001: name = GET_(vslo128); break;
        case 0b111100: name = GET_(vpkuwus128); break;
        case 0b111101: name = GET_(vsro128); break;
        }
        break;
      case 6:
        switch ((ExtractBits(instr, 21, 22) << 5) | (ExtractBits(instr, 26, 27) << 0)) {
        case 0b0100001: name = GET_(vpermwi128); break;
        }
        switch ((ExtractBits(instr, 21, 23) << 4) | (ExtractBits(instr, 26, 27) << 0)) {
        case 0b1100001: name = GET_(vpkd3d128); break;
        case 0b1110001: name = GET_(vrlimi128); break;
        }
        switch ((ExtractBits(instr, 21, 27) << 0)) {
        case 0b0100011: name = GET_(vcfpsxws128); break;
        case 0b0100111: name = GET_(vcfpuxws128); break;
        case 0b0101011: name = GET_(vcsxwfp128); break;
        case 0b0101111: name = GET_(vcuxwfp128); break;
        case 0b0110011: name = GET_(vrfim128); break;
        case 0b0110111: name = GET_(vrfin128); break;
        case 0b0111011: name = GET_(vrfip128); break;
        case 0b0111111: name = GET_(vrfiz128); break;
        case 0b1100011: name = GET_(vrefp128); break;
        case 0b1100111: name = GET_(vrsqrtefp128); break;
        case 0b1101011: name = GET_(vexptefp128); break;
        case 0b1101111: name = GET_(vlogefp128); break;
        case 0b1110011: name = GET_(vspltw128); break;
        case 0b1110111: name = GET_(vspltisw128); break;
        case 0b1111111: name = GET_(vupkd3d128); break;
        }
        switch ((ExtractBits(instr, 22, 24) << 3) | (ExtractBits(instr, 27, 27) << 0)) {
        case 0b000000: name = GET_(vcmpeqfp128); break;
        case 0b001000: name = GET_(vcmpgefp128); break;
        case 0b010000: name = GET_(vcmpgtfp128); break;
        case 0b011000: name = GET_(vcmpbfp128); break;
        case 0b100000: name = GET_(vcmpequw128); break;
        }
        switch ((ExtractBits(instr, 22, 25) << 2) | (ExtractBits(instr, 27, 27) << 0)) {
        case 0b000101: name = GET_(vrlw128); break;
        case 0b001101: name = GET_(vslw128); break;
        case 0b010101: name = GET_(vsraw128); break;
        case 0b011101: name = GET_(vsrw128); break;
        case 0b101000: name = GET_(vmaxfp128); break;
        case 0b101100: name = GET_(vminfp128); break;
        case 0b110000: name = GET_(vmrghw128); break;
        case 0b110100: name = GET_(vmrglw128); break;
        case 0b111000: name = GET_(vupkhsb128); break;
        case 0b111100: name = GET_(vupklsb128); break;
        }
        break;
      }

      if (name == "") { name = getNameTable()[PPCDecode(instr)]; }
      return name;
    }
  private:
    // Fast lookup table
    std::array<instructionHandler, 0x20000> table;
    std::array<instructionHandlerJIT, 0x20000> jitTable;
    std::array<std::string, 0x20000> nameTable;

    template <typename T>
    void fillTable(std::array<T, 0x20000> &t, u32 mainOp, u32 count, u32 sh,
      std::initializer_list<InstrInfo<T>> entries) noexcept {
      const bool isVxuMode = (count == static_cast<u32>(-1)) &&
        std::any_of(entries.begin(), entries.end(), [](const InstrInfo<T> &v) {
        return v.mask != 0;
          });

      for (const auto &v : entries) {
        if (isVxuMode) {
          // VXU-style masked mode
          for (u32 i = 0; i < 1u << 11; i++) {
            const u32 instr = i << 21;
            if ((instr & v.mask) == v.value) {
              const u32 key = (i << 6) | mainOp;
              c_at(t, key) = (i & 1) ? v.ptrRc : v.ptr0;
            }
          }
        } else if (sh < 11) {
          // Old-style table expansion
          for (u32 i = 0; i < 1u << (v.magn + (11 - sh - count)); i++) {
            for (u32 j = 0; j < 1u << sh; j++) {
              const u32 k = (((i << (count - v.magn)) | v.value) << sh) | j;
              c_at(t, (k << 6) | mainOp) = (k & 1) ? v.ptrRc : v.ptr0;
            }
          }
        } else {
          // Special fallback
          for (u32 i = 0; i < 1u << 11; i++) {
            c_at(t, (i << 6) | v.value) = (i & 1) ? v.ptrRc : v.ptr0;
          }
        }
      }
    }
  };
} // namespace PPCInterpreter
