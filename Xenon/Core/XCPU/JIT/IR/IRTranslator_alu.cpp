/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/IR/IRBuilder.h"
#include "Core/XCPU/JIT/IR/IRTranslatorOpcodes.h"

// Context Builder
#define B ctx.builder.get()

namespace Xe::XCPU::JIT {

  bool IRTranslate_addx(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {

    auto *result = B->Add(ctx.LoadGPR(instr.ra), ctx.LoadGPR(instr.rb));
    ctx.StoreGPR(instr.rd, result);

    // Handle OE bit (overflow enable)
    if (instr.oe) {
      // TODO: Implement overflow detection and XER[OV] update
      B->CreateComment("TODO: OE bit - overflow detection");
    }

    // Handle Rc bit (record CR0)
    if (instr.rc) {
      translator.UpdateCR0(ctx, result);
    }

    return true;
  }

}