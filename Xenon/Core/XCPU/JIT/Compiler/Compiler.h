/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#pragma once

#include <memory>
#include <vector>

#include "Core/XCPU/JIT/HIR/Arena.h"
#include "Core/XCPU/JIT/HIR/HIRBuilder.h"

namespace Xe::XCPU::Compiler {

class CompilerPass;

class Compiler {
public:
  explicit Compiler();
  ~Compiler();

  Arena *ScratchArena() { return &scratchArena; }

  void AddPass(std::unique_ptr<CompilerPass> pass);

  void Reset();

  bool Compile(HIR::HIRBuilder *builder);

private:
  Arena scratchArena;

  std::vector<std::unique_ptr<CompilerPass>> passes;
};

}  // namespace Xe::XCPU::Compiler
