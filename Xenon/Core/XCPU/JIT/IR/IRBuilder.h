/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "IRTypes.h"

#include <memory>
#include <unordered_map>
#include <vector>

//=============================================================================
// IR Builder
// ----------------------------------------------------------------------------
// Provides a convenient interface for constructing IR in SSA form.
// Handles value creation, basic block management, and maintains SSA properties.
//=============================================================================

namespace Xe::XCPU::JIT::IR {

  class IRBuilder {
  public:
    explicit IRBuilder(IRFunction *func) : function(func), currentBlock(nullptr) {}

    // Allocates a new value. Pre initialized to zero.
    IRValue *AllocValue() {
      auto value = std::make_unique<IRValue>();
      auto *ptr = value.get();
      ownedValues.push_back(std::move(value));
      return ptr;
    }

    // Allocates a new value as a copy of another.
    IRValue *Clone(IRValue *newValue) {
      auto value = AllocValue();
      value = newValue;
      return value;
    }

    // Creates a new basic block with the given name
    IRBasicBlock *CreateBasicBlock(const std::string &name) {
      if (!function) return nullptr;
      return function->CreateBasicBlock(name);
    }

    // Sets the current insertion point for new instructions
    void SetInsertPoint(IRBasicBlock *block) { currentBlock = block; }

    // Gets the current insertion point
    IRBasicBlock *GetInsertBlock() const { return currentBlock; }

    //
    // Constant Values Creation
    //

    // Create a zero constant of the given type
    IRValue *LoadZero(IRType type) {
      auto constant = AllocValue();
      constant->SetZero(type);
      return constant;
    }

    /// Integers

    // Create an 8-bit integer constant
    IRValue *LoadConstS8(s8 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create an 8-bit unsigned integer constant
    IRValue *LoadConstU8(u8 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create a 16-bit integer constant
    IRValue *LoadConstS16(s16 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create an 16-bit unsigned integer constant
    IRValue *LoadConstU16(u16 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create a 32-bit integer constant
    IRValue *LoadConstS32(s32 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create an 32-bit unsigned integer constant
    IRValue *LoadConstU32(u32 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create a 64-bit integer constant
    IRValue *LoadConstS64(s64 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create an 64-bit unsigned integer constant
    IRValue *LoadConstU64(u64 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create zero constant Int8
    IRValue *LoadZeroInt8() { return LoadZero(IRType::INT8); }

    // Create zero constant Int16
    IRValue *LoadZeroInt16() { return LoadZero(IRType::INT16); }

    // Create zero constant Int32
    IRValue *LoadZeroInt32() { return LoadZero(IRType::INT32); }

    // Create zero constant Int64
    IRValue *LoadZeroInt64() { return LoadZero(IRType::INT64); }

    /// Floats

    // Create a F32 constant
    IRValue *LoadConstFloat32(f32 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create a F64 constant
    IRValue *LoadConstFloat64(f64 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create zero constant Float 32
    IRValue *LoadZeroFloat32() { return LoadZero(IRType::FLOAT32); }

    // Create zero constant Float 64
    IRValue *LoadZeroFloat64() { return LoadZero(IRType::FLOAT64); }

    /// Vectors

    // Create a Vector128 constant
    IRValue *LoadConstVec128(Base::Vector128 value) {
      auto constant = AllocValue();
      constant->SetConstant(value);
      return constant;
    }

    // Create zero constant Vector 128
    IRValue *LoadZeroVec128() { return LoadZero(IRType::VEC128); }

    //
    // References Creation (SSA values)
    //

    // Create a reference to a PPC register
    IRRegister *CreateRegister(IRRegisterType regType, u32 index, IRType type) {
      auto reg = std::make_unique<IRRegister>(regType, index, type);
      auto *ptr = reg.get();
      ownedValues.push_back(std::move(reg));
      return ptr;
    }

    // Create a reference to a GPR (General Purpose Register)
    IRRegister *CreateGPR(u32 index) {
      return CreateRegister(IRRegisterType::GPR, index, IRType::INT64);
    }

    // Create a reference to an FPR (Floating Point Register)
    IRRegister *CreateFPR(u32 index) {
      return CreateRegister(IRRegisterType::FPR, index, IRType::FLOAT64);
    }

    // Create a reference to a Vector Register
    IRRegister *CreateVR(u32 index) {
      return CreateRegister(IRRegisterType::VR, index, IRType::VEC128);
    }

    // Create a reference to a Special Purpose Register
    IRRegister *CreateSPR(u32 index, IRType type = IRType::INT64) {
      return CreateRegister(static_cast<IRRegisterType>(index), index, type);
    }

    //
    // Registers Load/Store instructions
    //

    // Load from GPR[index]
    IRInstruction *LoadGPR(u32 index) {
      auto *inst = CreateInstruction(IROp::LoadGPR, IRType::INT64);
      inst->AddOperand(CreateGPR(index));
      inst->SetMetadata("reg_type", "GPR");
      inst->SetMetadata("reg_index", std::to_string(index));
      return inst;
    }

    // Store to GPR[index]
    IRInstruction *StoreGPR(u32 index, IRValue *value) {
      auto *inst = CreateInstruction(IROp::StoreGPR, IRType::Void);
      inst->AddOperand(CreateGPR(index));
      inst->AddOperand(value);
      inst->SetMetadata("reg_type", "GPR");
      inst->SetMetadata("reg_index", std::to_string(index));
      return inst;
    }

    // Load from FPR[index]
    IRInstruction *LoadFPR(u32 index) {
      auto *inst = CreateInstruction(IROp::LoadFPR, IRType::FLOAT64);
      inst->AddOperand(CreateFPR(index));
      inst->SetMetadata("reg_type", "FPR");
      inst->SetMetadata("reg_index", std::to_string(index));
      return inst;
    }

    // Store to FPR[index]
    IRInstruction *StoreFPR(u32 index, IRValue *value) {
      auto *inst = CreateInstruction(IROp::StoreFPR, IRType::Void);
      inst->AddOperand(CreateFPR(index));
      inst->AddOperand(value);
      inst->SetMetadata("reg_type", "FPR");
      inst->SetMetadata("reg_index", std::to_string(index));
      return inst;
    }

    // Load from VR[index]
    IRInstruction *LoadVR(u32 index) {
      auto *inst = CreateInstruction(IROp::LoadVR, IRType::VEC128);
      inst->AddOperand(CreateVR(index));
      inst->SetMetadata("reg_type", "VR");
      inst->SetMetadata("reg_index", std::to_string(index));
      return inst;
    }

    // Store to VR[index]
    IRInstruction *StoreVR(u32 index, IRValue *value) {
      auto *inst = CreateInstruction(IROp::StoreVR, IRType::Void);
      inst->AddOperand(CreateVR(index));
      inst->AddOperand(value);
      inst->SetMetadata("reg_type", "VR");
      inst->SetMetadata("reg_index", std::to_string(index));
      return inst;
    }

    // Load from SPR (using IRRegisterType for SPR identification)
    IRInstruction *LoadSPR(IRRegisterType sprType, IRType valueType = IRType::INT64) {
      auto *inst = CreateInstruction(IROp::LoadSPR, valueType);
      inst->AddOperand(CreateSPR(static_cast<u32>(sprType)));
      inst->SetMetadata("spr_name", IRRegisterTypeToString(sprType));
      return inst;
    }

    // Store to SPR (using IRRegisterType for SPR identification)
    IRInstruction *StoreSPR(IRRegisterType sprType, IRValue *value) {
      auto *inst = CreateInstruction(IROp::StoreSPR, IRType::Void);
      inst->AddOperand(LoadConstU32(static_cast<u32>(sprType)));
      inst->AddOperand(value);
      inst->SetMetadata("spr_name", IRRegisterTypeToString(sprType));
      return inst;
    }

    // Convenience methods for common SPRs
    IRInstruction *LoadLR() { return LoadSPR(IRRegisterType::LR); }
    IRInstruction *StoreLR(IRValue *value) { return StoreSPR(IRRegisterType::LR, value); }

    IRInstruction *LoadCTR() { return LoadSPR(IRRegisterType::CTR); }
    IRInstruction *StoreCTR(IRValue *value) { return StoreSPR(IRRegisterType::CTR, value); }

    IRInstruction *LoadXER() { return LoadSPR(IRRegisterType::XER, IRType::INT32); }
    IRInstruction *StoreXER(IRValue *value) { return StoreSPR(IRRegisterType::XER, value); }

    IRInstruction *LoadMSR() { return LoadSPR(IRRegisterType::MSR); }
    IRInstruction *StoreMSR(IRValue *value) { return StoreSPR(IRRegisterType::MSR, value); }

    //
    // IRInstructions Creation
    //

    // Create a return instruction
    IRValue *CreateReturn(IRValue *value = nullptr) {
      auto inst = std::make_unique<IRInstruction>(IROp::Return);
      auto *ptr = inst.get();
      if (currentSourceAddress != 0) { inst->SetSourceLocation(currentSourceAddress); }
      if (value) {
        inst->AddOperand(value);
      }
      if (currentBlock) {
        currentBlock->SetTerminator(std::move(inst));
      }
      return ptr;
    }

    // Load from memory
    IRValue *MemLoad(IRValue *address, IRType type) {
      auto *inst = CreateInstruction(IROp::MemLoad, type);
      inst->AddOperand(address);
      return inst;
    }

    // Store to memory
    IRValue *MemStore(IRValue *address, IRValue *value) {
      auto *inst = CreateInstruction(IROp::MemStore);
      inst->AddOperand(address);
      inst->AddOperand(value);
      return inst;
    }

    // Memset Memory (DCBZ/DCBZ128)
    IRValue *MemSet(IRValue *address, IRValue *value, IRValue *lenght) {
      auto *inst = CreateInstruction(IROp::MemSet);
      inst->AddOperand(address);
      inst->AddOperand(value);
      inst->AddOperand(lenght);
      return inst;
    }

    // Memory Barrier
    IRValue *MemoryBarrier() {
      auto *inst = CreateInstruction(IROp::MemoryBarrier);
      return inst;
    }

    // Set Rounding Mode
    IRValue *SetRoundingMode(IRValue *value) {
      auto *inst = CreateInstruction(IROp::SetRoundingMode);
      inst->AddOperand(value);
      return inst;
    }

    // Max
    IRValue *Max(IRValue *value1, IRValue *value2) {
      auto *inst = CreateInstruction(IROp::Max, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      return inst;
    }

    // Vector Max
    IRValue *VectorMax(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      auto *inst = CreateInstruction(IROp::VectorMax, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      inst->AddOperand(LoadConstU32(static_cast<u32>(typeToCompare)));
      return inst;
    }

    // Min
    IRValue *Min(IRValue *value1, IRValue *value2) {
      auto *inst = CreateInstruction(IROp::Min, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      return inst;
    }

    // Vector Min
    IRValue *VectorMin(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      auto *inst = CreateInstruction(IROp::VectorMin, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      inst->AddOperand(LoadConstU32(static_cast<u32>(typeToCompare)));
      return inst;
    }

    // Select
    IRValue *Select(IRValue *condition, IRValue *value1, IRValue *value2) {
      if (condition->IsConstant()) {
        return condition->IsConstantTrue() ? value1 : value2;
      }
      
      auto *inst = CreateInstruction(IROp::Select, value1->GetType());
      inst->AddOperand(condition);
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      return inst;
    }

    // Is True
    IRValue *IsTrue(IRValue *value) {
      if (value->IsConstant()) {
        return LoadConstU8(value->IsConstantTrue() ? 1 : 0);
      }

      auto *inst = CreateInstruction(IROp::IsTrue, IRType::INT8);
      inst->AddOperand(value);
      return inst;
    }

    // Is False
    IRValue *IsFalse(IRValue *value) {
      if (value->IsConstant()) {
        return LoadConstU8(value->IsConstantFalse() ? 1 : 0);
      }

      auto *inst = CreateInstruction(IROp::IsFalse, IRType::INT8);
      inst->AddOperand(value);
      return inst;
    }

    // Is NaN
    IRValue *IsNan(IRValue *value) {
      auto *inst = CreateInstruction(IROp::IsNaN, IRType::INT8);
      inst->AddOperand(value);
      return inst;
    }

    // Generic Compare
    IRValue *CreateCompare(IROp opType, IRValue *value1, IRValue *value2) {
      if (value1->IsConstant() && value2->IsConstant()) {
        return LoadConstS8(value1->Compare(opType, value2) ? 1 : 0);
      }
      
      auto *inst = CreateInstruction(opType, IRType::INT8);
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      return inst;
    }

    // Compare Equal
    IRValue *CompareEQ(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareEQ, value1, value2);
    }
    // Compare Not Equal
    IRValue *CompareNE(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareNE, value1, value2);
    }
    // Compare Signed Larger Than
    IRValue *CompareSLT(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareSLT, value1, value2);
    }
    // Compare Signed Larger or Equal
    IRValue *CompareSLE(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareSLE, value1, value2);
    }
    // Compare Signed Greater Than
    IRValue *CompareSGT(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareSGT, value1, value2);
    }
    // Compare Signed Greater Or Equal
    IRValue *CompareSGE(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareSGE, value1, value2);
    }
    // Compare Unsigned Larger Than
    IRValue *CompareULT(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareULT, value1, value2);
    }
    // Compare Unsigned Larger or Equal
    IRValue *CompareULE(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareULE, value1, value2);
    }
    // Compare Unsigned Greater Than
    IRValue *CompareUGT(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareUGT, value1, value2);
    }
    // Compare Unsigned Greater Or Equal
    IRValue *CompareUGE(IRValue *value1, IRValue *value2) {
      return CreateCompare(IROp::CompareUGE, value1, value2);
    }

    // Did Saturate
    IRValue *DidSaturate(IRValue *value1) {
      auto *inst = CreateInstruction(IROp::DidSaturate, IRType::INT8);
      inst->AddOperand(value1);
      return inst;
    }

    // Generic Vector Compare
    IRValue *CreateVectorCompare(IROp opType, IRValue *value1, IRValue *value2, IRType typeToCompare) {
      auto *inst = CreateInstruction(opType, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      inst->AddOperand(LoadConstU32(static_cast<u32>(typeToCompare)));
      return inst;
    }

    // Vector Compare EQ
    IRValue *VectorCompareEQ(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      return CreateVectorCompare(IROp::VectorCompareEQ, value1, value2, typeToCompare);
    }
    // Vector Compare Signed Greater Than
    IRValue *VectorCompareSGT(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      return CreateVectorCompare(IROp::VectorCompareSGT, value1, value2, typeToCompare);
    }
    // Vector Compare Signed Greater or Equal
    IRValue *VectorCompareSGE(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      return CreateVectorCompare(IROp::VectorCompareSGE, value1, value2, typeToCompare);
    }
    // Vector Compare Unsigned Greater Than
    IRValue *VectorCompareUGT(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      return CreateVectorCompare(IROp::VectorCompareUGT, value1, value2, typeToCompare);
    }
    // Vector Compare Unsigned Greater or Equal
    IRValue *VectorCompareUGE(IRValue *value1, IRValue *value2, IRType typeToCompare) {
      return CreateVectorCompare(IROp::VectorCompareUGE, value1, value2, typeToCompare);
    }

    // Add
    IRValue *Add(IRValue *value1, IRValue *value2) {
      if (value1->IsConstantZero()) { return value2; }
      else if (value2->IsConstantZero()) { return value1; }
      else if (value1->IsConstant() && value2->IsConstant()) {
        IRValue *dest = Clone(value1);
        dest->Add(value2);
        return dest;
      }

      auto *inst = CreateInstruction(IROp::Add, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      return inst;
    }

    // Add with the value of the Carry flag
    IRValue *AddWithCarry(IRValue *value1, IRValue *value2, IRValue *carry) {
      auto *inst = CreateInstruction(IROp::AddWithCarry, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      inst->AddOperand(carry);
      return inst;
    }

    // Vector Add
    IRValue *VectorAdd(IRValue *value1, IRValue *value2, IRType opernadType) {
      auto *inst = CreateInstruction(IROp::VectorAdd, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      inst->AddOperand(LoadConstU32(static_cast<u32>(opernadType)));
      return inst;
    }

    // Subtract
    IRValue *Sub(IRValue *value1, IRValue *value2) {
      // TODO: Add constant checks for runtime subtraction

      auto *inst = CreateInstruction(IROp::Sub, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      return inst;
    }

    // Vector Subtract
    IRValue *VectorSub(IRValue *value1, IRValue *value2, IRType opernadType) {
      auto *inst = CreateInstruction(IROp::VectorSub, value1->GetType());
      inst->AddOperand(value1);
      inst->AddOperand(value2);
      inst->AddOperand(LoadConstU32(static_cast<u32>(opernadType)));
      return inst;
    }


    // Debug and Metadata
    // ------------------

    // Create a comment instruction (for debugging)
    IRValue *CreateComment(const std::string &text) {
      auto *inst = CreateInstruction(IROp::Comment);
      inst->SetMetadata("text", text);
      return inst;
    }

    // Set source location for the next instructions
    void SetCurrentSourceLocation(u64 address) { currentSourceAddress = address; }

    // Move all owned values to the function for proper lifetime management
    // This should be called after building is complete
    void TransferValuesToFunction() {
      if (function) {
        function->TakeOwnership(std::move(ownedValues));
      }
    }

  private:
    IRFunction *function;
    IRBasicBlock *currentBlock;
    u64 currentSourceAddress = 0;
    std::vector<std::unique_ptr<IRValue>> ownedValues;

    // Core instruction creation helper
    IRInstruction *CreateInstruction(IROp op, IRType type = IRType::Void) {
      auto inst = std::make_unique<IRInstruction>(op, type);
      auto *ptr = inst.get();
      if (currentSourceAddress != 0) { inst->SetSourceLocation(currentSourceAddress); }
      if (currentBlock) {
        currentBlock->AddInstruction(std::move(inst));
      } else {
        // If no block, store in owned values temporarily
        ownedValues.push_back(std::move(inst));
      }
      return ptr;
    }
  };

} // namespace Xe::XCPU::JIT::IR
