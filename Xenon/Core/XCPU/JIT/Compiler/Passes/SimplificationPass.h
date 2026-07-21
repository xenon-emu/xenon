/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include "Core/XCPU/JIT/Compiler/Passes/ConditionalGroupSubpass.h"

namespace Xe::XCPU::Compiler::Passes {

class SimplificationPass : public ConditionalGroupSubpass {
public:
  SimplificationPass();
  ~SimplificationPass() override;

  bool Run(HIR::HIRBuilder *builder, bool &result) override;

private:
  bool EliminateConversions(HIR::HIRBuilder *builder);
  bool CheckTruncate(HIR::Instr *i);
  bool CheckByteSwap(HIR::Instr *i);

  bool SimplifyAssignments(HIR::HIRBuilder *builder);
  HIR::Value *CheckValue(HIR::Value *value, bool &result);
};

}  // namespace Xe::XCPU::Compiler::Passes
