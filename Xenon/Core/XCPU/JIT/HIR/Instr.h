/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/JIT/HIR/Value.h"
#include "Core/XCPU/JIT/HIR/Opcodes.h"

namespace Xe::XCPU::HIR {

class HIRBlock;
class Label;

// HIR Instruction
class Instr {
public:
  // HIR Block this instruction is linked to
  HIRBlock *block;
  // Next instruction in the block
  Instr *next;
  // Previous instruction in the block
  Instr *prev;

  // Opcode info pointer (opcode + signature/flags)
  const OpcodeInfo *opcode;
  u16 flags = 0;
  u32 ordinal;

  // Current instruction data (for branch/system instructions)
  uPPCInstr currentInstrData = { 0 };

  typedef union {
    Label *label;
    Value *value;
    u64 offset;
  } Op;

  Value *dest;
  Op src1;
  Op src2;
  Op src3;

  Value::Use *src1_use;
  Value::Use *src2_use;
  Value::Use *src3_use;

  void set_src1(Value *value);
  void set_src2(Value *value);
  void set_src3(Value *value);

  void MoveBefore(Instr *other);
  void Replace(const OpcodeInfo *newOpcode, u16 newFlags);
  void Remove();
};

}  // namespace Xe::XCPU::HIR
