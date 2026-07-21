/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

 // Modified for use on the Xenon Emulator.

#include "Base/Assert.h"
#include "Core/XCPU/JIT/HIR/HIREmitters/HIREmitters.h"

namespace Xe::XCPU::HIR {

//
// Helpers
//

// Effective Address [EA] calculations

// Apply alignment: Aplies alignment for EA calculations on SIMM values for instructions that require it.

// Form: instr.ra ? GPR[rA] : 0 + SIMM
Value *GetEA_rA_0_SIMM(HIRBuilder &b, const uPPCInstr &instr, bool applyAlignment = false) {
  if (instr.ra == 0) { return b.LoadConstantInt64(SignExtend16(applyAlignment ? (instr.simm16 & ~3) : (instr.simm16))); }
  else { return b.Add(b.LoadGPR(instr.ra), b.LoadConstantInt64(SignExtend16(applyAlignment ? (instr.simm16 & ~3) : (instr.simm16)))); }
}

// Form: GPR[rA] + SIMM
Value *GetEA_rA_SIMM(HIRBuilder &b, const uPPCInstr &instr, bool applyAlignment = false) {
  return b.Add(b.LoadGPR(instr.ra), b.LoadConstantInt64(SignExtend16(applyAlignment ? (instr.simm16 & ~3) : (instr.simm16))));
}

// Form: GPR[rA] + GPR[rB]
Value *GetEA_rA_rB(HIRBuilder &b, const uPPCInstr &instr) {
  return b.Add(b.LoadGPR(instr.ra), b.LoadGPR(instr.rb));
}

// Form: instr.ra ? GPR[rA] : 0 + GPR[rB]
Value *GetEA_rA_0_rB(HIRBuilder &b, const uPPCInstr &instr) {
  // instr.RA ? GPR[rA] : 0
  if (instr.ra == 0) { return b.LoadGPR(instr.rb); }
  else { return b.Add(b.LoadGPR(instr.ra), b.LoadGPR(instr.rb)); }
}

//
// Integer Loads
//

// Byte

// Load Byte and Zero (x'8800 0000')
s32 HIRInstrEmit_lbz(HIRBuilder &b, const uPPCInstr &instr) {
  // Perform the load
  Value *RD = b.ZeroExtend(b.Load(GetEA_rA_0_SIMM(b, instr), INT8_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, RD);
  return 0;
}

// Load Byte and Zero with Update (x'8C00 0000')
s32 HIRInstrEmit_lbzu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT8_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Byte and Zero with Update Indexed (x'7C00 00EE')
s32 HIRInstrEmit_lbzux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT8_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Byte and Zero Indexed (x'7C00 00AE')
s32 HIRInstrEmit_lbzx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT8_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Halfword

// Load Half Word Algebraic (x'A800 0000')
s32 HIRInstrEmit_lha(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Half Word Algebraic with Update (x'AC00 0000')
s32 HIRInstrEmit_lhau(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Half Word Algebraic with Update Indexed (x'7C00 02EE')
s32 HIRInstrEmit_lhaux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Half Word Algebraic Indexed (x'7C00 02AE')
s32 HIRInstrEmit_lhax(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Half Word and Zero (x'A000 0000')
s32 HIRInstrEmit_lhz(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Half Word and Zero with Update (x'A400 0000')
s32 HIRInstrEmit_lhzu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Half Word and Zero with Update Indexed (x'7C00 026E')
s32 HIRInstrEmit_lhzux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Half Word and Zero Indexed (x'7C00 022E')
s32 HIRInstrEmit_lhzx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT16_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Half Word Byte-Reverse Indexed (x'7C00 062C')
s32 HIRInstrEmit_lhbrx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.ByteSwap(b.Load(EA, INT16_TYPE)), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Word

// Load Word Algebraic (x'E800 0002')
s32 HIRInstrEmit_lwa(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA - Alignment needed
  Value *EA = GetEA_rA_0_SIMM(b, instr, true);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Word Algebraic Indexed (x'7C00 02AA')
s32 HIRInstrEmit_lwax(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Word Algebraic with Update Indexed (x'7C00 02EA')
s32 HIRInstrEmit_lwaux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.SignExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Word and Zero (x'8000 0000')
s32 HIRInstrEmit_lwz(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Word and Zero with Update (x'8400 0000')
s32 HIRInstrEmit_lwzu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Word and Zero with Update Indexed (x'7C00 006E')
s32 HIRInstrEmit_lwzux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Word and Zero Indexed (x'7C00 002E')
s32 HIRInstrEmit_lwzx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Word Byte-Reverse Indexed (x'7C00 042C')
s32 HIRInstrEmit_lwbrx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.ZeroExtend(b.ByteSwap(b.Load(EA, INT32_TYPE)), INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Doubleword

// Load Double Word (x'E800 0000')
s32 HIRInstrEmit_ld(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA - Alignment needed
  Value *EA = GetEA_rA_0_SIMM(b, instr, true);
  // Perform the load
  Value *rD = b.Load(EA, INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Double Word with Update (x'E800 0001')
s32 HIRInstrEmit_ldu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr, true);
  // Perform the load
  Value *rD = b.Load(EA, INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Double Word with Update Indexed (x'7C00 006A')
s32 HIRInstrEmit_ldux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA - Alignment needed
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.Load(EA, INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Double Word Indexed (x'7C00 002A')
s32 HIRInstrEmit_ldx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.Load(EA, INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Double Word Byte Reversed Indexed
s32 HIRInstrEmit_ldbrx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.ByteSwap(b.Load(EA, INT64_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

//
// Integer Stores
//

// Byte

// Store Byte (x'9800 0000')
s32 HIRInstrEmit_stb(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT8_TYPE));
  return 0;
}

// Store Byte with Update (x'9C00 0000')
s32 HIRInstrEmit_stbu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT8_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Byte with Update Indexed (x'7C00 01EE')
s32 HIRInstrEmit_stbux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT8_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Byte Indexed (x'7C00 01AE')
s32 HIRInstrEmit_stbx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT8_TYPE));
  return 0;
}

// Halfword

// Store Half Word (x'B000 0000')
s32 HIRInstrEmit_sth(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT16_TYPE));
  return 0;
}

// Store Half Word with Update (x'B400 0000')
s32 HIRInstrEmit_sthu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT16_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Half Word with Update Indexed (x'7C00 036E')
s32 HIRInstrEmit_sthux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT16_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Half Word Indexed (x'7C00 032E')
s32 HIRInstrEmit_sthx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT16_TYPE));
  return 0;
}

// Store Half Word Byte-Reverse Indexed (x'7C00 072C')
s32 HIRInstrEmit_sthbrx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.ByteSwap(b.Truncate(b.LoadGPR(instr.rd), INT16_TYPE)));
  return 0;
}

// Word

// Store Word (x'9000 0000')
s32 HIRInstrEmit_stw(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE));
  return 0;
}

// Store Multiple Word (x'BC00 0000')
s32 HIRInstrEmit_stmw(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the store
  for (u32 idx = 0; idx < 32 - instr.rs; ++idx) {
    b.Store(EA, b.Truncate(b.LoadGPR(instr.rd + idx), INT32_TYPE));
    EA = b.Add(EA, b.LoadConstantInt64(4));
  }
  return 0;
}

// Store Word with Update (x'9400 0000')
s32 HIRInstrEmit_stwu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Word with Update Indexed (x'7C00 016E')
s32 HIRInstrEmit_stwux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Word Indexed (x'7C00 012E')
s32 HIRInstrEmit_stwx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE));
  return 0;
}

// Store Word Byte-Reverse Indexed (x'7C00 052C')
s32 HIRInstrEmit_stwbrx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.ByteSwap(b.Truncate(b.LoadGPR(instr.rd), INT32_TYPE)));
  return 0;
}

// Doubleword

// Store Double Word (x'F800 0000')
s32 HIRInstrEmit_std(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA - Alignment needed
  Value *EA = GetEA_rA_0_SIMM(b, instr, true);
  // Perform the store
  b.Store(EA, b.LoadGPR(instr.rd));
  return 0;
}

// Store Double Word with Update (x'F800 0001')
s32 HIRInstrEmit_stdu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA - Alignment needed
  Value *EA = GetEA_rA_SIMM(b, instr, true);
  // Perform the store
  b.Store(EA, b.LoadGPR(instr.rd));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Double Word with Update Indexed (x'7C00 016A')
s32 HIRInstrEmit_stdux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the store
  b.Store(EA, b.LoadGPR(instr.rd));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Double Word Indexed (x'7C00 012A')
s32 HIRInstrEmit_stdx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.LoadGPR(instr.rd));
  return 0;
}

// Store Double Word Byte Reversed Indexed
s32 HIRInstrEmit_stdbrx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.ByteSwap(b.LoadGPR(instr.rd)));
  return 0;
}

// Integer load and store multiple (A-16)

// Load Multiple Word (x'B800 0000')
s32 HIRInstrEmit_lmw(HIRBuilder &b, const uPPCInstr &instr) {
  Value *EA = GetEA_rA_0_SIMM(b, instr);

  for (u32 idx = 0; idx < 32 - instr.rd; ++idx) {
    Value *rD = b.ZeroExtend(b.Load(EA, INT32_TYPE), INT64_TYPE);
    EA = b.Add(EA, b.LoadConstantInt64(4));
    b.StoreGPR(instr.rd + idx, rD);
  }
  return 0;
}

// Memory synchronization (A-18)

// Enforce In-Order Execution of I/O (x'7C00 06AC')
s32 HIRInstrEmit_eieio(HIRBuilder &b, const uPPCInstr &instr) {
  b.MemoryBarrier();
  return 0;
}

// Synchronize (x'7C00 04AC')
s32 HIRInstrEmit_sync(HIRBuilder &b, const uPPCInstr &instr) {
  b.MemoryBarrier();
  return 0;
}

// Instruction Synchronize (x'4C00 012C')
s32 HIRInstrEmit_isync(HIRBuilder &b, const uPPCInstr &instr) {
  b.Nop();
  return 0;
}

// Atomic load/store operations
// Done in the backend atm.

// Load Double Word and Reserve Indexed (x'7C00 00A8')
s32 HIRInstrEmit_ldarx(HIRBuilder &b, const uPPCInstr &instr) {
  // Issue memory barrier
  b.MemoryBarrier();
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Call Load reserverd with 64 bit type
  Value *rD = b.Load(EA, HIR::INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store value in our context
  b.StoreReserved(rD);
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Load Word and Reserve Indexed (x'7C00 0028')
s32 HIRInstrEmit_lwarx(HIRBuilder &b, const uPPCInstr &instr) {
  // Issue memory barrier
  b.MemoryBarrier();
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Call Load reserverd with 32 bit type
  Value *rD = b.ZeroExtend(b.Load(EA, HIR::INT32_TYPE), HIR::INT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store value in our context
  b.StoreReserved(rD);
  // Store loaded value
  b.StoreGPR(instr.rd, rD);
  return 0;
}

// Store Double Word Conditional Indexed (x'7C00 01AD')
s32 HIRInstrEmit_stdcx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Get rD
  Value *rD = b.ByteSwap(b.LoadGPR(instr.rd));
  // Get the reserved value
  Value *resValue = b.ByteSwap(b.LoadReserved());
  // Perform the atomic compare exchange
  Value *v = b.AtomicCompareExchange(EA, resValue, rD);
  // Set CR0 based on the result
  b.UpdateCRx(0, b.LoadConstantUint8(1), v, false, false);
  // Issue memory barrier
  b.MemoryBarrier();
  return 0;
}

// Store Word Conditional Indexed (x'7C00 012D')
s32 HIRInstrEmit_stwcx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Get rD
  Value *rD = b.ByteSwap(b.Truncate(b.LoadGPR(instr.rd), HIR::INT32_TYPE));
  // Get the reserved value
  Value *resValue = b.ByteSwap(b.Truncate(b.LoadReserved(), HIR::INT32_TYPE));
  // Perform the atomic compare exchange
  Value *v = b.AtomicCompareExchange(EA, resValue, rD);
  // Set CR0 based on the result
  b.UpdateCRx(0, b.LoadConstantUint8(1), v, false, false);
  // Issue memory barrier
  b.MemoryBarrier();
  return 0;
}

//
// Floating-pos32 Loads
//

// Load Floating-Pos32 Double (x'C800 0000')
s32 HIRInstrEmit_lfd(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the load
  Value *rD = b.Cast(b.Load(EA, INT64_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  return 0;
}

// Load Floating-Pos32 Double with Update (x'CC00 0000')
s32 HIRInstrEmit_lfdu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the load
  Value *rD = b.Cast(b.Load(EA, INT64_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Floating-Pos32 Double with Update Indexed (x'7C00 04EE')
s32 HIRInstrEmit_lfdux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.Cast(b.Load(EA, INT64_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Floating-Pos32 Double Indexed (x'7C00 04AE')
s32 HIRInstrEmit_lfdx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.Cast(b.Load(EA, INT64_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  return 0;
}

// Load Floating-Pos32 Single (x'C000 0000')
s32 HIRInstrEmit_lfs(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the load
  Value *rD = b.Convert(b.Cast(b.Load(EA, INT32_TYPE), FLOAT32_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  return 0;
}

// Load Floating-Pos32 Single with Update (x'C400 0000')
s32 HIRInstrEmit_lfsu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the load
  Value *rD = b.Convert(b.Cast(b.Load(EA, INT32_TYPE), FLOAT32_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Floating-Pos32 Single with Update Indexed (x'7C00 046E')
s32 HIRInstrEmit_lfsux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the load
  Value *rD = b.Convert(b.Cast(b.Load(EA, INT32_TYPE), FLOAT32_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Load Floating-Pos32 Single Indexed (x'7C00 042E')
s32 HIRInstrEmit_lfsx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the load
  Value *rD = b.Convert(b.Cast(b.Load(EA, INT32_TYPE), FLOAT32_TYPE), FLOAT64_TYPE);
  // Emit exception check
  b.StorageExceptionCheck();
  // Store loaded value
  b.StoreFPR(instr.rd, rD);
  return 0;
}

//
// Floating-pos32 Stores
//

// Store Floating-Pos32 Double (x'D800 0000')
s32 HIRInstrEmit_stfd(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.LoadFPR(instr.rd), INT64_TYPE));
  return 0;
}

// Store Floating-Pos32 Double with Update (x'DC00 0000')
s32 HIRInstrEmit_stfdu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.LoadFPR(instr.rd), INT64_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Floating-Pos32 Double with Update Indexed (x'7C00 05EE')
s32 HIRInstrEmit_stfdux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.LoadFPR(instr.rd), INT64_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Floating-Pos32 Double Indexed (x'7C00 05AE')
s32 HIRInstrEmit_stfdx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, (b.Cast(b.LoadFPR(instr.rd), INT64_TYPE)));
  return 0;
}

// Store Floating-Pos32 as Integer Word Indexed (x'7C00 07AE')
s32 HIRInstrEmit_stfiwx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, (b.Truncate(b.Cast(b.LoadFPR(instr.rd), INT64_TYPE),INT32_TYPE)));
  return 0;
}

// Store Floating-Pos32 Single (x'D000 0000')
s32 HIRInstrEmit_stfs(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.Convert(b.LoadFPR(instr.rd), FLOAT32_TYPE), INT32_TYPE));
  return 0;
}

// Store Floating-Pos32 Single with Update (x'D400 0000')
s32 HIRInstrEmit_stfsu(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_SIMM(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.Convert(b.LoadFPR(instr.rd), FLOAT32_TYPE), INT32_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Floating-Pos32 Single with Update Indexed (x'7C00 056E')
s32 HIRInstrEmit_stfsux(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.Convert(b.LoadFPR(instr.rd), FLOAT32_TYPE), INT32_TYPE));
  // Emit exception check
  b.StorageExceptionCheck();
  // Update EA
  b.StoreGPR(instr.ra, EA);
  return 0;
}

// Store Floating-Pos32 Single Indexed (x'7C00 052E')
s32 HIRInstrEmit_stfsx(HIRBuilder &b, const uPPCInstr &instr) {
  // Get EA
  Value *EA = GetEA_rA_0_rB(b, instr);
  // Perform the store
  b.Store(EA, b.Cast(b.Convert(b.LoadFPR(instr.rd), FLOAT32_TYPE), INT32_TYPE));
  return 0;
}

// Cache management (A-27)
// dcbf, dcbst, dcbt, dcbtst work with 128-byte cache lines, not 32-byte cache blocks, on the Xenon:
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/mp/src/mathlib/sseconst.cpp#L321
// https://randomascii.wordpress.com/2018/01/07/finding-a-cpu-design-bug-in-the-xbox-360/

s32 HIRInstrEmit_dcbf(HIRBuilder &b, const uPPCInstr &instr) {
  b.Nop();
  //Value *EA = CalculateEA_0(b, instr.ra, instr.rb);
  //b.CacheControl(EA, 128, CacheControlType::CACHE_CONTROL_TYPE_DATA_STORE_AND_FLUSH);
  return 0;
}

s32 HIRInstrEmit_dcbst(HIRBuilder &b, const uPPCInstr &instr) {
  b.Nop();
  //Value *EA = CalculateEA_0(b, instr.ra, instr.rb);
  //b.CacheControl(EA, 128, CacheControlType::CACHE_CONTROL_TYPE_DATA_STORE);
  return 0;
}

s32 HIRInstrEmit_dcbt(HIRBuilder &b, const uPPCInstr &instr) {
  b.Nop();
  //Value *EA = CalculateEA_0(b, instr.ra, instr.rb);
  //b.CacheControl(EA, 128, CacheControlType::CACHE_CONTROL_TYPE_DATA_TOUCH);
  return 0;
}

s32 HIRInstrEmit_dcbtst(HIRBuilder &b, const uPPCInstr &instr) {
  b.Nop();
  //Value *EA = CalculateEA_0(b, instr.ra, instr.rb);
  //b.CacheControl(EA, 128, CacheControlType::CACHE_CONTROL_TYPE_DATA_TOUCH_FOR_STORE);
  return 0;
}

s32 HIRInstrEmit_dcbz(HIRBuilder &b, const uPPCInstr &instr) {
  // EA <- (RA) + (RB)
  // memset(EA & ~31, 0, 32)
  Value *EA = GetEA_rA_0_rB(b, instr);
  // dcbz - 32 byte set
  s32 block_size = 32;
  s32 address_mask = ~31;
  b.Memset(b.And(EA, b.LoadConstantInt64(address_mask)), b.LoadZeroInt8(), b.LoadConstantInt64(block_size));
  return 0;
}

s32 HIRInstrEmit_dcbz128(HIRBuilder &b, const uPPCInstr &instr) {
  // EA <- (RA) + (RB)
  // memset(EA & ~31, 0, 32)
  Value *EA = GetEA_rA_0_rB(b, instr);
  // dcbz128 - 128 byte set
  s32 block_size = 128;
  s32 address_mask = ~127;
  b.Memset(b.And(EA, b.LoadConstantInt64(address_mask)), b.LoadZeroInt8(), b.LoadConstantInt64(block_size));
  return 0;
}

// Instruction Cache Block Invalidate (x'7C00 07AC')
s32 HIRInstrEmit_icbi(HIRBuilder &b, const uPPCInstr &instr) {
  b.Nop();
  return 0;
}

} // namespace Xe::XCPU::HIR