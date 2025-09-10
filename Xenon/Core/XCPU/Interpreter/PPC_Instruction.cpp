// Copyright 2025 Xenon Emulator Project. All rights reserved.

/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include "Base/Global.h"
#include "Core/XCPU/Xenon.h"

#include "PPC_Instruction.h"
#include "PPCInterpreter.h"

namespace PPCInterpreter {
  void PPCInterpreter_nop(PPU_STATE *ppuState) {
    // Do nothing
  }

  void PPCInterpreter_invalid(PPU_STATE *ppuState) {
    LOG_CRITICAL(Xenon, "PPC Interpreter: Invalid instruction found! Data: 0x{:X} (opcode, value[s]), address: 0x{:X}",
      _instr.opcode,
      curThread.CIA);
  }

  void PPCInterpreter_known_unimplemented(const char *name, PPU_STATE *ppuState) {
    LOG_CRITICAL(Xenon, "PPC Interpreter: {} is not implemented! Data: 0x{:X}, address: 0x{:X}",
      name,
      _instr.opcode,
      curThread.CIA);
  }

  void PPCInterpreterJIT_invalid(PPU_STATE *ppuState, JITBlockBuilder *b, PPCOpcode instr) {
    u32 opcodeEntry = PPCDecode(instr.opcode);
    std::string opName = ppcDecoder.getNameTable()[opcodeEntry];
    LOG_DEBUG(Xenon, "JIT: No emitter found for opcode '{}' (0x{:08X}) at addr 0x{:X}", opName, instr.opcode, curThread.CIA);
  }

  PPCDecoder::PPCDecoder() {
    fillTables();
    fillNameTables();
    fillJITTables();
  }

  void PPCDecoder::fillJITTables() {
#undef GET_
#undef GET
#undef GETRC
#define GET_(name) &PPCInterpreterJIT_##name
#define GET(name) GET_(name), GET_(name)
#define GETRC(name) GET_(name##x), GET_(name##x)

    for (auto &x : jitTable) {
      x = GET(invalid);
    }

    // Main opcodes (field 0..5)
#if defined(ARCH_X86) || defined(ARCH_X86_64)
    fillTable<instructionHandlerJIT>(jitTable, 0x00, 6, -1, {
      { 0x0A, GET(cmpli) },
      { 0x0B, GET(cmpi) },
      //{ 0x0C, GET(addic) },
      //{ 0x0D, GET(addic) },
      { 0x0E, GET(addi) },
      { 0x0F, GET(addis) },
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
    });
    // Group 0x1E opcodes (field 27..30)
    fillTable<instructionHandlerJIT>(jitTable, 0x1E, 4, 1, {
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
    fillTable<instructionHandlerJIT>(jitTable, 0x1F, 10, 1, {
      { 0x000, GET(cmp) },
      //{ 0x00A, GETRC(addc) },
      { 0x013, GET(mfocrf) },
      { 0x018, GETRC(slw) },
      { 0x01B, GETRC(sld) },
      { 0x01C, GETRC(and) },
      { 0x020, GET(cmpl) },
      { 0x03A, GETRC(cntlzd) },
      { 0x03C, GETRC(andc) },
      { 0x07C, GETRC(nor) },
      { 0x08A, GETRC(adde) },
      { 0x0E9, GETRC(mulld) },
      { 0x10A, GETRC(add) },
      { 0x13C, GETRC(xor) },
      { 0x153, GET(mfspr) },
      { 0x19C, GETRC(orc) },
      { 0x1BC, GETRC(or) },
      { 0x1DC, GETRC(nand) },
      { 0x218, GETRC(srw) },
      { 0x21B, GETRC(srd) },
      //{ 0x338, GETRC(srawi) },
      { 0x33A, GETRC(sradi) },
      { 0x33B, GETRC(sradi) },
    });
#endif // defined ARCH_X86 || ARCH_X86_64

    #undef GET_
    #undef GET
    #undef GETRC
  }
  void PPCDecoder::fillTables() {
    #define GET_(name) &PPCInterpreter_##name
    #define GET(name) GET_(name), GET_(name)
    #define GETRC(name) GET_(name##x), GET_(name##x)
    for (auto& x : table) {
      x = GET(invalid);
    }
    // Main opcodes (field 0..5)
    fillTable<instructionHandler>(table, 0x00, 6, -1, {
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
    fillTable<instructionHandler>(table, 0x13, 10, 1, {
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
    fillTable<instructionHandler>(table, 0x1E, 4, 1, {
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
    fillTable<instructionHandler>(table, 0x1F, 10, 1, {
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
    fillTable<instructionHandler>(table, 0x3A, 2, 0, {
      { 0x0, GET(ld) },
      { 0x1, GET(ldu) },
      { 0x2, GET(lwa) },
    });
    // Group 0x3B opcodes (field 21..30)
    fillTable<instructionHandler>(table, 0x3B, 10, 1, {
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
    fillTable<instructionHandler>(table, 0x3E, 2, 0, {
      { 0x0, GET(std) },
      { 0x1, GET(stdu) },
    });
    // Group 0x3F opcodes (field 21..30)
    fillTable<instructionHandler>(table, 0x3F, 10, 1, {
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
    #undef GET_
    #undef GET
    #undef GETRC
  }

  constexpr std::pair<const char*, char> getBCInfo(u32 bo, u32 bi) {
    std::pair<const char*, char> info{};

    // handle bd(n)z(f|t) and bd(n)z
    switch (bo) {
      case 0b00000:
      case 0b00001: return { "bdnzf", 'f' };
      case 0b00010:
      case 0b00011: return { "bdzf", 'f' };
      case 0b01000:
      case 0b01001: return { "bdnzt", 't' };
      case 0b01010:
      case 0b01011: return { "bdzt", 't' };
      case 0b10010: return { "bdz", '\0' };
      case 0b11010: return { "bdz", '-' };
      case 0b11011: return { "bdz", '+' };
      case 0b10000: return { "bdnz", '\0' };
      case 0b11000: return { "bdnz", '-' };
      case 0b11001: return { "bdnz", '+' };
    }

    const bool isMinus = (bo & 0b11) == 0b10;
    const bool isPlus  = (bo & 0b11) == 0b11;

    if ((bo & 0b11100) == 0b00100) {
      // bge/ble/bne/bns
      if (isMinus) info.second = '-';
      else if (isPlus) info.second = '+';

      switch (bi % 4) {
        case 0x0: info.first = "bge"; break;
        case 0x1: info.first = "ble"; break;
        case 0x2: info.first = "bne"; break;
        case 0x3: info.first = "bns"; break;
      }
      return info;
    }

    if ((bo & 0b11100) == 0b01100) {
      // blt/bgt/beq/bso
      if (isMinus) info.second = '-';
      else if (isPlus) info.second = '+';

      switch (bi % 4) {
        case 0x0: info.first = "blt"; break;
        case 0x1: info.first = "bgt"; break;
        case 0x2: info.first = "beq"; break;
        case 0x3: info.first = "bso"; break;
      }
      return info;
    }

    return info; // default: empty pair
  }

  const std::string PPCInterpreter_getFullName(u32 instr) {
    if (instr == 0x60000000) {
      return "nop";
    }

    PPCOpcode op;
    op.opcode = instr;
    u32 decodedInstr = PPCDecode(instr);
    std::string baseName = ppcDecoder.getNameTable()[decodedInstr];

    switch (Base::JoaatStringHash(baseName)) {
    case "cmpi"_j: {
      return op.l10 ? "cmpdi" : "cmpwi";
    } break;
    case "addic"_j: {
      return op.main & 1 ? "addic." : "addic";
    } break;
    case "addi"_j: {
      return op.ra == 0 ? "li" : "addi";
    } break;
    case "addis"_j: {
      return op.ra == 0 ? "lis" : "addis";
    } break;
    case "bc"_j: {
      const u32 bo = op.bo;
      const u32 bi = op.bi;
      const s32 bd = op.ds * 4;
      const u32 aa = op.aa;
      const u32 lk = op.lk;

      const auto [inst, sign] = getBCInfo(bo, bi);
      if (!inst) {
        return "bc";
      }

      std::string finalInstr = inst;
      if (lk)
        finalInstr += 'l';
      if (aa)
        finalInstr += 'a';
      if (sign)
        finalInstr += sign;
      return finalInstr;
    } break;
    case "b"_j: {
      const u32 li = op.bt24;
      const u32 aa = op.aa;
      const u32 lk = op.lk;

      switch (lk) {
      case 0: {
        switch (aa) {
          case 0: return "b";
          case 1: return "ba";
        }
      } break;
      case 1: {
        switch (aa) {
          case 0: return "bl";
          case 1: return "bla";
        }
      } break;
      }
    } break;
    case "bclr"_j: {
      const u32 bo = op.bo;
      const u32 bi = op.bi;
      const u32 bh = op.bh;
      const u32 lk = op.lk;

      if (bo == 0b10100) {
        return lk ? "blrl" : "blr";
      }

      const auto [inst, sign] = getBCInfo(bo, bi);
      if (!inst) {
        return "bclr";
      }

      std::string finalInstr = std::string(inst) + (lk ? "lrl" : "lr");
      if (sign)
        finalInstr += sign;
      return finalInstr;
    } break;
    case "bcctr"_j: {
      const u32 bo = op.bo;
      const u32 bi = op.bi;
      const u32 bh = op.bh;
      const u32 lk = op.lk;

      if (bo == 0b10100) {
        return lk ? "bctrl" : "bctr";
      }

      const auto [inst, sign] = getBCInfo(bo, bi);
      if (!inst || inst[1] == 'd') {
        return "bcctr";
      }

      std::string finalInstr = std::string(inst) + (lk ? "lrl" : "lr");
      finalInstr += lk ? "ctrl" : "ctr";
      if (sign)
        finalInstr += sign;
      return finalInstr;
    } break;
    }

    return baseName;
  }
}
