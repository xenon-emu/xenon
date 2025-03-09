// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_isync(PPU_STATE *ppuState) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_eieio(PPU_STATE* hCore)
{
    // Do nothing.
}

void PPCInterpreter::PPCInterpreter_sc(PPU_STATE *ppuState) {
  SC_FORM_LEV;

  // Raise the exception.
  ppuState->ppuThread[ppuState->currentThread].exceptReg |= PPU_EX_SC;
  ppuState->ppuThread[ppuState->currentThread].exceptHVSysCall = LEV & 1;
}

void PPCInterpreter::PPCInterpreter_slbmte(PPU_STATE *ppuState) {
  X_FORM_rS_rB;

  u64 VSID = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 0, 51);

  u8 Ks = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 52, 52);
  u8 Kp = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 53, 53);
  u8 N = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 54, 54);
  u8 L = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 55, 55);
  u8 C = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 56, 56);
  u8 LP = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rS], 57, 59);

  u64 ESID = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rB], 0, 35);
  bool V = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rB], 36, 36);
  u16 Index = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rB], 52, 63);

  // VSID is VA 0-52 bit, the remaining 28 bits are adress data
  // so whe shift 28 bits left here so we only do it once per entry.
  // This speeds MMU translation since the shift is only done once.
  VSID = VSID << 28;

  ppuState->ppuThread[ppuState->currentThread].SLB[Index].ESID = ESID;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].VSID = VSID;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].V = V;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].Kp = Kp;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].Ks = Ks;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].N = N;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].L = L;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].C = C;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].LP = LP;
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].vsidReg =
      ppuState->ppuThread[ppuState->currentThread].GPR[rS];
  ppuState->ppuThread[ppuState->currentThread].SLB[Index].esidReg =
      ppuState->ppuThread[ppuState->currentThread].GPR[rB];
}

void PPCInterpreter::PPCInterpreter_slbie(PPU_STATE *ppuState) {
  X_FORM_rB;

  // ESID.
  const u64 ESID = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rB], 0, 35);
  // Class.
  const u8 C = QGET(ppuState->ppuThread[ppuState->currentThread].GPR[rB], 36, 36);

  for (auto &slbEntry : ppuState->ppuThread[ppuState->currentThread].SLB) {
    if (slbEntry.V && slbEntry.C == C && slbEntry.ESID == ESID) {
      slbEntry.V = false;
    }
  }
}

void PPCInterpreter::PPCInterpreter_rfid(PPU_STATE *ppuState) {
  u64 srr1, new_msr, diff_msr;
  u32 b3, b, usr;

  // Compose new MSR as per specs
  srr1 = ppuState->ppuThread[ppuState->currentThread].SPR.SRR1;
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

  b3 = BGET(ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex, 64, 3);

  // MSR.51 = (MSR.3 & SRR1.51) | ((~MSR.3) & MSR.51)
  if ((b3 && BGET(srr1, 64, 51)) ||
      (!b3 &&
       BGET(ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex, 64, 51))) {
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
  diff_msr = ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex ^ new_msr;

  // NB: we dont do half-modes
  if (diff_msr & QMASK(58, 59)) {
    if (usr) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.IR = true;
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.DR = true;
    } else if (new_msr & QMASK(58, 59)) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.IR = true;
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.DR = true;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.IR = false;
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.DR = false;
    }
  }

  ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex = new_msr;
  ppuState->ppuThread[ppuState->currentThread].NIA =
      ppuState->ppuThread[ppuState->currentThread].SPR.SRR0 & ~3;

  // Clear exception taken flag.
  ppuState->ppuThread[ppuState->currentThread].exceptionTaken = false;
}

void PPCInterpreter::PPCInterpreter_tw(PPU_STATE *ppuState) {
  X_FORM_TO_rA_rB;

  sl32 a = static_cast<sl32>(ppuState->ppuThread[ppuState->currentThread].GPR[rA]);
  sl32 b = static_cast<sl32>(ppuState->ppuThread[ppuState->currentThread].GPR[rB]);

  if ((a < b && BGET(TO, 5, 0)) || (a > b && BGET(TO, 5, 1)) ||
      (a == b && BGET(TO, 5, 2)) || (static_cast<u32>(a) < static_cast<u32>(b) && BGET(TO, 5, 3)) ||
      (static_cast<u32>(a) > static_cast<u32>(b) && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(ppuState, b);
  }
}

void PPCInterpreter::PPCInterpreter_twi(PPU_STATE *ppuState) {
  D_FORM_TO_rA_SI;
  SI = EXTS(SI, 16);

  sl32 a = static_cast<sl32>(ppuState->ppuThread[ppuState->currentThread].GPR[rA]);

  if ((a < static_cast<sl32>(SI) && BGET(TO, 5, 0)) || (a > static_cast<sl32>(SI) && BGET(TO, 5, 1)) ||
      (a == static_cast<sl32>(SI) && BGET(TO, 5, 2)) || (static_cast<u32>(a) < SI && BGET(TO, 5, 3)) ||
      (static_cast<u32>(a) > SI && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(SI));
  }
}

void PPCInterpreter::PPCInterpreter_tdi(PPU_STATE *ppuState) {
  D_FORM_TO_rA_SI;
  SI = EXTS(SI, 16);

  s64 rAReg = static_cast<s64>(ppuState->ppuThread[ppuState->currentThread].GPR[rA]);

  if ((rAReg < static_cast<s64>(SI) && BGET(TO, 5, 0)) ||
      (rAReg > static_cast<s64>(SI) && BGET(TO, 5, 1)) ||
      (rAReg == static_cast<s64>(SI) && BGET(TO, 5, 2)) ||
      (static_cast<u64>(rAReg) < SI && BGET(TO, 5, 3)) ||
      (static_cast<u64>(rAReg) > SI && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(ppuState, static_cast<u32>(SI));
  }
}

void PPCInterpreter::PPCInterpreter_mfspr(PPU_STATE *ppuState) {
  u64 rS, crm = 0;
  PPC_OPC_TEMPL_XFX(ppuState->ppuThread[ppuState->currentThread].CI.opcode, rS, crm);
  u32 sprNum = ppuState->ppuThread[ppuState->currentThread].CI.spr;
  sprNum = ((sprNum & 0x1f) << 5) | ((sprNum >> 5) & 0x1F);

  u64 value = 0;

  switch (sprNum) {
  case SPR_LR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.LR;
    break;
  case SPR_CTR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.CTR;
    break;
  case SPR_DEC:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.DEC;
    break;
  case SPR_CFAR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.CFAR;
    break;
  case SPR_VRSAVE:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.VRSAVE;
    break;
  case SPR_HRMOR:
    value = ppuState->SPR.HRMOR;
    break;
  case SPR_RMOR:
    value = ppuState->SPR.RMOR;
    break;
  case SPR_PIR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.PIR;
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
  case SPR_HID6:
    value = ppuState->SPR.HID6;
    break;
  case SPR_LPCR:
    value = ppuState->SPR.LPCR;
    break;
  case SPR_PpeTlbIndexHint:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.PPE_TLB_Index_Hint;
    break;
  case SPR_HSPRG0:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.HSPRG0;
    break;
  case SPR_HSPRG1:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.HSPRG1;
    break;
  case SPR_TSCR:
    value = ppuState->SPR.TSCR;
    break;
  case SPR_TTR:
    value = ppuState->SPR.TTR;
    break;
  case SPR_PVR:
    value = ppuState->SPR.PVR.PVR_Hex;
    break;
  case SPR_SPRG0:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.SPRG0;
    break;
  case SPR_SPRG1:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.SPRG1;
    break;
  case SPR_SPRG2:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.SPRG2;
    break;
  case SPR_SPRG3:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.SPRG3;
    break;
  case SPR_SRR0:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.SRR0;
    break;
  case SPR_SRR1:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.SRR1;
    break;
  case SPR_XER:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.XER.XER_Hex;
    break;
  case SPR_DSISR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.DSISR;
    break;
  case SPR_DAR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.DAR;
    break;
  case SPR_TB:
    value = ppuState->SPR.TB;
    break;
  case SPR_TBL_RO:
    value = ppuState->SPR.TB;
    break;
  case SPR_TBU_RO:
    value = (ppuState->SPR.TB & 0xFFFFFFFF00000000);
    break;
  case SPR_DABR:
    value = ppuState->ppuThread[ppuState->currentThread].SPR.DABR;
    break;
  case SPR_CTRLRD:
    value = ppuState->SPR.CTRL;
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: 0x{:#x}", ppuState->ppuName, static_cast<u8>(ppuState->currentThread), sprNum);
    break;
  }

  ppuState->ppuThread[ppuState->currentThread].GPR[rS] = value;
}

void PPCInterpreter::PPCInterpreter_mtspr(PPU_STATE *ppuState) {
  XFX_FORM_rD_spr;
  switch (spr) {
  case SPR_DEC:
    ppuState->ppuThread[ppuState->currentThread].SPR.DEC =
        static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  case SPR_SDR1:
    ppuState->SPR.SDR1 = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_DAR:
    ppuState->ppuThread[ppuState->currentThread].SPR.DAR =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_DSISR:
    ppuState->ppuThread[ppuState->currentThread].SPR.DSISR =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_CTR:
    ppuState->ppuThread[ppuState->currentThread].SPR.CTR =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_LR:
    ppuState->ppuThread[ppuState->currentThread].SPR.LR =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_CFAR:
    ppuState->ppuThread[ppuState->currentThread].SPR.CFAR =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_VRSAVE:
    ppuState->ppuThread[ppuState->currentThread].SPR.VRSAVE =
        static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  case SPR_LPCR:
    ppuState->SPR.LPCR = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HID0:
    ppuState->SPR.HID0 = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HID1:
    ppuState->SPR.HID1 = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HID4:
    ppuState->SPR.HID4 = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HID6:
    ppuState->SPR.HID6 = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_SRR0:
    ppuState->ppuThread[ppuState->currentThread].SPR.SRR0 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_SRR1:
    ppuState->ppuThread[ppuState->currentThread].SPR.SRR1 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HRMOR:
    ppuState->SPR.HRMOR = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_PpeTlbIndex:
    ppuState->SPR.PPE_TLB_Index = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_PpeTlbRpn:
    ppuState->SPR.PPE_TLB_RPN = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_PpeTlbVpn:
    ppuState->SPR.PPE_TLB_VPN = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    mmuAddTlbEntry(ppuState);
    break;
  case SPR_TTR:
    ppuState->SPR.TTR = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_TSCR:
    ppuState->SPR.TSCR = static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  case SPR_HSPRG0:
    ppuState->ppuThread[ppuState->currentThread].SPR.HSPRG0 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HSPRG1:
    ppuState->ppuThread[ppuState->currentThread].SPR.HSPRG1 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_CTRLWR:
    ppuState->SPR.CTRL = static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    // Also do the write on SPR_CTRLRD
    break;
  case SPR_RMOR:
    ppuState->SPR.RMOR = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_HDEC:
    ppuState->SPR.HDEC = static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  case SPR_LPIDR:
    ppuState->SPR.LPIDR = static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  case SPR_SPRG0:
    ppuState->ppuThread[ppuState->currentThread].SPR.SPRG0 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_SPRG1:
    ppuState->ppuThread[ppuState->currentThread].SPR.SPRG1 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_SPRG2:
    ppuState->ppuThread[ppuState->currentThread].SPR.SPRG2 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_SPRG3:
    ppuState->ppuThread[ppuState->currentThread].SPR.SPRG3 =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_DABR:
    ppuState->ppuThread[ppuState->currentThread].SPR.DABR =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_DABRX:
    ppuState->ppuThread[ppuState->currentThread].SPR.DABRX =
        ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_XER:
    ppuState->ppuThread[ppuState->currentThread].SPR.XER.XER_Hex =
        static_cast<u32>(ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  case SPR_TBL_WO:
    ppuState->SPR.TB = ppuState->ppuThread[ppuState->currentThread].GPR[rD];
    break;
  case SPR_TBU_WO:
    ppuState->SPR.TB = ppuState->SPR.TB |=
        (ppuState->ppuThread[ppuState->currentThread].GPR[rD] << 32);
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) SPR {:#x} ={:#x}", ppuState->ppuName, static_cast<u8>(ppuState->currentThread),
        spr, ppuState->ppuThread[ppuState->currentThread].GPR[rD]);
    break;
  }
}

void PPCInterpreter::PPCInterpreter_mfmsr(PPU_STATE *ppuState) {
  X_FORM_rD;
  ppuState->ppuThread[ppuState->currentThread].GPR[rD] =
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex;
}

void PPCInterpreter::PPCInterpreter_mtmsr(PPU_STATE *ppuState) {
  X_FORM_rS;

  ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex =
      ppuState->ppuThread[ppuState->currentThread].GPR[rS];
}

void PPCInterpreter::PPCInterpreter_mtmsrd(PPU_STATE *ppuState) {
  X_FORM_rS_L;

  if (L) {
    /*
            Bits 48 and 62 of register RS are placed into the corresponding bits
       of the MSR. The remaining bits of the MSR are unchanged.
    */

    // Bit 48 = MSR[EE]
    if ((ppuState->ppuThread[ppuState->currentThread].GPR[rS] & 0x8000) == 0x8000) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE = 1;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE = 0;
    }
    // Bit 62 = MSR[RI]
    if ((ppuState->ppuThread[ppuState->currentThread].GPR[rS] & 0x2) == 0x2) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.RI = 1;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.RI = 0;
    }
  } else // L = 0
  {
    /*
            The result of ORing bits 0 and 1 of register RS is placed into MSR0.
       The result of ORing bits 48 and 49 of register RS is placed into MSR48.
       The result of ORing bits 58 and 49 of register RS is placed into MSR58.
       The result of ORing bits 59 and 49 of register RS is placed into MSR59.
       Bits 1:2, 4:47, 49:50, 52:57, and 60:63 of register RS are placed into
       the corresponding bits of the MSR.
    */
    u64 regRS = ppuState->ppuThread[ppuState->currentThread].GPR[rS];
    ppuState->ppuThread[ppuState->currentThread].SPR.MSR.MSR_Hex = regRS;

    // MSR0 = (RS)0 | (RS)1
    if ((regRS & 0x8000000000000000) || (regRS & 0x4000000000000000)) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.SF = 1;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.SF = 0;
    }

    // MSR48 = (RS)48 | (RS)49
    if ((regRS & 0x8000) || (regRS & 0x4000)) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE = 1;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.EE = 0;
    }

    // MSR58 = (RS)58 | (RS)49
    if ((regRS & 0x20) || (regRS & 0x4000)) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.IR = 1;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.IR = 0;
    }

    // MSR59 = (RS)59 | (RS)49
    if ((regRS & 0x10) || (regRS & 0x4000)) {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.DR = 1;
    } else {
      ppuState->ppuThread[ppuState->currentThread].SPR.MSR.DR = 0;
    }
  }
}

void PPCInterpreter::PPCInterpreter_sync(PPU_STATE* hCore) {
    // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbf(PPU_STATE* hCore) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbi(PPU_STATE* hCore) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbt(PPU_STATE* hCore) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_dcbtst(PPU_STATE* hCore) {
  // Do nothing.
}
