/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

// Load Byte and Zero (x'8800 0000')
void PPCInterpreter::PPCInterpreterJIT_lbz(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data8);
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

// Load Byte and Zero with Update (x'8C00 0000')
void PPCInterpreter::PPCInterpreterJIT_lbzu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data8);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data8);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data8 = newGP8();    // byte return from MMURead8
  x86::Gp data64 = newGP64();  // zero-extended result
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); } 
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead8), FuncSignature::build<u8, sPPEState*, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data8);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data64.r32());
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data64.r32());
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data64.r32());
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead32), FuncSignature::build<u32, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data64.r32());
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->bind(endLabel);
}

// Load Double Word (x'E800 0000')
void PPCInterpreter::PPCInterpreterJIT_ld(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead64), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data64);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp data64 = newGP64();
  x86::Gp exceptReg = newGP16();

  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  // Invoke the MMU Read
  InvokeNode *read = nullptr;
  COMP->invoke(&read, imm((void *)MMURead64), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  read->setArg(0, b->ppeState->Base());
  read->setArg(1, EA);
  read->setArg(2, ePPUThread_None);
  read->setRet(0, data64);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.rd), data64);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
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
  COMP->invoke(&write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r8());
  write->setArg(3, ePPUThread_None);
}

// Store Byte with Update (x'9C00 0000')
void PPCInterpreter::PPCInterpreterJIT_stbu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  COMP->invoke(&write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r8());
  write->setArg(3, ePPUThread_None);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  COMP->invoke(&write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r8());
  write->setArg(3, ePPUThread_None);
  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);
  COMP->mov(GPRPtr(instr.ra), EA);
  COMP->bind(endLabel);
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
  COMP->invoke(&write, imm((void *)MMUWrite8), FuncSignature::build<void, sPPEState *, u64, u8, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r8());
  write->setArg(3, ePPUThread_None);
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
  COMP->invoke(&write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r32());
  write->setArg(3, ePPUThread_None);
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
  COMP->invoke(&write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r32());
  write->setArg(3, ePPUThread_None);
}

// Store Word with Update (x'9400 0000')
void PPCInterpreter::PPCInterpreterJIT_stwu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  COMP->invoke(&write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r32());
  write->setArg(3, ePPUThread_None);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  COMP->invoke(&write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r32());
  write->setArg(3, ePPUThread_None);
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
  COMP->invoke(&write, imm((void *)MMUWrite32), FuncSignature::build<void, sPPEState *, u64, u32, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData.r32());
  write->setArg(3, ePPUThread_None);
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
  COMP->invoke(&write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData);
  write->setArg(3, ePPUThread_None);
}

// Store Double Word with Update (x'F800 0001')
void PPCInterpreter::PPCInterpreterJIT_stdu(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, imm<s16>(instr.simm16 & ~3));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  COMP->invoke(&write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData);
  write->setArg(3, ePPUThread_None);
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
  Label endLabel = COMP->newLabel();
  x86::Gp EA = newGP64();
  x86::Gp rSData = newGP64();
  x86::Gp exceptReg = newGP16();
  COMP->mov(EA, GPRPtr(instr.ra));
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(rSData, GPRPtr(instr.rs));
  // Invoke the MMU Write
  InvokeNode *write = nullptr;
  COMP->invoke(&write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData);
  write->setArg(3, ePPUThread_None);
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
  COMP->invoke(&write, imm((void *)MMUWrite64), FuncSignature::build<void, sPPEState *, u64, u64, ePPUThreadID>());
  write->setArg(0, b->ppeState->Base());
  write->setArg(1, EA);
  write->setArg(2, rSData);
  write->setArg(3, ePPUThread_None);
}

#endif