/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "Core/XCPU/JIT/Compiler/Passes/DeadCodeEliminationPass.h"

namespace Xe::XCPU::Compiler::Passes {

// TODO(benvanik): remove when enums redefined.
using namespace HIR;

using HIR::HIRBuilder;
using HIR::Instr;
using HIR::Value;

DeadCodeEliminationPass::DeadCodeEliminationPass() : CompilerPass() {}

DeadCodeEliminationPass::~DeadCodeEliminationPass() {}

bool DeadCodeEliminationPass::Run(HIRBuilder *builder) {
  // ContextPromotion/DSE will likely leave around a lot of dead statements.
  // Code generated for comparison/testing produces many unused statements and
  // with proper use analysis it should be possible to remove most of them:
  // After context promotion/simplification:
  //   v33.i8 = compare_ult v31.i32, 0
  //   v34.i8 = compare_ugt v31.i32, 0
  //   v35.i8 = compare_eq v31.i32, 0
  //   store_context +300, v33.i8
  //   store_context +301, v34.i8
  //   store_context +302, v35.i8
  //   branch_true v35.i8, loc_8201A484
  // After DSE:
  //   v33.i8 = compare_ult v31.i32, 0
  //   v34.i8 = compare_ugt v31.i32, 0
  //   v35.i8 = compare_eq v31.i32, 0
  //   branch_true v35.i8, loc_8201A484
  // After DCE:
  //   v35.i8 = compare_eq v31.i32, 0
  //   branch_true v35.i8, loc_8201A484

  // This also removes useless ASSIGNs:
  //   v1 = v0
  //   v2 = add v1, v1
  // becomes:
  //   v2 = add v0, v0

  // We process DCE by reverse iterating over instructions and looking at the
  // use count of the dest value. If it's zero, we can safely remove the
  // instruction. Once we do that, the use counts of any of the src ops may
  // go to zero and we recursively kill up the graph. This is kind of
  // nasty in that we walk randomly and scribble all over memory, but (I think)
  // it's better than doing passes over all instructions until we quiesce.
  // To prevent our iteration from getting all messed up we just replace
  // all removed ops with NOP and then do a single pass that removes them
  // all.

  bool any_instr_removed = false;
  bool any_locals_removed = false;
  auto block = builder->getCurrentBlock();
  while (block) {
    // Walk instructions in reverse.
    Instr *i = block->instrTail;
    while (i) {
      auto prev = i->prev;

      auto opcode = i->opcode;
      if (!(opcode->flags & OPCODE_FLAG_VOLATILE) && i->dest &&
        !i->dest->use_head) {
        // Has no uses and is not volatile. This instruction can die!
        MakeNopRecursive(i);
        any_instr_removed = true;
      }
      else if (opcode == &OPCODE_ASSIGN_info) {
        // Assignment. These are useless, so just try to remove by completely
        // replacing the value.
        ReplaceAssignment(i);
      }

      i = prev;
    }

    // Walk instructions forward.
    i = block->instrHead;
    while (i) {
      auto next = i->next;

      auto opcode = i->opcode;
      if (opcode == &OPCODE_STORE_LOCAL_info) {
        // Check to see if the store has any interceeding uses after the load.
        // If not, it can be removed (as the local is just passing through the
        // function).
        // We do this after the previous pass so that removed code doesn't keep
        // the local alive.
        if (!CheckLocalUse(i)) {
          any_locals_removed = true;
        }
      }

      i = next;
    }

    block = NULL; // No block
  }

  // Remove all nops.
  if (any_instr_removed) {
    block = builder->getCurrentBlock();
    while (block) {
      Instr *i = block->instrHead;
      while (i) {
        Instr *next = i->next;
        if (i->opcode == &OPCODE_NOP_info) {
          // Nop - remove!
          i->Remove();
        }
        i = next;
      }
      block = NULL; // No block
    }
  }

  return true;
}

void DeadCodeEliminationPass::MakeNopRecursive(Instr *i) {
  i->opcode = &HIR::OPCODE_NOP_info;
  i->dest->def = NULL;
  i->dest = NULL;

#define MAKE_NOP_SRC(n)                                  \
  if (i->src##n##_use) {                                 \
    Value::Use* use = i->src##n##_use;                   \
    Value* value = i->src##n.value;                      \
    i->src##n##_use = NULL;                              \
    i->src##n.value = NULL;                              \
    value->RemoveUse(use);                               \
    if (!value->use_head) {                              \
      /* Value is now unused, so recursively kill it. */ \
      if (value->def && value->def != i) {               \
        MakeNopRecursive(value->def);                    \
      }                                                  \
    }                                                    \
  }
  MAKE_NOP_SRC(1);
  MAKE_NOP_SRC(2);
  MAKE_NOP_SRC(3);
}

void DeadCodeEliminationPass::ReplaceAssignment(Instr *i) {
  auto src = i->src1.value;
  auto dest = i->dest;

  auto use = dest->use_head;
  while (use) {
    auto use_instr = use->instr;
    if (use_instr->src1.value == dest) {
      use_instr->set_src1(src);
    }
    if (use_instr->src2.value == dest) {
      use_instr->set_src2(src);
    }
    if (use_instr->src3.value == dest) {
      use_instr->set_src3(src);
    }
    use = use->next;
  }

  i->Remove();
}

bool DeadCodeEliminationPass::CheckLocalUse(Instr *i) {
  auto src = i->src2.value;

  auto use = src->use_head;
  if (use) {
    auto use_instr = use->instr;
    if (use_instr->opcode != &OPCODE_LOAD_LOCAL_info) {
      // A valid use (probably). Keep it.
      return true;
    }

    // Load/store are paired. They can both be removed.
    use_instr->Remove();
  }

  i->Remove();

  return false;
}

}  // namespace Xe::XCPU::Compiler::Passes