/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/IR/IRBuilder.h"
#include "Core/XCPU/JIT/IR/IRTranslatorOpcodes.h"

namespace Xe::XCPU::JIT {

  bool IRTranslate_bclr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {
    // Mark block as terminated
    ctx.blockTerminated = true;

    return true;
  }

} // namespace Xe::XCPU::JIT