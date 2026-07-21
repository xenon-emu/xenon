/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/HIR/HIREmitters/HIREmitters.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

namespace Xe::XCPU::HIR {

// Move from One Condition Register Field (x'7C20 0026')
s32 HIRInstrEmit_mfocrf(HIRBuilder &f, const uPPCInstr &instr) {
  Value *cr = f.LoadCR();

  if (instr.l11) {
    // MFOCRF - build a 32-bit mask from the CRM field.
    // Each bit in CRM (8 bits) selects a 4-bit CR field.
    // Bit 0x80 = CR0 (bits 28-31), bit 0x01 = CR7 (bits 0-3).
    u32 crMask = 0;
    u32 bit = 0x80;
    u32 count = 0;

    for (; bit; bit >>= 1) {
      crMask <<= 4;
      if (instr.crm & bit) {
        crMask |= 0xF;
        count++;
      }
    }

    if (count == 1) {
      Value *masked = f.And(cr, f.LoadConstantUint32(crMask));
      f.StoreGPR(instr.rd, f.ZeroExtend(masked, INT64_TYPE));
    } else {
      // Undefined behavior per architecture spec - store zero.
      f.StoreGPR(instr.rd, f.LoadZeroInt64());
    }
  } else {
    // MFCR - store full CR value.
    f.StoreGPR(instr.rd, f.ZeroExtend(cr, INT64_TYPE));
  }

  return 0;
}

// General Trap Emission
s32 InstrEmit_Trap(HIRBuilder &f, Value *va, Value *vb, uint32_t TO) {
  // if (a < b) & TO[0] then TRAP
  // if (a > b) & TO[1] then TRAP
  // if (a = b) & TO[2] then TRAP
  // if (a <u b) & TO[3] then TRAP
  // if (a >u b) & TO[4] then TRAP
  // Bits swapped:
  // 01234
  // 43210
  if (!TO) {
    return 0;
  }
  Value *v = nullptr;
  if (TO & (1 << 4)) {
    // a < b
    auto cmp = f.CompareSLT(va, vb);
    v = v ? f.Or(v, cmp) : cmp;
  }
  if (TO & (1 << 3)) {
    // a > b
    auto cmp = f.CompareSGT(va, vb);
    v = v ? f.Or(v, cmp) : cmp;
  }
  if (TO & (1 << 2)) {
    // a = b
    auto cmp = f.CompareEQ(va, vb);
    v = v ? f.Or(v, cmp) : cmp;
  }
  if (TO & (1 << 1)) {
    // a <u b
    auto cmp = f.CompareULT(va, vb);
    v = v ? f.Or(v, cmp) : cmp;
  }
  if (TO & (1 << 0)) {
    // a >u b
    auto cmp = f.CompareUGT(va, vb);
    v = v ? f.Or(v, cmp) : cmp;
  }
  if (v) {
    f.TrapTrue(v);
  }
  return 0;
}

s32 HIRInstrEmit_td(HIRBuilder &f, const uPPCInstr &instr) {
  Value *ra = f.LoadGPR(instr.ra);
  Value *rb = f.LoadGPR(instr.rb);
  return InstrEmit_Trap(f, ra, rb, instr.rd);
}

s32 HIRInstrEmit_tdi(HIRBuilder &f, const uPPCInstr &instr) {
  Value *ra = f.LoadGPR(instr.ra);
  Value *rb = f.LoadConstantInt64(SignExtend16(instr.simm16));
  return InstrEmit_Trap(f, ra, rb, instr.rd);
}

s32 HIRInstrEmit_tw(HIRBuilder &f, const uPPCInstr &instr) {
  Value *ra = f.SignExtend(f.Truncate(f.LoadGPR(instr.ra), INT32_TYPE), INT64_TYPE);
  Value *rb = f.SignExtend(f.Truncate(f.LoadGPR(instr.rb), INT32_TYPE), INT64_TYPE);
  return InstrEmit_Trap(f, ra, rb, instr.rd);
}

s32 HIRInstrEmit_twi(HIRBuilder &f, const uPPCInstr &instr) {
  if (instr.ra == 0 && instr.rd == 0x1F) {
    uint16_t type = (uint16_t)SignExtend16(instr.simm16);
    f.Trap(type);
    return 0;
  }

  Value *ra = f.SignExtend(f.Truncate(f.LoadGPR(instr.ra), INT32_TYPE), INT64_TYPE);
  Value *rb = f.LoadConstantInt64(SignExtend16(instr.simm16));
  return InstrEmit_Trap(f, ra, rb, instr.rd);
}

//
// System instructions - fallback to interpreter via CallInterpreter.
// These store the raw instruction data in the HIR Instr and the backend
// writes it to sPPUThread::CI before invoking the interpreter function.
//

s32 HIRInstrEmit_sc(HIRBuilder &f, const uPPCInstr &instr) {
  f.EmitSyscall(instr);
  return 0;
}

s32 HIRInstrEmit_rfid(HIRBuilder &f, const uPPCInstr &instr) {
  f.EmitRFID(instr);
  return 0;
}

s32 HIRInstrEmit_mfspr(HIRBuilder &f, const uPPCInstr &instr) {
  f.StoreGPR(instr.rd, f.EmitMFSPR(instr));
  return 0;
}

s32 HIRInstrEmit_mtspr(HIRBuilder &f, const uPPCInstr &instr) {
  f.EmitMTSPR(f.LoadGPR(instr.rs), instr);
  return 0;
}

s32 HIRInstrEmit_mfmsr(HIRBuilder &f, const uPPCInstr &instr) {
  f.StoreGPR(instr.rd, f.LoadMSR());
  return 0;
}

s32 HIRInstrEmit_mtmsr(HIRBuilder &f, const uPPCInstr &instr) {
  // Operates only in the low 32 bits of MSR
  Value *newMSR = f.And(f.LoadMSR(), f.LoadConstantInt64(0xFFFFFFFF00000000));
  f.StoreMSR(f.Or(newMSR, f.Truncate(f.LoadGPR(instr.rs), HIR::INT32_TYPE)));
  return 0;
}

s32 HIRInstrEmit_mtmsrd(HIRBuilder &f, const uPPCInstr &instr) {
  f.EmitMTMSRD(f.LoadGPR(instr.rs), instr);
  return 0;
}


s32 HIRInstrEmit_mcrf(HIRBuilder &b, const uPPCInstr &instr) {
  // CR[4*crfD : 4*crfD+3] <- CR[4*crfS : 4*crfS+3]
  // Copy a 4-bit CR field from one position to another.
  u32 crfD = instr.crfd;
  u32 crfS = instr.crfs;

  // Load the source field (4 bits).
  Value *srcField = b.LoadCR(static_cast<u8>(crfS));

  // Build the mask for the destination field.
  u32 shift = (7 - crfD) * 4;
  u32 clearMask = ~(0xFu << shift);

  Value *cr = b.LoadCR();
  // Clear the destination field.
  cr = b.And(cr, b.LoadConstantUint32(clearMask));
  // Shift the source field into the destination position.
  Value *shiftedSrc = b.Shl(b.ZeroExtend(srcField, INT32_TYPE), static_cast<u8>(shift));
  cr = b.Or(cr, shiftedSrc);
  b.StoreCR(cr);
  return 0;
}


s32 HIRInstrEmit_mtocrf(HIRBuilder &f, const uPPCInstr &instr) {
  Value *cr = f.LoadCR();
  Value *rSValue = f.LoadGPR(instr.rs);

  u32 crMask = 0;
  u32 bit = 0x80;

  for (; bit; bit >>= 1) {
    crMask <<= 4;
    if (instr.crm & bit) {
      crMask |= 0xF;
    }
  }

  Value *crMasked = f.And(cr, f.LoadConstantUint32(~crMask));
  Value *rsMasked = f.And(f.Truncate(rSValue, HIR::INT32_TYPE), f.LoadConstantUint32(crMask));
  f.StoreCR(f.Or(crMasked, rsMasked));
  return 0;
}

s32 HIRInstrEmit_slbmte(HIRBuilder &f, const uPPCInstr &instr) {
  f.CallInterpreter(instr, reinterpret_cast<void *>(&PPCInterpreter::PPCInterpreter_slbmte));
  return 0;
}

s32 HIRInstrEmit_slbie(HIRBuilder &f, const uPPCInstr &instr) {
  f.CallInterpreter(instr, reinterpret_cast<void *>(&PPCInterpreter::PPCInterpreter_slbie));
  return 0;
}

s32 HIRInstrEmit_slbia(HIRBuilder &f, const uPPCInstr &instr) {
  f.CallInterpreter(instr, reinterpret_cast<void *>(&PPCInterpreter::PPCInterpreter_slbia));
  return 0;
}

s32 HIRInstrEmit_tlbiel(HIRBuilder &f, const uPPCInstr &instr) {
  f.CallInterpreter(instr, reinterpret_cast<void *>(&PPCInterpreter::PPCInterpreter_tlbiel));
  return 0;
}

s32 HIRInstrEmit_tlbie(HIRBuilder &f, const uPPCInstr &instr) {
  f.CallInterpreter(instr, reinterpret_cast<void *>(&PPCInterpreter::PPCInterpreter_tlbie));
  return 0;
}

s32 HIRInstrEmit_tlbsync(HIRBuilder &f, const uPPCInstr &instr) {
  f.CallInterpreter(instr, reinterpret_cast<void *>(&PPCInterpreter::PPCInterpreter_tlbsync));
  return 0;
}

s32 HIRInstrEmit_mftb(HIRBuilder &f, const uPPCInstr &instr) {
  Value *time = f.LoadTimeBase();

  const u32 spr = (instr.spr >> 5) | ((instr.spr & 0x1f) << 5);
  if (spr == 268) {
    // TB - full bits.
  } else {
    // TBU - upper bits only.
    time = f.Shr(time, 32);
  }
  f.StoreGPR(instr.rd, time);
  return 0;
}

// slbmfev/slbmfee: No interpreter implementations exist yet - emit as nop.
s32 HIRInstrEmit_slbmfev(HIRBuilder &f, const uPPCInstr &instr) {
  f.Nop();
  return 0;
}

s32 HIRInstrEmit_slbmfee(HIRBuilder &f, const uPPCInstr &instr) {
  f.Nop();
  return 0;
}

} // namespace Xe::XCPU::HIR