// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_isync(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_eieio(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_sc(PPU_STATE *ppuState) {
  SC_FORM_LEV;

  // Raise the exception.
  _ex |= PPU_EX_SC;
  curThread.exceptHVSysCall = LEV & 1;
}

void PPCInterpreter::PPCInterpreter_slbmte(PPU_STATE *ppuState) {
  X_FORM_rS_rB;

  u64 VSID = QGET(GPR(rS), 0, 51);

  u8 Ks = QGET(GPR(rS), 52, 52);
  u8 Kp = QGET(GPR(rS), 53, 53);
  u8 N = QGET(GPR(rS), 54, 54);
  u8 L = QGET(GPR(rS), 55, 55);
  u8 C = QGET(GPR(rS), 56, 56);
  u8 LP = QGET(GPR(rS), 57, 59);

  u64 ESID = QGET(GPR(rB), 0, 35);
  bool V = QGET(GPR(rB), 36, 36);
  u16 Index = QGET(GPR(rB), 52, 63);

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
  curThread.SLB[Index].vsidReg = GPR(rS);
  curThread.SLB[Index].esidReg = GPR(rB);
}

void PPCInterpreter::PPCInterpreter_slbie(PPU_STATE *ppuState) {
  X_FORM_rB;

  // ESID.
  const u64 ESID = QGET(GPR(rB), 0, 35);
  // Class.
  const u8 C = QGET(GPR(rB), 36, 36);

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

void PPCInterpreter::PPCInterpreter_rfid(PPU_STATE *ppuState) {
  u64 srr1, new_msr, diff_msr;
  u32 b3, b, usr;

  // Compose new MSR as per specs
  srr1 = curThread.SPR.SRR1;
  new_msr = 0;

  usr = BGET(srr1, 64, 49);
  if (usr) {
    //(("WARNING!! Cannot really do Problem mode"));
    BSET(new_msr, 64, 49);
  }

  // MSR.0 = SRR1.0 | SRR1.1
  b = BGET(srr1, 64, 0) || BGET(srr1, 64, 1);
  if (b) {
    BSET(new_msr, 64, 0);
  }

  b3 = BGET(curThread.SPR.MSR.MSR_Hex, 64, 3);

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
  diff_msr = curThread.SPR.MSR.MSR_Hex ^ new_msr;

  // NB: we dont do half-modes
  if (diff_msr & QMASK(58, 59)) {
    curThread.SPR.MSR.IR = usr != 0 || (new_msr & QMASK(58, 59)) != 0;
    curThread.SPR.MSR.DR = usr != 0 || (new_msr & QMASK(58, 59)) != 0;
  }

  curThread.SPR.MSR.MSR_Hex = new_msr;
  curThread.NIA = curThread.SPR.SRR0 & ~3;

  // Clear exception taken flag.
  curThread.exceptionTaken = false;
}

void PPCInterpreter::PPCInterpreter_tw(PPU_STATE *ppuState) {
  X_FORM_TO_rA_rB;

  sl32 a = static_cast<sl32>(GPR(rA));
  sl32 b = static_cast<sl32>(GPR(rB));

  if ((a < b && BGET(TO, 5, 0)) || (a > b && BGET(TO, 5, 1)) ||
      (a == b && BGET(TO, 5, 2)) || (static_cast<u32>(a) < static_cast<u32>(b) && BGET(TO, 5, 3)) ||
      (static_cast<u32>(a) > static_cast<u32>(b) && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(ppuState, b);
  }
}

void PPCInterpreter::PPCInterpreter_twi(PPU_STATE *ppuState) {
  D_FORM_TO_rA_SI;
  SI = EXTS(SI, 16);

  sl32 a = static_cast<s32>(GPR(rA));

  if ((a < static_cast<s32>(SI) && BGET(TO, 5, 0)) || (a > static_cast<s32>(SI) && BGET(TO, 5, 1)) ||
      (a == static_cast<s32>(SI) && BGET(TO, 5, 2)) || (static_cast<u32>(a) < SI && BGET(TO, 5, 3)) ||
      (static_cast<u32>(a) > SI && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(SI));
  }
}

void PPCInterpreter::PPCInterpreter_tdi(PPU_STATE *ppuState) {
  D_FORM_TO_rA_SI;
  SI = EXTS(SI, 16);

  s64 rAReg = static_cast<s64>(GPR(rA));

  if ((rAReg < static_cast<s64>(SI) && BGET(TO, 5, 0)) ||
      (rAReg > static_cast<s64>(SI) && BGET(TO, 5, 1)) ||
      (rAReg == static_cast<s64>(SI) && BGET(TO, 5, 2)) ||
      (static_cast<u64>(rAReg) < SI && BGET(TO, 5, 3)) ||
      (static_cast<u64>(rAReg) > SI && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(SI));
  }
}

void PPCInterpreter::PPCInterpreter_mfspr(PPU_STATE *ppuState) {
  u64 rS = 0, crm = 0;
  PPC_OPC_TEMPL_XFX(_instr.opcode, rS, crm);
  u32 sprNum = _instr.spr;
  sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);

  u64 value = 0;

  switch (sprNum) {
  case SPR_XER:
    value = curThread.SPR.XER.XER_Hex;
    break;
  case SPR_LR:
    value = curThread.SPR.LR;
    break;
  case SPR_CTR:
    value = curThread.SPR.CTR;
    break;
  case SPR_DSISR:
    value = curThread.SPR.DSISR;
    break;
  case SPR_DAR:
    value = curThread.SPR.DAR;
    break;
  case SPR_DEC:
    value = curThread.SPR.DEC;
    break;
  case SPR_SDR1:
    value = ppuState->SPR.SDR1;
    break;
  case SPR_SRR0:
    value = curThread.SPR.SRR0;
    break;
  case SPR_SRR1:
    value = curThread.SPR.SRR1;
    break;
  case SPR_CFAR:
    value = curThread.SPR.CFAR;
    break;
  case SPR_CTRLRD:
    value = ppuState->SPR.CTRL;
    break;
  case SPR_VRSAVE:
    value = curThread.SPR.VRSAVE;
    break;
  case SPR_TBL_RO:
    value = ppuState->SPR.TB;
    break;
  case SPR_TBU_RO:
    value = (ppuState->SPR.TB & 0xFFFFFFFF00000000);
    break;
  case SPR_SPRG0:
    value = curThread.SPR.SPRG0;
    break;
  case SPR_SPRG1:
    value = curThread.SPR.SPRG1;
    break;
  case SPR_SPRG2:
    value = curThread.SPR.SPRG2;
    break;
  case SPR_SPRG3:
    value = curThread.SPR.SPRG3;
    break;
  case SPR_TB:
    value = ppuState->SPR.TB;
    break;
  case SPR_PVR:
    value = ppuState->SPR.PVR.PVR_Hex;
    break;
  case SPR_HSPRG0:
    value = curThread.SPR.HSPRG0;
    break;
  case SPR_HSPRG1:
    value = curThread.SPR.HSPRG1;
    break;
  case SPR_RMOR:
    value = ppuState->SPR.RMOR;
    break;
  case SPR_HRMOR:
    value = ppuState->SPR.HRMOR;
    break;
  case SPR_LPCR:
    value = ppuState->SPR.LPCR;
    break;
  case SPR_TSCR:
    value = ppuState->SPR.TSCR;
    break;
  case SPR_TTR:
    value = ppuState->SPR.TTR;
    break;
  case SPR_PpeTlbIndexHint:
    value = curThread.SPR.PPE_TLB_Index_Hint;
    break;
  case SPR_HID0:
    value = ppuState->SPR.HID0;
    break;
  case SPR_HID1:
    value = ppuState->SPR.HID1;
    break;
  case SPR_HID4:
    value = ppuState->SPR.HID4;
    break;
  case SPR_DABR:
    value = curThread.SPR.DABR;
    break;
  case SPR_HID6:
    value = ppuState->SPR.HID6;
    break;
  case SPR_PIR:
    value = curThread.SPR.PIR;
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: {:#x}", ppuState->ppuName, static_cast<u8>(curThreadId), sprNum);
    break;
  }

  GPR(rS) = value;
}

void PPCInterpreter::PPCInterpreter_mtspr(PPU_STATE *ppuState) {
  XFX_FORM_rD_spr;
  switch (spr) {
  case SPR_XER:
    curThread.SPR.XER.XER_Hex = static_cast<u32>(GPR(rD));
    // Clear the unused bits in XER (35:56).
    curThread.SPR.XER.XER_Hex &= 0xE000007F;
    break;
  case SPR_LR:
    curThread.SPR.LR = GPR(rD);
    break;
  case SPR_CTR:
    curThread.SPR.CTR = GPR(rD);
    break;
  case SPR_DSISR:
    curThread.SPR.DSISR = GPR(rD);
    break;
  case SPR_DAR:
    curThread.SPR.DAR = GPR(rD);
    break;
  case SPR_DEC:
    curThread.SPR.DEC = static_cast<u32>(GPR(rD));
    break;
  case SPR_SDR1:
    ppuState->SPR.SDR1 = GPR(rD);
    break;
  case SPR_SRR0:
    curThread.SPR.SRR0 = GPR(rD);
    break;
  case SPR_SRR1:
    curThread.SPR.SRR1 = GPR(rD);
    break;
  case SPR_CFAR:
    curThread.SPR.CFAR = GPR(rD);
    break;
  case SPR_CTRLRD:
  case SPR_CTRLWR:
    ppuState->SPR.CTRL = static_cast<u32>(GPR(rD));
    break;
  case SPR_VRSAVE:
    curThread.SPR.VRSAVE = static_cast<u32>(GPR(rD));
    break;
  case SPR_SPRG0:
    curThread.SPR.SPRG0 = GPR(rD);
    break;
  case SPR_SPRG1:
    curThread.SPR.SPRG1 = GPR(rD);
    break;
  case SPR_SPRG2:
    curThread.SPR.SPRG2 = GPR(rD);
    break;
  case SPR_SPRG3:
    curThread.SPR.SPRG3 = GPR(rD);
    break;
  case SPR_TBL_WO:
    ppuState->SPR.TB = GPR(rD);
    break;
  case SPR_TBU_WO:
    ppuState->SPR.TB = ppuState->SPR.TB |= (GPR(rD) << 32);
    break;
  case SPR_HSPRG0:
    curThread.SPR.HSPRG0 = GPR(rD);
    break;
  case SPR_HSPRG1:
    curThread.SPR.HSPRG1 = GPR(rD);
    break;
  case SPR_HDEC:
    ppuState->SPR.HDEC = static_cast<u32>(GPR(rD));
    break;
  case SPR_RMOR:
    ppuState->SPR.RMOR = GPR(rD);
    break;
  case SPR_HRMOR:
    ppuState->SPR.HRMOR = GPR(rD);
    break;
  case SPR_LPCR:
    ppuState->SPR.LPCR = GPR(rD);
    break;
  case SPR_LPIDR:
    ppuState->SPR.LPIDR = static_cast<u32>(GPR(rD));
    break;
  case SPR_TSCR:
    ppuState->SPR.TSCR = static_cast<u32>(GPR(rD));
    break;
  case SPR_TTR:
    ppuState->SPR.TTR = GPR(rD);
    break;
  case SPR_PpeTlbIndex:
    ppuState->SPR.PPE_TLB_Index = GPR(rD);
    break;
  case SPR_PpeTlbIndexHint:
    curThread.SPR.PPE_TLB_Index_Hint = GPR(rD);
    break;
  case SPR_PpeTlbVpn:
    ppuState->SPR.PPE_TLB_VPN = GPR(rD);
    mmuAddTlbEntry(ppuState);
    break;
  case SPR_PpeTlbRpn:
    ppuState->SPR.PPE_TLB_RPN = GPR(rD);
    break;
  case SPR_HID0:
    ppuState->SPR.HID0 = GPR(rD);
    break;
  case SPR_HID1:
    ppuState->SPR.HID1 = GPR(rD);
    break;
  case SPR_HID4:
    ppuState->SPR.HID4 = GPR(rD);
    break;
  case SPR_HID6:
    ppuState->SPR.HID6 = GPR(rD);
    break;
  case SPR_DABR:
    curThread.SPR.DABR = GPR(rD);
    break;
  case SPR_DABRX:
    curThread.SPR.DABRX = GPR(rD);
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) SPR {:#x} ={:#x}", ppuState->ppuName, static_cast<u8>(curThreadId), spr, GPR(rD));
    break;
  }
}

void PPCInterpreter::PPCInterpreter_mfmsr(PPU_STATE *ppuState) {
  X_FORM_rD;
  GPR(rD) = curThread.SPR.MSR.MSR_Hex;
}

void PPCInterpreter::PPCInterpreter_mtmsr(PPU_STATE *ppuState) {
  X_FORM_rS;

  curThread.SPR.MSR.MSR_Hex = GPR(rS);
}

void PPCInterpreter::PPCInterpreter_mtmsrd(PPU_STATE *ppuState) {
  X_FORM_rS_L;

  if (L) {
    /*
       Bits 48 and 62 of register RS are placed into the corresponding bits
       of the MSR. The remaining bits of the MSR are unchanged.
    */

    // Bit 48 = MSR[EE]
    curThread.SPR.MSR.EE = (GPR(rS) & 0x8000) == 0x8000 ? 1 : 0;
    // Bit 62 = MSR[RI]
    curThread.SPR.MSR.RI = (GPR(rS) & 0x2) == 0x2 ? 1 : 0;
  } else { // L = 0
    /*
       The result of ORing bits 0 and 1 of register RS is placed into MSR0.
       The result of ORing bits 48 and 49 of register RS is placed into MSR48.
       The result of ORing bits 58 and 49 of register RS is placed into MSR58.
       The result of ORing bits 59 and 49 of register RS is placed into MSR59.
       Bits 1:2, 4:47, 49:50, 52:57, and 60:63 of register RS are placed into
       the corresponding bits of the MSR.
    */
    u64 regRS = GPR(rS);
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
}

void PPCInterpreter::PPCInterpreter_sync(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbf(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbi(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbt(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbtst(PPU_STATE *ppuState) {
  // Do nothing.
}
