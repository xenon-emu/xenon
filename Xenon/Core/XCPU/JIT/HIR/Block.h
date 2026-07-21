/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include "Core/XCPU/JIT/HIR/Arena.h"

namespace llvm {
  class BitVector;
}  // namespace llvm

namespace Xe::XCPU::HIR {
class HIRBuilder;
class Instr;
class Label;

class HIRBlock {
public:
  Arena *arena;

  llvm::BitVector *incomingValues;

  Label *labelHead;
  Label *labelTail;

  Instr *instrHead;
  Instr *instrTail;

  u16 ordinal;

  void AssertNoCycles();
};

} // namespace Xe::XCPU::HIR
