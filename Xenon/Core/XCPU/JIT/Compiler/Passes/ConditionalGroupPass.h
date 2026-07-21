/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include <cmath>
#include <vector>

#include "Core/XCPU/JIT/Compiler/CompilerPass.h"
#include "Core/XCPU/JIT/Compiler/Passes/ConditionalGroupSubpass.h"

namespace Xe::XCPU::Compiler::Passes {

class ConditionalGroupPass : public CompilerPass {
public:
  ConditionalGroupPass();
  virtual ~ConditionalGroupPass() override;

  bool Initialize(Compiler *compiler) override;

  bool Run(HIR::HIRBuilder *builder) override;

  void AddPass(std::unique_ptr<CompilerPass> pass);

private:
  std::vector<std::unique_ptr<CompilerPass>> passes;
};

} // namespace Xe::XCPU::Compiler::Passes
