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

class ConstantPropagationPass : public ConditionalGroupSubpass {
public:
  ConstantPropagationPass();
  ~ConstantPropagationPass() override;

  bool Run(HIR::HIRBuilder *builder, bool &result) override;
};

}  // namespace Xe::XCPU::Compiler::Passes
