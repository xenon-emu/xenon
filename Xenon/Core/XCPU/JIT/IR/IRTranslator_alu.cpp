/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/IR/IRBuilder.h"
#include "Core/XCPU/JIT/IR/IRTranslatorOpcodes.h"
#include "Core/XCPU/PPU/PowerPC.h"

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
      //translator.UpdateCR0(ctx, result);
    }

    return true;
  }

  bool IRTranslate_mfspr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {
    /*
      Move From Special Purpose Register
      n <- spr[5-9] || spr[0-4]
      if length(SPR(n)) = 64 then
      rD <- SPR(n)
      else
        rD <- (32)0 || SPR(n)
  */

  // Decode SPR number (swap the two 5-bit fields)
    u32 sprNum = instr.spr;
    sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);

    IR::IRValue *result = nullptr;

    // Map SPR number to the appropriate special register
    switch (static_cast<eXenonSPR>(sprNum)) {
    case eXenonSPR::XER:
      result = ctx.LoadXER();
      break;
    case eXenonSPR::LR:
      result = ctx.LoadLR();
      break;
    case eXenonSPR::CTR:
      result = ctx.LoadCTR();
      break;
    case eXenonSPR::CFAR:
      result = ctx.LoadCFAR();
      break;
      // Add more SPR cases as needed
    default:
      B->CreateComment("MFSPR: Unimplemented SPR " + std::to_string(sprNum));
      result = B->LoadConstS64(0);
      break;
    }

    ctx.StoreGPR(instr.rs, result);
    return true;
  }

  bool IRTranslate_mtspr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {
    /*
   Move To Special Purpose Register
      n <- spr[5-9] || spr[0-4]
  SPR(n) <- (rS)
    */

    // Decode SPR number (swap the two 5-bit fields)
    u32 sprNum = instr.spr;
    sprNum = ((sprNum & 0x1F) << 5) | ((sprNum >> 5) & 0x1F);

    auto *rSVal = ctx.LoadGPR(instr.rd);

    // Map SPR number to the appropriate special register
    switch (static_cast<eXenonSPR>(sprNum)) {
    case eXenonSPR::XER:
      ctx.StoreXER(rSVal);
      break;
    case eXenonSPR::LR:
      ctx.StoreLR(rSVal);
      break;
    case eXenonSPR::CTR:
      ctx.StoreCTR(rSVal);
      break;
    case eXenonSPR::CFAR:
      ctx.StoreCFAR(rSVal);
      break;
      // Add more SPR cases as needed
    default:
      B->CreateComment("MTSPR: Unimplemented SPR " + std::to_string(sprNum));
      break;
    }

    return true;
  }

}