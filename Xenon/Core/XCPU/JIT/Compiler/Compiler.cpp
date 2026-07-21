/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Core/XCPU/JIT/Compiler/Compiler.h"
#include "Core/XCPU/JIT/Compiler/CompilerPass.h"

namespace Xe::XCPU::Compiler {

Compiler::Compiler() {}

Compiler::~Compiler() { Reset(); }

void Compiler::AddPass(std::unique_ptr<CompilerPass> pass) {
  pass->Initialize(this);
  passes.push_back(std::move(pass));
}

void Compiler::Reset() {}

bool Compiler::Compile(HIR::HIRBuilder *builder) {
  // TODO(benvanik): sophisticated stuff. Run passes in parallel, run until they
  //                 stop changing things, etc.
  for (size_t i = 0; i < passes.size(); ++i) {
    auto &pass = passes[i];
    scratchArena.Reset();
    if (!pass->Run(builder)) {
      return false;
    }
  }

  return true;
}

}  // namespace Xe::XCPU::Compiler