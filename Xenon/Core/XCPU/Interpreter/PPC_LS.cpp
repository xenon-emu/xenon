// Copyright 2025 Xenon Emulator Project

#include <assert.h>

#include "PPCInterpreter.h"

//
// Store Byte
//

#define DBG_LOAD(x) // std::cout << x

void PPCInterpreter::PPCInterpreter_dcbst(PPU_STATE *hCore) {
  // Temporarely disable caching.
  return;
}

void PPCInterpreter::PPCInterpreter_dcbz(PPU_STATE *hCore) {
  X_FORM_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  EA = EA & ~(128 - 1); // Cache line size

  // Temporarely diasable caching.
  for (u8 n = 0; n < 128; n += sizeof(u64))
    MMUWrite(intXCPUContext, hCore, 0, EA + n, sizeof(u64));
  return;

  // As far as i can tell, XCPU does all the crypto, scrambling of
  // data on L2 cache, and DCBZ is used for creating cache blocks
  // and also erasing them.
}

void PPCInterpreter::PPCInterpreter_icbi(PPU_STATE* hCore)
{
    // Do nothing.
}

void PPCInterpreter::PPCInterpreter_stb(PPU_STATE *hCore) {
  D_FORM_rS_rA_D;
  D = EXTS(D, 16);
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  MMUWrite8(hCore, EA, (u8)hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

void PPCInterpreter::PPCInterpreter_stbu(PPU_STATE *hCore) {
  D_FORM_rS_rA_D;
  D = EXTS(D, 16);
  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  MMUWrite8(hCore, EA, (u8)hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_stbux(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite8(hCore, EA, (u8)hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_stbx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite8(hCore, EA, (u8)hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

//
// Store Halfword
//

void PPCInterpreter::PPCInterpreter_sth(PPU_STATE *hCore) {
  D_FORM_rS_rA_D;
  D = EXTS(D, 16);
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  MMUWrite16(hCore, EA, (u16)hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

void PPCInterpreter::PPCInterpreter_sthbrx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite16(
      hCore, EA,
      std::byteswap<u16>((u16)hCore->ppuThread[hCore->currentThread].GPR[rS]));
}

void PPCInterpreter::PPCInterpreter_sthu(PPU_STATE *hCore) {
  D_FORM_rS_rA_D;
  D = EXTS(D, 16);
  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  MMUWrite16(hCore, EA, (u16)hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_sthux(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite16(hCore, EA, (u16)hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_sthx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite16(hCore, EA, (u16)hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

//
// Store String Word
//

// Weird string instruction
void PPCInterpreter::PPCInterpreter_stswi(PPU_STATE *hCore) {
  X_FORM_rS_rA_NB_XO;

  u64 EA = 0;
  if (rA != 0) {
    EA = hCore->ppuThread[hCore->currentThread].GPR[rA];
  }

  u32 n = 32;
  if (NB != 0) {
    n = NB;
  }

  u64 r = rS - 1;
  u32 i = 0;
  while (n > 0) {
    if (i == 0) {
      r++;
      r &= 31;
    }
    MMUWrite8(hCore, EA,
              (hCore->ppuThread[hCore->currentThread].GPR[r] >> (24 - i)) &
                  0xFF);

    i += 8;
    if (i == 32)
      i = 0;
    EA++;
    n--;
  }
}

void PPCInterpreter::PPCInterpreter_stmw(PPU_STATE* hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  for (u32 idx = rD; idx < 32; ++idx, EA += 4) {
    MMUWrite32(hCore, EA, static_cast<u32>(hCore->ppuThread[hCore->currentThread].GPR[idx]));
  }
}

//
// Store Word
//

void PPCInterpreter::PPCInterpreter_stw(PPU_STATE *hCore) {
  D_FORM_rS_rA_D;
  D = EXTS(D, 16);
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  MMUWrite32(hCore, EA, (u32)hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

void PPCInterpreter::PPCInterpreter_stwbrx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite32(hCore, EA,
             std::byteswap<u32>(static_cast<u32>(
                 hCore->ppuThread[hCore->currentThread].GPR[rS])));
}

void PPCInterpreter::PPCInterpreter_stwcx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u64 RA = EA;
  u32 CR = 0;

  // If address is not aligned by 4, then we must issue a trap.

  if (hCore->ppuThread[hCore->currentThread].SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  if (hCore->ppuThread[hCore->currentThread].ppuRes->V) {
    // Translate address
    MMUTranslateAddress(&RA, hCore, true);
    if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
        hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
      return;

    intXCPUContext->xenonRes.AcquireLock();
    if (hCore->ppuThread[hCore->currentThread].ppuRes->V) {
      if (hCore->ppuThread[hCore->currentThread].ppuRes->resAddr == RA) {
        // std::cout << " * Res OK, storing data 0x" <<
        // (u32)hCore->ppuThread[hCore->currentThread].GPR[rS] << std::endl;
        bool soc = false;
        u32 data = std::byteswap<u32>(
            (u32)hCore->ppuThread[hCore->currentThread].GPR[rS]);
        RA = mmuContructEndAddressFromSecEngAddr(RA, &soc);
        sysBus->Write(RA, data, 4);
        intXCPUContext->xenonRes.Check(RA);
        BSET(CR, 4, CR_BIT_EQ);
      } else {
        intXCPUContext->xenonRes.Decrement();
        hCore->ppuThread[hCore->currentThread].ppuRes->V = false;
      }
    }
    intXCPUContext->xenonRes.ReleaseLock();
  }

  ppcUpdateCR(hCore, 0, CR);
}

void PPCInterpreter::PPCInterpreter_stwu(PPU_STATE *hCore) {
  D_FORM_rS_rA_D;
  D = EXTS(D, 16);
  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  MMUWrite32(hCore, EA, (u32)hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_stwux(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite32(hCore, EA, (u32)hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_stwx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite32(hCore, EA, (u32)hCore->ppuThread[hCore->currentThread].GPR[rD]);
}

//
// Store Doubleword
//

void PPCInterpreter::PPCInterpreter_std(PPU_STATE *hCore) {
  DS_FORM_rS_rA_DS;
  DS = EXTS(DS, 14) << 2;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + DS;
  MMUWrite64(hCore, EA, hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

void PPCInterpreter::PPCInterpreter_stdcx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u64 RA = EA;
  u32 CR = 0;

  if (hCore->ppuThread[hCore->currentThread].SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  // If address is not aligned by 4, the we must issue a trap.
  if (hCore->ppuThread[hCore->currentThread].ppuRes->V) {
    MMUTranslateAddress(&RA, hCore, true);
    if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
        hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
      return;

    intXCPUContext->xenonRes.AcquireLock();
    if (hCore->ppuThread[hCore->currentThread].ppuRes->V) {
      if (hCore->ppuThread[hCore->currentThread].ppuRes->resAddr == (RA & ~7)) {
        u64 data =
            std::byteswap<u64>(hCore->ppuThread[hCore->currentThread].GPR[rS]);
        bool soc = false;
        RA = mmuContructEndAddressFromSecEngAddr(RA, &soc);
        sysBus->Write(RA, data, 8);
        BSET(CR, 4, CR_BIT_EQ);
      } else {
        intXCPUContext->xenonRes.Decrement();
        hCore->ppuThread[hCore->currentThread].ppuRes->V = false;
      }
    }
    intXCPUContext->xenonRes.ReleaseLock();
  }

  ppcUpdateCR(hCore, 0, CR);
}

void PPCInterpreter::PPCInterpreter_stdu(PPU_STATE *hCore) {
  DS_FORM_rD_rA_DS;
  DS = EXTS(DS, 14) << 2;
  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + DS;
  MMUWrite64(hCore, EA, hCore->ppuThread[hCore->currentThread].GPR[rD]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_stdux(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite64(hCore, EA, hCore->ppuThread[hCore->currentThread].GPR[rS]);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_stdx(PPU_STATE *hCore) {
  X_FORM_rS_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  MMUWrite64(hCore, EA, hCore->ppuThread[hCore->currentThread].GPR[rS]);
}

//
// Store Floating
//
void PPCInterpreter::PPCInterpreter_stfd(PPU_STATE *hCore) {
  D_FORM_FrS_rA_D;

  D = EXTS(D, 16);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;

  MMUWrite64(hCore, EA,
             hCore->ppuThread[hCore->currentThread].FPR[FrS].valueAsU64);
}

//
// Load Byte
//

void PPCInterpreter::PPCInterpreter_lbz(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  u8 data = MMURead8(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lbz: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_lbzu(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  u8 data = MMURead8(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lbzu: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lbzux(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u8 data = MMURead8(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lbzux: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lbzx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u8 data = MMURead8(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lbzx: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

//
// Load Halfword
//

void PPCInterpreter::PPCInterpreter_lha(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  u16 unsignedWord = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lha: Addr 0x" << EA << " data = 0x" << unsignedWord << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = EXTS(unsignedWord, 16);
}

void PPCInterpreter::PPCInterpreter_lhau(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  u16 unsignedWord = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lhau: Addr 0x" << EA << " data = 0x" << unsignedWord << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = EXTS(unsignedWord, 16);
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lhax(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;
  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u16 unsignedWord = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lhax: Addr 0x" << EA << " data = 0x" << unsignedWord << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = EXTS(unsignedWord, 16);
}

void PPCInterpreter::PPCInterpreter_lhbrx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];

  u16 data = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lhbrx: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = std::byteswap<u16>(data);
}

void PPCInterpreter::PPCInterpreter_lhz(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  u16 data = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_lhzu(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  u16 data = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lhzu: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lhzux(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u16 data = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lhzux: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lhzx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u16 data = MMURead16(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;
  DBG_LOAD("lhzx: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

//
// String Word
//

// Werid intruction
void PPCInterpreter::PPCInterpreter_lswi(PPU_STATE *hCore) {
  X_FORM_rD_rA_NB_XO;

  u64 EA = 0;
  if (rA != 0) {
    EA = hCore->ppuThread[hCore->currentThread].GPR[rA];
  }

  u32 n = 32;
  if (NB != 0) {
    n = NB;
  }

  u64 r = rD - 1;
  u32 i = 0;

  while (n > 0) {
    if (i == 0) {
      r++;
      r &= 31;
      hCore->ppuThread[hCore->currentThread].GPR[r] = 0;
    }

    const u32 temp_value = MMURead8(hCore, EA) << (24 - i);

    hCore->ppuThread[hCore->currentThread].GPR[r] |= temp_value;

    i += 8;
    if (i == 32)
      i = 0;
    EA++;
    n--;
  }
}

void PPCInterpreter::PPCInterpreter_lmw(PPU_STATE* hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  for (u32 idx = rD; idx < 32; ++idx, EA += 4) {
    hCore->ppuThread[hCore->currentThread].GPR[idx] =
        MMURead32(hCore, EA);
  }
}

//
// Load Word
//

void PPCInterpreter::PPCInterpreter_lwa(PPU_STATE *hCore) {
  DS_FORM_rD_rA_DS;
  DS = EXTS(DS, 14) << 2;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + DS;
  u32 unsignedWord = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lwa: Addr 0x" << EA << " data = 0x" << unsignedWord << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = EXTS(unsignedWord, 32);
}

void PPCInterpreter::PPCInterpreter_lwax(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u32 unsignedWord = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lwax: Addr 0x" << EA << " data = 0x" << unsignedWord << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = EXTS(unsignedWord, 32);
}

void PPCInterpreter::PPCInterpreter_lwarx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];

  // If address is not aligned by 4, the we must issue a trap.

  u64 RA = EA;
  MMUTranslateAddress(&RA, hCore, false);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].ppuRes->V = true;
  hCore->ppuThread[hCore->currentThread].ppuRes->resAddr = RA;
  intXCPUContext->xenonRes.Increment();
  u32 data = MMURead32(hCore, EA);

  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lwarx: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_lwbrx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u32 data = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rD] = std::byteswap<u32>(data);
}

void PPCInterpreter::PPCInterpreter_lwz(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;
  u32 data = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_lwzu(PPU_STATE *hCore) {
  D_FORM_rD_rA_D;
  D = EXTS(D, 16);

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + D;
  u32 data = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lwzu: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lwzux(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] +
           hCore->ppuThread[hCore->currentThread].GPR[rB];

  u32 data = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("lwzux: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_lwzx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u32 data = MMURead32(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;
  DBG_LOAD("lwzx: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

//
// Load Doubleword
//

void PPCInterpreter::PPCInterpreter_ld(PPU_STATE *hCore) {
  DS_FORM_rD_rA_DS;
  DS = EXTS(DS, 14) << 2;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + DS;
  u64 data = MMURead64(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("ld: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_ldbrx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
    hCore->ppuThread[hCore->currentThread].GPR[rB];

  u64 RA = EA & ~7;

  u64 data = MMURead64(hCore, EA);

  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
    hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("ldbrx: Addr 0x" << std::hex << EA << " data = 0x" << std::hex << (int)data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_ldarx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];

  // if not aligned by 8 trap

  u64 RA = EA & ~7;
  MMUTranslateAddress(&RA, hCore, false);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  hCore->ppuThread[hCore->currentThread].ppuRes->resAddr = RA;
  hCore->ppuThread[hCore->currentThread].ppuRes->V = true;
  intXCPUContext->xenonRes.Increment();

  u64 data = MMURead64(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;
  DBG_LOAD("ldarx:Addr 0x" << std::hex << EA << " data = 0x" << std::hex << (int)data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_ldu(PPU_STATE *hCore) {
  DS_FORM_rD_rA_DS;
  DS = EXTS(DS, 14) << 2;

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] + DS;
  u64 data = MMURead64(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("ldu: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_ldux(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = hCore->ppuThread[hCore->currentThread].GPR[rA] +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u64 data = MMURead64(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("ldux: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
  hCore->ppuThread[hCore->currentThread].GPR[rA] = EA;
}

void PPCInterpreter::PPCInterpreter_ldx(PPU_STATE *hCore) {
  X_FORM_rD_rA_rB;

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) +
           hCore->ppuThread[hCore->currentThread].GPR[rB];
  u64 data = MMURead64(hCore, EA);
  if (hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASEGM ||
      hCore->ppuThread[hCore->currentThread].exceptReg & PPU_EX_DATASTOR)
    return;

  DBG_LOAD("ldx: Addr 0x" << EA << " data = 0x" << data << std::endl;)
  hCore->ppuThread[hCore->currentThread].GPR[rD] = data;
}

void PPCInterpreter::PPCInterpreter_lfd(PPU_STATE *hCore) {

  D_FORM_FrD_rA_D;
  D = EXTS(D, 16);

  // Check if Floating Point is available.
  assert(hCore->ppuThread[hCore->currentThread].SPR.MSR.FP == 1);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;

  hCore->ppuThread[hCore->currentThread].FPR[FrD].valueAsDouble =
      static_cast<double>(MMURead64(hCore, EA));
}

void PPCInterpreter::PPCInterpreter_lfs(PPU_STATE *hCore) {
  D_FORM_FrD_rA_D;
  D = EXTS(D, 16);

  // Check if Floating Point is available.
  assert(hCore->ppuThread[hCore->currentThread].SPR.MSR.FP == 1);

  u64 EA = (rA ? hCore->ppuThread[hCore->currentThread].GPR[rA] : 0) + D;

  SFPRegister singlePresFP;
  singlePresFP.valueAsU32 = MMURead32(hCore, EA);
  hCore->ppuThread[hCore->currentThread].FPR[FrD].valueAsDouble =
      (double)singlePresFP.valueAsFloat;
}
