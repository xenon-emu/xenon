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

CompilerPass::CompilerPass() :
  compiler(nullptr)
{}

CompilerPass::~CompilerPass() = default;

bool CompilerPass::Initialize(Compiler *comp) {
  compiler = comp;
  return true;
}

Arena *CompilerPass::ScratchArena() const {
  return compiler->ScratchArena();
}

}  // namespace Xe::XCPU::Compiler