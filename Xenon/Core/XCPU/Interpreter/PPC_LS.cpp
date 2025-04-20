// Copyright 2025 Xenon Emulator Project

#include <bit>

#include "Base/Assert.h"
#include "PPCInterpreter.h"

//
// Store Byte
//

void PPCInterpreter::PPCInterpreter_dcbst(PPU_STATE *ppuState) {
  // Temporarily disable caching
  return;
}

void PPCInterpreter::PPCInterpreter_dcbz(PPU_STATE *ppuState) {
  u64 EA = (_instr.ra ? GPRi(ra) : 0) + GPRi(rb);
  EA = EA & ~(128 - 1); // Cache line size

  // Temporarily disable caching
  for (u8 n = 0; n < 128; n += sizeof(u64))
    MMUWrite64(ppuState, EA + n, 0);
  return;

  // As far as I can tell, XCPU does all the crypto, scrambling of
  // data on L2 cache, and DCBZ is used for creating cache blocks
  // and also erasing them
}

void PPCInterpreter::PPCInterpreter_icbi(PPU_STATE *ppuState) {
  // Do nothing
}

// Store Byte (x'9800 0000')
void PPCInterpreter::PPCInterpreter_stb(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 1) <- rS[56-63]
  */

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite8(ppuState, EA, static_cast<u8>(GPRi(rs)));
}

// Store Byte with Update (x'9C00 0000')
void PPCInterpreter::PPCInterpreter_stbu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 1) <- rS[56-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite8(ppuState, EA, static_cast<u8>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Byte with Update Indexed (x'7C00 01EE')
void PPCInterpreter::PPCInterpreter_stbux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 1) <- rS[56-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite8(ppuState, EA, static_cast<u8>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Byte Indexed (x'7C00 01AE')
void PPCInterpreter::PPCInterpreter_stbx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 1) <- rS[56-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite8(ppuState, EA, static_cast<u8>(GPRi(rs)));
}

//
// Store Halfword
//

// Store Half Word (x'B000 0000')
void PPCInterpreter::PPCInterpreter_sth(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 2) <- rS[48-6316-31]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite16(ppuState, EA, static_cast<u16>(GPRi(rs)));
}

// Store Half Word Byte-Reverse Indexed (x'7C00 072C')
void PPCInterpreter::PPCInterpreter_sthbrx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 2) <- rS[56-63] || rS[48-55]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite16(ppuState, EA, byteswap_be<u16>(static_cast<u16>(GPRi(rs))));
}

// Store Half Word with Update (x'B400 0000')
void PPCInterpreter::PPCInterpreter_sthu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 2) <- rS[48-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite16(ppuState, EA, static_cast<u16>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Half Word with Update Indexed (x'7C00 036E')
void PPCInterpreter::PPCInterpreter_sthux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 2) <- rS[48-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite16(ppuState, EA, static_cast<u16>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Half Word Indexed (x'7C00 032E')
void PPCInterpreter::PPCInterpreter_sthx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 2) <- rS[48-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite16(ppuState, EA, static_cast<u16>(GPRi(rs)));
}

// Store Multiple Word (x'BC00 0000')
void PPCInterpreter::PPCInterpreter_stmw(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  r <- rS
  do while r < 31
    MEM(EA, 4) <- GPRi(r)[32-63]
    r <- r + 1
    EA <- EA + 4
  */

  u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  for (u32 idx = _instr.rs; idx < 32; ++idx, EA += 4) {
    MMUWrite32(ppuState, EA, static_cast<u32>(GPR(idx)));
  }
}

// Store String Word Immediate (x'7C00 05AA')
void PPCInterpreter::PPCInterpreter_stswi(PPU_STATE *ppuState) {
  /*
  if rA = 0 then EA <- 0
  else EA <- (rA)
  if NB = 0 then n <- 32
  else n <- NB
  r <- rS - 1
  i <- 32
  do while n > 0
    if i = 32 then r <- r + 1 (mod 32)
    MEM(EA, 1) <- GPRi(r)[i-i + 7]
    i <- i + 8
    if i = 64 then i <- 32
    EA <- EA + 1
    n <- n - 1
  */

  u64 EA = _instr.ra ? GPRi(ra) : 0;
  u64 N = _instr.rb ? _instr.rb : 32;
  u8 reg = _instr.rd;

  while (N > 0) {
    if (N > 3) {
      MMUWrite32(ppuState, EA, static_cast<u32>(GPR(reg)));
      EA += 4;
      N -= 4;
    } else {
      u32 buf = static_cast<u32>(GPR(reg));
      while (N > 0) {
        N = N - 1;
        MMUWrite8(ppuState, EA, (0xFF000000 & buf) >> 24);
        buf <<= 8;
        EA++;
      }
    }
    reg = (reg + 1) % 32;
  }
}

//
// Store Word
//

// Store Word (x'9000 0000')
void PPCInterpreter::PPCInterpreter_stw(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 4) <- rS[32-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite32(ppuState, EA, static_cast<u32>(GPRi(rs)));
}

// Store Word Byte-Reverse Indexed (x'7C00 052C')
void PPCInterpreter::PPCInterpreter_stwbrx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 4) <- rS[56-63] || rS[48-55] || rS[40-47] || rS[32-39]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppuState, EA, byteswap_be<u32>(static_cast<u32>(GPRi(rs))));
}

// Store Word Conditional Indexed (x'7C00 012D')
void PPCInterpreter::PPCInterpreter_stwcx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  if RESERVE then
    if RESERVE_ADDR = physical_addr(EA)
      MEM(EA, 4) <- rS[32-63]
      CR0 <- 0b00 || 0b1 || XER[SO]
    else
      u <- undefined 1-bit value
      if u then MEM(EA, 4) <- rS[32-63]
      CR0 <- 0b00 || u || XER[SO]
    RESERVE <- 0
  else
    CR0 <- 0b00 || 0b0 || XER[SO]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 RA = EA;
  u32 CR = 0;

  // If address is not aligned by 4, then we must issue a trap

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  if (curThread.ppuRes->V) {
    // Translate address
    MMUTranslateAddress(&RA, ppuState, true);
    if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
      return;

    intXCPUContext->xenonRes.AcquireLock();
    if (curThread.ppuRes->V) {
      if (curThread.ppuRes->resAddr == RA) {
        bool soc = false;
        u32 data = byteswap_be<u32>(static_cast<u32>(GPRi(rs)));
        RA = mmuContructEndAddressFromSecEngAddr(RA, &soc);
        sysBus->Write(RA, (u8*)&data, 4);
        intXCPUContext->xenonRes.Check(RA);
        BSET(CR, 4, CR_BIT_EQ);
      } else {
        intXCPUContext->xenonRes.Decrement();
        curThread.ppuRes->V = false;
      }
    }
    intXCPUContext->xenonRes.ReleaseLock();
  }

  ppcUpdateCR(ppuState, 0, CR);
}

// Store Word with Update (x'9400 0000')
void PPCInterpreter::PPCInterpreter_stwu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 4) <- rS[32-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite32(ppuState, EA, static_cast<u32>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Word with Update Indexed (x'7C00 016E')
void PPCInterpreter::PPCInterpreter_stwux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 4) <- rS[32-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite32(ppuState, EA, static_cast<u32>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Word Indexed (x'7C00 012E')
void PPCInterpreter::PPCInterpreter_stwx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 4) <- rS[32-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppuState, EA, static_cast<u32>(GPRi(rs)));
}

//
// Store Doubleword
//

// Store Double Word (x'F800 0000')
void PPCInterpreter::PPCInterpreter_std(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(ds || 0b00)
  (MEM(EA, 8)) <- (rS)
  */
  const u64 EA = (_instr.simm16 & ~3) + (_instr.ra ? GPRi(ra) : 0);
  MMUWrite64(ppuState, EA, GPRi(rs));
}

// Store Double Word Conditional Indexed (x'7C00 01AD')
void PPCInterpreter::PPCInterpreter_stdcx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  if RESERVE then
    if RESERVE_ADDR = physical_addr(EA)
    MEM(EA, 8) <- (rS)
    CR0 <- 0b00 || 0b1 || XER[SO]
  else
    u <- undefined 1-bit value
    if u then MEM(EA, 8) <- (rS)
    CR0 <- 0b00 || u || XER[SO]
  RESERVE <- 0
  else
    CR0 <- 0b00 || 0b0 || XER[SO]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 RA = EA;
  u32 CR = 0;

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  // If address is not aligned by 4, the we must issue a trap
  if (curThread.ppuRes->V) {
    MMUTranslateAddress(&RA, ppuState, true);

    if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
      return;

    intXCPUContext->xenonRes.AcquireLock();
    if (curThread.ppuRes->V) {
      if (curThread.ppuRes->resAddr == (RA & ~7)) {
        u64 data =
            byteswap_be<u64>(GPRi(rs));
        bool soc = false;
        RA = mmuContructEndAddressFromSecEngAddr(RA, &soc);
        sysBus->Write(RA, (u8*)&data, 8);
        BSET(CR, 4, CR_BIT_EQ);
      } else {
        intXCPUContext->xenonRes.Decrement();
        curThread.ppuRes->V = false;
      }
    }
    intXCPUContext->xenonRes.ReleaseLock();
  }

  ppcUpdateCR(ppuState, 0, CR);
}

// Store Double Word with Update (x'F800 0001')
void PPCInterpreter::PPCInterpreter_stdu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(ds || 0b00)
  (MEM(EA, 8)) <- (rS)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + (_instr.simm16 & ~3);
  MMUWrite64(ppuState, EA, GPRi(rs));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Double Word with Update Indexed (x'7C00 016A')
void PPCInterpreter::PPCInterpreter_stdux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 8) <- (rS)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite64(ppuState, EA, GPRi(rs));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Double Word Indexed (x'7C00 012A')
void PPCInterpreter::PPCInterpreter_stdx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  (MEM(EA, 8)) <- (rS)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite64(ppuState, EA, GPRi(rs));
}

//
// Store Floating
//

// Store Floating-Point Double (x'D800 0000')
void PPCInterpreter::PPCInterpreter_stfd(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 8) <- (frS)
  */
  const u64 EA = _instr.ra || 1 ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite64(ppuState, EA, FPRi(frs).valueAsU64);
}

//
// Load Byte
//

// Load Byte and Zero (x'8800 0000')
void PPCInterpreter::PPCInterpreter_lbz(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  rD <- (56)0 || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  u8 data = MMURead8(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Byte and Zero with Update (x'8C00 0000')
void PPCInterpreter::PPCInterpreter_lbzu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(d)
  rD <- (56)0 || MEM(EA, 1)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  u8 data = MMURead8(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Byte and Zero with Update Indexed (x'7C00 00EE')
void PPCInterpreter::PPCInterpreter_lbzux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  rD <- (56)0 || MEM(EA, 1)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  u8 data = MMURead8(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Byte and Zero Indexed (x'7C00 00AE')
void PPCInterpreter::PPCInterpreter_lbzx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (56)0 || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u8 data = MMURead8(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

//
// Load Halfword
//

// Load Half Word Algebraic (x'A800 0000')
void PPCInterpreter::PPCInterpreter_lha(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  rD <- EXTS(MEM(EA, 2))
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  u16 unsignedWord = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
}

// Load Half Word Algebraic with Update (x'AC00 0000')
void PPCInterpreter::PPCInterpreter_lhau(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(d)
  rD <- EXTS(MEM(EA, 2))
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  u16 unsignedWord = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
  GPRi(ra) = EA;
}

// Load Half Word Algebraic Indexed (x'7C00 02AE')
void PPCInterpreter::PPCInterpreter_lhax(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- EXTS(MEM(EA, 2))
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u16 unsignedWord = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
}

// Load Half Word Byte-Reverse Indexed (x'7C00 062C')
void PPCInterpreter::PPCInterpreter_lhbrx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (48)0 || MEM(EA + 1, 1) || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u16 data = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = byteswap_be<u16>(data);
}

// Load Half Word and Zero (x'A000 0000')
void PPCInterpreter::PPCInterpreter_lhz(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b <- EXTS(d)
  rD <- (48)0 || MEM(EA, 2)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  u16 data = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Half Word and Zero with Update (x'A400 0000')
void PPCInterpreter::PPCInterpreter_lhzu(PPU_STATE *ppuState) {
  /*
  EA <- rA + EXTS(d)
  rD <- (48)0 || MEM(EA, 2)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  u16 data = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Half Word and Zero with Update Indexed (x'7C00 026E')
void PPCInterpreter::PPCInterpreter_lhzux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  rD <- (48)0 || MEM(EA, 2)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  u16 data = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Half Word and Zero Indexed (x'7C00 022E')
void PPCInterpreter::PPCInterpreter_lhzx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (48)0 || MEM(EA, 2)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u16 data = MMURead16(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

//
// String Word
//

// Load Multiple Word (x'B800 0000')
void PPCInterpreter::PPCInterpreter_lmw(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  r <- rD
  do while r < 31
    GPRi(r) <- (32)0 || MEM(EA, 4)
    r <- r + 1
    EA <- EA + 4
  */
  u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  for (u32 idx = _instr.rd; idx < 32; ++idx, EA += 4) {
    GPR(idx) = MMURead32(ppuState, EA);
  }
}

// Load String Word Immediate (x'7C00 04AA')
void PPCInterpreter::PPCInterpreter_lswi(PPU_STATE *ppuState) {
  /*
  if rA = 0 then EA <- 0
  else EA <- (rA)
  if NB = 0 then n <- 32
  else n <- NB
  r <- rD - 1
  i <- 320
  do while n > 0
    if i = 32 then
      r <- r + 1 (mod 32)
      GPRi(r) <- 0
    GPRi(r)[i-i + 7] <- MEM(EA, 1)
    i <- i + 8
    if i = 6432 then i <- 320
    EA <- EA + 1
    n <- n - 1
  */
  u64 EA = _instr.ra ? GPRi(ra) : 0;
  u64 N = _instr.rb ? _instr.rb : 32;
  u8 reg = _instr.rd;

  while (N > 0) {
    if (N > 3) {
      GPR(reg) = MMURead32(ppuState, EA);
      EA += 4;
      N -= 4;
    } else {
      u32 buf = 0;
      u32 i = 3;
      while (N > 0) {
        N = N - 1;
        buf |= MMURead8(ppuState, EA) << (i * 8);
        EA++;
        i--;
      }
      GPR(reg) = buf;
    }
    reg = (reg + 1) % 32;
  }
}

//
// Load Word
//

// Load Word Algebraic (x'E800 0002')
void PPCInterpreter::PPCInterpreter_lwa(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(ds || 0b00)
  rD <- EXTS(MEM(EA, 4))
  */
  const u64 EA = (_instr.simm16 & ~3) + (_instr.ra ? GPRi(ra) : 0);
  u32 unsignedWord = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 32);
}

// Load Word and Reserve Indexed (x'7C00 0028')
void PPCInterpreter::PPCInterpreter_lwarx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  RESERVE <- 1
  RESERVE_ADDR <- physical_addr(EA)
  rD <- (32)0 || MEM(EA,4)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);

  // TODO: If address is not aligned by 4, the we must issue a trap
  u64 RA = EA;
  MMUTranslateAddress(&RA, ppuState, false);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  curThread.ppuRes->V = true;
  curThread.ppuRes->resAddr = RA;
  intXCPUContext->xenonRes.Increment();
  u32 data = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

    GPRi(rd) = data;
}

// Load Word Algebraic Indexed (x'7C00 02AA')
void PPCInterpreter::PPCInterpreter_lwax(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- EXTS(MEM(EA, 4))
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u32 unsignedWord = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 32);
}

// Load Word Byte-Reverse Indexed (x'7C00 042C')
void PPCInterpreter::PPCInterpreter_lwbrx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (32)0 || MEM(EA + 3, 1) || MEM(EA + 2, 1) || MEM(EA + 1, 1) || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u32 data = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = byteswap_be<u32>(data);
}

// Load Word and Zero (x'8000 0000')
void PPCInterpreter::PPCInterpreter_lwz(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  rD <- (32)0 || MEM(EA, 4)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  u32 data = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Word and Zero with Update (x'8400 0000')
void PPCInterpreter::PPCInterpreter_lwzu(PPU_STATE *ppuState) {
  /*
  EA <- rA + EXTS(d)
  rD <- (32)0 || MEM(EA, 4)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  u32 data = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Word and Zero with Update Indexed (x'7C00 006E')
void PPCInterpreter::PPCInterpreter_lwzux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  rD <- (32)0 || MEM(EA, 4)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  u32 data = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Word and Zero Indexed (x'7C00 002E')
void PPCInterpreter::PPCInterpreter_lwzx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + rB
  rD <- (32)0 || MEM(EA, 4)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u32 data = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

//
// Load Doubleword
//

// Load Double Word (x'E800 0000')
void PPCInterpreter::PPCInterpreter_ld(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(ds || 0b00)
  rD <- MEM(EA, 8)
  */
  const u64 EA = (_instr.simm16 & ~3) + (_instr.ra ? GPRi(ra) : 0);
  u64 data = MMURead64(ppuState, EA);
  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Double Word Byte Reversed Indexed
void PPCInterpreter::PPCInterpreter_ldbrx(PPU_STATE *ppuState) {
  // TODO(bitsh1ft3r): Add instruction def and check for that ~7 to be correct
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 RA = EA & ~7;
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Double Word and Reserve Indexed (x'7C00 00A8')
void PPCInterpreter::PPCInterpreter_ldarx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  RESERVE <- 1
  RESERVE_ADDR <- physical_addr(EA)
  rD <- MEM(EA, 8)
  */

  // TODO: if the address is not aligned by 8 issue a trap
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 RA = EA & ~7;
  MMUTranslateAddress(&RA, ppuState, false);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  curThread.ppuRes->resAddr = RA;
  curThread.ppuRes->V = true;
  intXCPUContext->xenonRes.Increment();

  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Double Word with Update (x'E800 0001')
void PPCInterpreter::PPCInterpreter_ldu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(ds || 0b00)
  rD <- MEM(EA, 8)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + (_instr.simm16 & ~3);
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Double Word with Update Indexed (x'7C00 006A')
void PPCInterpreter::PPCInterpreter_ldux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  rD <- MEM(EA, 8)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Double Word Indexed (x'7C00 002A')
void PPCInterpreter::PPCInterpreter_ldx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- MEM(EA, 8)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Floating-Point Double (x'C800 0000')
void PPCInterpreter::PPCInterpreter_lfd(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  frD <- MEM(EA, 8)
  */
  // Check if floating point is enabled
  ASSERT(curThread.SPR.MSR.FP == 1);

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).valueAsDouble = static_cast<double>(data);
}

// Load Floating-Point Double-Indexed (x'C800 0000')
void PPCInterpreter::PPCInterpreter_lfdx(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  frD <- MEM(EA, 8)
  */
  // Check if floating point is enabled
  ASSERT(curThread.SPR.MSR.FP == 1);

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).valueAsDouble = static_cast<double>(data);
}

// Load Floating-Point Double with Update
void PPCInterpreter::PPCInterpreter_lfdu(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + EXTS(d)
  frD <- MEM(EA, 8)
  rA <- EA
  */
  // Check if floating point is enabled
  ASSERT(curThread.SPR.MSR.FP == 1);

  const u64 EA = GPRi(ra) + _instr.simm16;
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).valueAsDouble = static_cast<double>(data);
  GPRi(ra) = EA;
}

// Load Floating-Point Double with Update Indexed
void PPCInterpreter::PPCInterpreter_lfdux(PPU_STATE *ppuState) {
  /*
  EA <- (rA) + (rB)
  frD <- MEM(EA, 8)
  rA <- EA
  */
  // Check if floating point is enabled
  ASSERT(curThread.SPR.MSR.FP == 1);

  const u64 EA = GPRi(ra) + GPRi(rb);
  u64 data = MMURead64(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).valueAsDouble = static_cast<double>(data);
  GPRi(ra) = EA;
}

// Load Floating-Point Single (x'C000 0000')
void PPCInterpreter::PPCInterpreter_lfs(PPU_STATE *ppuState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  frD <- DOUBLE(MEM(EA, 4))
  */
  // Check if Floating Point is available
  ASSERT(curThread.SPR.MSR.FP == 1);

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  SFPRegister singlePresFP;
  singlePresFP.valueAsU32 = MMURead32(ppuState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).valueAsDouble = static_cast<double>(singlePresFP.valueAsFloat);
}
