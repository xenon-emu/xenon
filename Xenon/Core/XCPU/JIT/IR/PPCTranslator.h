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
    IR::IRValue *xerValue = nullptr;          // Fixed-Point Exception Register
    IR::IRValue *lrValue = nullptr;           // Link Register
    IR::IRValue *ctrValue = nullptr;          // Count Register
    IR::IRValue *cfarValue = nullptr;         // Used in Linux, unknown definition atm.
    IR::IRValue *dsisrValue = nullptr;        // Data Storage Interrupt Status Register
    IR::IRValue *darValue = nullptr;          // Data Address Register
    IR::IRValue *decValue = nullptr;          // Decrementer Register
    IR::IRValue *srr0Value = nullptr;         // Machine Status Save/Restore Register 0
    IR::IRValue *srr1Value = nullptr;         // Machine Status Save/Restore Register 1
    IR::IRValue *accrValue = nullptr;         // Address Compare Control Register
    IR::IRValue *vrsaveValue = nullptr;       // VXU Register Save
    IR::IRValue *sprg0Value = nullptr;        // Software Use Special Purpose Register 0
    IR::IRValue *sprg1Value = nullptr;        // Software Use Special Purpose Register 1
    IR::IRValue *sprg2Value = nullptr;        // Software Use Special Purpose Register 2
    IR::IRValue *sprg3Value = nullptr;        // Software Use Special Purpose Register 3
    IR::IRValue *hsprg0Value = nullptr;       // Hypervisor Software Use Special Purpose Register 0
    IR::IRValue *hsprg1Value = nullptr;       // Hypervisor Software Use Special Purpose Register 1
    IR::IRValue *hsrr0Value = nullptr;        // Hypervisor Save Restore Register 0
    IR::IRValue *hsrr1Value = nullptr;        // Hypervisor Save Restore Register 1
    IR::IRValue *tsrlValue = nullptr;         // Thread Status Register Local
    IR::IRValue *tsrrValue = nullptr;         // Thread Status Register Remote
    IR::IRValue *PPE_TLB_Index_HintValue = nullptr;   // PPE Translation Lookaside Buffer Index Hint Register
    IR::IRValue *dabrValue = nullptr;         // Data Address Beakpoint Register
    IR::IRValue *dabrxValue = nullptr;        // Data Address Beakpoint Register Extension
    IR::IRValue *msrValue = nullptr;          // Machine State Register
    IR::IRValue *pirValue = nullptr;          // Processor Identification Register

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

    //
    // Load/Stores
    //

    /// Registers

    // GPR's

    // Get the current value of a GPR (loads from register if not already in SSA map)
    IR::IRValue *LoadGPR(u32 index) {
      auto it = gprValues.find(index);
      if (it != gprValues.end()) {
        return it->second;
      }

      // First access - load from register using high-level IR op
      auto *value = builder->LoadGPR(index);
      gprValues[index] = value;
      return value;
    }

    // Emit a store to a GPR and update SSA mapping
    void StoreGPR(u32 index, IR::IRValue *value) {
      gprValues[index] = value;
      builder->StoreGPR(index, value);
    }

    // FPR's

    // Get the current value of an FPR
    IR::IRValue *LoadFPR(u32 index) {
      auto it = fprValues.find(index);
      if (it != fprValues.end()) {
        return it->second;
      }

      // First access - load from register using high-level IR op
      auto *value = builder->LoadFPR(index);
      fprValues[index] = value;
      return value;
    }

    // Emit a store to an FPR and update SSA mapping
    void StoreFPR(u32 index, IR::IRValue *value) {
      fprValues[index] = value;
      builder->StoreFPR(index, value);
    }

    // VR's

    // Get the current value of a VR
    IR::IRValue *LoadVR(u32 index) {
      auto it = vrValues.find(index);
      if (it != vrValues.end()) {
        return it->second;
      }

      // First access - load from register using high-level IR op
      auto *value = builder->LoadVR(index);
      vrValues[index] = value;
      return value;
    }

    // Emit a store to a VR and update SSA mapping
    void StoreVR(u32 index, IR::IRValue *value) {
      vrValues[index] = value;
      builder->StoreVR(index, value);
    }

    // SPR's

    // XER

    // Get the current value of XER
    IR::IRValue *LoadXER() {
      if (xerValue) return xerValue;
      xerValue = builder->LoadSPR(IR::IRRegisterType::XER, IR::IRType::INT32);
      return xerValue;
    }

    // Emit a store to XER and update SSA mapping
    void StoreXER(IR::IRValue *value) {
      xerValue = value;
      builder->StoreSPR(IR::IRRegisterType::XER, value);
    }

    /// LR

    // Get the current value of LR
    IR::IRValue *LoadLR() {
      if (lrValue) return lrValue;
      lrValue = builder->LoadSPR(IR::IRRegisterType::LR);
      return lrValue;
    }

    // Emit a store to LR and update SSA mapping
    void StoreLR(IR::IRValue *value) {
      lrValue = value;
      builder->StoreSPR(IR::IRRegisterType::LR, value);
    }

    /// CTR

    // Get the current value of CTR
    IR::IRValue *LoadCTR() {
      if (ctrValue) return ctrValue;
      ctrValue = builder->LoadSPR(IR::IRRegisterType::CTR);
      return ctrValue;
    }

    // Emit a store to CTR and update SSA mapping
    void StoreCTR(IR::IRValue *value) {
      ctrValue = value;
      builder->StoreSPR(IR::IRRegisterType::CTR, value);
    }

    /// CFAR

    // Get the current value of CFAR
    IR::IRValue *LoadCFAR() {
      if (cfarValue) return cfarValue;
      cfarValue = builder->LoadSPR(IR::IRRegisterType::CFAR);
      return cfarValue;
    }

    // Emit a store to CFAR and update SSA mapping
    void StoreCFAR(IR::IRValue *value) {
      cfarValue = value;
      builder->StoreSPR(IR::IRRegisterType::CFAR, value);
    }

    /// DSISR

    // Get the current value of DSISR
    IR::IRValue *LoadDSISR() {
      if (dsisrValue) return dsisrValue;
      dsisrValue = builder->LoadSPR(IR::IRRegisterType::DSISR, IR::IRType::INT32);
      return dsisrValue;
    }

    // Emit a store to DSISR and update SSA mapping
    void StoreDSISR(IR::IRValue *value) {
      dsisrValue = value;
      builder->StoreSPR(IR::IRRegisterType::DSISR, value);
    }

    /// DAR

    // Get the current value of DAR
    IR::IRValue *LoadDAR() {
      if (darValue) return darValue;
      darValue = builder->LoadSPR(IR::IRRegisterType::DAR);
      return darValue;
    }

    // Emit a store to DAR and update SSA mapping
    void StoreDAR(IR::IRValue *value) {
      darValue = value;
      builder->StoreSPR(IR::IRRegisterType::DAR, value);
    }

    /// DEC

    // Get the current value of DEC
    IR::IRValue *LoadDEC() {
      if (decValue) return decValue;
      decValue = builder->LoadSPR(IR::IRRegisterType::DEC, IR::IRType::INT32);
      return decValue;
    }

    // Emit a store to DEC and update SSA mapping
    void StoreDEC(IR::IRValue *value) {
      decValue = value;
      builder->StoreSPR(IR::IRRegisterType::DEC, value);
    }

    /// SRR0

    // Get the current value of SRR0
    IR::IRValue *LoadSRR0() {
      if (srr0Value) return srr0Value;
      srr0Value = builder->LoadSPR(IR::IRRegisterType::SRR0);
      return srr0Value;
    }

    // Emit a store to SRR0 and update SSA mapping
    void StoreSRR0(IR::IRValue *value) {
      srr0Value = value;
      builder->StoreSPR(IR::IRRegisterType::SRR0, value);
    }

    /// SRR1

    // Get the current value of SRR1
    IR::IRValue *LoadSRR1() {
      if (srr1Value) return srr1Value;
      srr1Value = builder->LoadSPR(IR::IRRegisterType::SRR1);
      return srr1Value;
    }

    // Emit a store to SRR1 and update SSA mapping
    void StoreSRR1(IR::IRValue *value) {
      srr1Value = value;
      builder->StoreSPR(IR::IRRegisterType::SRR1, value);
    }

    /// ACCR

    // Get the current value of ACCR
    IR::IRValue *LoadACCR() {
      if (accrValue) return accrValue;
      accrValue = builder->LoadSPR(IR::IRRegisterType::ACCR);
      return accrValue;
    }

    // Emit a store to ACCR and update SSA mapping
    void StoreACCR(IR::IRValue *value) {
      accrValue = value;
      builder->StoreSPR(IR::IRRegisterType::ACCR, value);
    }

    /// VRSAVE

    // Get the current value of VRSAVE
    IR::IRValue *LoadVRSAVE() {
      if (vrsaveValue) return vrsaveValue;
      vrsaveValue = builder->LoadSPR(IR::IRRegisterType::VRSAVE, IR::IRType::INT32);
      return vrsaveValue;
    }

    // Emit a store to VRSAVE and update SSA mapping
    void StoreVRSAVE(IR::IRValue *value) {
      vrsaveValue = value;
      builder->StoreSPR(IR::IRRegisterType::VRSAVE, value);
    }

    /// SPRG0

    // Get the current value of SPRG0
    IR::IRValue *LoadSPRG0() {
      if (sprg0Value) return sprg0Value;
      sprg0Value = builder->LoadSPR(IR::IRRegisterType::SPRG0);
      return sprg0Value;
    }

    // Emit a store to SPRG0 and update SSA mapping
    void StoreSPRG0(IR::IRValue *value) {
      sprg0Value = value;
      builder->StoreSPR(IR::IRRegisterType::SPRG0, value);
    }

    /// SPRG1

    // Get the current value of SPRG1
    IR::IRValue *LoadSPRG1() {
      if (sprg1Value) return sprg1Value;
      sprg1Value = builder->LoadSPR(IR::IRRegisterType::SPRG1);
      return sprg1Value;
    }

    // Emit a store to SPRG1 and update SSA mapping
    void StoreSPRG1(IR::IRValue *value) {
      sprg1Value = value;
      builder->StoreSPR(IR::IRRegisterType::SPRG1, value);
    }

    /// SPRG2

    // Get the current value of SPRG2
    IR::IRValue *LoadSPRG2() {
      if (sprg2Value) return sprg2Value;
      sprg2Value = builder->LoadSPR(IR::IRRegisterType::SPRG2);
      return sprg2Value;
    }

    // Emit a store to SPRG2 and update SSA mapping
    void StoreSPRG2(IR::IRValue *value) {
      sprg2Value = value;
      builder->StoreSPR(IR::IRRegisterType::SPRG2, value);
    }

    /// SPRG3

    // Get the current value of SPRG3
    IR::IRValue *LoadSPRG3() {
      if (sprg3Value) return sprg3Value;
      sprg3Value = builder->LoadSPR(IR::IRRegisterType::SPRG3);
      return sprg3Value;
    }

    // Emit a store to SPRG3 and update SSA mapping
    void StoreSPRG3(IR::IRValue *value) {
      sprg3Value = value;
      builder->StoreSPR(IR::IRRegisterType::SPRG3, value);
    }

    /// HSPRG0

    // Get the current value of HSPRG0
    IR::IRValue *LoadHSPRG0() {
      if (hsprg0Value) return hsprg0Value;
      hsprg0Value = builder->LoadSPR(IR::IRRegisterType::HSPRG0);
      return hsprg0Value;
    }

    // Emit a store to HSPRG0 and update SSA mapping
    void StoreHSPRG0(IR::IRValue *value) {
      hsprg0Value = value;
      builder->StoreSPR(IR::IRRegisterType::HSPRG0, value);
    }

    /// HSPRG1

    // Get the current value of HSPRG1
    IR::IRValue *LoadHSPRG1() {
      if (hsprg1Value) return hsprg1Value;
      hsprg1Value = builder->LoadSPR(IR::IRRegisterType::HSPRG1);
      return hsprg1Value;
    }

    // Emit a store to HSPRG1 and update SSA mapping
    void StoreHSPRG1(IR::IRValue *value) {
      hsprg1Value = value;
      builder->StoreSPR(IR::IRRegisterType::HSPRG1, value);
    }

    /// HSRR0

    // Get the current value of HSRR0
    IR::IRValue *LoadHSRR0() {
      if (hsrr0Value) return hsrr0Value;
      hsrr0Value = builder->LoadSPR(IR::IRRegisterType::HSRR0);
      return hsrr0Value;
    }

    // Emit a store to HSRR0 and update SSA mapping
    void StoreHSRR0(IR::IRValue *value) {
      hsrr0Value = value;
      builder->StoreSPR(IR::IRRegisterType::HSRR0, value);
    }

    /// HSRR1

    // Get the current value of HSRR1
    IR::IRValue *LoadHSRR1() {
      if (hsrr1Value) return hsrr1Value;
      hsrr1Value = builder->LoadSPR(IR::IRRegisterType::HSRR1);
      return hsrr1Value;
    }

    // Emit a store to HSRR1 and update SSA mapping
    void StoreHSRR1(IR::IRValue *value) {
      hsrr1Value = value;
      builder->StoreSPR(IR::IRRegisterType::HSRR1, value);
    }

    /// TSRL

    // Get the current value of TSRL
    IR::IRValue *LoadTSRL() {
      if (tsrlValue) return tsrlValue;
      tsrlValue = builder->LoadSPR(IR::IRRegisterType::TSRL);
      return tsrlValue;
    }

    // Emit a store to TSRL and update SSA mapping
    void StoreTSRL(IR::IRValue *value) {
      tsrlValue = value;
      builder->StoreSPR(IR::IRRegisterType::TSRL, value);
    }

    /// TSRR

        // Get the current value of TSRR
    IR::IRValue *LoadTSRR() {
      if (tsrrValue) return tsrrValue;
      tsrrValue = builder->LoadSPR(IR::IRRegisterType::TSRR);
      return tsrrValue;
    }

    // Emit a store to TSRR and update SSA mapping
    void StoreTSRR(IR::IRValue *value) {
      tsrrValue = value;
      builder->StoreSPR(IR::IRRegisterType::TSRR, value);
    }

    /// PPE_TLB_Index_Hint

    // Get the current value of PPE_TLB_Index_Hint
    IR::IRValue *LoadPPE_TLB_Index_Hint() {
      if (PPE_TLB_Index_HintValue) return PPE_TLB_Index_HintValue;
      PPE_TLB_Index_HintValue = builder->LoadSPR(IR::IRRegisterType::PPE_TLB_Index_Hint);
      return PPE_TLB_Index_HintValue;
    }

    // Emit a store to PPE_TLB_Index_Hint and update SSA mapping
    void StorePPE_TLB_Index_Hint(IR::IRValue *value) {
      PPE_TLB_Index_HintValue = value;
      builder->StoreSPR(IR::IRRegisterType::PPE_TLB_Index_Hint, value);
    }

    /// DABR

      // Get the current value of DABR
    IR::IRValue *LoadDABR() {
      if (dabrValue) return dabrValue;
      dabrValue = builder->LoadSPR(IR::IRRegisterType::DABR);
      return dabrValue;
    }

    // Emit a store to DABR and update SSA mapping
    void StoreDABR(IR::IRValue *value) {
      dabrValue = value;
      builder->StoreSPR(IR::IRRegisterType::DABR, value);
    }

    /// DABRX

    // Get the current value of DABRX
    IR::IRValue *LoadDABRX() {
      if (dabrxValue) return dabrxValue;
      dabrxValue = builder->LoadSPR(IR::IRRegisterType::DABRX);
      return dabrxValue;
    }

    // Emit a store to DABRX and update SSA mapping
    void StoreDABRX(IR::IRValue *value) {
      dabrxValue = value;
      builder->StoreSPR(IR::IRRegisterType::DABRX, value);
    }

    /// MSR

    // Get the current value of MSR
    IR::IRValue *LoadMSR() {
      if (msrValue) return msrValue;
      msrValue = builder->LoadSPR(IR::IRRegisterType::MSR);
      return msrValue;
    }

    // Emit a store to MSR and update SSA mapping
    void StoreMSR(IR::IRValue *value) {
      msrValue = value;
      builder->StoreSPR(IR::IRRegisterType::MSR, value);
    }

    /// PIR

    // Get the current value of PIR
    IR::IRValue *LoadPIR() {
      if (pirValue) return pirValue;
      pirValue = builder->LoadSPR(IR::IRRegisterType::PIR, IR::IRType::INT32);
      return pirValue;
    }

    // Emit a store to PIR and update SSA mapping
    void StorePIR(IR::IRValue *value) {
      pirValue = value;
      builder->StoreSPR(IR::IRRegisterType::PIR, value);
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
