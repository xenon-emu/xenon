/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Core/XCPU/JIT/Compiler/Passes/ContextPromotionPass.h"
#include "Core/XCPU/JIT/Compiler/Compiler.h"
#include "Core/XCPU/PPU/PowerPC.h"

namespace Xe::XCPU::Compiler::Passes {

// TODO(benvanik): remove when enums redefined.
using namespace HIR;

using HIR::HIRBlock;
using HIR::HIRBuilder;
using HIR::Instr;
using HIR::Value;

ContextPromotionPass::ContextPromotionPass() : CompilerPass() {}

ContextPromotionPass::~ContextPromotionPass() {}

bool ContextPromotionPass::Initialize(Compiler *compiler) {
  if (!CompilerPass::Initialize(compiler)) {
    return false;
  }

  // This is a terrible implementation.
  contextValues.resize(sizeof(sPPUThread));
  context_validity_.resize(static_cast<uint32_t>(sizeof(sPPUThread)));

  return true;
}

bool ContextPromotionPass::Run(HIRBuilder *builder) {
  // Like mem2reg, but because context memory is unaliasable it's easier to
  // check and convert LoadContext/StoreContext into value operations.
  // Example of load->value promotion:
  //   v0 = load_context +100
  //   store_context +200, v0
  //   v1 = load_context +100  <-- replace with v1 = v0
  //   store_context +200, v1
  //
  // It'd be possible in this stage to also remove redundant context stores:
  // Example of dead store elimination:
  //   store_context +100, v0  <-- removed due to following store
  //   store_context +100, v1
  // This is more generally done by DSE, however if it could be done here
  // instead as it may be faster (at least on the block-level).

  // Promote loads to values.
  // Process each block independently, for now.
  auto block = builder->getCurrentBlock();
  while (block) {
    PromoteBlock(block);
    block = NULL; //No next block
  }

  // Remove all dead stores.
  // This will break debugging as we can't recover this information when
  // trying to extract stack traces/register values, so we don't do that.
  if (false) { //(!cvars::debug && !cvars::store_all_context_values) {
    block = builder->getCurrentBlock();
    while (block) {
      RemoveDeadStoresBlock(block);
      block = NULL; //No next block
    }
  }

  return true;
}

void ContextPromotionPass::PromoteBlock(HIRBlock *block) {
  auto &validity = context_validity_;
  std::fill(validity.begin(), validity.end(), 0);

  Instr *i = block->instrHead;
  while (i) {
    auto next = i->next;

    if (i->opcode->flags & OPCODE_FLAG_VOLATILE) {
      std::fill(validity.begin(), validity.end(), 0);
    } else if (i->opcode == &OPCODE_LOAD_CONTEXT_info) {
      size_t offset = i->src1.offset;

      if (offset < validity.size() && validity[offset]) {
        Value *previous_value = contextValues[offset];
        i->opcode = &HIR::OPCODE_ASSIGN_info;
        i->set_src1(previous_value);
      } else if (offset < validity.size()) {
        contextValues[offset] = i->dest;
        validity[offset] = 1;
      }
    } else if (i->opcode == &OPCODE_STORE_CONTEXT_info) {
      size_t offset = i->src1.offset;

      if (offset < validity.size()) {
        contextValues[offset] = i->src2.value;
        validity[offset] = 1;
      }
    }

    i = next;
  }
}

void ContextPromotionPass::RemoveDeadStoresBlock(HIRBlock *block) {
  auto &validity = context_validity_;
  std::fill(validity.begin(), validity.end(), 0);

  Instr *i = block->instrTail;
  while (i) {
    Instr *prev = i->prev;

    if (i->opcode->flags & (OPCODE_FLAG_VOLATILE | OPCODE_FLAG_BRANCH)) {
      std::fill(validity.begin(), validity.end(), 0);
    } else if (i->opcode == &OPCODE_STORE_CONTEXT_info) {
      size_t offset = i->src1.offset;

      if (offset < validity.size()) {
        if (!validity[offset]) {
          validity[offset] = 1;
        } else {
          i->Remove();
        }
      }
    }

    i = prev;
  }
}

} // namespace Xe::XCPU::Compiler::Passes