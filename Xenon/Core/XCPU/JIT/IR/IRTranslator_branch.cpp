/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/IR/IRBuilder.h"
#include "Core/XCPU/JIT/IR/IRTranslatorOpcodes.h"

namespace Xe::XCPU::JIT {

  bool IRTranslate_bclr(PPCTranslator &translator, TranslationContext &ctx, uPPCInstr instr) {
    auto *builder = ctx.builder.get();

    const u32 bo = instr.bo;
    const u32 bi = instr.bi;
    const bool lk = instr.lk;

    // Create basic blocks for control flow
    auto *currentBlock = builder->GetInsertBlock();
    auto *takeBranchBlock = builder->CreateBasicBlock("bclr_take");
    auto *fallThroughBlock = builder->CreateBasicBlock("bclr_fallthrough");
    auto *decrementCtrBlock = (bo & 0x4) == 0 ? builder->CreateBasicBlock("bclr_dec_ctr") : nullptr;
    auto *checkCondBlock = builder->CreateBasicBlock("bclr_check_cond");

    // Step 1: Decrement CTR if BO[2] == 0
    if ((bo & 0x4) == 0) {
      builder->SetInsertPoint(currentBlock);
      builder->CreateBranch(decrementCtrBlock);

      builder->SetInsertPoint(decrementCtrBlock);

      // Load CTR, decrement, store back
      auto *ctrVal = ctx.LoadCTR();
      if (!ctrVal) {
        auto *ctrReg = builder->CreateRegister(IR::IRRegisterType::SPR, static_cast<u32>(eXenonSPR::CTR), IR::IRType::INT64);
        ctrVal = builder->LoadReg(ctrReg);
      }

      auto *one = builder->LoadConstInt64(1);
      auto *newCtr = builder->Sub(ctrVal, one);

      // Store CTR and update SSA
      ctx.StoreCTR(newCtr);

      builder->CreateBranch(checkCondBlock);
    }
    else {
      builder->SetInsertPoint(currentBlock);
      builder->CreateBranch(checkCondBlock);
    }

    // Step 2: Check CTR condition (ctrOk)
    // ctrOk = (BO[2] != 0) | ((CTR != 0) ^ BO[1])
    builder->SetInsertPoint(checkCondBlock);

    IR::IRValue *ctrCondition = nullptr;
    if ((bo & 0x4) == 0) {
      // Need to check CTR
      auto *ctrVal = ctx.LoadCTR();
      if (!ctrVal) {
        auto *ctrReg = builder->CreateRegister(IR::IRRegisterType::SPR, static_cast<u32>(eXenonSPR::CTR), IR::IRType::INT64);
        ctrVal = builder->LoadReg(ctrReg);
      }

      auto *zero = builder->LoadConstInt64(0);
      auto *ctrNotZero = builder->CmpNE(ctrVal, zero);

      if (bo & 0x2) {
        // BO[1] == 1: branch if CTR == 0 (invert ctrNotZero)
        auto *ctrIsZero = builder->CmpEQ(ctrVal, zero);
        ctrCondition = ctrIsZero;
      } else {
        // BO[1] == 0: branch if CTR != 0
        ctrCondition = ctrNotZero;
      }
    } else {
      // BO[2] == 1: CTR check always passes
      ctrCondition = builder->LoadConstInt8(1);
    }

    // Step 3: Check CR condition (condOk)
    // condOk = (BO[4] != 0) | (CR[BI] == BO[3])
    IR::IRValue *crCondition = nullptr;
    if ((bo & 0x10) == 0) {
      // Need to check CR[BI]
      auto *crFieldIdx = builder->LoadConstInt32(bi / 4);
      auto *crBitIdx = builder->LoadConstInt32(bi % 4);
      auto *crBit = builder->CreateCRGetBit(crFieldIdx, crBitIdx);

      if (bo & 0x8) {
        // BO[3] == 1: branch if CR bit is set
        auto *one = builder->LoadConstInt8(1);
        crCondition = builder->CmpEQ(crBit, one);
      } else {
        // BO[3] == 0: branch if CR bit is clear
        auto *zero = builder->LoadConstInt8(0);
        crCondition = builder->CmpEQ(crBit, zero);
      }
    } else {
      // BO[4] == 1: CR check always passes
      crCondition = builder->LoadConstInt8(1);
    }

    // Step 4: Combine conditions
    auto *finalCondition = builder->And(
      builder->CreateZExt(ctrCondition, IR::IRType::INT64),
      builder->CreateZExt(crCondition, IR::IRType::INT64)
    );

    auto *shouldBranch = builder->CmpNE(finalCondition, builder->LoadConstInt64(0));

    // Step 5: Create conditional branch
    builder->CreateBranchCond(shouldBranch, takeBranchBlock, fallThroughBlock);

    // Step 6: Take branch block - set NIA to LR & ~3, optionally update LR
    builder->SetInsertPoint(takeBranchBlock);

    // Load LR value
    auto *lrVal = ctx.LoadLR();
    if (!lrVal) {
      auto *lrReg = builder->CreateRegister(IR::IRRegisterType::SPR, static_cast<u32>(eXenonSPR::LR), IR::IRType::INT64);
      lrVal = builder->LoadReg(lrReg);
    }

    // Mask off low 2 bits: LR & ~3
    auto *mask = builder->LoadConstInt64(~3ULL);
    auto *targetAddr = builder->And(lrVal, mask);

    // Store as next instruction address (metadata for backend)
    targetAddr->SetMetadata("branch_target", "lr");

    // If LK bit is set, save return address (CIA + 4) to LR
    if (lk) {
      auto *returnAddr = builder->LoadConstInt64(ctx.currentAddress + 4);
      ctx.StoreLR(returnAddr);
    }

    // Step 7: Fall-through block - continue to next instruction
    builder->SetInsertPoint(fallThroughBlock);

    // Create return instruction (block terminator)
    builder->CreateReturn();

    // Mark block as terminated
    ctx.blockTerminated = true;

    return true;
  }

} // namespace Xe::XCPU::JIT