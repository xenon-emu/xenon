/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/JIT/IR/IRTypes.h"
#include "Core/XCPU/JIT/IR/IRBuilder.h"

#include "Core/XCPU/PPU/PowerPC.h"

#include <memory>
#include <unordered_map>
#include <format>

//=============================================================================
// PPC to IR Frontend
// ----------------------------------------------------------------------------
// Translates PowerPC instructions to IR for JIT compilation
// Maintains SSA form and handles PPC-specific semantics
//=============================================================================

namespace Xe::XCPU::JIT {

  // Forward declarations
  class PPCTranslator;
  struct TranslationContext;

  //=============================================================================
  // Translation Context
  //=============================================================================

  /// Holds state during translation of a PPC block to IR
  struct TranslationContext {
    // Current PPU state
    sPPEState *ppeState = nullptr;

    // Current function being built
    IR::IRFunction *function = nullptr;

    // IR builder
    std::unique_ptr<IR::IRBuilder> builder;

    // Current PPC address being translated
    u64 currentAddress = 0;

    // Entry basic block
    IR::IRBasicBlock *entryBlock = nullptr;

    // Map of PPC addresses to basic blocks (for branch targets)
    std::unordered_map<u64, IR::IRBasicBlock *> addressToBlock;

    // Map of PPC registers to their current SSA values
    // This maintains the SSA form - each write creates a new value
    std::unordered_map<u32, IR::IRValue *> gprValues;       // GPR r0-r31
    std::unordered_map<u32, IR::IRValue *> fprValues;       // FPR f0-f31
    std::unordered_map<u32, IR::IRValue *> vrValues;        // VR v0-v127

    // Special register values
    // TODO: Fill the missing regs
    IR::IRValue *lrValue = nullptr; // Link Register
    IR::IRValue *ctrValue = nullptr;   // Count Register
    IR::IRValue *xer = nullptr;   // XER
    IR::IRValue *crValue = nullptr;    // Condition Register
    IR::IRValue *fpscr = nullptr; // FPSCR

    // Block has been terminated (branch/return)
    bool blockTerminated = false;

    TranslationContext(sPPEState *state, IR::IRFunction *func)
      : ppeState(state), function(func) {
      builder = std::make_unique<IR::IRBuilder>(func);
    }

    // Get or create a basic block for a PPC address
    IR::IRBasicBlock *GetOrCreateBlock(u64 address) {
      auto it = addressToBlock.find(address);
      if (it != addressToBlock.end()) {
        return it->second;
      }

      std::string blockName = std::format("block_{:X}", address);
      auto *block = builder->CreateBasicBlock(blockName);
      addressToBlock[address] = block;
      return block;
    }

    // Get the current value of a GPR (loads from register if not already in SSA map)
    IR::IRValue *LoadGPR(u32 index) {
      auto it = gprValues.find(index);
      if (it != gprValues.end()) {
        return it->second;
      }

      // First access - load from register
      auto *reg = builder->CreateGPR(index);
      auto *value = builder->LoadReg(reg);
      gprValues[index] = value;
      return value;
    }

    // Emit a store to a GPR and update SSA mapping
    void StoreGPR(u32 index, IR::IRValue *value) {
      gprValues[index] = value;
      auto *reg = builder->CreateGPR(index);
      builder->StoreReg(reg, value);
    }

    // Get the current value of an FPR
    IR::IRValue *LoadFPR(u32 index) {
      auto it = fprValues.find(index);
      if (it != fprValues.end()) {
        return it->second;
      }

      // First access - load from register
      auto *reg = builder->CreateFPR(index);
      auto *value = builder->LoadReg(reg);
      fprValues[index] = value;
      return value;
    }

    // Emit a store to an FPR and update SSA mapping
    void StoreFPR(u32 index, IR::IRValue *value) {
      fprValues[index] = value;
      auto *reg = builder->CreateFPR(index);
      builder->StoreReg(reg, value);
    }

    /// Get the current value of a VR
    IR::IRValue *LoadVR(u32 index) {
      auto it = vrValues.find(index);
      if (it != vrValues.end()) {
        return it->second;
      }

      // First access - load from register
      auto *reg = builder->CreateVR(index);
      auto *value = builder->LoadReg(reg);
      vrValues[index] = value;
      return value;
    }

    // Emit a store to a VR and update SSA mapping
    void StoreVR(u32 index, IR::IRValue *value) {
      vrValues[index] = value;
      auto *reg = builder->CreateVR(index);
      builder->StoreReg(reg, value);
    }

    // Get the current value of LR
    IR::IRValue *LoadLR() {
      if (lrValue) return lrValue;
      auto *reg = builder->CreateRegister(IR::IRRegisterType::LR, 0, IR::IRType::INT64);
      lrValue = builder->LoadReg(reg);
      return lrValue;
    }

    // Emit a store to LR and update SSA mapping
    void StoreLR(IR::IRValue *value) {
      lrValue = value;
      auto *reg = builder->CreateRegister(IR::IRRegisterType::LR, 0, IR::IRType::INT64);
      builder->StoreReg(reg, value);
    }

    // Get the current value of CTR
    IR::IRValue *LoadCTR() {
      if (ctrValue) return ctrValue;
      auto *reg = builder->CreateRegister(IR::IRRegisterType::CTR, 0, IR::IRType::INT64);
      ctrValue = builder->LoadReg(reg);
      return ctrValue;
    }

    // Emit a store to CTR and update SSA mapping
    void StoreCTR(IR::IRValue *value) {
      ctrValue = value;
      auto *reg = builder->CreateRegister(IR::IRRegisterType::CTR, 0, IR::IRType::INT64);
      builder->StoreReg(reg, value);
    }
  };

  //=============================================================================
  // PPC Translator
  //=============================================================================

  // Main translator class for converting PPC to IR
  class PPCTranslator {
  public:
    PPCTranslator() = default;
    ~PPCTranslator() = default;

    // Translate a PPC block to IR
    std::unique_ptr<IR::IRFunction> TranslateBlock(sPPEState *ppeState, u64 startAddress, u32 maxInstructions = 1000);

    // Update condition register 0 based on value
    void UpdateCR0(TranslationContext &ctx, IR::IRValue *value);

  private:
    // Translate a single PPC instruction to IR
    bool TranslateInstruction(TranslationContext &ctx, uPPCInstr instruction, u64 address);

    // Helper functions
  // ----------------

  // Compute effective address EA for load/store

    // EA = value(rA) + value(rB)
    IR::IRValue *ComputeEAIndexed(TranslationContext &ctx, u32 rA, u32 rB);
    // EA = rA ? value(rA) : 0 + rB
    IR::IRValue *ComputeEA_0_Indexed(TranslationContext &ctx, u32 rA, u32 rB);
    // EA = value(rA) + IMM
    IR::IRValue *ComputeEAImmediate(TranslationContext &ctx, u32 rA, u64 imm);
    // EA = rA ? value(rA) : 0 + IMM
    IR::IRValue *ComputeEA_0_Immediate(TranslationContext &ctx, u32 rA, u64 imm);

    // Sign extend a value
    IR::IRValue *SignExtend(TranslationContext &ctx, IR::IRValue *value, IR::IRType targetType);

    /// Zero extend a value
    IR::IRValue *ZeroExtend(TranslationContext &ctx, IR::IRValue *value, IR::IRType targetType);
  };

} // namespace Xe::XCPU::JIT
