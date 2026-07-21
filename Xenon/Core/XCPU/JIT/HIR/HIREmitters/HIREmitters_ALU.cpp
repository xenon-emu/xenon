/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include "Base/Global.h"
#include "Core/XCPU/JIT/HIR/HIREmitters/HIRDecoder.h"

namespace Xe::XCPU::HIR {

  // Constructor, fills the opcode table.
  HIRDecoder::HIRDecoder() { fillTables(); }

  // Emit invalid instruction
  int HIRInstrEmit_invalid(HIRBuilder &f, const uPPCInstr &instr) {
    f.Comment("****** Invalid instruction ******");
    return 0;
  }

  void HIRDecoder::fillTables() {
#define GET_(name) &HIRInstrEmit_##name
#define GET(name) GET_(name), GET_(name)
#define GETRC(name) GET_(name##x), GET_(name##x)
    for (auto &x : table) {
      x = GET(invalid);
    }
    // Main opcodes (field 0..5)
    fillTable<instructionHandlerHIR>(table, 0x00, 6, -1, {
      { 0x02, GET(tdi) },
      { 0x03, GET(twi) },
      { 0x07, GET(mulli) },
     { 0x08, GETRC(subfic) },
      { 0x0A, GET(cmpli) },
      { 0x0B, GET(cmpi) },
     { 0x0C, GETRC(addic) },
     { 0x0D, GETRC(addic) },
      { 0x0E, GET(addi) },
      { 0x0F, GET(addis) },
      { 0x10, GET(bcx) },
      { 0x11, GET(sc) },
      { 0x12, GET(bx) },
      { 0x14, GETRC(rlwimi) },
      { 0x15, GETRC(rlwinm) },
      { 0x17, GETRC(rlwnm) },
      { 0x18, GET(ori) },
      { 0x19, GET(oris) },
      { 0x1A, GET(xori) },
      { 0x1B, GET(xoris) },
      { 0x1C, GETRC(andi) },
      { 0x1D, GETRC(andis) },
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
    fillTable<instructionHandlerHIR>(table, 0x13, 10, 1, {
      { 0x000, GET(mcrf) },
      { 0x010, GET(bclrx) },
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
      { 0x210, GET(bcctrx) },
      });
    // Group 0x1E opcodes (field 27..30)
    fillTable<instructionHandlerHIR>(table, 0x1E, 4, 1, {
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
    fillTable<instructionHandlerHIR>(table, 0x1F, 10, 1, {
      { 0x000, GET(cmp) },
      { 0x004, GET(tw) },
      { 0x006, GET(lvsl) },
      { 0x007, GET(lvebx) },
     { 0x008, GETRC(subfc) },
      //{ 0x208, GETRC(subfco) },
      { 0x009, GETRC(mulhdu) },
     { 0x00A, GETRC(addc) },
      //{ 0x20A, GETRC(addco) },
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
      //{ 0x228, GETRC(subfo) },
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
      //{ 0x268, GETRC(nego) },
      { 0x077, GET(lbzux) },
      { 0x07C, GETRC(nor) },
      { 0x087, GET(stvebx) },
     { 0x088, GETRC(subfe) },
      //{ 0x288, GETRC(subfeo) },
     { 0x08A, GETRC(adde) },
      //{ 0x28A, GETRC(addeo) },
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
      //{ 0x2C8, GETRC(subfzeo) },
      { 0x0CA, GETRC(addze) },
      //{ 0x2CA, GETRC(addzeo) },
      { 0x0D6, GET(stdcx) },
      { 0x0D7, GET(stbx) },
      { 0x0E7, GET(stvx) },
     { 0x0E8, GETRC(subfme) },
      //{ 0x2E8, GETRC(subfmeo) },
      { 0x0E9, GETRC(mulld) },
      //{ 0x2E9, GETRC(mulldo) },
     { 0x0EA, GETRC(addme) },
      //{ 0x2EA, GETRC(addmeo) },
      { 0x0EB, GETRC(mullw) },
      //{ 0x2EB, GETRC(mullwo) },
      { 0x0F6, GET(dcbtst) },
      { 0x0F7, GET(stbux) },
      { 0x10A, GETRC(add) },
      //{ 0x30A, GETRC(addo) },
      { 0x116, GET(dcbt) },
      { 0x117, GET(lhzx) },
      { 0x11C, GETRC(eqv) },
      //{ 0x112, GET(tlbiel) },
      //{ 0x132, GET(tlbie) },
      //{ 0x136, GET(eciwx) },
      { 0x137, GET(lhzux) },
      { 0x13C, GETRC(xor) },
      { 0x153, GET(mfspr) },
      { 0x155, GET(lwax) },
      //{ 0x156, GET(dst) },
      { 0x157, GET(lhax) },
      { 0x167, GET(lvxl) },
      { 0x173, GET(mftb) },
      { 0x175, GET(lwaux) },
      //{ 0x176, GET(dstst) },
      { 0x177, GET(lhaux) },
      //{ 0x192, GET(slbmte) },
      { 0x197, GET(sthx) },
      { 0x19C, GET(orcx) },
      //{ 0x1B2, GET(slbie) },
      //{ 0x1B6, GET(ecowx) },
      { 0x1B7, GET(sthux) },
      { 0x1BC, GETRC(or) },
      { 0x1C9, GETRC(divdu) },
      //{ 0x3C9, GETRC(divduo) },
      { 0x1CB, GETRC(divwu) },
      //{ 0x3CB, GETRC(divwuo) },
      { 0x1D3, GET(mtspr) },
      //{ 0x1D6, GET(dcbi) },
      { 0x1DC, GETRC(nand) },
      //{ 0x1F2, GET(slbia) },
      { 0x1E7, GET(stvxl) },
      { 0x1E9, GETRC(divd) },
      //{ 0x3E9, GETRC(divdo) },
      { 0x1EB, GETRC(divw) },
      //{ 0x3EB, GETRC(divwo) },
      { 0x207, GET(lvlx) },
      { 0x214, GET(ldbrx) },
      //{ 0x215, GET(lswx) },
      { 0x216, GET(lwbrx) },
      { 0x217, GET(lfsx) },
      { 0x218, GETRC(srw) },
      { 0x21B, GETRC(srd) },
      { 0x227, GET(lvrx) },
      //{ 0x236, GET(tlbsync) },
      { 0x237, GET(lfsux) },
      //{ 0x239, GET(mfsrin) },
      //{ 0x253, GET(mfsr) },
      //{ 0x255, GET(lswi) },
      { 0x256, GET(sync) },
      { 0x257, GET(lfdx) },
      { 0x277, GET(lfdux) },
      { 0x287, GET(stvlx) },
      { 0x294, GET(stdbrx) },
      //{ 0x295, GET(stswx) },
      { 0x296, GET(stwbrx) },
      { 0x297, GET(stfsx) },
      { 0x2A7, GET(stvrx) },
      { 0x2B7, GET(stfsux) },
      //{ 0x2D5, GET(stswi) },
      { 0x2D7, GET(stfdx) },
      { 0x2F7, GET(stfdux) },
      { 0x307, GET(lvlxl) },
      { 0x316, GET(lhbrx) },
      { 0x318, GETRC(sraw) },
      { 0x31A, GETRC(srad) },
      { 0x327, GET(lvrxl) },
      //{ 0x336, GET(dss) },
      { 0x338, GETRC(srawi) },
      { 0x33A, GETRC(sradi) },
      { 0x33B, GETRC(sradi) },
      //{ 0x353, GET(slbmfev) },
      { 0x356, GET(eieio) },
      { 0x387, GET(stvlxl) },
      //{ 0x393, GET(slbmfee) },
      { 0x396, GET(sthbrx) },
      { 0x39A, GETRC(extsh) },
      { 0x3A7, GET(stvrxl) },
      { 0x3BA, GETRC(extsb) },
      { 0x3D7, GET(stfiwx) },
      { 0x3DA, GETRC(extsw) },
      { 0x3D6, GET(icbi) },
      //{ 0x3F6, GET(dcbz) },
      });
    // Group 0x3A opcodes (field 30..31)
    fillTable<instructionHandlerHIR>(table, 0x3A, 2, 0, {
      { 0x0, GET(ld) },
      { 0x1, GET(ldu) },
      { 0x2, GET(lwa) },
      });
    // Group 0x3B opcodes (field 21..30)
    fillTable<instructionHandlerHIR>(table, 0x3B, 10, 1, {
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
    fillTable<instructionHandlerHIR>(table, 0x3E, 2, 0, {
      { 0x0, GET(std) },
      { 0x1, GET(stdu) },
      });
    // Group 0x3F opcodes (field 21..30)
    fillTable<instructionHandlerHIR>(table, 0x3F, 10, 1, {
      //{ 0x026, GETRC(mtfsb1) },
      //{ 0x040, GET(mcrfs) },
      //{ 0x046, GETRC(mtfsb0) },
      //{ 0x086, GETRC(mtfsfi) },
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


//
// Helpers
//

Value *AddDidCarry(HIRBuilder &b, Value *v1, Value *v2) {
  return b.CompareUGT(b.Truncate(v2, INT32_TYPE), b.Not(b.Truncate(v1, INT32_TYPE)));
}

Value *SubDidCarry(HIRBuilder &b, Value *v1, Value *v2) {
  return b.Or(b.CompareUGT(b.Truncate(v1, INT32_TYPE), b.Not(b.Neg(b.Truncate(v2, INT32_TYPE)))),
    b.IsFalse(b.Truncate(v2, INT32_TYPE)));
}

// https://github.com/sebastianbiallas/pearpc/blob/0b3c823f61456faa677f6209545a7b906e797421/src/cpu/cpu_generic/ppc_tools.h#L26
Value *AddWithCarryDidCarry(HIRBuilder &b, Value *v1, Value *v2, Value *v3) {
  v1 = b.Truncate(v1, INT32_TYPE);
  v2 = b.Truncate(v2, INT32_TYPE);
  ASSERT(v3->type == INT8_TYPE);
  v3 = b.ZeroExtend(v3, INT32_TYPE);
  return b.Or(b.CompareULT(b.Add(b.Add(v1, v2), v3), v3), b.CompareULT(b.Add(v1, v2), v1));
}


//
// Definitions for ALU HIR Emitters
//

s32 HIRInstrEmit_addx(HIRBuilder &b, const uPPCInstr &instr) {
  // RD <- (RA) + (RB)
  Value *v = b.Add(b.LoadGPR(instr.ra), b.LoadGPR(instr.rb));
  b.StoreGPR(instr.rd, v);
  
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
  }
  
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_addcx(HIRBuilder &b, const uPPCInstr &instr) {
  // RD <- (RA) + (RB)
  // CA <- carry bit
  Value *ra = b.LoadGPR(instr.ra);
  Value *rb = b.LoadGPR(instr.rb);
  Value *v = b.Add(ra, rb);
  b.StoreGPR(instr.rd, v);
  
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
  }
  else {
    b.StoreCA(AddDidCarry(b, ra, rb));
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }

  return 0;
}

s32 HIRInstrEmit_addex(HIRBuilder &b, const uPPCInstr &instr) {
  // RD <- (RA) + (RB) + XER[CA]
  // CA <- carry bit
  Value *ra = b.LoadGPR(instr.ra);
  Value *rb = b.LoadGPR(instr.rb);
  Value *v = b.AddWithCarry(ra, rb, b.LoadCA());
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    // e.update_xer_with_overflow(EFLAGS OF?);
  }
  else {
    b.StoreCA(AddWithCarryDidCarry(b, ra, rb, b.LoadCA()));
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_addi(HIRBuilder &b, const uPPCInstr &instr) {
  // if RA = 0 then
  //   RT <- EXTS(SI)
  // else
  //   RT <- (RA) + EXTS(SI)
  Value *si = b.LoadConstantInt64(SignExtend16(instr.simm16));
  Value *v = si;
  if (instr.ra) {
    v = b.Add(b.LoadGPR(instr.ra), si);
  }
  b.StoreGPR(instr.rd, v);
  return 0;
}

s32 HIRInstrEmit_addicx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- (RA) + EXTS(SI)
  // CA <- carry bit
  Value *ra = b.LoadGPR(instr.ra);
  Value *v = b.Add(b.LoadGPR(instr.ra), b.LoadConstantInt64(SignExtend16(instr.simm16)));
  b.StoreGPR(instr.rd, v);
  b.StoreCA(AddDidCarry(b, ra, b.LoadConstantInt64(SignExtend16(instr.simm16))));
  if (instr.main & 1) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_addis(HIRBuilder &b, const uPPCInstr &instr) {
  // if RA = 0 then
  //   RT <- EXTS(SI) || i16.0
  // else
  //   RT <- (RA) + EXTS(SI) || i16.0
  Value *si = b.LoadConstantInt64(SignExtend16(instr.simm16) << 16);
  Value *v = si;
  if (instr.ra) {
    v = b.Add(b.LoadGPR(instr.ra), si);
  }
  b.StoreGPR(instr.rd, v);
  return 0;
}

s32 HIRInstrEmit_addmex(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- (RA) + CA - 1
  // CA <- carry bit
  Value *ra = b.LoadGPR(instr.ra);
  Value *v = b.AddWithCarry(ra, b.LoadConstantInt64(-1), b.LoadCA());
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
  } else {
    // Just CA update.
    b.StoreCA(AddWithCarryDidCarry(b, ra, b.LoadConstantInt64(-1), b.LoadCA()));
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_addzex(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- (RA) + CA
  // CA <- carry bit
  Value *ra = b.LoadGPR(instr.ra);
  Value *v = b.AddWithCarry(ra, b.LoadZeroInt64(), b.LoadCA());
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
  } else {
    // Just CA update.
    b.StoreCA(AddWithCarryDidCarry(b, ra, b.LoadZeroInt64(), b.LoadCA()));
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_divdx(HIRBuilder &b, const uPPCInstr &instr) {
  // dividend <- (RA)
  // divisor <- (RB)
  // if divisor = 0 then
  //   if OE = 1 then
  //     XER[OV] <- 1
  //   return
  // RT <- dividend ÷ divisor
  Value *divisor = b.LoadGPR(instr.rb);
  // TODO(benvanik): check if zero
  //                 if OE=1, set XER[OV] = 1
  //                 else skip the divide
  Value *v = b.Div(b.LoadGPR(instr.ra), divisor);
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_divdux(HIRBuilder &b, const uPPCInstr &instr) {
  // dividend <- (RA)
  // divisor <- (RB)
  // if divisor = 0 then
  //   if OE = 1 then
  //     XER[OV] <- 1
  //   return
  // RT <- dividend ÷ divisor
  Value *divisor = b.LoadGPR(instr.rb);
  // TODO(benvanik): check if zero
  //                 if OE=1, set XER[OV] = 1
  //                 else skip the divide
  Value *v = b.Div(b.LoadGPR(instr.ra), divisor, ARITHMETIC_UNSIGNED);
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    // If we are OE=1 we need to clear the overflow bit.
    // e.update_xer_with_overflow(e.get_uint64(0));
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_divwx(HIRBuilder &b, const uPPCInstr &instr) {
  // dividend[0:31] <- (RA)[32:63]
  // divisor[0:31] <- (RB)[32:63]
  // if divisor = 0 then
  //   if OE = 1 then
  //     XER[OV] <- 1
  //   return
  // RT[32:63] <- dividend ÷ divisor
  // RT[0:31] <- undefined
  Value *divisor = b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE);
  // TODO(benvanik): check if zero
  //                 if OE=1, set XER[OV] = 1
  //                 else skip the divide
  Value *v = b.Div(b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE), divisor);
  v = b.ZeroExtend(v, INT64_TYPE);
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    // If we are OE=1 we need to clear the overflow bit.
    // e.update_xer_with_overflow(e.get_uint64(0));
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_divwux(HIRBuilder &b, const uPPCInstr &instr) {
  // dividend[0:31] <- (RA)[32:63]
  // divisor[0:31] <- (RB)[32:63]
  // if divisor = 0 then
  //   if OE = 1 then
  //     XER[OV] <- 1
  //   return
  // RT[32:63] <- dividend ÷ divisor
  // RT[0:31] <- undefined
  Value *divisor = b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE);
  // TODO(benvanik): check if zero
  //                 if OE=1, set XER[OV] = 1
  //                 else skip the divide
  Value *v = b.Div(b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE), divisor,
    ARITHMETIC_UNSIGNED);
  v = b.ZeroExtend(v, INT64_TYPE);
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    // If we are OE=1 we need to clear the overflow bit.
    // e.update_xer_with_overflow(e.get_uint64(0));
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_mulhdx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ((RA) × (RB) as 128)[0:63]
  if (instr.oe) {
    // With XER update.
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  Value *v = b.MulHi(b.LoadGPR(instr.ra), b.LoadGPR(instr.rb));
  b.StoreGPR(instr.rd, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_mulhdux(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ((RA) × (RB) as 128)[0:63]
  if (instr.oe) {
    // With XER update.
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  Value *v =
    b.MulHi(b.LoadGPR(instr.ra), b.LoadGPR(instr.rb), ARITHMETIC_UNSIGNED);
  b.StoreGPR(instr.rd, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_mulhwx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT[32:64] <- ((RA)[32:63] × (RB)[32:63])[0:31]
  if (instr.oe) {
    // With XER update.
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  Value *v = b.SignExtend(b.MulHi(b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE),
    b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE)),
    INT64_TYPE);
  b.StoreGPR(instr.rd, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_mulhwux(HIRBuilder &b, const uPPCInstr &instr) {
  // RT[32:64] <- ((RA)[32:63] × (RB)[32:63])[0:31]
  if (instr.oe) {
    // With XER update.
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  Value *v = b.ZeroExtend(
    b.MulHi(b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE),
      b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE), ARITHMETIC_UNSIGNED),
    INT64_TYPE);
  b.StoreGPR(instr.rd, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_mulldx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ((RA) × (RB))[64:127]
  if (instr.oe) {
    // With XER update.
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  Value *v = b.Mul(b.LoadGPR(instr.ra), b.LoadGPR(instr.rb));
  b.StoreGPR(instr.rd, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_mulli(HIRBuilder &b, const uPPCInstr &instr) {
  // prod[0:127] <- (RA) × EXTS(SI)
  // RT <- prod[64:127]
  Value *v = b.Mul(b.LoadGPR(instr.ra), b.LoadConstantInt64(SignExtend16(instr.simm16)));
  b.StoreGPR(instr.rd, v);
  return 0;
}

s32 HIRInstrEmit_mullwx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- (RA)[32:63] × (RB)[32:63]
  if (instr.oe) {
    // With XER update.
    INSTRNOTIMPLEMENTED();
    return 1;
  }
  Value *v = b.Mul(
    b.SignExtend(b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE), INT64_TYPE),
    b.SignExtend(b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE), INT64_TYPE));
  b.StoreGPR(instr.rd, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_negx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + 1
  if (instr.oe) {
    // With XER update.
    // This is a different codepath as we need to use llvm.ssub.with.overflow.

    // if RA == 0x8000000000000000 then no-op and set OV=1
    // This may just magically do that...

    INSTRNOTIMPLEMENTED();
    return 1;
    // Function* ssub_with_overflow = Intrinsic::getDeclaration(
    //    e.gen_module(), Intrinsic::ssub_with_overflow, jit_type_nint);
    // jit_value_t v = b.CreateCall2(ssub_with_overflow,
    //                         e.get_int64(0), b.LoadGPR(instr.ra));
    // jit_value_t v0 = b.CreateExtractValue(v, 0);
    // b.StoreGPR(instr.rd, v0);
    // e.update_xer_with_overflow(b.CreateExtractValue(v, 1));

    // if (instr.rc) {
    //  // With cr0 update.
    //  b.UpdateCRx(0, v0, e.get_int64(0), true);
    //}
  } else {
    // No OE bit setting.
    Value *v = b.Neg(b.LoadGPR(instr.ra));
    b.StoreGPR(instr.rd, v);
    if (instr.rc) {
      b.UpdateCR0(v);
    }
  }
  return 0;
}

s32 HIRInstrEmit_subfx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + (RB) + 1
  Value *v = b.Sub(b.LoadGPR(instr.rb), b.LoadGPR(instr.ra));
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
    // e.update_xer_with_overflow(EFLAGS??);
  }

  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_subfcx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + (RB) + 1
  Value *ra = b.LoadGPR(instr.ra);
  Value *rb = b.LoadGPR(instr.rb);
  Value *v = b.Sub(rb, ra);
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
    // e.update_xer_with_overflow(EFLAGS??);
  } else {
    b.StoreCA(SubDidCarry(b, rb, ra));
  }

  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_subficx(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + EXTS(SI) + 1
  Value *ra = b.LoadGPR(instr.ra);
  Value *v = b.Sub(b.LoadConstantInt64(SignExtend16(instr.simm16)), ra);
  b.StoreGPR(instr.rd, v);
  b.StoreCA(SubDidCarry(b, b.LoadConstantInt64(SignExtend16(instr.simm16)), ra));
  return 0;
}

s32 HIRInstrEmit_subfex(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + (RB) + CA
  Value *not_ra = b.Not(b.LoadGPR(instr.ra));
  Value *rb = b.LoadGPR(instr.rb);
  Value *v = b.AddWithCarry(not_ra, rb, b.LoadCA());
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
    // e.update_xer_with_overflow_and_carry(b.CreateExtractValue(v, 1));
  } else {
    b.StoreCA(AddWithCarryDidCarry(b, not_ra, rb, b.LoadCA()));
  }

  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_subfmex(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + CA - 1
  Value *not_ra = b.Not(b.LoadGPR(instr.ra));
  Value *v = b.AddWithCarry(not_ra, b.LoadConstantInt64(-1), b.LoadCA());
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
    // e.update_xer_with_overflow_and_carry(b.CreateExtractValue(v, 1));
  } else {
    b.StoreCA(
      AddWithCarryDidCarry(b, not_ra, b.LoadConstantInt64(-1), b.LoadCA()));
  }

  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_subfzex(HIRBuilder &b, const uPPCInstr &instr) {
  // RT <- ¬(RA) + CA
  Value *not_ra = b.Not(b.LoadGPR(instr.ra));
  Value *v = b.AddWithCarry(not_ra, b.LoadZeroInt64(), b.LoadCA());
  b.StoreGPR(instr.rd, v);
  if (instr.oe) {
    INSTRNOTIMPLEMENTED();
    return 1;
    // e.update_xer_with_overflow_and_carry(b.CreateExtractValue(v, 1));
  } else {
    b.StoreCA(AddWithCarryDidCarry(b, not_ra, b.LoadZeroInt64(), b.LoadCA()));
  }

  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

// Integer compare (A-4)

s32 HIRInstrEmit_cmp(HIRBuilder &b, const uPPCInstr &instr) {
  Value *lhs;
  Value *rhs;
  if (instr.l10) {
    lhs = b.LoadGPR(instr.ra);
    rhs = b.LoadGPR(instr.rb);
  } else {
    lhs = b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE);
    rhs = b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE);
  }
  b.UpdateCRx(instr.crfd, lhs, rhs, true, false);
  return 0;
}

s32 HIRInstrEmit_cmpi(HIRBuilder &b, const uPPCInstr &instr) {
  Value *lhs;
  Value *rhs;
  if (instr.l10) {
    lhs = b.LoadGPR(instr.ra);
    rhs = b.LoadConstantInt64(SignExtend16(instr.simm16));
  } else {
    lhs = b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE);
    rhs = b.LoadConstantInt32(SignExtend16(instr.simm16));
  }
  b.UpdateCRx(instr.crfd, lhs, rhs, true, false);
  return 0;
}

s32 HIRInstrEmit_cmpl(HIRBuilder &b, const uPPCInstr &instr) {
  Value *lhs;
  Value *rhs;
  if (instr.l10) {
    lhs = b.LoadGPR(instr.ra);
    rhs = b.LoadGPR(instr.rb);
  } else {
    lhs = b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE);
    rhs = b.Truncate(b.LoadGPR(instr.rb), INT32_TYPE);
  }
  b.UpdateCRx(instr.crfd, lhs, rhs, false, false);
  return 0;
}

s32 HIRInstrEmit_cmpli(HIRBuilder &b, const uPPCInstr &instr) {
  Value *lhs;
  Value *rhs;
  if (instr.l10) {
    lhs = b.LoadGPR(instr.ra);
    rhs = b.LoadConstantUint64(instr.uimm16);
  } else {
    lhs = b.Truncate(b.LoadGPR(instr.ra), INT32_TYPE);
    rhs = b.LoadConstantUint32(instr.uimm16);
  }
  b.UpdateCRx(instr.crfd, lhs, rhs, false, false);
  return 0;
}

// Integer logical (A-5)

s32 HIRInstrEmit_andx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) & (RB)
  Value *ra = b.And(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_andcx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) & ¬(RB)
  Value *ra = b.AndNot(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_andix(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) & (i48.0 || UI)
  Value *ra = b.And(b.LoadGPR(instr.rd), b.LoadConstantUint64(ZeroExtend16(instr.simm16)));
  b.StoreGPR(instr.ra, ra);
  b.UpdateCRx(0, ra);
  return 0;
}

s32 HIRInstrEmit_andisx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) & (i32.0 || UI || i16.0)
  Value *ra =
    b.And(b.LoadGPR(instr.rd), b.LoadConstantUint64(ZeroExtend16(instr.simm16) << 16));
  b.StoreGPR(instr.ra, ra);
  b.UpdateCRx(0, ra);
  return 0;
}

s32 HIRInstrEmit_cntlzdx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- 0
  // do while n < 64
  //   if (RS)[n] = 1 then leave n
  //   n <- n + 1
  // RA <- n
  Value *v = b.CountLeadingZeros(b.LoadGPR(instr.rd));
  v = b.ZeroExtend(v, INT64_TYPE);
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_cntlzwx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- 32
  // do while n < 64
  //   if (RS)[n] = 1 then leave n
  //   n <- n + 1
  // RA <- n - 32
  Value *v = b.CountLeadingZeros(b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE));
  v = b.ZeroExtend(v, INT64_TYPE);
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_eqvx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) == (RB)
  Value *ra = b.Not(b.Xor(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb)));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_extsbx(HIRBuilder &b, const uPPCInstr &instr) {
  // s <- (RS)[56]
  // RA[56:63] <- (RS)[56:63]
  // RA[0:55] <- i56.s
  Value *rt = b.LoadGPR(instr.rd);
  rt = b.SignExtend(b.Truncate(rt, INT8_TYPE), INT64_TYPE);
  b.StoreGPR(instr.ra, rt);
  if (instr.rc) {
    b.UpdateCRx(0, rt);
  }
  return 0;
}

s32 HIRInstrEmit_extshx(HIRBuilder &b, const uPPCInstr &instr) {
  // s <- (RS)[48]
  // RA[48:63] <- (RS)[48:63]
  // RA[0:47] <- 48.s
  Value *rt = b.LoadGPR(instr.rd);
  rt = b.SignExtend(b.Truncate(rt, INT16_TYPE), INT64_TYPE);
  b.StoreGPR(instr.ra, rt);
  if (instr.rc) {
    b.UpdateCRx(0, rt);
  }
  return 0;
}

s32 HIRInstrEmit_extswx(HIRBuilder &b, const uPPCInstr &instr) {
  // s <- (RS)[32]
  // RA[32:63] <- (RS)[32:63]
  // RA[0:31] <- i32.s
  Value *rt = b.LoadGPR(instr.rd);
  rt = b.SignExtend(b.Truncate(rt, INT32_TYPE), INT64_TYPE);
  b.StoreGPR(instr.ra, rt);
  if (instr.rc) {
    b.UpdateCRx(0, rt);
  }
  return 0;
}

s32 HIRInstrEmit_nandx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- ¬((RS) & (RB))
  Value *ra = b.Not(b.And(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb)));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_norx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- ¬((RS) | (RB))
  Value *ra = b.Not(b.Or(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb)));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_orx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) | (RB)
  if (instr.rd == instr.rb && instr.rd == instr.ra && !instr.rc) {
    // Sometimes used as no-op.
    b.Nop();
    return 0;
  }
  Value *ra;
  if (instr.rd == instr.rb) {
    ra = b.LoadGPR(instr.rd);
  } else {
    ra = b.Or(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb));
  }
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_orcx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) | ¬(RB)
  Value *ra = b.Or(b.LoadGPR(instr.rd), b.Not(b.LoadGPR(instr.rb)));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_ori(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) | (i48.0 || UI)
  if (!instr.ra && !instr.rd && !instr.simm16) {
    b.Nop();
    return 0;
  }
  Value *ra = b.Or(b.LoadGPR(instr.rd), b.LoadConstantUint64(ZeroExtend16(instr.simm16)));
  b.StoreGPR(instr.ra, ra);
  return 0;
}

s32 HIRInstrEmit_oris(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) | (i32.0 || UI || i16.0)
  Value *ra =
    b.Or(b.LoadGPR(instr.rd), b.LoadConstantUint64(ZeroExtend16(instr.simm16) << 16));
  b.StoreGPR(instr.ra, ra);
  return 0;
}

s32 HIRInstrEmit_xorx(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) XOR (RB)
  Value *ra = b.Xor(b.LoadGPR(instr.rd), b.LoadGPR(instr.rb));
  b.StoreGPR(instr.ra, ra);
  if (instr.rc) {
    b.UpdateCRx(0, ra);
  }
  return 0;
}

s32 HIRInstrEmit_xori(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) XOR (i48.0 || UI)
  Value *ra = b.Xor(b.LoadGPR(instr.rd), b.LoadConstantUint64(ZeroExtend16(instr.simm16)));
  b.StoreGPR(instr.ra, ra);
  return 0;
}

s32 HIRInstrEmit_xoris(HIRBuilder &b, const uPPCInstr &instr) {
  // RA <- (RS) XOR (i32.0 || UI || i16.0)
  Value *ra =
    b.Xor(b.LoadGPR(instr.rd), b.LoadConstantUint64(ZeroExtend16(instr.simm16) << 16));
  b.StoreGPR(instr.ra, ra);
  return 0;
}

// Integer rotate (A-6)

s32 HIRInstrEmit_rldclx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- rB[58:63]
  // r <- ROTL[64](rS, n)
  // b <- mb[5] || mb[0:4]
  // m <- MASK(b, 63)
  // rA <- r & m
  Value *n = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE),
    b.LoadConstantInt8(0x3F));

  u32 mb = instr.mbe64;
  u64 m = CreateMask(mb, 63);
  Value *v = b.LoadGPR(instr.rd);

  v = b.RotateLeft(v, n);
  if (m != 0xFFFFFFFFFFFFFFFF) {
    v = b.And(v, b.LoadConstantUint64(m));
  }

  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rldcrx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- rB[58:63]
  // r <- ROTL[64](rS, n)
  // b <- mb[5] || mb[0:4]
  // m <- MASK(0, b)
  // rA <- r & m
  Value *n = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE),
    b.LoadConstantInt8(0x3F));

  u32 mb = instr.mbe64;
  u64 m = CreateMask(0, mb);
  Value *v = b.LoadGPR(instr.rd);

  v = b.RotateLeft(v, n);
  if (m != 0xFFFFFFFFFFFFFFFF) {
    v = b.And(v, b.LoadConstantUint64(m));
  }

  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rldicx(HIRBuilder &b, const uPPCInstr &instr) {
  u32 sh = instr.sh64;
  u32 mb = instr.mbe64;
  u64 m = CreateMask(mb, instr.sh64 ^ 63);
  Value *v = b.LoadGPR(instr.rd);
  if (sh) {
    v = b.RotateLeft(v, b.LoadConstantInt8(sh));
  }
  if (m != 0xFFFFFFFFFFFFFFFF) {
    v = b.And(v, b.LoadConstantUint64(m));
  }
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rldiclx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- sh[5] || sh[0:4]
  // r <- ROTL64((RS), n)
  // b <- mb[5] || mb[0:4]
  // m <- MASK(b, 63)
  // RA <- r & m
  u32 sh = instr.sh64;
  u32 mb = instr.mbe64;
  u64 m = CreateMask(mb, 63);
  Value *v = b.LoadGPR(instr.rd);
  if (sh == 64 - mb) {
    // srdi == rldicl ra,rs,64-n,n
    v = b.Shr(v, int8_t(mb));
  } else {
    if (sh) {
      v = b.RotateLeft(v, b.LoadConstantInt8(sh));
    }
    if (m != 0xFFFFFFFFFFFFFFFF) {
      v = b.And(v, b.LoadConstantUint64(m));
    }
  }
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rldicrx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- sh[5] || sh[0:4]
  // r <- ROTL64((RS), n)
  // e <- me[5] || me[0:4]
  // m <- MASK(0, e)
  // RA <- r & m
  u32 sh = instr.sh64;
  u32 mb = instr.mbe64;
  u64 m = CreateMask(0, mb);
  Value *v = b.LoadGPR(instr.rd);
  if (mb == 63 - sh) {
    // sldi ==  rldicr ra,rs,n,63-n
    v = b.Shl(v, int8_t(sh));
  } else {
    if (sh) {
      v = b.RotateLeft(v, b.LoadConstantInt8(sh));
    }
    if (m != 0xFFFFFFFFFFFFFFFF) {
      v = b.And(v, b.LoadConstantUint64(m));
    }
  }
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rldimix(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- sh[5] || sh[0:4]
  // r <- ROTL64((RS), n)
  // b <- me[5] || me[0:4]
  // m <- MASK(b, ¬n)
  // RA <- (r & m) | ((RA)&¬m)
  u32 sh = instr.sh64;
  u32 mb = instr.mbe64;
  u64 m = CreateMask(mb, sh ^ 63);
  Value *v = b.LoadGPR(instr.rd);
  if (sh) {
    v = b.RotateLeft(v, b.LoadConstantInt8(sh));
  }
  if (m != 0xFFFFFFFFFFFFFFFF) {
    Value *ra = b.LoadGPR(instr.ra);
    v = b.Or(b.And(v, b.LoadConstantUint64(m)),
      b.And(ra, b.LoadConstantUint64(~m)));
  }
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rlwimix(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- SH
  // r <- ROTL32((RS)[32:63], n)
  // m <- MASK(MB+32, ME+32)
  // RA <- r&m | (RA)&¬m
  Value *v = b.LoadGPR(instr.rd);
  // (x||x)
  v = b.Or(b.Shl(v, 32), b.ZeroExtend(b.Truncate(v, INT32_TYPE), INT64_TYPE));
  if (instr.sh32) {
    v = b.RotateLeft(v, b.LoadConstantInt8(instr.sh32));
  }
  // Compiler sometimes masks with 0xFFFFFFFF (identity) - avoid the work here
  // as our truncation/zero-extend does it for us.
  u64 m = CreateMask(instr.mb32 + 32, instr.me32 + 32);
  if (m != 0xFFFFFFFFFFFFFFFFull) {
    v = b.And(v, b.LoadConstantUint64(m));
  }
  v = b.Or(v, b.And(b.LoadGPR(instr.ra), b.LoadConstantUint64(~m)));
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rlwinmx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- SH
  // r <- ROTL32((RS)[32:63], n)
  // m <- MASK(MB+32, ME+32)
  // RA <- r & m
  Value *v = b.LoadGPR(instr.rd);

  // (x||x)
  v = b.Or(b.Shl(v, 32), b.ZeroExtend(b.Truncate(v, INT32_TYPE), INT64_TYPE));

  // TODO(benvanik): optimize srwi
  // TODO(benvanik): optimize slwi
  // The compiler will generate a bunch of these for the special case of SH=0.
  // Which seems to just select some bits and set cr0 for use with a branch.
  // We can detect this and do less work.
  if (instr.sh32) {
    v = b.RotateLeft(v, b.LoadConstantInt8(instr.sh32));
  }
  // Compiler sometimes masks with 0xFFFFFFFF (identity) - avoid the work here
  // as our truncation/zero-extend does it for us.
  u64 m = CreateMask(instr.mb32 + 32, instr.me32 + 32);
  if (m != 0xFFFFFFFFFFFFFFFFull) {
    v = b.And(v, b.LoadConstantUint64(m));
  }
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_rlwnmx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- (RB)[59:63]
  // r <- ROTL32((RS)[32:63], n)
  // m <- MASK(MB+32, ME+32)
  // RA <- r & m
  Value *sh =
    b.And(b.Truncate(b.LoadGPR(instr.sh32), INT8_TYPE), b.LoadConstantInt8(0x1F));
  Value *v = b.LoadGPR(instr.rd);
  // (x||x)
  v = b.Or(b.Shl(v, 32), b.ZeroExtend(b.Truncate(v, INT32_TYPE), INT64_TYPE));
  v = b.RotateLeft(v, sh);
  v = b.And(v, b.LoadConstantUint64(CreateMask(instr.mb32 + 32, instr.me32 + 32)));
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

// Integer shift (A-7)

s32 HIRInstrEmit_sldx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- (RB)[58:63]
  // r <- ROTL64((RS), n)
  // if (RB)[57] = 0 then
  //   m <- MASK(0, 63-n)
  // else
  //   m <- i64.0
  // RA <- r & m
  Value *sh = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE), b.LoadConstantInt8(0x7F));
  Value *v = b.Select(b.IsTrue(b.Shr(sh, 6)), b.LoadZeroInt64(), b.Shl(b.LoadGPR(instr.rd), sh));
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_slwx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- (RB)[59:63]
  // r <- ROTL32((RS)[32:63], n)
  // if (RB)[58] = 0 then
  //   m <- MASK(32, 63-n)
  // else
  //   m <- i64.0
  // RA <- r & m
  Value *sh = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE), b.LoadConstantInt8(0x3F));
  Value *v = b.Select(b.IsTrue(b.Shr(sh, 5)), b.LoadZeroInt32(), 
    b.Shl(b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE), sh));
  v = b.ZeroExtend(v, INT64_TYPE);
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_srdx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- (RB)[58:63]
  // r <- ROTL64((RS), 64-n)
  // if (RB)[57] = 0 then
  //   m <- MASK(n, 63)
  // else
  //   m <- i64.0
  // RA <- r & m
  Value *sh = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE), b.LoadConstantInt8(0x7F));
  Value *v = b.Select(b.IsTrue(b.And(sh, b.LoadConstantInt8(0x40))),
    b.LoadZeroInt64(), b.Shr(b.LoadGPR(instr.rd), sh));
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_srwx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- (RB)[59:63]
  // r <- ROTL32((RS)[32:63], 64-n)
  // if (RB)[58] = 0 then
  //   m <- MASK(n+32, 63)
  // else
  //   m <- i64.0
  // RA <- r & m
  Value *sh = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE), b.LoadConstantInt8(0x3F));
  Value *v = b.Select(b.IsTrue(b.And(sh, b.LoadConstantInt8(0x20))), b.LoadZeroInt32(),
      b.Shr(b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE), sh));
  v = b.ZeroExtend(v, INT64_TYPE);
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_sradx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- rB[58-63]
  // r <- ROTL[64](rS, 64 - n)
  // if rB[57] = 0 then m ← MASK(n, 63)
  // else m ← (64)0
  // S ← rS[0]
  // rA <- (r & m) | (((64)S) & ¬ m)
  // XER[CA] <- S & ((r & ¬ m) ¦ 0)
  // if n == 0: rA <- rS, XER[CA] = 0
  // if n >= 64: rA <- 64 sign bits of rS, XER[CA] = sign bit of rS
  Value *rt = b.LoadGPR(instr.rd);
  Value *sh = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE), b.LoadConstantInt8(0x7F));
  Value *clamp_sh = b.Min(sh, b.LoadConstantInt8(0x3F));
  Value *v = b.Sha(rt, clamp_sh);

  // CA is set if any bits are shifted out of the right and if the result
  // is negative.
  Value *ca = b.And(b.IsTrue(b.Shr(rt, 63)), b.CompareNE(b.Shl(v, clamp_sh), rt));
  b.StoreCA(ca);

  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_sradix(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- sh[5] || sh[0-4]
  // r <- ROTL[64](rS, 64 - n)
  // m ← MASK(n, 63)
  // S ← rS[0]
  // rA <- (r & m) | (((64)S) & ¬ m)
  // XER[CA] <- S & ((r & ¬ m) ¦ 0)
  // if n == 0: rA <- rS, XER[CA] = 0
  // if n >= 64: rA <- 64 sign bits of rS, XER[CA] = sign bit of rS
  Value *v = b.LoadGPR(instr.rd);
  int8_t sh = instr.sh64;

  // CA is set if any bits are shifted out of the right and if the result
  // is negative.
  if (sh) {
    u64 mask = CreateMask(64 - sh, 63);
    Value *ca = b.And(b.Truncate(b.Shr(v, 63), INT8_TYPE), b.IsTrue(b.And(v, b.LoadConstantUint64(mask))));
    b.StoreCA(ca);

    v = b.Sha(v, sh);
  } else {
    b.StoreCA(b.LoadZeroInt8());
  }

  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_srawx(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- rB[59-63]
  // r <- ROTL32((RS)[32:63], 64-n)
  // m <- MASK(n+32, 63)
  // s <- (RS)[32]
  // RA <- r&m | (i64.s)&¬m
  // CA <- s & ((r&¬m)[32:63]≠0)
  // if n == 0: rA <- sign_extend(rS), XER[CA] = 0
  // if n >= 32: rA <- 64 sign bits of rS, XER[CA] = sign bit of lo_32(rS)
  Value *rt = b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE);
  Value *sh = b.And(b.Truncate(b.LoadGPR(instr.rb), INT8_TYPE), b.LoadConstantInt8(0x3F));
  Value *clamp_sh = b.Min(sh, b.LoadConstantInt8(0x1F));
  Value *v = b.Sha(rt, b.Min(sh, clamp_sh));

  // CA is set if any bits are shifted out of the right and if the result
  // is negative.
  Value *ca = b.And(b.IsTrue(b.Shr(rt, 31)), b.CompareNE(b.Shl(v, clamp_sh), rt));
  b.StoreCA(ca);

  v = b.SignExtend(v, INT64_TYPE);
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

s32 HIRInstrEmit_srawix(HIRBuilder &b, const uPPCInstr &instr) {
  // n <- SH
  // r <- ROTL32((RS)[32:63], 64-n)
  // m <- MASK(n+32, 63)
  // s <- (RS)[32]
  // RA <- r&m | (i64.s)&¬m
  // CA <- s & ((r&¬m)[32:63]≠0)
  // if n == 0: rA <- sign_extend(rS), XER[CA] = 0
  // if n >= 32: rA <- 64 sign bits of rS, XER[CA] = sign bit of lo_32(rS)
  Value *v = b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE);
  Value *ca;
  if (!instr.rb) {
    // No shift, just a fancy sign extend and CA clearer.
    v = b.SignExtend(v, INT64_TYPE);
    ca = b.LoadZeroInt8();
  } else {
    // CA is set if any bits are shifted out of the right and if the result
    // is negative.
    u32 mask = (u32)CreateMask(64 - instr.rb, 63);
    ca = b.And(b.Truncate(b.Shr(v, 31), INT8_TYPE), b.IsTrue(b.And(v, b.LoadConstantUint32(mask))));

    v = b.Sha(v, (int8_t)instr.rb), v = b.SignExtend(v, INT64_TYPE);
  }
  b.StoreCA(ca);
  b.StoreGPR(instr.ra, v);
  if (instr.rc) {
    b.UpdateCR0(v);
  }
  return 0;
}

// Condition Register logical instructions

// Helper to extract a single CR bit (0-31) from the CR register.
static Value *LoadCRBit(HIRBuilder &b, u32 bitIndex) {
  // CR is stored as a 32-bit value with bit 0 being the MSB.
  // To extract bit N, we shift right by (31 - N) and mask with 1.
  Value *cr = b.LoadCR();
  Value *shifted = b.Shr(cr, static_cast<s8>(31 - bitIndex));
  return b.Truncate(b.And(shifted, b.LoadConstantUint32(1)), INT8_TYPE);
}

// Helper to store a single CR bit (0-31) into the CR register.
static void StoreCRBit(HIRBuilder &b, u32 bitIndex, Value *bitValue) {
  // Create a mask to clear the target bit.
  u32 mask = ~(1u << (31 - bitIndex));
  Value *cr = b.LoadCR();
  // Clear the bit.
  cr = b.And(cr, b.LoadConstantUint32(mask));
  // Shift the new bit value into position and OR it in.
  Value *shiftedBit = b.Shl(b.ZeroExtend(bitValue, INT32_TYPE), static_cast<s8>(31 - bitIndex));
  cr = b.Or(cr, shiftedBit);
  b.StoreCR(cr);
}

s32 HIRInstrEmit_crand(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- CR[crbA] & CR[crbB]
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *result = b.And(cra, crb);
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_crandc(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- CR[crbA] & ~CR[crbB]
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *notB = b.Xor(crb, b.LoadConstantInt8(1));
  Value *result = b.And(cra, notB);
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_creqv(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- CR[crbA] == CR[crbB] (XNOR)
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  // XNOR = NOT(XOR) = 1 XOR (a XOR b)
  Value *xorResult = b.Xor(cra, crb);
  Value *result = b.Xor(xorResult, b.LoadConstantInt8(1));
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_crnand(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- ~(CR[crbA] & CR[crbB])
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *andResult = b.And(cra, crb);
  Value *result = b.Xor(andResult, b.LoadConstantInt8(1));
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_crnor(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- ~(CR[crbA] | CR[crbB])
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *orResult = b.Or(cra, crb);
  Value *result = b.Xor(orResult, b.LoadConstantInt8(1));
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_cror(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- CR[crbA] | CR[crbB]
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *result = b.Or(cra, crb);
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_crorc(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- CR[crbA] | ~CR[crbB]
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *notB = b.Xor(crb, b.LoadConstantInt8(1));
  Value *result = b.Or(cra, notB);
  StoreCRBit(b, instr.crbd, result);
  return 0;
}

s32 HIRInstrEmit_crxor(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[crbD] <- CR[crbA] ^ CR[crbB]
  Value *cra = LoadCRBit(b, instr.crba);
  Value *crb = LoadCRBit(b, instr.crbb);
  Value *result = b.Xor(cra, crb);
  StoreCRBit(b, instr.crbd, result);
  return 0;
}


}
