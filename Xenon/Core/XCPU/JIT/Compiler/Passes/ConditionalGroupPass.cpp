/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Core/XCPU/JIT/Compiler/Passes/ConditionalGroupPass.h"
#include "Core/XCPU/JIT/Compiler/Compiler.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace Xe::XCPU::Compiler::Passes {

// TODO(benvanik): remove when enums redefined.
using namespace HIR;

using HIR::Block;
using HIR::HIRBuilder;
using HIR::Instr;
using HIR::Value;

ConditionalGroupPass::ConditionalGroupPass() : CompilerPass() {}

ConditionalGroupPass::~ConditionalGroupPass() {}

bool ConditionalGroupPass::Initialize(Compiler *compiler) {
  if (!CompilerPass::Initialize(compiler)) {
    return false;
  }

  for (size_t i = 0; i < passes.size(); ++i) {
    auto &pass = passes[i];
    if (!pass->Initialize(compiler)) {
      return false;
    }
  }

  return true;
}

bool ConditionalGroupPass::Run(HIRBuilder *builder) {
  bool dirty;
  int loops = 0;
  do {
    ASSERT(loops < 20);  // arbitrary number
    dirty = false;
    for (size_t i = 0; i < passes.size(); ++i) {
      ScratchArena()->Reset();
      auto &pass = passes[i];
      auto subpass = dynamic_cast<ConditionalGroupSubpass *>(pass.get());
      if (!subpass) {
        if (!pass->Run(builder)) {
          return false;
        }
      }
      else {
        bool result = false;
        if (!subpass->Run(builder, result)) {
          return false;
        }
        dirty |= result;
      }
    }
    loops++;
  } while (dirty);
  return true;
}

void ConditionalGroupPass::AddPass(std::unique_ptr<CompilerPass> pass) {
  passes.push_back(std::move(pass));
}

}  // namespace Xe::XCPU::Compiler::Passes