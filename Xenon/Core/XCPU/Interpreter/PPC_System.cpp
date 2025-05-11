// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

// Instruction Synchronize
void PPCInterpreter::PPCInterpreter_isync(PPU_STATE *ppuState) {
  // Do nothing
}

// Enforce In-order Execution of I/O
void PPCInterpreter::PPCInterpreter_eieio(PPU_STATE *ppuState) {
  // Do nothing
}

// System Call
void PPCInterpreter::PPCInterpreter_sc(PPU_STATE *ppuState) {
  SC_FORM_LEV;

  // Raise the exception.
  _ex |= PPU_EX_SC;
  curThread.exceptHVSysCall = LEV & 1;
}

// SLB Move To Entry
void PPCInterpreter::PPCInterpreter_slbmte(PPU_STATE *ppuState) {
  u64 VSID = QGET(GPRi(rs), 0, 51);

  const u8 Ks = QGET(GPRi(rs), 52, 52);
  const u8 Kp = QGET(GPRi(rs), 53, 53);
  const u8 N = QGET(GPRi(rs), 54, 54);
  const u8 L = QGET(GPRi(rs), 55, 55);
  const u8 C = QGET(GPRi(rs), 56, 56);
  const u8 LP = QGET(GPRi(rs), 57, 59);

  const u64 ESID = QGET(GPRi(rb), 0, 35);
  bool V = QGET(GPRi(rb), 36, 36);
  const u16 Index = QGET(GPRi(rb), 52, 63);

  // VSID is VA 0-52 bit, the remaining 28 bits are adress data
  // so whe shift 28 bits left here so we only do it once per entry.
  // This speeds MMU translation since the shift is only done once.
  VSID = VSID << 28;

  curThread.SLB[Index].ESID = ESID;
  curThread.SLB[Index].VSID = VSID;
  curThread.SLB[Index].V = V;
  curThread.SLB[Index].Kp = Kp;
  curThread.SLB[Index].Ks = Ks;
  curThread.SLB[Index].N = N;
  curThread.SLB[Index].L = L;
  curThread.SLB[Index].C = C;
  curThread.SLB[Index].LP = LP;
  curThread.SLB[Index].vsidReg = GPRi(rs);
  curThread.SLB[Index].esidReg = GPRi(rb);
}

// SLB Invalidate Entry
void PPCInterpreter::PPCInterpreter_slbie(PPU_STATE *ppuState) {
  // ESID
  const u64 ESID = QGET(GPRi(rb), 0, 35);
  // Class
  const u8 C = QGET(GPRi(rb), 36, 36);

  for (auto &slbEntry : curThread.SLB) {
    if (slbEntry.V && slbEntry.C == C && slbEntry.ESID == ESID) {
      slbEntry.V = false;
    }
  }

  // Should only invalidate entries for a specific set of addresses.
  // Invalidate both ERAT's *** BUG *** !!!
  curThread.iERAT.invalidateAll();
  curThread.dERAT.invalidateAll();
}

// Return From Interrupt Doubleword
void PPCInterpreter::PPCInterpreter_rfid(PPU_STATE *ppuState) {

  // Compose new MSR as per specs
  const u64 srr1 = curThread.SPR.SRR1;
  u64 new_msr = 0;

  const u32 usr = BGET(srr1, 64, 49);
  if (usr) {
    //(("WARNING!! Cannot really do Problem mode"));
    BSET(new_msr, 64, 49);
  }

  // MSR.0 = SRR1.0 | SRR1.1
  const u32 b = BGET(srr1, 64, 0) || BGET(srr1, 64, 1);
  if (b) {
    BSET(new_msr, 64, 0);
  }

  const u32 b3 = BGET(curThread.SPR.MSR.MSR_Hex, 64, 3);

  // MSR.51 = (MSR.3 & SRR1.51) | ((~MSR.3) & MSR.51)
  if ((b3 && BGET(srr1, 64, 51)) || (!b3 && BGET(curThread.SPR.MSR.MSR_Hex, 64, 51))) {
    BSET(new_msr, 64, 51);
  }

  // MSR.3 = MSR.3 & SRR1.3
  if (b3 && BGET(srr1, 64, 3)) {
    BSET(new_msr, 64, 3);
  }

  // MSR.48 = SRR1.48
  if (BGET(srr1, 64, 48)) {
    BSET(new_msr, 64, 48);
  }

  // MSR.58 = SRR1.58 | SRR1.49
  if (usr || BGET(srr1, 64, 58)) {
    BSET(new_msr, 64, 58);
  }

  // MSR.59 = SRR1.59 | SRR1.49
  if (usr || BGET(srr1, 64, 59)) {
    BSET(new_msr, 64, 59);
  }

  // MSR.1:2,4:32,37:41,49:50,52:57,60:63 = SRR1.<same>
  new_msr = new_msr | (srr1 & (QMASK(1, 2) | QMASK(4, 32) | QMASK(37, 41) |
                               QMASK(49, 50) | QMASK(52, 57) | QMASK(60, 63)));

  // See what changed and take actions
  // NB: we ignore a bunch of bits..
  const u64 diff_msr = curThread.SPR.MSR.MSR_Hex ^ new_msr;

  // NB: we dont do half-modes
  if (diff_msr & QMASK(58, 59)) {
    curThread.SPR.MSR.IR = usr != 0 || (new_msr & QMASK(58, 59)) != 0;
    curThread.SPR.MSR.DR = usr != 0 || (new_msr & QMASK(58, 59)) != 0;
  }

  curThread.SPR.MSR.MSR_Hex = new_msr;
  curThread.NIA = curThread.SPR.SRR0 & ~3;

  // Check for 32-bit mode of operation.
  if (!curThread.SPR.MSR.SF)
    curThread.NIA = static_cast<u32>(curThread.NIA);

  // Clear exception taken flag.
  curThread.exceptionTaken = false;
}

// Trap Word
void PPCInterpreter::PPCInterpreter_tw(PPU_STATE *ppuState) {
  const sl32 a = static_cast<sl32>(GPRi(ra));
  const sl32 b = static_cast<sl32>(GPRi(rb));

  if ((a < b && (_instr.bo & 0x10)) ||
      (a > b && (_instr.bo & 0x8)) ||
      (a == b && (_instr.bo & 0x4)) ||
      (static_cast<u32>(a) < static_cast<u32>(b) && (_instr.bo & 0x2)) ||
      (static_cast<u32>(a) > static_cast<u32>(b) && (_instr.bo & 0x1))) {
    ppcInterpreterTrap(ppuState, b);
  }
}

// Trap Word Immediate
void PPCInterpreter::PPCInterpreter_twi(PPU_STATE *ppuState) {
  const s64 sA = GPRi(ra), sB = _instr.simm16;
  const u64 a = sA, b = sB;

  if (((_instr.bo & 0x10) && sA < sB) ||
      ((_instr.bo & 0x8) && sA > sB) ||
      ((_instr.bo & 0x4) && sA == sB) ||
      ((_instr.bo & 0x2) && a < b) ||
      ((_instr.bo & 0x1) && a > b)) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(sB));
  }
}

// Trap Doubleword
void PPCInterpreter::PPCInterpreter_td(PPU_STATE *ppuState) {
  const s64 sA = GPRi(ra), sB = GPRi(rb);
  const u64 a = sA, b = sB;

  if (((_instr.bo & 0x10) && sA < sB) ||
      ((_instr.bo & 0x8) && sA > sB) ||
      ((_instr.bo & 0x4) && sA == sB) ||
      ((_instr.bo & 0x2) && a < b) ||
      ((_instr.bo & 0x1) && a > b)) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(sB));
  }
}

// Trap Doubleword Immediate
void PPCInterpreter::PPCInterpreter_tdi(PPU_STATE *ppuState) {
  const s64 sA = GPRi(ra), sB = _instr.simm16;
  const u64 a = sA, b = sB;

  if (((_instr.bo & 0x10) && sA < sB) ||
      ((_instr.bo & 0x8) && sA > sB) ||
      ((_instr.bo & 0x4) && sA == sB) ||
      ((_instr.bo & 0x2) && a < b) ||
      ((_instr.bo & 0x1) && a > b)) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(sB));
  }
}

// Move From Special Purpose Register
void PPCInterpreter::PPCInterpreter_mfspr(PPU_STATE *ppuState) {
  u32 spr = _instr.spr;
  spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F);

  switch (spr) {
  case SPR_XER:
    GPRi(rs) = curThread.SPR.XER.XER_Hex;
    break;
  case SPR_LR:
    GPRi(rs) = curThread.SPR.LR;
    break;
  case SPR_CTR:
    GPRi(rs) = curThread.SPR.CTR;
    break;
  case SPR_DSISR:
    GPRi(rs) = curThread.SPR.DSISR;
    break;
  case SPR_DAR:
    GPRi(rs) = curThread.SPR.DAR;
    break;
  case SPR_DEC:
    GPRi(rs) = curThread.SPR.DEC;
    break;
  case SPR_SDR1:
    GPRi(rs) = ppuState->SPR.SDR1;
    break;
  case SPR_SRR0:
    GPRi(rs) = curThread.SPR.SRR0;
    break;
  case SPR_SRR1:
    GPRi(rs) = curThread.SPR.SRR1;
    break;
  case SPR_CFAR:
    GPRi(rs) = curThread.SPR.CFAR;
    break;
  case SPR_CTRLRD:
    GPRi(rs) = ppuState->SPR.CTRL;
    break;
  case SPR_VRSAVE:
    GPRi(rs) = curThread.SPR.VRSAVE;
    break;
  case SPR_TBL_RO:
    GPRi(rs) = ppuState->SPR.TB;
    break;
  case SPR_TBU_RO:
    GPRi(rs) = (ppuState->SPR.TB & 0xFFFFFFFF00000000);
    break;
  case SPR_SPRG0:
    GPRi(rs) = curThread.SPR.SPRG0;
    break;
  case SPR_SPRG1:
    GPRi(rs) = curThread.SPR.SPRG1;
    break;
  case SPR_SPRG2:
    GPRi(rs) = curThread.SPR.SPRG2;
    break;
  case SPR_SPRG3:
    GPRi(rs) = curThread.SPR.SPRG3;
    break;
  case SPR_TB:
    GPRi(rs) = ppuState->SPR.TB;
    break;
  case SPR_PVR:
    GPRi(rs) = ppuState->SPR.PVR.PVR_Hex;
    break;
  case SPR_HSPRG0:
    GPRi(rs) = curThread.SPR.HSPRG0;
    break;
  case SPR_HSPRG1:
    GPRi(rs) = curThread.SPR.HSPRG1;
    break;
  case SPR_RMOR:
    GPRi(rs) = ppuState->SPR.RMOR;
    break;
  case SPR_HRMOR:
    GPRi(rs) = ppuState->SPR.HRMOR;
    break;
  case SPR_LPCR:
    GPRi(rs) = ppuState->SPR.LPCR;
    break;
  case SPR_TSCR:
    GPRi(rs) = ppuState->SPR.TSCR;
    break;
  case SPR_TTR:
    GPRi(rs) = ppuState->SPR.TTR;
    break;
  case SPR_PpeTlbIndexHint:
    GPRi(rs) = curThread.SPR.PPE_TLB_Index_Hint;
    break;
  case SPR_HID0:
    GPRi(rs) = ppuState->SPR.HID0;
    break;
  case SPR_HID1:
    GPRi(rs) = ppuState->SPR.HID1;
    break;
  case SPR_HID4:
    GPRi(rs) = ppuState->SPR.HID4;
    break;
  case SPR_DABR:
    GPRi(rs) = curThread.SPR.DABR;
    break;
  case SPR_HID6:
    GPRi(rs) = ppuState->SPR.HID6;
    break;
  case SPR_PIR:
    GPRi(rs) = curThread.SPR.PIR;
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: 0x{:X}", ppuState->ppuName, static_cast<u8>(curThreadId), spr);
    break;
  }
}

// Move To Special Purpose Register
void PPCInterpreter::PPCInterpreter_mtspr(PPU_STATE *ppuState) {
  u32 spr = _instr.spr;
  spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F);

  switch (spr) {
  case SPR_XER:
    curThread.SPR.XER.XER_Hex = static_cast<u32>(GPRi(rd));
    // Clear the unused bits in XER (35:56)
    curThread.SPR.XER.XER_Hex &= 0xE000007F;
    break;
  case SPR_LR:
    curThread.SPR.LR = GPRi(rd);
    break;
  case SPR_CTR:
    curThread.SPR.CTR = GPRi(rd);
    break;
  case SPR_DSISR:
    curThread.SPR.DSISR = GPRi(rd);
    break;
  case SPR_DAR:
    curThread.SPR.DAR = GPRi(rd);
    break;
  case SPR_DEC:
    curThread.SPR.DEC = static_cast<u32>(GPRi(rd));
    break;
  case SPR_SDR1:
    ppuState->SPR.SDR1 = GPRi(rd);
    break;
  case SPR_SRR0:
    curThread.SPR.SRR0 = GPRi(rd);
    break;
  case SPR_SRR1:
    curThread.SPR.SRR1 = GPRi(rd);
    break;
  case SPR_CFAR:
    curThread.SPR.CFAR = GPRi(rd);
    break;
  case SPR_CTRLRD:
  case SPR_CTRLWR:
    ppuState->SPR.CTRL = static_cast<u32>(GPRi(rd));
    break;
  case SPR_VRSAVE:
    curThread.SPR.VRSAVE = static_cast<u32>(GPRi(rd));
    break;
  case SPR_SPRG0:
    curThread.SPR.SPRG0 = GPRi(rd);
    break;
  case SPR_SPRG1:
    curThread.SPR.SPRG1 = GPRi(rd);
    break;
  case SPR_SPRG2:
    curThread.SPR.SPRG2 = GPRi(rd);
    break;
  case SPR_SPRG3:
    curThread.SPR.SPRG3 = GPRi(rd);
    break;
  case SPR_TBL_WO:
    ppuState->SPR.TB = GPRi(rd);
    break;
  case SPR_TBU_WO:
    ppuState->SPR.TB = ppuState->SPR.TB |= (GPRi(rd) << 32);
    break;
  case SPR_HSPRG0:
    curThread.SPR.HSPRG0 = GPRi(rd);
    break;
  case SPR_HSPRG1:
    curThread.SPR.HSPRG1 = GPRi(rd);
    break;
  case SPR_HDEC:
    ppuState->SPR.HDEC = static_cast<u32>(GPRi(rd));
    break;
  case SPR_RMOR:
    ppuState->SPR.RMOR = GPRi(rd);
    break;
  case SPR_HRMOR:
    ppuState->SPR.HRMOR = GPRi(rd);
    break;
  case SPR_LPCR:
    ppuState->SPR.LPCR = GPRi(rd);
    break;
  case SPR_LPIDR:
    ppuState->SPR.LPIDR = static_cast<u32>(GPRi(rd));
    break;
  case SPR_TSCR:
    ppuState->SPR.TSCR = static_cast<u32>(GPRi(rd));
    break;
  case SPR_TTR:
    ppuState->SPR.TTR = GPRi(rd);
    break;
  case SPR_PpeTlbIndex:
    ppuState->SPR.PPE_TLB_Index = GPRi(rd);
    break;
  case SPR_PpeTlbIndexHint:
    curThread.SPR.PPE_TLB_Index_Hint = GPRi(rd);
    break;
  case SPR_PpeTlbVpn:
    ppuState->SPR.PPE_TLB_VPN = GPRi(rd);
    mmuAddTlbEntry(ppuState);
    break;
  case SPR_PpeTlbRpn:
    ppuState->SPR.PPE_TLB_RPN = GPRi(rd);
    break;
  case SPR_HID0:
    ppuState->SPR.HID0 = GPRi(rd);
    break;
  case SPR_HID1:
    ppuState->SPR.HID1 = GPRi(rd);
    break;
  case SPR_HID4:
    ppuState->SPR.HID4 = GPRi(rd);
    break;
  case SPR_HID6:
    ppuState->SPR.HID6 = GPRi(rd);
    break;
  case SPR_DABR:
    curThread.SPR.DABR = GPRi(rd);
    break;
  case SPR_DABRX:
    curThread.SPR.DABRX = GPRi(rd);
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) SPR 0x{:X} =0x{:X}", ppuState->ppuName, static_cast<u8>(curThreadId), spr, GPRi(rd));
    break;
  }
}

// Move From Machine State Register
void PPCInterpreter::PPCInterpreter_mfmsr(PPU_STATE *ppuState) {
  GPRi(rd) = curThread.SPR.MSR.MSR_Hex;
}

// Move To Machine State Register
void PPCInterpreter::PPCInterpreter_mtmsr(PPU_STATE *ppuState) {
  curThread.SPR.MSR.MSR_Hex = GPRi(rs);

  // Check for 32-bit mode of operation
  if (!curThread.SPR.MSR.SF)
    curThread.NIA = static_cast<u32>(curThread.NIA);
}

// Move To Machine State Register Doubleword
void PPCInterpreter::PPCInterpreter_mtmsrd(PPU_STATE *ppuState) {
  if (_instr.l15) {
    /*
       Bits 48 and 62 of register RS are placed into the corresponding bits
       of the MSR. The remaining bits of the MSR are unchanged
    */

    // Bit 48 = MSR[EE]
    curThread.SPR.MSR.EE = (GPRi(rs) & 0x8000) == 0x8000 ? 1 : 0;
    // Bit 62 = MSR[RI]
    curThread.SPR.MSR.RI = (GPRi(rs) & 0x2) == 0x2 ? 1 : 0;
  } else { // L = 0
    /*
       The result of ORing bits 00 and 01 of register RS is placed into MSR0
       The result of ORing bits 48 and 49 of register RS is placed into MSR48
       The result of ORing bits 58 and 49 of register RS is placed into MSR58
       The result of ORing bits 59 and 49 of register RS is placed into MSR59
       Bits 1:2, 4:47, 49:50, 52:57, and 60:63 of register RS are placed into
       the corresponding bits of the MSR
    */
    const u64 regRS = GPRi(rs);
    curThread.SPR.MSR.MSR_Hex = regRS;

    // MSR0 = (RS)0 | (RS)1
    curThread.SPR.MSR.SF = (regRS & 0x8000000000000000) || (regRS & 0x4000000000000000) ? 1 : 0;

    // MSR48 = (RS)48 | (RS)49
    curThread.SPR.MSR.EE = (regRS & 0x8000) || (regRS & 0x4000) ? 1 : 0;

    // MSR58 = (RS)58 | (RS)49
    curThread.SPR.MSR.IR = (regRS & 0x20) || (regRS & 0x4000) ? 1 : 0;

    // MSR59 = (RS)59 | (RS)49
    curThread.SPR.MSR.DR = (regRS & 0x10) || (regRS & 0x4000) ? 1 : 0;
  }

  // Check for 32-bit mode of operation.
  if (!curThread.SPR.MSR.SF)
    curThread.NIA = static_cast<u32>(curThread.NIA);
}

// Synchronize
void PPCInterpreter::PPCInterpreter_sync(PPU_STATE *ppuState) {
  // Do nothing
}

// Data Cache Block Flush
void PPCInterpreter::PPCInterpreter_dcbf(PPU_STATE *ppuState) {
  // Do nothing
}

// Data Cache Block Invalidate
void PPCInterpreter::PPCInterpreter_dcbi(PPU_STATE *ppuState) {
  // Do nothing
}

// Data Cache Block Touch
void PPCInterpreter::PPCInterpreter_dcbt(PPU_STATE *ppuState) {
  // Do nothing
}

// Data Cache Block Touch for Store
void PPCInterpreter::PPCInterpreter_dcbtst(PPU_STATE *ppuState) {
  // Do nothing
}
