/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "PPCTranslator.h"

#include "Base/Logging/Log.h"
#include "Core/XCPU/JIT/IR/IRTranslatorDecoder.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

namespace Xe::XCPU::JIT {

  //=============================================================================
  // Main Translation Entry Point
  //=============================================================================

  std::unique_ptr<IR::IRFunction> PPCTranslator::TranslateBlock(sPPEState *ppeState, u64 startAddress, u32 maxInstructions) {
    // Create IR function
    std::string funcName = "ppc_func_" + std::to_string(startAddress);
    auto function = std::make_unique<IR::IRFunction>(funcName, startAddress);

    // Create translation context
    TranslationContext ctx(ppeState, function.get());

    // Create entry block
    ctx.entryBlock = ctx.builder->CreateBasicBlock("entry");
    ctx.builder->SetInsertPoint(ctx.entryBlock);

    // Add function metadata
    function->SetMetadata("start_address", std::to_string(startAddress));
    function->SetMetadata("type", "ppc_block");

    // Translate instructions
    u64 currentAddr = startAddress;
    u32 instructionCount = 0;

    while (instructionCount < maxInstructions) {
      ctx.currentAddress = currentAddr;
      ctx.builder->SetCurrentSourceLocation(currentAddr);

      // Fetch instruction (this would normally go through MMU)
      u32 instrData = 0; // Simple ADD

      instrData = PPCInterpreter::MMURead32(ppeState, currentAddr);

      uPPCInstr instruction;
      instruction.opcode = instrData;

      // Translate the instruction
      if (!TranslateInstruction(ctx, instruction, currentAddr)) {
        LOG_ERROR(JIT, "Failed to translate instruction at {:#x}", currentAddr);
        return nullptr;
      }

      // Check if block was terminated due to some exception in MMU
      if (ctx.blockTerminated) { break; }

      // Move to next instruction
      currentAddr += 4;
      instructionCount++;
    }

    // If block wasn't terminated, add a return
    if (!ctx.blockTerminated) {
      ctx.builder->CreateReturn();
    }

    // Transfer ownership of all values (constants, registers) to the function
    // This ensures they live as long as the function
    ctx.builder->TransferValuesToFunction();

    return function;
  }

  //=============================================================================
  // Instruction Translation
  //=============================================================================

  bool PPCTranslator::TranslateInstruction(TranslationContext &ctx, uPPCInstr instr, u64 address) {
    // Decode instruction opcode
    u32 opcode = instr.opcode;
    
    // Add debug comment
    std::string comment = "PPC Code @ " + FMT("{:#x}", address) + " InstrData = " + FMT("{:#x}", opcode);
    ctx.builder->CreateComment(comment);

    IRTranslatorHandler handler = irTranslatorDecoder.Decode(opcode);

    // Check if instruction is unimplemented
    if (handler == &IRTranslate_invalid) {
      LOG_ERROR(JIT, "IR Translator: Unimplemented instruction at {:#x}, opcode={:#x}", address, opcode);
      return false;
    }

    // Execute the handler to emit IR
    return handler(*this, ctx, instr);
  }


  //=============================================================================
  // Helper Functions
  //=============================================================================

  // TODO: Move these to IRTranslator_LoadStore.cpp

  IR::IRValue *PPCTranslator::ComputeEAIndexed(TranslationContext &ctx, u32 rA, u32 rB) {
    auto *rAVal = ctx.LoadGPR(rA);
    auto *rBVal = ctx.LoadGPR(rB);
    return ctx.builder->Add(rAVal, rBVal);
  }

  IR::IRValue *PPCTranslator::ComputeEA_0_Indexed(TranslationContext &ctx, u32 rA, u32 rB) {
    auto *rBVal = ctx.LoadGPR(rB);
    // rA = 0?
    if (rA == 0) {
      // EA = rB Value
      return rBVal;
    }

    auto *rAVal = ctx.LoadGPR(rA);
    return ctx.builder->Add(rAVal, rBVal);
  }

  IR::IRValue *PPCTranslator::ComputeEAImmediate(TranslationContext &ctx, u32 rA, u64 imm) {
    auto *rAVal = ctx.LoadGPR(rA);
    auto *immVal = ctx.builder->LoadConstInt64(imm);
    return ctx.builder->Add(rAVal, immVal);
  }


  IR::IRValue *PPCTranslator::ComputeEA_0_Immediate(TranslationContext &ctx, u32 rA, u64 imm) {
    auto *immVal = ctx.builder->LoadConstInt64(imm);
    // rA = 0?
    if (rA == 0) {
      // EA = rB Value
      return immVal;
    }

    auto *rAVal = ctx.LoadGPR(rA);
    return ctx.builder->Add(rAVal, immVal);
  }

  void PPCTranslator::UpdateCR0(TranslationContext &ctx, IR::IRValue *value) {
    // TODO: Implement CR0 update
    // CR0 bits: LT, GT, EQ, SO
    // Compare value with 0 and set CR0 bits accordingly
  }

  IR::IRValue *PPCTranslator::SignExtend(TranslationContext &ctx, IR::IRValue *value, IR::IRType targetType) {
    return ctx.builder->CreateSExt(value, targetType);
  }

  IR::IRValue *PPCTranslator::ZeroExtend(TranslationContext &ctx, IR::IRValue *value, IR::IRType targetType) {
    return ctx.builder->CreateZExt(value, targetType);
  }

} // namespace Xe::XCPU::JIT
