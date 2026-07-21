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


#ifdef _MSVC_VER_
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#include <llvm/ADT/BitVector.h>
#pragma warning(pop)
#else
//#include <llvm/ADT/BitVector.h>
#endif  // XE_COMPILER_MSVC

namespace Xe::XCPU::Compiler::Passes {

class ContextPromotionPass : public CompilerPass {
public:
  ContextPromotionPass();
  virtual ~ContextPromotionPass() override;

  bool Initialize(Compiler *compiler) override;

  bool Run(HIR::HIRBuilder *builder) override;

private:
  void PromoteBlock(HIR::HIRBlock *block);
  void RemoveDeadStoresBlock(HIR::HIRBlock *block);

private:
  std::vector<HIR::Value *> contextValues;
  std::vector<u8> context_validity_;
};

} // namespace Xe::XCPU::Compiler::Passes