/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

// Load Byte and Zero (x'8800 0000')
void PPCInterpreter::PPCInterpreterJIT_lbz(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data8);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  // Zero-extend the loaded byte into the 64-bit GPR and store.
  COMP->movzx(data64, data8);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

void PPCInterpreter::PPCInterpreterJIT_lha(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_DForm_Halfword(b, instr);
  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movsx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
}

void PPCInterpreter::PPCInterpreterJIT_lhau(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s64>(instr.simm16));

  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movsx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
  COMP->mov(GPRPtr(instr.ra), EA);
}

void PPCInterpreter::PPCInterpreterJIT_lhaux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_XForm_Halfword(b, instr, false);
  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movsx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
  COMP->mov(GPRPtr(instr.ra), EA);
}

void PPCInterpreter::PPCInterpreterJIT_lhax(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_XForm_Halfword(b, instr, true);
  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movsx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
}

void PPCInterpreter::PPCInterpreterJIT_lhbrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_XForm_Halfword(b, instr, true);
  x86::Gp value16 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  COMP->ror(value16.r16(), 8);

  x86::Gp result32 = newGP32();
  COMP->movzx(result32, value16.r16());
  COMP->mov(GPRPtr(instr.rd), result32.r64());
}

void PPCInterpreter::PPCInterpreterJIT_lhz(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_DForm_Halfword(b, instr);
  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movzx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
}

void PPCInterpreter::PPCInterpreterJIT_lhzu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s64>(instr.simm16));

  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movzx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
  COMP->mov(GPRPtr(instr.ra), EA);
}

void PPCInterpreter::PPCInterpreterJIT_lhzux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_XForm_Halfword(b, instr, false);
  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movzx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
  COMP->mov(GPRPtr(instr.ra), EA);
}

void PPCInterpreter::PPCInterpreterJIT_lhzx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_XForm_Halfword(b, instr, true);
  x86::Gp value32 = EmitMMURead16(b, EA);
  EmitDataExceptionEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movzx(result, value32.r16());
  COMP->mov(GPRPtr(instr.rd), result);
}

void PPCInterpreter::PPCInterpreterJIT_lwa(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitDSFormEA(b, instr);
  x86::Gp value32 = EmitMMURead32(b, EA);

  EmitDataAccessEarlyExit(b);

  x86::Gp result = newGP64();
  COMP->movsxd(result, value32);
  COMP->mov(GPRPtr(instr.rd), result);
}

// Load Byte and Zero with Update (x'8C00 0000')
void PPCInterpreter::PPCInterpreterJIT_lbzu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data8);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  // Zero-extend the loaded byte into the 64-bit GPR and store.
  COMP->movzx(data64, data8);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Load Byte and Zero with Update Indexed (x'7C00 00EE')
void PPCInterpreter::PPCInterpreterJIT_lbzux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data8);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  // Zero-extend the loaded byte into the 64-bit GPR and store.
  COMP->movzx(data64, data8);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Load Byte and Zero Indexed (x'7C00 00AE')
void PPCInterpreter::PPCInterpreterJIT_lbzx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState*, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data8);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  // Zero-extend the loaded byte into the 64-bit GPR and store.
  COMP->movzx(data64, data8);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

// Load Word and Zero (x'8000 0000')
void PPCInterpreter::PPCInterpreterJIT_lwz(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64.r32());
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

// Load Word and Zero with Update (x'8400 0000')
void PPCInterpreter::PPCInterpreterJIT_lwzu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64.r32());
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Load Word and Zero with Update Indexed (x'7C00 006E')
void PPCInterpreter::PPCInterpreterJIT_lwzux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64.r32());
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Load Word and Zero Indexed (x'7C00 002E')
void PPCInterpreter::PPCInterpreterJIT_lwzx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64.r32());
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

// Load Word Byte-Reverse Indexed (x'7C00 042C')
void PPCInterpreter::PPCInterpreterJIT_lwbrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64.r32());
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->bswap(data64.r32());
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

// Load Double Word (x'E800 0000')
void PPCInterpreter::PPCInterpreterJIT_ld(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead64), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

// Load Double Word with Update (x'E800 0001')
void PPCInterpreter::PPCInterpreterJIT_ldu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)MMURead64), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Load Double Word with Update Indexed (x'7C00 006A')
void PPCInterpreter::PPCInterpreterJIT_ldux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode* read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void*)MMURead64), FuncSignature::build<u64, sPPEState*, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Load Double Word Indexed (x'7C00 002A')
void PPCInterpreter::PPCInterpreterJIT_ldx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode* read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void*)MMURead64), FuncSignature::build<u64, sPPEState*, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, data64);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

void PPCInterpreter::PPCInterpreterJIT_ldbrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  /*
    if rA = 0 then b <- 0
    else b <- (rA)
    EA <- b + (rB)
    rD <- byte_reverse(MEM(EA, 8))
  */

  // EA = (rA ? rA + rB : rB)
  x86::Gp EA = newGP64();
  if (instr.ra) {
    COMP->mov(EA, GPRPtr(instr.ra));
    COMP->add(EA, GPRPtr(instr.rb));
  } else {
    COMP->mov(EA, GPRPtr(instr.rb));
  }

  // data = MMURead64(...)
  x86::Gp data = EmitMMURead64(b, EA);

  // Bail out if MMU raised a data exception
  EmitDataExceptionEarlyExit(b);

  // Byte-reverse 64-bit
#if defined(ARCH_X86_64)
  // bswap r64 is available on x86-64
  COMP->bswap(data);
#else
  // Fallback
  x86::Gp hi = newGP32();
  x86::Gp lo = newGP32();
  x86::Gp tmp = newGP64();

  COMP->mov(lo, data.r32());
  COMP->mov(hi, x86::dword_ptr(data, 4));
  COMP->bswap(lo);
  COMP->bswap(hi);

  COMP->mov(tmp, hi.r64());
  COMP->shl(tmp, 32);
  COMP->movzx(data, lo);
  COMP->or_(data, tmp);
#endif

  // rD = swapped data
  COMP->mov(GPRPtr(instr.rd), data);
}

void PPCInterpreter::PPCInterpreterJIT_lmw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp idx = newGP32();
  x86::Gp gprBase = newGP64();
  x86::Gp gprPtr = newGP64();
  x86::Gp off = newGP64();
  x86::Gp value32 = newGP32();
  x86::Gp value64 = newGP64();
  x86::Gp exceptReg = newGP32();

  Label loop = newLabel();
  Label done = newLabel();
  Label noEx = newLabel();

  // EA <- (rA ? GPR[ra] : 0) + EXTS(d)
  if (instr.ra) {
    COMP->mov(EA, GPRPtr(instr.ra));
    COMP->add(EA, imm<s64>(instr.simm16));
  } else {
    COMP->mov(EA, imm<s64>(instr.simm16));
  }

  // idx <- rD
  COMP->mov(idx, imm<u32>(instr.rd));

  // gprBase = &GPR[0]
  COMP->lea(gprBase, GPRPtr(0));

  COMP->bind(loop);
  COMP->cmp(idx, imm<u32>(32));
  COMP->jge(done);

  // value32 = MMURead32(ppeState, EA, ePPUThread_None)
  InvokeNode *read = nullptr;
  Xe::JITCompat::Invoke(b->compiler, read, imm((void *)PPCInterpreter::MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(read, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(read, 1, EA);
  Xe::JITCompat::SetArg(read, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(read, 0, value32);

  COMP->movzx(exceptReg, EXPtr());
  COMP->test(exceptReg, imm<u32>(ppuDataSegmentEx | ppuDataStorageEx));
  COMP->jz(noEx);
  COMP->ret();
  COMP->bind(noEx);

  // gprPtr = &GPR[idx]
  COMP->mov(off.r32(), idx.r32());
  if (sizeof(u64) == 8) {
    COMP->shl(off, 3);
  } else {
    COMP->imul(off, imm<int>(sizeof(u64)));
  }
  COMP->mov(gprPtr, gprBase);
  COMP->add(gprPtr, off);

  // GPR[idx] = zero-extended value32
  COMP->mov(value64.r32(), value32.r32()); // zero-extends to 64-bit host reg
  COMP->mov(x86::qword_ptr(gprPtr), value64);

  COMP->add(EA, imm(4));
  COMP->inc(idx);
  COMP->jmp(loop);

  COMP->bind(done);
}

//
// Store
//

// Store Byte (x'9800 0000')
void PPCInterpreter::PPCInterpreterJIT_stb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Read
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r8());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

// Store Byte with Update (x'9C00 0000')
void PPCInterpreter::PPCInterpreterJIT_stbu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r8());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Store Byte with Update Indexed (x'7C00 01EE')
void PPCInterpreter::PPCInterpreterJIT_stbux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r8());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

void PPCInterpreter::PPCInterpreterJIT_sth(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_DForm_Halfword(b, instr);
  x86::Gp rSData = newGP32();

  COMP->mov(rSData, GPRPtr(instr.rs));
  EmitMMUWrite16(b, EA, rSData);
}

void PPCInterpreter::PPCInterpreterJIT_sthu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP32();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s64>(instr.simm16));

  COMP->mov(rSData, GPRPtr(instr.rs));
  EmitMMUWrite16(b, EA, rSData);

  EmitDataExceptionEarlyExit(b);
  COMP->mov(GPRPtr(instr.ra), EA);
}

void PPCInterpreter::PPCInterpreterJIT_sthux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = EmitEA_XForm_Halfword(b, instr, false);
  x86::Gp rSData = newGP32();

  COMP->mov(rSData, GPRPtr(instr.rs));
  EmitMMUWrite16(b, EA, rSData);

  EmitDataExceptionEarlyExit(b);
  COMP->mov(GPRPtr(instr.ra), EA);
}

void PPCInterpreter::PPCInterpreterJIT_stmw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp idx = newGP32();
  x86::Gp gprBase = newGP64();
  x86::Gp gprPtr = newGP64();
  x86::Gp off = newGP64();
  x86::Gp value64 = newGP64();
  x86::Gp exceptReg = newGP32();

  Label loop = newLabel();
  Label done = newLabel();
  Label noEx = newLabel();

  // EA <- (rA ? GPR[ra] : 0) + EXTS(d)
  if (instr.ra) {
    COMP->mov(EA, GPRPtr(instr.ra));
    COMP->add(EA, imm<s64>(instr.simm16));
  } else {
    COMP->mov(EA, imm<s64>(instr.simm16));
  }

  // idx <- rS
  COMP->mov(idx, imm<u32>(instr.rs));

  // gprBase = &GPR[0]
  COMP->lea(gprBase, GPRPtr(0));

  COMP->bind(loop);
  COMP->cmp(idx, imm<u32>(32));
  COMP->jge(done);

  // gprPtr = &GPR[idx]
  COMP->mov(off.r32(), idx.r32());
  if (sizeof(u64) == 8) {
    COMP->shl(off, 3);
  } else {
    COMP->imul(off, imm<int>(sizeof(u64)));
  }
  COMP->mov(gprPtr, gprBase);
  COMP->add(gprPtr, off);

  // value64 = GPR[idx]
  COMP->mov(value64, x86::qword_ptr(gprPtr));

  // MMUWrite32(ppeState, EA, (u32)value64, ePPUThread_None)
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)PPCInterpreter::MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, value64.r32());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);

  // Optional but safer: stop on DSI/segment exception mid-loop.
  COMP->movzx(exceptReg, EXPtr());
  COMP->test(exceptReg, imm<u32>(ppuDataSegmentEx | ppuDataStorageEx));
  COMP->jz(noEx);
  COMP->ret();
  COMP->bind(noEx);

  COMP->add(EA, imm(4));
  COMP->inc(idx);
  COMP->jmp(loop);

  COMP->bind(done);
}

// Store Byte Indexed (x'7C00 01AE')
void PPCInterpreter::PPCInterpreterJIT_stbx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Read
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r8());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

// Store Word (x'9000 0000')
void PPCInterpreter::PPCInterpreterJIT_stw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r32());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

// Store Word Byte - Reverse Indexed(x'7C00 052C')
void PPCInterpreter::PPCInterpreterJIT_stwbrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  COMP->bswap(rSData.r32());
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r32());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

// Store Word with Update (x'9400 0000')
void PPCInterpreter::PPCInterpreterJIT_stwu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r32());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Store Word with Update Indexed (x'7C00 016E')
void PPCInterpreter::PPCInterpreterJIT_stwux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r32());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Store Word Indexed (x'7C00 012E')
void PPCInterpreter::PPCInterpreterJIT_stwx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData.r32());
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

// Store Double Word (x'F800 0000')
void PPCInterpreter::PPCInterpreterJIT_std(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData);
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

// Store Double Word with Update (x'F800 0001')
void PPCInterpreter::PPCInterpreterJIT_stdu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData);
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Store Double Word with Update Indexed (x'7C00 016A')
void PPCInterpreter::PPCInterpreterJIT_stdux(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData);
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
}

// Store Double Word Indexed (x'7C00 012A')
void PPCInterpreter::PPCInterpreterJIT_stdx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  Xe::JITCompat::Invoke(b->compiler, write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(write, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(write, 1, EA);
  Xe::JITCompat::SetArg(write, 2, rSData);
  Xe::JITCompat::SetArg(write, 3, ePPUThread_None);
}

//
// Atomic Reservation Instructions
//

// Load Word And Reserve Indexed (x'7C00 0028')
void PPCInterpreter::PPCInterpreterJIT_lwarx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();

  x86::Gp EA = newGP64();           // Effective address
  x86::Gp hostPtr = newGP64();      // Host memory pointer
  x86::Gp data64 = newGP64();       // Zero-extended result
  x86::Gp exceptReg = newGP16();    // Exception check

  if (instr.ra != 0) {
    COMP->mov(EA, GPRPtr(instr.ra));
  } else {
    COMP->xor_(EA, EA);
  }
  COMP->add(EA, GPRPtr(instr.rb));

  InvokeNode *mmuTranslation = nullptr;
  Xe::JITCompat::Invoke(b->compiler, mmuTranslation, imm((void *)JITTranslateAndGetHostPtr),
               FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(mmuTranslation, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(mmuTranslation, 1, EA);
  Xe::JITCompat::SetArg(mmuTranslation, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(mmuTranslation, 0, hostPtr);

  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(data64.r32(), x86::dword_ptr(hostPtr));
  COMP->mov(b->threadCtx->scalar(&sPPUThread::atomicResHostPtr), hostPtr);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::atomicResExpected), data64);
  COMP->bswap(data64.r32());
  COMP->mov(GPRPtr(instr.rd), data64);

  COMP->bind(endLabel);
}

// Store Word Conditional Indexed (x'7C00 012D')
void PPCInterpreter::PPCInterpreterJIT_stwcx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label successLabel = newLabel();
  Label updateCR = newLabel();
  Label failLabel = newLabel();

  x86::Gp EA = newGP64();           // Effective Address
  x86::Gp hostPtr = newGP64();      // New Host Pointer
  x86::Gp resHostPtr = newGP64();   // Stored Reservation Pointer
  x86::Gp storeValue = newGP32();   // Value to store
  x86::Gp expectedValue = newGP32();// Expected value for CAS
  x86::Gp crValue = newGP32();      // CR0 value
  x86::Gp xerValue = newGP32();     // XER value
  x86::Gp zero64 = newGP64();       // For clearing reservation

  // Calculate EA
  if (instr.ra != 0) {
    COMP->mov(EA, GPRPtr(instr.ra));
  } else {
    COMP->xor_(EA, EA);
  }
  COMP->add(EA, GPRPtr(instr.rb));

  // Translate to Host Pointer
  InvokeNode *mmuTranslation = nullptr;
  Xe::JITCompat::Invoke(b->compiler, mmuTranslation, imm((void *)JITTranslateAndGetHostPtr),
               FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(mmuTranslation, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(mmuTranslation, 1, EA);
  Xe::JITCompat::SetArg(mmuTranslation, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(mmuTranslation, 0, hostPtr);

  // Check for DSI/ISI
  x86::Gp exceptReg = newGP16();
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC)); // DStor | DSeg
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(failLabel);

  // Load reservation pointer
  COMP->mov(resHostPtr, b->threadCtx->scalar(&sPPUThread::atomicResHostPtr));

  // Verify address matches reservation
  COMP->cmp(hostPtr, resHostPtr);
  COMP->jne(failLabel);

  // Address matches, attempt CAS
  COMP->mov(expectedValue, b->threadCtx->scalar(&sPPUThread::atomicResExpected));
  COMP->mov(storeValue, GPRPtr(instr.rs));
  COMP->bswap(storeValue.r32()); // Convert to LE for host memory

  COMP->lock();
  COMP->cmpxchg(x86::dword_ptr(hostPtr), storeValue.r32(), expectedValue.r32());
  COMP->jnz(failLabel); // CAS failed (ZF=0)

  // Success
  COMP->mov(crValue, imm(2)); // EQ bit set
  COMP->jmp(successLabel);

  // Fail (Address mismatch or CAS failure)
  COMP->bind(failLabel);
  COMP->xor_(crValue, crValue); // EQ bit clear

  COMP->bind(successLabel);

  // Clear reservation (consume it)
  COMP->xor_(zero64, zero64);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::atomicResHostPtr), zero64);

  COMP->bind(updateCR);

  // SO bit (summary overflow)
#ifdef __LITTLE_ENDIAN__
  COMP->mov(xerValue.r32(), SPRPtr(XER));
  COMP->shr(xerValue.r32(), imm(31));
#else
  COMP->mov(xerValue.r32(), SPRPtr(XER));
  COMP->and_(xerValue.r32(), imm(1));
#endif
  COMP->shl(xerValue, imm(3 - CR_BIT_SO));
  COMP->or_(crValue, xerValue);

  // Set CR0 field
  J_SetCRField(b, crValue, 0);
}

// Load Double Word And Reserve Indexed (x'7C00 00A8')
void PPCInterpreter::PPCInterpreterJIT_ldarx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = newLabel();

  x86::Gp EA = newGP64();           // Effective address
  x86::Gp hostPtr = newGP64();      // Host memory pointer
  x86::Gp data64 = newGP64();       // Loaded data
  x86::Gp exceptReg = newGP16();    // Exception check

  // Step 1: Calculate EA = (rA|0) + rB
  if (instr.ra != 0) {
    COMP->mov(EA, GPRPtr(instr.ra));
  } else {
    COMP->xor_(EA, EA);
  }
  COMP->add(EA, GPRPtr(instr.rb));

  InvokeNode *mmuTranslation = nullptr;
  Xe::JITCompat::Invoke(b->compiler, mmuTranslation, imm((void *)JITTranslateAndGetHostPtr),
               FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(mmuTranslation, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(mmuTranslation, 1, EA);
  Xe::JITCompat::SetArg(mmuTranslation, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(mmuTranslation, 0, hostPtr);

  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(data64, x86::qword_ptr(hostPtr));
  COMP->mov(b->threadCtx->scalar(&sPPUThread::atomicResHostPtr), hostPtr);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::atomicResExpected), data64);
  COMP->bswap(data64);
  COMP->mov(GPRPtr(instr.rd), data64);

  COMP->bind(endLabel);
}

// Store Double Word Conditional Indexed (x'7C00 01AD')
void PPCInterpreter::PPCInterpreterJIT_stdcx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label successLabel = newLabel();
  Label updateCR = newLabel();
  Label failLabel = newLabel();

  x86::Gp EA = newGP64();           // Effective Address
  x86::Gp hostPtr = newGP64();      // New Host Pointer
  x86::Gp resHostPtr = newGP64();   // Stored Reservation Pointer
  x86::Gp storeValue = newGP64();   // Value to store
  x86::Gp expectedValue = newGP64();// Expected value for CAS
  x86::Gp crValue = newGP32();      // CR0 value
  x86::Gp xerValue = newGP32();     // XER value
  x86::Gp zero64 = newGP64();       // For clearing reservation

  // Calculate EA
  if (instr.ra != 0) {
    COMP->mov(EA, GPRPtr(instr.ra));
  } else {
    COMP->xor_(EA, EA);
  }
  COMP->add(EA, GPRPtr(instr.rb));

  // Translate to Host Pointer
  InvokeNode *mmuTranslation = nullptr;
  Xe::JITCompat::Invoke(b->compiler, mmuTranslation, imm((void *)JITTranslateAndGetHostPtr),
               FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  Xe::JITCompat::SetArg(mmuTranslation, 0, b->ppeState->Base());
  Xe::JITCompat::SetArg(mmuTranslation, 1, EA);
  Xe::JITCompat::SetArg(mmuTranslation, 2, ePPUThread_None);
  Xe::JITCompat::SetRet(mmuTranslation, 0, hostPtr);

  // Check for DSI/ISI
  x86::Gp exceptReg = newGP16();
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC)); // DStor | DSeg
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(failLabel);

  // Load reservation pointer
  COMP->mov(resHostPtr, b->threadCtx->scalar(&sPPUThread::atomicResHostPtr));

  // Verify address matches reservation
  COMP->cmp(hostPtr, resHostPtr);
  COMP->jne(failLabel);

  // Address matches, attempt CAS
  COMP->mov(expectedValue, b->threadCtx->scalar(&sPPUThread::atomicResExpected));
  COMP->mov(storeValue, GPRPtr(instr.rs));
  COMP->bswap(storeValue); // Convert to LE for host memory

  COMP->lock();
  COMP->cmpxchg(x86::qword_ptr(hostPtr), storeValue, expectedValue);
  COMP->jnz(failLabel); // CAS failed (ZF=0)

  // Success
  COMP->mov(crValue, imm(2)); // EQ bit set
  COMP->jmp(successLabel);

  // Fail
  COMP->bind(failLabel);
  COMP->xor_(crValue, crValue); // EQ bit clear

  COMP->bind(successLabel);

  // Clear reservation (consume it)
  COMP->xor_(zero64, zero64);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::atomicResHostPtr), zero64);

  COMP->bind(updateCR);

  // SO bit (summary overflow)
#ifdef __LITTLE_ENDIAN__
  COMP->mov(xerValue.r32(), SPRPtr(XER));
  COMP->shr(xerValue.r32(), imm(31));
#else
  COMP->mov(xerValue.r32(), SPRPtr(XER));
  COMP->and_(xerValue.r32(), imm(1));
#endif
  COMP->shl(xerValue, imm(3 - CR_BIT_SO));
  COMP->or_(crValue, xerValue);

  // Set CR0 field
  J_SetCRField(b, crValue, 0);
}

#endif