// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "PPCInterpreter.h"

void PPCInterpreter::PPCInterpreter_isync(PPU_STATE *hCore) {
  // Do nothing.
}

void PPCInterpreter::PPCInterpreter_eieio(PPU_STATE* hCore)
{
    // Do nothing.
}

void PPCInterpreter::PPCInterpreter_sc(PPU_STATE *hCore) {
  SC_FORM_LEV;

  // Raise the exception.
  hCore->ppuThread[hCore->currentThread].exceptReg |= PPU_EX_SC;
  hCore->ppuThread[hCore->currentThread].exceptHVSysCall = LEV & 1;
}

void PPCInterpreter::PPCInterpreter_slbmte(PPU_STATE *hCore) {
  X_FORM_rS_rB;

  u64 VSID = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 0, 51);

  u8 Ks = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 52, 52);
  u8 Kp = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 53, 53);
  u8 N = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 54, 54);
  u8 L = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 55, 55);
  u8 C = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 56, 56);
  u8 LP = QGET(hCore->ppuThread[hCore->currentThread].GPR[rS], 57, 59);

  u64 ESID = QGET(hCore->ppuThread[hCore->currentThread].GPR[rB], 0, 35);
  bool V = QGET(hCore->ppuThread[hCore->currentThread].GPR[rB], 36, 36);
  u16 Index = QGET(hCore->ppuThread[hCore->currentThread].GPR[rB], 52, 63);

  // VSID is VA 0-52 bit, the remaining 28 bits are adress data
  // so whe shift 28 bits left here so we only do it once per entry.
  // This speeds MMU translation since the shift is only done once.
  VSID = VSID << 28;

  hCore->ppuThread[hCore->currentThread].SLB[Index].ESID = ESID;
  hCore->ppuThread[hCore->currentThread].SLB[Index].VSID = VSID;
  hCore->ppuThread[hCore->currentThread].SLB[Index].V = V;
  hCore->ppuThread[hCore->currentThread].SLB[Index].Kp = Kp;
  hCore->ppuThread[hCore->currentThread].SLB[Index].Ks = Ks;
  hCore->ppuThread[hCore->currentThread].SLB[Index].N = N;
  hCore->ppuThread[hCore->currentThread].SLB[Index].L = L;
  hCore->ppuThread[hCore->currentThread].SLB[Index].C = C;
  hCore->ppuThread[hCore->currentThread].SLB[Index].LP = LP;
  hCore->ppuThread[hCore->currentThread].SLB[Index].vsidReg =
      hCore->ppuThread[hCore->currentThread].GPR[rS];
  hCore->ppuThread[hCore->currentThread].SLB[Index].esidReg =
      hCore->ppuThread[hCore->currentThread].GPR[rB];
}

void PPCInterpreter::PPCInterpreter_slbie(PPU_STATE *hCore) {
  X_FORM_rB;

  // ESID.
  const u64 ESID = QGET(hCore->ppuThread[hCore->currentThread].GPR[rB], 0, 35);
  // Class.
  const u8 C = QGET(hCore->ppuThread[hCore->currentThread].GPR[rB], 36, 36);

  for (auto &slbEntry : hCore->ppuThread[hCore->currentThread].SLB) {
    if (slbEntry.V && slbEntry.C == C && slbEntry.ESID == ESID) {
      slbEntry.V = false;
    }
  }
}

void PPCInterpreter::PPCInterpreter_rfid(PPU_STATE *hCore) {
  u64 srr1, new_msr, diff_msr;
  u32 b3, b, usr;

  // Compose new MSR as per specs
  srr1 = hCore->ppuThread[hCore->currentThread].SPR.SRR1;
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

  b3 = BGET(hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex, 64, 3);

  // MSR.51 = (MSR.3 & SRR1.51) | ((~MSR.3) & MSR.51)
  if ((b3 && BGET(srr1, 64, 51)) ||
      (!b3 &&
       BGET(hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex, 64, 51))) {
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
  diff_msr = hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex ^ new_msr;

  // NB: we dont do half-modes
  if (diff_msr & QMASK(58, 59)) {
    if (usr) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.IR = true;
      hCore->ppuThread[hCore->currentThread].SPR.MSR.DR = true;
    } else if (new_msr & QMASK(58, 59)) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.IR = true;
      hCore->ppuThread[hCore->currentThread].SPR.MSR.DR = true;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.IR = false;
      hCore->ppuThread[hCore->currentThread].SPR.MSR.DR = false;
    }
  }

  hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex = new_msr;
  hCore->ppuThread[hCore->currentThread].NIA =
      hCore->ppuThread[hCore->currentThread].SPR.SRR0 & ~3;

  // Clear exception taken flag.
  hCore->ppuThread[hCore->currentThread].exceptionTaken = false;
}

void PPCInterpreter::PPCInterpreter_tw(PPU_STATE *hCore) {
  X_FORM_TO_rA_rB;

  long a = (long)hCore->ppuThread[hCore->currentThread].GPR[rA];
  long b = (long)hCore->ppuThread[hCore->currentThread].GPR[rB];

  if ((a < b && BGET(TO, 5, 0)) || (a > b && BGET(TO, 5, 1)) ||
      (a == b && BGET(TO, 5, 2)) || ((u32)a < (u32)b && BGET(TO, 5, 3)) ||
      ((u32)a > (u32)b && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(hCore, b);
  }
}

void PPCInterpreter::PPCInterpreter_twi(PPU_STATE *hCore) {
  D_FORM_TO_rA_SI;
  SI = EXTS(SI, 16);

  long a = (long)hCore->ppuThread[hCore->currentThread].GPR[rA];

  if ((a < (long)SI && BGET(TO, 5, 0)) || (a > (long)SI && BGET(TO, 5, 1)) ||
      (a == (long)SI && BGET(TO, 5, 2)) || ((u32)a < SI && BGET(TO, 5, 3)) ||
      ((u32)a > SI && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(hCore, (u32)SI);
  }
}

void PPCInterpreter::PPCInterpreter_tdi(PPU_STATE *hCore) {
  D_FORM_TO_rA_SI;
  SI = EXTS(SI, 16);

  s64 rAReg = (s64)hCore->ppuThread[hCore->currentThread].GPR[rA];

  if ((rAReg < (s64)SI && BGET(TO, 5, 0)) ||
      (rAReg > (s64)SI && BGET(TO, 5, 1)) ||
      (rAReg == (s64)SI && BGET(TO, 5, 2)) ||
      ((u64)rAReg < SI && BGET(TO, 5, 3)) ||
      ((u64)rAReg > SI && BGET(TO, 5, 4))) {
    ppcInterpreterTrap(hCore, (u32)SI);
  }
}

void PPCInterpreter::PPCInterpreter_mfspr(PPU_STATE *hCore) {
  u64 rS, crm = 0;
  PPC_OPC_TEMPL_XFX(hCore->ppuThread[hCore->currentThread].CI.opcode, rS, crm);
  u32 sprNum = hCore->ppuThread[hCore->currentThread].CI.spr;
  sprNum = ((sprNum & 0x1f) << 5) | ((sprNum >> 5) & 0x1F);

  u64 value = 0;

  switch (sprNum) {
  case SPR_LR:
    value = hCore->ppuThread[hCore->currentThread].SPR.LR;
    break;
  case SPR_CTR:
    value = hCore->ppuThread[hCore->currentThread].SPR.CTR;
    break;
  case SPR_DEC:
    value = hCore->ppuThread[hCore->currentThread].SPR.DEC;
    break;
  case SPR_CFAR:
    value = hCore->ppuThread[hCore->currentThread].SPR.CFAR;
    break;
  case SPR_VRSAVE:
    value = hCore->ppuThread[hCore->currentThread].SPR.VRSAVE;
    break;
  case SPR_HRMOR:
    value = hCore->SPR.HRMOR;
    break;
  case SPR_RMOR:
    value = hCore->SPR.RMOR;
    break;
  case SPR_PIR:
    value = hCore->ppuThread[hCore->currentThread].SPR.PIR;
    break;
  case SPR_HID0:
    value = hCore->SPR.HID0;
    break;
  case SPR_HID1:
    value = hCore->SPR.HID1;
    break;
  case SPR_HID4:
    value = hCore->SPR.HID4;
    break;
  case SPR_HID6:
    value = hCore->SPR.HID6;
    break;
  case SPR_LPCR:
    value = hCore->SPR.LPCR;
    break;
  case SPR_PpeTlbIndexHint:
    value = hCore->ppuThread[hCore->currentThread].SPR.PPE_TLB_Index_Hint;
    break;
  case SPR_HSPRG0:
    value = hCore->ppuThread[hCore->currentThread].SPR.HSPRG0;
    break;
  case SPR_HSPRG1:
    value = hCore->ppuThread[hCore->currentThread].SPR.HSPRG1;
    break;
  case SPR_TSCR:
    value = hCore->SPR.TSCR;
    break;
  case SPR_TTR:
    value = hCore->SPR.TTR;
    break;
  case SPR_PVR:
    value = hCore->SPR.PVR.PVR_Hex;
    break;
  case SPR_SPRG0:
    value = hCore->ppuThread[hCore->currentThread].SPR.SPRG0;
    break;
  case SPR_SPRG1:
    value = hCore->ppuThread[hCore->currentThread].SPR.SPRG1;
    break;
  case SPR_SPRG2:
    value = hCore->ppuThread[hCore->currentThread].SPR.SPRG2;
    break;
  case SPR_SPRG3:
    value = hCore->ppuThread[hCore->currentThread].SPR.SPRG3;
    break;
  case SPR_SRR0:
    value = hCore->ppuThread[hCore->currentThread].SPR.SRR0;
    break;
  case SPR_SRR1:
    value = hCore->ppuThread[hCore->currentThread].SPR.SRR1;
    break;
  case SPR_XER:
    value = hCore->ppuThread[hCore->currentThread].SPR.XER.XER_Hex;
    break;
  case SPR_DSISR:
    value = hCore->ppuThread[hCore->currentThread].SPR.DSISR;
    break;
  case SPR_DAR:
    value = hCore->ppuThread[hCore->currentThread].SPR.DAR;
    break;
  case SPR_TB:
    value = hCore->SPR.TB;
    break;
  case SPR_TBL_RO:
    value = hCore->SPR.TB;
    break;
  case SPR_TBU_RO:
    value = (hCore->SPR.TB & 0xFFFFFFFF00000000);
    break;
  case SPR_DABR:
    value = hCore->ppuThread[hCore->currentThread].SPR.DABR;
    break;
  case SPR_CTRLRD:
    value = hCore->SPR.CTRL;
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) mfspr: Unknown SPR: 0x{:#x}", hCore->ppuName, (u8)hCore->currentThread, sprNum);
    break;
  }

  hCore->ppuThread[hCore->currentThread].GPR[rS] = value;
}

void PPCInterpreter::PPCInterpreter_mtspr(PPU_STATE *hCore) {
  XFX_FORM_rD_spr;
  switch (spr) {
  case SPR_DEC:
    hCore->ppuThread[hCore->currentThread].SPR.DEC =
        static_cast<u32>(hCore->ppuThread[hCore->currentThread].GPR[rD]);
    break;
  case SPR_SDR1:
    hCore->SPR.SDR1 = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_DAR:
    hCore->ppuThread[hCore->currentThread].SPR.DAR =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_DSISR:
    hCore->ppuThread[hCore->currentThread].SPR.DSISR =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_CTR:
    hCore->ppuThread[hCore->currentThread].SPR.CTR =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_LR:
    hCore->ppuThread[hCore->currentThread].SPR.LR =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_CFAR:
    hCore->ppuThread[hCore->currentThread].SPR.CFAR =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_VRSAVE:
    hCore->ppuThread[hCore->currentThread].SPR.VRSAVE =
        static_cast<u32>(hCore->ppuThread[hCore->currentThread].GPR[rD]);
    break;
  case SPR_LPCR:
    hCore->SPR.LPCR = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HID0:
    hCore->SPR.HID0 = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HID1:
    hCore->SPR.HID1 = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HID4:
    hCore->SPR.HID4 = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HID6:
    hCore->SPR.HID6 = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_SRR0:
    hCore->ppuThread[hCore->currentThread].SPR.SRR0 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_SRR1:
    hCore->ppuThread[hCore->currentThread].SPR.SRR1 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HRMOR:
    hCore->SPR.HRMOR = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_PpeTlbIndex:
    hCore->SPR.PPE_TLB_Index = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_PpeTlbRpn:
    hCore->SPR.PPE_TLB_RPN = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_PpeTlbVpn:
    hCore->SPR.PPE_TLB_VPN = hCore->ppuThread[hCore->currentThread].GPR[rD];
    mmuAddTlbEntry(hCore);
    break;
  case SPR_TTR:
    hCore->SPR.TTR = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_TSCR:
    hCore->SPR.TSCR = (u32)hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HSPRG0:
    hCore->ppuThread[hCore->currentThread].SPR.HSPRG0 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HSPRG1:
    hCore->ppuThread[hCore->currentThread].SPR.HSPRG1 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_CTRLWR:
    hCore->SPR.CTRL = (u32)hCore->ppuThread[hCore->currentThread].GPR[rD];
    // Also do the write on SPR_CTRLRD
    break;
  case SPR_RMOR:
    hCore->SPR.RMOR = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_HDEC:
    hCore->SPR.HDEC = (u32)hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_LPIDR:
    hCore->SPR.LPIDR = (u32)hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_SPRG0:
    hCore->ppuThread[hCore->currentThread].SPR.SPRG0 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_SPRG1:
    hCore->ppuThread[hCore->currentThread].SPR.SPRG1 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_SPRG2:
    hCore->ppuThread[hCore->currentThread].SPR.SPRG2 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_SPRG3:
    hCore->ppuThread[hCore->currentThread].SPR.SPRG3 =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_DABR:
    hCore->ppuThread[hCore->currentThread].SPR.DABR =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_DABRX:
    hCore->ppuThread[hCore->currentThread].SPR.DABRX =
        hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_XER:
    hCore->ppuThread[hCore->currentThread].SPR.XER.XER_Hex =
        static_cast<u32>(hCore->ppuThread[hCore->currentThread].GPR[rD]);
    break;
  case SPR_TBL_WO:
    hCore->SPR.TB = hCore->ppuThread[hCore->currentThread].GPR[rD];
    break;
  case SPR_TBU_WO:
    hCore->SPR.TB = hCore->SPR.TB |=
        (hCore->ppuThread[hCore->currentThread].GPR[rD] << 32);
    break;
  default:
    LOG_ERROR(Xenon, "{}(Thrd{:#d}) SPR {:#x} ={:#x}", hCore->ppuName, (u8)hCore->currentThread,
        spr, hCore->ppuThread[hCore->currentThread].GPR[rD]);
    break;
  }
}

void PPCInterpreter::PPCInterpreter_mfmsr(PPU_STATE *hCore) {
  X_FORM_rD;
  hCore->ppuThread[hCore->currentThread].GPR[rD] =
      hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex;
}

void PPCInterpreter::PPCInterpreter_mtmsr(PPU_STATE *hCore) {
  X_FORM_rS;

  hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex =
      hCore->ppuThread[hCore->currentThread].GPR[rS];
}

void PPCInterpreter::PPCInterpreter_mtmsrd(PPU_STATE *hCore) {
  X_FORM_rS_L;

  if (L) {
    /*
            Bits 48 and 62 of register RS are placed into the corresponding bits
       of the MSR. The remaining bits of the MSR are unchanged.
    */

    // Bit 48 = MSR[EE]
    if ((hCore->ppuThread[hCore->currentThread].GPR[rS] & 0x8000) == 0x8000) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.EE = 1;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.EE = 0;
    }
    // Bit 62 = MSR[RI]
    if ((hCore->ppuThread[hCore->currentThread].GPR[rS] & 0x2) == 0x2) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.RI = 1;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.RI = 0;
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
    u64 regRS = hCore->ppuThread[hCore->currentThread].GPR[rS];
    hCore->ppuThread[hCore->currentThread].SPR.MSR.MSR_Hex = regRS;

    // MSR0 = (RS)0 | (RS)1
    if ((regRS & 0x8000000000000000) || (regRS & 0x4000000000000000)) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.SF = 1;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.SF = 0;
    }

    // MSR48 = (RS)48 | (RS)49
    if ((regRS & 0x8000) || (regRS & 0x4000)) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.EE = 1;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.EE = 0;
    }

    // MSR58 = (RS)58 | (RS)49
    if ((regRS & 0x20) || (regRS & 0x4000)) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.IR = 1;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.IR = 0;
    }

    // MSR59 = (RS)59 | (RS)49
    if ((regRS & 0x10) || (regRS & 0x4000)) {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.DR = 1;
    } else {
      hCore->ppuThread[hCore->currentThread].SPR.MSR.DR = 0;
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
