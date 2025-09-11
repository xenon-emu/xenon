// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include <bit>

#include "Base/Assert.h"
#include "PPCInterpreter.h"

using namespace Base;

//#define VXU_LOAD_DEBUG

//
// Utilities
//

// Single-precision load floating-point instructions convert single-precision data to double-precision format
// prior to loading the operands into the target FPR.
// For double-precision floating-point load instructions, no conversion is required as the data from memory is 
// copied directly into the FPRs.
// This means that we need some conversion algorithm, luckily this is already described in PPC_PEM.pdf, chapter 
// C.6 Floating - Point Load Instructions
inline u64 ConvertToDouble(u32 inValue) {
  u64 word = inValue;
  u64 exp = (word >> 23) & 0xff; // As denoted by the PPC_PEM manual.
  u64 frac = word & 0x007fffff;

  if (exp > 0 && exp < 255) {
    /*
    If WORD[1-8] > 0 and WORD[1-8] < 255 then normalized operand
      frD[0-1] <- WORD[0-1]
      frD[2] <- ~ WORD[1]
      frD[3] <- ~ WORD[1]
      frD[4] <- ~ WORD[1]
      frD[5-63] <- WORD[2-31] || (29)0
    */
    u64 x = !(exp >> 7);
    u64 z = x << 61 | x << 60 | x << 59;
    return ((word & 0xc0000000) << 32) | z | ((word & 0x3fffffff) << 29);
  } else if (exp == 0 && frac != 0) {
    /*
    If WORD[1-8] = 0 and WORD[9-31] - 0 then denormalized operand
      sign <- WORD[0]
      exp <- -126
      frac[0-52] <- 0b0 || WORD[9-31] || (29)0
      normalize the operand
      Do while frac[0] = 0
      frac <- frac[1-52] || 0b0
      exp <- exp - 1
      End
      frD[0] <- sign
      frD[1-11] <- exp + 1023
      frD[12-63] <- frac[1-52]
    */
    exp = 1023 - 126;
    do
    {
      frac <<= 1;
      exp -= 1;
    } while ((frac & 0x00800000) == 0);

    return ((word & 0x80000000) << 32) | (exp << 52) | ((frac & 0x007fffff) << 29);
  } else {
    /*
    If WORD[1-8] = 255 or WORD[1-31] = 0 then Infinity / QNaN / SNaN / Zero operand
      frD[0-1] <- WORD[0-1]
      frD[2] <- WORD[1]
      frD[3] <- WORD[1]
      frD[4] <- WORD[1]
      frD[5-63] <- WORD[2-31] || (29)0
    */
    u64 y = exp >> 7;
    u64 z = y << 61 | y << 60 | y << 59;
    return ((word & 0xc0000000) << 32) | z | ((word & 0x3fffffff) << 29);
  }
}

// There are three basic forms of store instruction, single - precision, double - precision, and integer. The integer
// form is provided by the stfiwx instruction.Because the FPRs support only floating - point double format for
// floating - point data, single - precision store floating - point instructions convert double - precision data to
// single precision format prior to storing the operands into memory.
inline u32 ConvertToSingle(u64 inValue) {
  const u32 exp = u32((inValue >> 52) & 0x7ff);

  if (exp > 896 || (inValue & ~0x8000000000000000ULL) == 0) {
    /*
    No Denormalization Required (includes Zero/Infinity/NaN):
    if frS[1-11] > 896 or frS[1-63] = 0 then
      WORD[0-1] <- frS[0-1]
      WORD[2-31] <- frS[5-34]
    */
    return static_cast<u32>(((inValue >> 32) & 0xc0000000) | ((inValue >> 29) & 0x3fffffff));
  } else if (exp >= 874) {
    /*
    Denormalization Required
    if 874 <= frS[1-11] <= 896 then
      sign <- frS[0]
      exp <- frS[1-11] - 1023
      frac <- 0b1 || frS[12-63]
      Denormalize operand
        Do while exp < -126
          frac <- 0b0 || frac[0-62]
          exp <- exp + 1
        End
      WORD[0] <- sign
      WORD[1-8] <- 0x00
      WORD[9-31] <- frac[1-23]
    */
    u32 t = static_cast<u32>(0x80000000 | ((inValue & 0x000FFFFFFFFFFFFFULL) >> 21));
    t = t >> (905 - exp);
    t |= static_cast<u32>((inValue >> 32) & 0x80000000);
    return t;
  } else {
    /*
    WORD <- undefined
    */
    return static_cast<u32>(((inValue >> 32) & 0xc0000000) | ((inValue >> 29) & 0x3fffffff));
  }
}

//
// Store Byte
//

// Data Cache Block Store
void PPCInterpreter::PPCInterpreter_dcbst(sPPEState *ppeState) {
  // Temporarily disable caching
  return;
}

// Data Cache Block set to Zero
void PPCInterpreter::PPCInterpreter_dcbz(sPPEState *ppeState) {
  u64 EA = (_instr.ra ? GPRi(ra) : 0) + GPRi(rb);
  EA = EA & ~(128 - 1); // Cache line size

  // Temporarily disable caching
  for (u8 n = 0; n < 128; n += sizeof(u64))
    MMUWrite64(ppeState, EA + n, 0);
  return;

  // As far as I can tell, XCPU does all the crypto, scrambling of
  // data on L2 cache, and DCBZ is used for creating cache blocks
  // and also erasing them
}

// Instruction Cache Block Invalidate
void PPCInterpreter::PPCInterpreter_icbi(sPPEState *ppeState) {
  // Do nothing
}

// Store Byte (x'9800 0000')
void PPCInterpreter::PPCInterpreter_stb(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 1) <- rS[56-63]
  */

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite8(ppeState, EA, static_cast<u8>(GPRi(rs)));
}

// Store Byte with Update (x'9C00 0000')
void PPCInterpreter::PPCInterpreter_stbu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 1) <- rS[56-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite8(ppeState, EA, static_cast<u8>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Byte with Update Indexed (x'7C00 01EE')
void PPCInterpreter::PPCInterpreter_stbux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 1) <- rS[56-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite8(ppeState, EA, static_cast<u8>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Byte Indexed (x'7C00 01AE')
void PPCInterpreter::PPCInterpreter_stbx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 1) <- rS[56-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite8(ppeState, EA, static_cast<u8>(GPRi(rs)));
}

//
// Store Halfword
//

// Store Half Word (x'B000 0000')
void PPCInterpreter::PPCInterpreter_sth(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 2) <- rS[48-6316-31]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite16(ppeState, EA, static_cast<u16>(GPRi(rs)));
}

// Store Half Word Byte-Reverse Indexed (x'7C00 072C')
void PPCInterpreter::PPCInterpreter_sthbrx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 2) <- rS[56-63] || rS[48-55]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite16(ppeState, EA, byteswap_be<u16>(static_cast<u16>(GPRi(rs))));
}

// Store Half Word with Update (x'B400 0000')
void PPCInterpreter::PPCInterpreter_sthu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 2) <- rS[48-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite16(ppeState, EA, static_cast<u16>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Half Word with Update Indexed (x'7C00 036E')
void PPCInterpreter::PPCInterpreter_sthux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 2) <- rS[48-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite16(ppeState, EA, static_cast<u16>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Half Word Indexed (x'7C00 032E')
void PPCInterpreter::PPCInterpreter_sthx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 2) <- rS[48-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite16(ppeState, EA, static_cast<u16>(GPRi(rs)));
}

// Store Multiple Word (x'BC00 0000')
void PPCInterpreter::PPCInterpreter_stmw(sPPEState *ppeState) {
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
    MMUWrite32(ppeState, EA, static_cast<u32>(GPR(idx)));
  }
}

// Store String Word Immediate (x'7C00 05AA')
void PPCInterpreter::PPCInterpreter_stswi(sPPEState *ppeState) {
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
      MMUWrite32(ppeState, EA, static_cast<u32>(GPR(reg)));
      EA += 4;
      N -= 4;
    } else {
      u32 buf = static_cast<u32>(GPR(reg));
      while (N > 0) {
        N = N - 1;
        MMUWrite8(ppeState, EA, (0xFF000000 & buf) >> 24);
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
void PPCInterpreter::PPCInterpreter_stw(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 4) <- rS[32-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite32(ppeState, EA, static_cast<u32>(GPRi(rs)));
}

// Store Word Byte-Reverse Indexed (x'7C00 052C')
void PPCInterpreter::PPCInterpreter_stwbrx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 4) <- rS[56-63] || rS[48-55] || rS[40-47] || rS[32-39]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppeState, EA, byteswap_be<u32>(static_cast<u32>(GPRi(rs))));
}

// Store Word Conditional Indexed (x'7C00 012D')
void PPCInterpreter::PPCInterpreter_stwcx(sPPEState *ppeState) {
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
  u64 RA = EA & ~7;
  u32 CR = 0;

  // TODO: If address is not aligned by 4, then we must issue a trap.

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  // Translate address
  MMUTranslateAddress(&RA, ppeState, true);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  if (curThread.ppuRes->valid) {
    xenonContext->xenonRes.LockGuard([&] {
      if (curThread.ppuRes->valid) {
        if (curThread.ppuRes->reservedAddr == RA) {
          MMUWrite32(ppeState, EA, static_cast<u32>(GPRi(rs)));
          BSET(CR, 4, CR_BIT_EQ);
        } else {
          xenonContext->xenonRes.Decrement();
          curThread.ppuRes->valid = false;
        }
      }
    });
  }

  ppcUpdateCR(ppeState, 0, CR);
}

// Store Word with Update (x'9400 0000')
void PPCInterpreter::PPCInterpreter_stwu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 4) <- rS[32-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite32(ppeState, EA, static_cast<u32>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Word with Update Indexed (x'7C00 016E')
void PPCInterpreter::PPCInterpreter_stwux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 4) <- rS[32-63]
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite32(ppeState, EA, static_cast<u32>(GPRi(rs)));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Word Indexed (x'7C00 012E')
void PPCInterpreter::PPCInterpreter_stwx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 4) <- rS[32-63]
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppeState, EA, static_cast<u32>(GPRi(rs)));
}

//
// Store Doubleword
//

// Store Double Word (x'F800 0000')
void PPCInterpreter::PPCInterpreter_std(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(ds || 0b00)
  (MEM(EA, 8)) <- (rS)
  */
  const u64 EA = (_instr.simm16 & ~3) + (_instr.ra ? GPRi(ra) : 0);
  MMUWrite64(ppeState, EA, GPRi(rs));
}

// Store Double Word Conditional Indexed (x'7C00 01AD')
void PPCInterpreter::PPCInterpreter_stdcx(sPPEState *ppeState) {
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
  u64 RA = EA & ~7;
  u32 CR = 0;

  if (curThread.SPR.XER.SO)
    BSET(CR, 4, CR_BIT_SO);

  // TODO: If the address is not aligned by 4, then we must issue a trap.

  MMUTranslateAddress(&RA, ppeState, true);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  if (curThread.ppuRes->valid) {
    xenonContext->xenonRes.LockGuard([&] {
      if (curThread.ppuRes->valid) {
        if (curThread.ppuRes->reservedAddr == RA) {
          MMUWrite64(ppeState, EA, GPRi(rd));
          BSET(CR, 4, CR_BIT_EQ);
        } else {
          xenonContext->xenonRes.Decrement();
          curThread.ppuRes->valid = false;
        }
      }
    });
  }

  ppcUpdateCR(ppeState, 0, CR);
}

// Store Double Word with Update (x'F800 0001')
void PPCInterpreter::PPCInterpreter_stdu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(ds || 0b00)
  (MEM(EA, 8)) <- (rS)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + (_instr.simm16 & ~3);
  MMUWrite64(ppeState, EA, GPRi(rs));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Double Word with Update Indexed (x'7C00 016A')
void PPCInterpreter::PPCInterpreter_stdux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 8) <- (rS)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite64(ppeState, EA, GPRi(rs));

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Double Word Indexed (x'7C00 012A')
void PPCInterpreter::PPCInterpreter_stdx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  (MEM(EA, 8)) <- (rS)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite64(ppeState, EA, GPRi(rs));
}

//
// Store Floating
//

// Store Floating-Point Single (x'D000 0000')
void PPCInterpreter::PPCInterpreter_stfs(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 4) <- SINGLE(frS)
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite32(ppeState, EA, ConvertToSingle(FPRi(frs).asU64()));
}

// Store Floating-Point Single with Update (x'D400 0000')
void PPCInterpreter::PPCInterpreter_stfsu(sPPEState* ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 4) <-  SINGLE(frS)
  rA <-  EA
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite32(ppeState, EA, ConvertToSingle(FPRi(frs).asU64()));
  
  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Floating-Point Single with Update Indexed
void PPCInterpreter::PPCInterpreter_stfsux(sPPEState* ppeState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 4) <-  SINGLE(frS)
  rA <-  EA
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppeState, EA, ConvertToSingle(FPRi(frs).asU64()));
  
  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Floating-Point Single Indexed (x'7C00 052E')
void PPCInterpreter::PPCInterpreter_stfsx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 4) <- SINGLE(frS)
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppeState, EA, ConvertToSingle(FPRi(frs).asU64()));
}

// Store Floating-Point Double (x'D800 0000')
void PPCInterpreter::PPCInterpreter_stfd(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  MEM(EA, 8) <- (frS)
  */

  CHECK_FPU;

  const u64 EA = _instr.ra || 1 ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  MMUWrite64(ppeState, EA, FPRi(frs).asU64());
}

// Store Floating-Point Double Indexed
void PPCInterpreter::PPCInterpreter_stfdx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 8) <- (frS)
  */

  CHECK_FPU;

  const u64 EA = _instr.ra || 1 ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite64(ppeState, EA, FPRi(frs).asU64());
}

// Store Floating-Point Double with Update
void PPCInterpreter::PPCInterpreter_stfdu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  MEM(EA, 8) <- (frS)
  rA <- EA
  */

  CHECK_FPU;

  const u64 EA = GPRi(ra) + _instr.simm16;
  MMUWrite64(ppeState, EA, FPRi(frs).asU64());

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Floating-Point Double with Update Indexed
void PPCInterpreter::PPCInterpreter_stfdux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  MEM(EA, 8) <- (frS)
  rA <- EA
  */

  CHECK_FPU;

  const u64 EA = GPRi(ra) + GPRi(rb);
  MMUWrite64(ppeState, EA, FPRi(frs).asU64());

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(ra) = EA;
}

// Store Floating-Point as Integer Word Indexed (x'7C00 07AE')
void PPCInterpreter::PPCInterpreter_stfiwx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  MEM(EA, 4) <- frS[32-63]
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  MMUWrite32(ppeState, EA, FPRi(frs).asU32());
}

//
// Store Vector
//

// Store Vector Indexed
void PPCInterpreter::PPCInterpreter_stvx(sPPEState *ppeState) {
  /*
  if rA=0 then b <- 0
  else b <- (rA)
  EA <- (b + (rB)) & 0xFFFF_FFFF_FFFF_FFF0
  if the processor is in big-endian mode
   then MEM(EA,16) <- (vS)
   else MEM(EA,16) <- (vS)64:127 || (vS)0:63
  */

  CHECK_VXU;

  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  MMUWrite32(ppeState, EA, VRi(vs).dword[0]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 1), VRi(vs).dword[1]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 2), VRi(vs).dword[2]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 3), VRi(vs).dword[3]);
}

// Store Vector Indexed 128
void PPCInterpreter::PPCInterpreter_stvx128(sPPEState* ppeState) {
  CHECK_VXU;

  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  MMUWrite32(ppeState, EA, VR(VMX128_1_VD128).dword[0]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 1), VR(VMX128_1_VD128).dword[1]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 2), VR(VMX128_1_VD128).dword[2]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 3), VR(VMX128_1_VD128).dword[3]);
}

// Store Vector Element Word Indexed (x'7C00 018E')
void PPCInterpreter::PPCInterpreter_stvewx(sPPEState* ppeState) {
  /*
    if rA=0 then b <- 0
    else b <- (rA)
    EA <- (b + (rB)) & 0xFFFF_FFFF_FFFF_FFFC
    eb <- EA[60:63]
    if the processor is in big-endian mode
     then MEM(EA,4) <- (vS)eb*8:(eb*8)+31
     else MEM(EA,4) <- (vS)96-eb*8:127-(eb*8)
  */

  CHECK_VXU;

  u64 EA = ((_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~3);
  const u8 eb = EA & 0xF;

  MMUWrite32(ppeState, EA, VRi(vd).dword[eb / 4]);
}

// Store Vector Element Word Indexed 128
void PPCInterpreter::PPCInterpreter_stvewx128(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = ((_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~3);
  const u8 eb = EA & 0xF;

  MMUWrite32(ppeState, EA, VR(VMX128_1_VD128).dword[eb / 4]);
}

// Store Vector Right Indexed
void PPCInterpreter::PPCInterpreter_stvrx(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;
  EA &= ~0xF;

  // If EB = 0, then memory is not modified by the instruction.
  if (eb == 0) {
    return;
  }

  // Need to byteswap the bytes prior to the operation because of endianness.
  Vector128 vec = VRi(vs);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  auto bytes = vec.bytes;
  for (u32 i = 0; i < eb; ++i) {
    MMUWrite8(ppeState, EA + i, bytes[(16 - eb) + i]);
  }
}

// Store Vector Right Indexed 128
void PPCInterpreter::PPCInterpreter_stvrx128(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;
  EA &= ~0xF;

  // If EB = 0, then memory is not modified by the instruction.
  if (eb == 0) {
    return;
  }

  // Need to byteswap the bytes prior to the operation because of endianness.
  Vector128 vec = VR(VMX128_1_VD128);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  auto bytes = vec.bytes;
  for (u32 i = 0; i < eb; ++i) {
    MMUWrite8(ppeState, EA + i, bytes[(16 - eb) + i]);
  }
}

// Store Vector Left Indexed
void PPCInterpreter::PPCInterpreter_stvlx(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;

  // Need to byteswap the bytes prior to the operation because of endianness.
  Vector128 vec = VRi(vs);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  auto bytes = vec.bytes;
  for (int i = 0; i < 16 - eb; ++i) {
    MMUWrite8(ppeState, EA + i, bytes[i]);
  }
}

// Store Vector Left Indexed 128
void PPCInterpreter::PPCInterpreter_stvlx128(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;

  Vector128 vec = VR(VMX128_1_VD128);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  auto bytes = vec.bytes;
  for (int i = 0; i < 16 - eb; ++i) {
    MMUWrite8(ppeState, EA + i, bytes[i]);
  }
}

// Store Vector Left Indexed LRU 128
void PPCInterpreter::PPCInterpreter_stvlxl128(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;

  Vector128 vec = VR(VMX128_1_VD128);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  auto bytes = vec.bytes;
  for (int i = 0; i < 16 - eb; ++i) {
    MMUWrite8(ppeState, EA + i, bytes[i]);
  }
}

// Store Vector Indexed LRU (x'7C00 03CE')
void PPCInterpreter::PPCInterpreter_stvxl(sPPEState *ppeState) {
  /*
  if rA=0 then b <- 0
  else b <- (rA)
  EA <- (b + (rB)) & 0xFFFF_FFFF_FFFF_FFF0
  if the processor is in big-endian mode
   then MEM(EA,16) <- (vS)
   else MEM(EA,16) <- (vS)64:127 || (vS)0:63
  */

  CHECK_VXU;

  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  MMUWrite32(ppeState, EA, VRi(vs).dword[0]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 1), VRi(vs).dword[1]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 2), VRi(vs).dword[2]);
  MMUWrite32(ppeState, EA + (sizeof(u32) * 3), VRi(vs).dword[3]);
}

//
// Load Byte
//

// Load Byte and Zero (x'8800 0000')
void PPCInterpreter::PPCInterpreter_lbz(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  rD <- (56)0 || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  const u8 data = MMURead8(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Byte and Zero with Update (x'8C00 0000')
void PPCInterpreter::PPCInterpreter_lbzu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  rD <- (56)0 || MEM(EA, 1)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  const u8 data = MMURead8(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Byte and Zero with Update Indexed (x'7C00 00EE')
void PPCInterpreter::PPCInterpreter_lbzux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  rD <- (56)0 || MEM(EA, 1)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  const u8 data = MMURead8(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Byte and Zero Indexed (x'7C00 00AE')
void PPCInterpreter::PPCInterpreter_lbzx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (56)0 || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u8 data = MMURead8(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

//
// Load Halfword
//

// Load Half Word Algebraic (x'A800 0000')
void PPCInterpreter::PPCInterpreter_lha(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  rD <- EXTS(MEM(EA, 2))
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  const u16 unsignedWord = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
}

// Load Half Word Algebraic with Update (x'AC00 0000')
void PPCInterpreter::PPCInterpreter_lhau(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  rD <- EXTS(MEM(EA, 2))
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  const u16 unsignedWord = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
  GPRi(ra) = EA;
}

// Load Half Word Algebraic with Update
void PPCInterpreter::PPCInterpreter_lhaux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  rD <- EXTS(MEM(EA, 2))
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  const u16 unsignedWord = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
  GPRi(ra) = EA;
}

// Load Half Word Algebraic Indexed (x'7C00 02AE')
void PPCInterpreter::PPCInterpreter_lhax(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- EXTS(MEM(EA, 2))
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u16 unsignedWord = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 16);
}

// Load Half Word Byte-Reverse Indexed (x'7C00 062C')
void PPCInterpreter::PPCInterpreter_lhbrx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (48)0 || MEM(EA + 1, 1) || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u16 data = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = byteswap_be<u16>(data);
}

// Load Half Word and Zero (x'A000 0000')
void PPCInterpreter::PPCInterpreter_lhz(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b <- EXTS(d)
  rD <- (48)0 || MEM(EA, 2)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  const u16 data = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Half Word and Zero with Update (x'A400 0000')
void PPCInterpreter::PPCInterpreter_lhzu(sPPEState *ppeState) {
  /*
  EA <- rA + EXTS(d)
  rD <- (48)0 || MEM(EA, 2)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  const u16 data = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Half Word and Zero with Update Indexed (x'7C00 026E')
void PPCInterpreter::PPCInterpreter_lhzux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  rD <- (48)0 || MEM(EA, 2)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  const u16 data = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Half Word and Zero Indexed (x'7C00 022E')
void PPCInterpreter::PPCInterpreter_lhzx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (48)0 || MEM(EA, 2)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u16 data = MMURead16(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

//
// String Word
//

// Load Multiple Word (x'B800 0000')
void PPCInterpreter::PPCInterpreter_lmw(sPPEState *ppeState) {
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
    GPR(idx) = MMURead32(ppeState, EA);
  }
}

// Load String Word Immediate (x'7C00 04AA')
void PPCInterpreter::PPCInterpreter_lswi(sPPEState *ppeState) {
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
      GPR(reg) = MMURead32(ppeState, EA);
      EA += 4;
      N -= 4;
    } else {
      u32 buf = 0;
      u32 i = 3;
      while (N > 0) {
        N = N - 1;
        buf |= MMURead8(ppeState, EA) << (i * 8);
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
void PPCInterpreter::PPCInterpreter_lwa(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(ds || 0b00)
  rD <- EXTS(MEM(EA, 4))
  */
  const u64 EA = (_instr.simm16 & ~3) + (_instr.ra ? GPRi(ra) : 0);
  const u32 unsignedWord = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 32);
}

// Load Word and Reserve Indexed (x'7C00 0028')
void PPCInterpreter::PPCInterpreter_lwarx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  RESERVE <- 1
  RESERVE_ADDR <- physical_addr(EA)
  rD <- (32)0 || MEM(EA,4)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 RA = EA & ~7;

  // TODO: If address is not aligned by 4, then we must issue a trap.

  MMUTranslateAddress(&RA, ppeState, false);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  curThread.ppuRes->valid = true;
  curThread.ppuRes->reservedAddr = RA;
  xenonContext->xenonRes.Increment();

  u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

    GPRi(rd) = data;
}

// Load Word Algebraic Indexed (x'7C00 02AA')
void PPCInterpreter::PPCInterpreter_lwax(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- EXTS(MEM(EA, 4))
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u32 unsignedWord = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 32);
}

// Load Word Algebraic with Update Indexed
void PPCInterpreter::PPCInterpreter_lwaux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  rD <- EXTS(MEM(EA, 4))
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  const u32 unsignedWord = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = EXTS(unsignedWord, 32);
  GPRi(ra) = EA;
}

// Load Word Byte-Reverse Indexed (x'7C00 042C')
void PPCInterpreter::PPCInterpreter_lwbrx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- (32)0 || MEM(EA + 3, 1) || MEM(EA + 2, 1) || MEM(EA + 1, 1) || MEM(EA, 1)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = byteswap_be<u32>(data);
}

// Load Word and Zero (x'8000 0000')
void PPCInterpreter::PPCInterpreter_lwz(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  rD <- (32)0 || MEM(EA, 4)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Word and Zero with Update (x'8400 0000')
void PPCInterpreter::PPCInterpreter_lwzu(sPPEState *ppeState) {
  /*
  EA <- rA + EXTS(d)
  rD <- (32)0 || MEM(EA, 4)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + _instr.simm16;
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Word and Zero with Update Indexed (x'7C00 006E')
void PPCInterpreter::PPCInterpreter_lwzux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  rD <- (32)0 || MEM(EA, 4)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Word and Zero Indexed (x'7C00 002E')
void PPCInterpreter::PPCInterpreter_lwzx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + rB
  rD <- (32)0 || MEM(EA, 4)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

//
// Load Doubleword
//

// Load Double Word (x'E800 0000')
void PPCInterpreter::PPCInterpreter_ld(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(ds || 0b00)
  rD <- MEM(EA, 8)
  */
  const u64 EA = (_instr.simm16 & ~3) + (_instr.ra ? GPRi(ra) : 0);
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Double Word Byte Reversed Indexed
void PPCInterpreter::PPCInterpreter_ldbrx(sPPEState *ppeState) {
  // TODO(bitsh1ft3r): Add instruction def and check for that ~7 to be correct
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u64 RA = EA & ~7;
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Double Word and Reserve Indexed (x'7C00 00A8')
void PPCInterpreter::PPCInterpreter_ldarx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  RESERVE <- 1
  RESERVE_ADDR <- physical_addr(EA)
  rD <- MEM(EA, 8)
  */

  // TODO: If the address is not aligned by 8 then we must issue a trap.
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  u64 RA = EA & ~7;

  MMUTranslateAddress(&RA, ppeState, false);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  curThread.ppuRes->reservedAddr = RA;
  curThread.ppuRes->valid = true;
  xenonContext->xenonRes.Increment();

  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Double Word with Update (x'E800 0001')
void PPCInterpreter::PPCInterpreter_ldu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(ds || 0b00)
  rD <- MEM(EA, 8)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + (_instr.simm16 & ~3);
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Double Word with Update Indexed (x'7C00 006A')
void PPCInterpreter::PPCInterpreter_ldux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  rD <- MEM(EA, 8)
  rA <- EA
  */
  const u64 EA = GPRi(ra) + GPRi(rb);
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
  GPRi(ra) = EA;
}

// Load Double Word Indexed (x'7C00 002A')
void PPCInterpreter::PPCInterpreter_ldx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  rD <- MEM(EA, 8)
  */
  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  GPRi(rd) = data;
}

// Load Floating-Point Single Indexed (x'7C00 042E')
void PPCInterpreter::PPCInterpreter_lfsx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  frD <- DOUBLE(MEM(EA, 4))
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(ConvertToDouble(data));
}

// Load Floating-Point Single with Update Indexed
void PPCInterpreter::PPCInterpreter_lfsux(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  frD <- DOUBLE(MEM(EA, 4))
  rA <- EA
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(ConvertToDouble(data));
  
  GPRi(ra) = EA;
}

// Load Floating-Point Double (x'C800 0000')
void PPCInterpreter::PPCInterpreter_lfd(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  frD <- MEM(EA, 8)
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(data);
}

// Load Floating-Point Double-Indexed (x'C800 0000')
void PPCInterpreter::PPCInterpreter_lfdx(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + (rB)
  frD <- MEM(EA, 8)
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb);
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(data);
}

// Load Floating-Point Double with Update
void PPCInterpreter::PPCInterpreter_lfdu(sPPEState *ppeState) {
  /*
  EA <- (rA) + EXTS(d)
  frD <- MEM(EA, 8)
  rA <- EA
  */

  CHECK_FPU;

  const u64 EA = GPRi(ra) + _instr.simm16;
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(data);
  GPRi(ra) = EA;
}

// Load Floating-Point Double with Update Indexed
void PPCInterpreter::PPCInterpreter_lfdux(sPPEState *ppeState) {
  /*
  EA <- (rA) + (rB)
  frD <- MEM(EA, 8)
  rA <- EA
  */

  CHECK_FPU;

  const u64 EA = GPRi(ra) + GPRi(rb);
  const u64 data = MMURead64(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(data);
  GPRi(ra) = EA;
}

// Load Floating-Point Single (x'C000 0000')
void PPCInterpreter::PPCInterpreter_lfs(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  frD <- DOUBLE(MEM(EA, 4))
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;

  u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(ConvertToDouble(data));
}

// Load Floating-Point Single with Update
void PPCInterpreter::PPCInterpreter_lfsu(sPPEState *ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  EA <- b + EXTS(d)
  frD <- DOUBLE(MEM(EA, 4))
  rA <- EA
  */

  CHECK_FPU;

  const u64 EA = _instr.ra ? GPRi(ra) + _instr.simm16 : _instr.simm16;
  
  u32 data = MMURead32(ppeState, EA);

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  FPRi(frd).setValue(ConvertToDouble(data));

  GPRi(ra) = EA;
}

//
// Load Vector
//

// NOTE: lvexx Instructions effectively behave like lvx.

// Load Vector Element Byte Indexed (x'7C00 000E')
void PPCInterpreter::PPCInterpreter_lvebx(sPPEState* ppeState) {
  /*
   if rA=0 then b <- 0
   else         b <- (rA)
   EA <- b + (rB)
   eb <- EA60:63 
   vD <- undefined
   (vD)eb×8 :(eb×8)+7 <- MEM(EA,1)
  */
  
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  Vector128 vector = {};

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvebx [EA = {:#x}, eb = {:#x} data = {:#x}]", (u32)EA, eb, data);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VRi(vd) = vector;
}

// Load Vector Element Halfword Indexed (x'7C00 004E')
void PPCInterpreter::PPCInterpreter_lvehx(sPPEState* ppeState) {
  /*
   if rA=0 then b <- 0
   else        
   b <- (rA)
   EA <- (b + (rB)) & 0xFFFF_FFFF_FFFF_FFFE 
   eb <- EA60:63
   vD <- undefined
   (vD)(eb×8):(eb×8) + 15 <- MEM(EA,2)
  */
  
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  Vector128 vector = {};

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvehx [EA = {:#x}, eb = {:#x} data = {:#x}]", (u32)EA, eb, data);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VRi(vd) = vector;
}

// Load Vector Element Word Indexed (x'7C00 008E')
void PPCInterpreter::PPCInterpreter_lvewx(sPPEState* ppeState) {
  /*
  if rA=0 then b <- 0
  else b <- (rA)
  EA <- (b + (rB)) & (~3)
  eb <- EA60:63
  vD <- undefined
  if the processor is in big-endian mode
   then vDeb*8:(eb*8)+31<- MEM(EA,4)
   else vD96-(eb*8):127-(eb*8)<- MEM(EA,4)
  */

  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  Vector128 vector = {};

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvewx [EA = {:#x}, eb = {:#x} data = {:#x}]", (u32)EA, eb, data);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VRi(vd) = vector;
}

// Vector128 Load Vector Element Word Indexed
void PPCInterpreter::PPCInterpreter_lvewx128(sPPEState* ppeState) {
  CHECK_VXU;

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  Vector128 vector = {};

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvewx128 [EA = {:#x}, eb = {:#x} data = {:#x}]", (u32)EA, eb, data);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VR(VMX128_1_VD128) = vector;
}

// Load Vector Indexed (x'7C00 00CE')
void PPCInterpreter::PPCInterpreter_lvx(sPPEState* ppeState) {
  /*
  if rA=0 then b <- 0
  else b <- (rA)
  EA <- (b + (rB)) & (~0xF)
  if the processor is in big-endian mode
   then vD <- MEM(EA,16)
   else vD <- MEM(EA+8,8) || MEM(EA,8)
  */
  
  CHECK_VXU;

  Vector128 vector{};
  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  u8 vrd = _instr.vd;
  LOG_DEBUG(Xenon, "lvx [EA {:#x}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vrd, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG


  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VRi(vd) = vector;
}

// Load Vector Indexed 128
void PPCInterpreter::PPCInterpreter_lvx128(sPPEState *ppeState) {

  CHECK_VXU;

  Vector128 vector {};
  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  u8 vrd = VMX128_1_VD128;
  LOG_DEBUG(Xenon, "lvx128 [EA {:#x}] vR{:#d} = [dw0: {:#x}, dw1: {:#x}, dw2: {:#x}, dw3: {:#x}]",
    (u32)EA, vrd, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VR(VMX128_1_VD128) = vector;
}

// // Load Vector Indexed LRU 128
void PPCInterpreter::PPCInterpreter_lvxl128(sPPEState* ppeState) {
  CHECK_VXU;

  Vector128 vector{};
  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  u8 vrd = VMX128_1_VD128;
  LOG_DEBUG(Xenon, "lvxl128 [EA {:#x}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vrd, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VR(VMX128_1_VD128) = vector;
}

// Load Vector Indexed LRU (x'7C00 02CE')
void PPCInterpreter::PPCInterpreter_lvxl(sPPEState *ppeState) {
  /*
  if rA=0 then b <- 0
  else b <- (rA)
  EA <- (b + (rB)) & (~0xF)
  if the processor is in big-endian mode
   then vD <- MEM(EA,16)
   else vD <- MEM(EA+8,8) || MEM(EA,8)
  */

  CHECK_VXU;

  Vector128 vector{};
  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb)) & ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  u8 vrd = _instr.vd;
  LOG_DEBUG(Xenon, "lvxl [EA {:#x}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vrd, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  VRi(vd) = vector;
}

// Load Vector Left Indexed (x'7C00 040E')
void PPCInterpreter::PPCInterpreter_lvlx(sPPEState *ppeState) {
  /*
  if rA=0 then base <- 0
  else base <- (rA)
  EA <- (base + (rB))
  eb <- EA60:63
  (vD) <- MEM(EA,16-eb) || (eb*8) (0)
  */

  CHECK_VXU;

  Vector128 vector{};

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;
  EA &= ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvlx [EA {:#x}] Temp VR = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  vector.dword[0] = byteswap_be<u32>(vector.dword[0]);
  vector.dword[1] = byteswap_be<u32>(vector.dword[1]);
  vector.dword[2] = byteswap_be<u32>(vector.dword[2]);
  vector.dword[3] = byteswap_be<u32>(vector.dword[3]);

  u8 i = 0;
  for (i = 0; i < 16 - eb; ++i)
    VRi(vd).bytes[i] = vector.bytes[i + eb];

  while (i < 16)
    VRi(vd).bytes[i++] = 0;
 
  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);

#ifdef VXU_LOAD_DEBUG
  u8 vrd = _instr.vd;
  LOG_DEBUG(Xenon, "lvlx [EB {:#d}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    eb, vrd, VRi(vd).dword[0], VRi(vd).dword[1], VRi(vd).dword[2], VRi(vd).dword[3]);
#endif // VXU_LOAD_DEBUG
}

// Load Vector Left Indexed 128
void PPCInterpreter::PPCInterpreter_lvlx128(sPPEState* ppeState) {
  CHECK_VXU;

  Vector128 vector{};

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;
  EA &= ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvlx128 [EA {:#x}] Temp VR = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  vector.dword[0] = byteswap_be<u32>(vector.dword[0]);
  vector.dword[1] = byteswap_be<u32>(vector.dword[1]);
  vector.dword[2] = byteswap_be<u32>(vector.dword[2]);
  vector.dword[3] = byteswap_be<u32>(vector.dword[3]);

  u8 i = 0;
  for (i = 0; i < 16 - eb; ++i)
    VR(VMX128_1_VD128).bytes[i] = vector.bytes[i + eb];

  while (i < 16)
    VR(VMX128_1_VD128).bytes[i++] = 0;

  VR(VMX128_1_VD128).dword[0] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[0]);
  VR(VMX128_1_VD128).dword[1] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[1]);
  VR(VMX128_1_VD128).dword[2] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[2]);
  VR(VMX128_1_VD128).dword[3] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[3]);

#ifdef VXU_LOAD_DEBUG
  u8 vrd = VMX128_1_VD128;
  LOG_DEBUG(Xenon, "lvlx128 [EB {:#d}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    eb, vrd, VR(VMX128_1_VD128).dword[0], VR(VMX128_1_VD128).dword[1], VR(VMX128_1_VD128).dword[2], VR(VMX128_1_VD128).dword[3]);
#endif // VXU_LOAD_DEBUG
}


// Load Vector Right Indexed (x'7C00 044E')
void PPCInterpreter::PPCInterpreter_lvrx(sPPEState *ppeState) {
  /*
  if rA=0 then base <- 0
  else base <- (rA)
  EA <- (base + (rB))
  eb <- EA[60:63]
  (vD) <- (16-eb-8) (0) || MEM(EA-eb,eb)
  */

  CHECK_VXU;

  Vector128 vector{};

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;
  EA &= ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvrx [EA {:#x}] Temp VR = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  vector.dword[0] = byteswap_be<u32>(vector.dword[0]);
  vector.dword[1] = byteswap_be<u32>(vector.dword[1]);
  vector.dword[2] = byteswap_be<u32>(vector.dword[2]);
  vector.dword[3] = byteswap_be<u32>(vector.dword[3]);

  u8 i = 0;

  while (i < (16 - eb)) {
    VRi(vd).bytes[i] = 0;
    ++i;
  }

  while (i < 16) {
    VRi(vd).bytes[i] = vector.bytes[i - (16 - eb)];
    ++i;
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);

#ifdef VXU_LOAD_DEBUG
  u8 vrd = _instr.vd;
  LOG_DEBUG(Xenon, "lvrx [EB {:#d}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    eb, vrd, VRi(vd).dword[0], VRi(vd).dword[1], VRi(vd).dword[2], VRi(vd).dword[3]);
#endif // VXU_LOAD_DEBUG
}

// Load Vector Right Indexed 128
void PPCInterpreter::PPCInterpreter_lvrx128(sPPEState* ppeState) {
  CHECK_VXU;

  Vector128 vector{};

  u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 eb = EA & 0xF;
  EA &= ~0xF;

  vector.dword[0] = MMURead32(ppeState, EA);
  vector.dword[1] = MMURead32(ppeState, EA + (sizeof(u32) * 1));
  vector.dword[2] = MMURead32(ppeState, EA + (sizeof(u32) * 2));
  vector.dword[3] = MMURead32(ppeState, EA + (sizeof(u32) * 3));

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvrx128 [EA {:#x}] Temp VR = [{:#x}, {:#x}, {:#x}, {:#x}]",
    (u32)EA, vector.dword[0], vector.dword[1], vector.dword[2], vector.dword[3]);
#endif // VXU_LOAD_DEBUG

  if (_ex & PPU_EX_DATASEGM || _ex & PPU_EX_DATASTOR)
    return;

  vector.dword[0] = byteswap_be<u32>(vector.dword[0]);
  vector.dword[1] = byteswap_be<u32>(vector.dword[1]);
  vector.dword[2] = byteswap_be<u32>(vector.dword[2]);
  vector.dword[3] = byteswap_be<u32>(vector.dword[3]);

  u8 i = 0;

  while (i < (16 - eb)) {
    VR(VMX128_1_VD128).bytes[i] = 0;
    ++i;
  }

  while (i < 16) {
    VR(VMX128_1_VD128).bytes[i] = vector.bytes[i - (16 - eb)];
    ++i;
  }

  VR(VMX128_1_VD128).dword[0] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[0]);
  VR(VMX128_1_VD128).dword[1] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[1]);
  VR(VMX128_1_VD128).dword[2] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[2]);
  VR(VMX128_1_VD128).dword[3] = byteswap_be<u32>(VR(VMX128_1_VD128).dword[3]);

#ifdef VXU_LOAD_DEBUG
  u8 vrd = VMX128_1_VD128;
  LOG_DEBUG(Xenon, "lvrx128 [EB {:#d}] vR{:#d} = [{:#x}, {:#x}, {:#x}, {:#x}]",
    eb, vrd, VR(VMX128_1_VD128).dword[0], VR(VMX128_1_VD128).dword[1], VR(VMX128_1_VD128).dword[2], VR(VMX128_1_VD128).dword[3]);
#endif // VXU_LOAD_DEBUG
}

// Load Vector for Shift Left (x'7C00 000C')
void PPCInterpreter::PPCInterpreter_lvsl(sPPEState* ppeState) {
  /*
  if rA = 0 then b <- 0
  else b <- (rA)
  10 11
  15 16
  20 21
  addr[0:63] <- b + (rB)
  sh <- addr[60:63]
  if sh = 0x0 then vD[0-127] <- 0x000102030405060708090A0B0C0D0E0F
  if sh = 0x1 then vD[0-127] <- 0x0102030405060708090A0B0C0D0E0F10
  if sh = 0x2 then vD[0-127] <- 0x02030405060708090A0B0C0D0E0F1011
  if sh = 0x3 then vD[0-127] <- 0x030405060708090A0B0C0D0E0F101112
  if sh = 0x4 then vD[0-127] <- 0x0405060708090A0B0C0D0E0F10111213
  if sh = 0x5 then vD[0-127] <- 0x05060708090A0B0C0D0E0F1011121314
  if sh = 0x6 then vD[0-127] <- 0x060708090A0B0C0D0E0F101112131415
  if sh = 0x7 then vD[0-127] <- 0x0708090A0B0C0D0E0F10111213141516
  if sh = 0x8 then vD[0-127] <- 0x08090A0B0C0D0E0F1011121314151617
  if sh = 0x9 then vD[0-127] <- 0x090A0B0C0D0E0F101112131415161718
  if sh = 0xA then vD[0-127] <- 0x0A0B0C0D0E0F10111213141516171819
  if sh = 0xB then vD[0-127] <- 0x0B0C0D0E0F101112131415161718191A
  if sh = 0xC then vD[0-127] <- 0x0C0D0E0F101112131415161718191A1B
  if sh = 0xD then vD[0-127] <- 0x0D0E0F101112131415161718191A1B1C
  if sh = 0xE then vD[0-127] <- 0x0E0F101112131415161718191A1B1C1D
  if sh = 0xF then vD[0-127] <- 0x0F101112131415161718191A1B1C1D1E
  */

  CHECK_VXU;

  Vector128 vector{};
  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 sh = EA & 0xF;

#ifdef VXU_LOAD_DEBUG
  u8 vrd = _instr.vd;
  LOG_DEBUG(Xenon, "lvsl: EA {:#x}, sh {:#x}, VrD {:#d} = [{:#x},{:#x},{:#x},{:#x}]", EA, sh, vrd, VRi(vd).dword[0], 
    VRi(vd).dword[1], VRi(vd).dword[2], VRi(vd).dword[3]);
#endif // VXU_LOAD_DEBUG

#define MAKE_VECTOR(x,y,z,w) vector.dword[0] = x; vector.dword[1] = y; vector.dword[2] = z; vector.dword[3] = w;

  switch (sh) {
  case 0x0:  MAKE_VECTOR(0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F); break;
  case 0x1:  MAKE_VECTOR(0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10); break;
  case 0x2:  MAKE_VECTOR(0x02030405, 0x06070809, 0x0A0B0C0D, 0x0E0F1011); break;
  case 0x3:  MAKE_VECTOR(0x03040506, 0x0708090A, 0x0B0C0D0E, 0x0F101112); break;
  case 0x4:  MAKE_VECTOR(0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213); break;
  case 0x5:  MAKE_VECTOR(0x05060708, 0x090A0B0C, 0x0D0E0F10, 0x11121314); break;
  case 0x6:  MAKE_VECTOR(0x06070809, 0x0A0B0C0D, 0x0E0F1011, 0x12131415); break;
  case 0x7:  MAKE_VECTOR(0x0708090A, 0x0B0C0D0E, 0x0F101112, 0x13141516); break;
  case 0x8:  MAKE_VECTOR(0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617); break;
  case 0x9:  MAKE_VECTOR(0x090A0B0C, 0x0D0E0F10, 0x11121314, 0x15161718); break;
  case 0xA:  MAKE_VECTOR(0x0A0B0C0D, 0x0E0F1011, 0x12131415, 0x16171819); break;
  case 0xB:  MAKE_VECTOR(0x0B0C0D0E, 0x0F101112, 0x13141516, 0x1718191A); break;
  case 0xC:  MAKE_VECTOR(0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B); break;
  case 0xD:  MAKE_VECTOR(0x0D0E0F10, 0x11121314, 0x15161718, 0x191A1B1C); break;
  case 0xE:  MAKE_VECTOR(0x0E0F1011, 0x12131415, 0x16171819, 0x1A1B1C1D); break;
  case 0xF:  MAKE_VECTOR(0x0F101112, 0x13141516, 0x1718191A, 0x1B1C1D1E); break;
  }

#undef MAKE_VECTOR

  VRi(vd) = vector;

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvsl: VrD {:#d} = [{:#x},{:#x},{:#x},{:#x}]", vrd, VRi(vd).dword[0],
    VRi(vd).dword[1], VRi(vd).dword[2], VRi(vd).dword[3]);
#endif // VXU_LOAD_DEBUG
}

// Load Vector for Shift Right (x'7C00 004C')
void PPCInterpreter::PPCInterpreter_lvsr(sPPEState* ppeState) {
  /*
   if rA = 0 then  b <- 0
   else            b <- (rA)
   EA <- b + (rB)
   sh <- EA[60:63]

   if sh=0x0 then (vD) <- 0x101112131415161718191A1B1C1D1E1F
   if sh=0x1 then (vD) <- 0x0F101112131415161718191A1B1C1D1E
   if sh=0x2 then (vD) <- 0x0E0F101112131415161718191A1B1C1D
   if sh=0x3 then (vD) <- 0x0D0E0F101112131415161718191A1B1C
   if sh=0x4 then (vD) <- 0x0C0D0E0F101112131415161718191A1B
   if sh=0x5 then (vD) <- 0x0B0C0D0E0F101112131415161718191A
   if sh=0x6 then (vD) <- 0x0A0B0C0D0E0F10111213141516171819
   if sh=0x7 then (vD) <- 0x090A0B0C0D0E0F101112131415161718
   if sh=0x8 then (vD) <- 0x08090A0B0C0D0E0F1011121314151617
   if sh=0x9 then (vD) <- 0x0708090A0B0C0D0E0F10111213141516
   if sh=0xA then (vD) <- 0x060708090A0B0C0D0E0F101112131415
   if sh=0xB then (vD) <- 0x05060708090A0B0C0D0E0F1011121314
   if sh=0xC then (vD) <- 0x0405060708090A0B0C0D0E0F10111213
   if sh=0xD then (vD) <- 0x030405060708090A0B0C0D0E0F101112
   if sh=0xE then (vD) <- 0x02030405060708090A0B0C0D0E0F1011
   if sh=0xF then (vD) <- 0x0102030405060708090A0B0C0D0E0F10
  */

  CHECK_VXU;

  Vector128 vector{};
  const u64 EA = (_instr.ra ? GPRi(ra) + GPRi(rb) : GPRi(rb));
  const u8 sh = EA & 0xF;

#ifdef VXU_LOAD_DEBUG
  u8 vrd = _instr.vd;
  LOG_DEBUG(Xenon, "lvsr: EA {:#x}, sh {:#x}, VrD {:#d} = [{:#x},{:#x},{:#x},{:#x}]", EA, sh, vrd, VRi(vd).dword[0],
    VRi(vd).dword[1], VRi(vd).dword[2], VRi(vd).dword[3]);
#endif // VXU_LOAD_DEBUG

#define MAKE_VECTOR(x,y,z,w) vector.dword[0] = x; vector.dword[1] = y; vector.dword[2] = z; vector.dword[3] = w;

  switch (sh) {
  case 0x0:  MAKE_VECTOR(0x10111213, 0x14151617, 0x18191A1B, 0x1C1D1E1F); break;
  case 0x1:  MAKE_VECTOR(0x0F101112, 0x13141516, 0x1718191A, 0x1B1C1D1E); break;
  case 0x2:  MAKE_VECTOR(0x0E0F1011, 0x12131415, 0x16171819, 0x1A1B1C1D); break;
  case 0x3:  MAKE_VECTOR(0x0D0E0F10, 0x11121314, 0x15161718, 0x191A1B1C); break;
  case 0x4:  MAKE_VECTOR(0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B); break;
  case 0x5:  MAKE_VECTOR(0x0B0C0D0E, 0x0F101112, 0x13141516, 0x1718191A); break;
  case 0x6:  MAKE_VECTOR(0x0A0B0C0D, 0x0E0F1011, 0x12131415, 0x16171819); break;
  case 0x7:  MAKE_VECTOR(0x090A0B0C, 0x0D0E0F10, 0x11121314, 0x15161718); break;
  case 0x8:  MAKE_VECTOR(0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617); break;
  case 0x9:  MAKE_VECTOR(0x0708090A, 0x0B0C0D0E, 0x0F101112, 0x13141516); break;
  case 0xA:  MAKE_VECTOR(0x06070809, 0x0A0B0C0D, 0x0E0F1011, 0x12131415); break;
  case 0xB:  MAKE_VECTOR(0x05060708, 0x090A0B0C, 0x0D0E0F10, 0x11121314); break;
  case 0xC:  MAKE_VECTOR(0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213); break;
  case 0xD:  MAKE_VECTOR(0x03040506, 0x0708090A, 0x0B0C0D0E, 0x0F101112); break;
  case 0xE:  MAKE_VECTOR(0x02030405, 0x06070809, 0x0A0B0C0D, 0x0E0F1011); break;
  case 0xF:  MAKE_VECTOR(0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10); break;
  }

#undef MAKE_VECTOR

  VRi(vd) = vector;

#ifdef VXU_LOAD_DEBUG
  LOG_DEBUG(Xenon, "lvsr: VrD {:#d} = [{:#x},{:#x},{:#x},{:#x}]", vrd, VRi(vd).dword[0],
    VRi(vd).dword[1], VRi(vd).dword[2], VRi(vd).dword[3]);
#endif // VXU_LOAD_DEBUG
}
