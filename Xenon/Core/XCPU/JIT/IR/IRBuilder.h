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

    // Basic Block Management
    // ----------------------

    // Creates a new basic block with the given name
    IRBasicBlock *CreateBasicBlock(const std::string &name) {
      if (!function) return nullptr;
      return function->CreateBasicBlock(name);
    }

    // Sets the current insertion point for new instructions
    void SetInsertPoint(IRBasicBlock *block) {
      currentBlock = block;
    }

    // Gets the current insertion point
    IRBasicBlock *GetInsertBlock() const {
      return currentBlock;
    }

    // Constant Creation
    // -----------------

    // Create an integer constant
    IRConstantInt *CreateConstInt(IRType type, u64 value) {
      auto *constant = new IRConstantInt(type, value);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create an 8-bit integer constant
    IRConstantInt *LoadConstInt8(u8 value) {
      return CreateConstInt(IRType::INT8, value);
    }

    // Create a 16-bit integer constant
    IRConstantInt *LoadConstInt16(u16 value) {
      return CreateConstInt(IRType::INT16, value);
    }

    // Create a 32-bit integer constant
    IRConstantInt *LoadConstInt32(u32 value) {
      return CreateConstInt(IRType::INT32, value);
    }

    // Create a 64-bit integer constant
    IRConstantInt *LoadConstInt64(u64 value) {
      return CreateConstInt(IRType::INT64, value);
    }

    // Create a F32 constant
    IRConstantFloat32 *LoadConstFloat32(f32 value) {
      auto *constant = new IRConstantFloat32(IRType::FLOAT32, value);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create a F64 constant
    IRConstantFloat64 *LoadConstFloat64(f64 value) {
      auto *constant = new IRConstantFloat64(IRType::FLOAT64, value);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create a Vector128 constant
    IRConstantVec128 *LoadConstFloat64(Base::Vector128 value) {
      auto *constant = new IRConstantVec128(IRType::VEC128, value);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create a zero constant of the given type
    IRConstantInt *CreateConstZero(IRType type) {
      auto *constant = new IRConstantInt(type, 0);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create zero constant Int8
    IRConstantInt *LoadZeroInt8() { return CreateConstZero(IRType::INT8); }

    // Create zero constant Int16
    IRConstantInt *LoadZeroInt16() { return CreateConstZero(IRType::INT16); }

    // Create zero constant Int32
    IRConstantInt *LoadZeroInt32() { return CreateConstZero(IRType::INT32); }

    // Create zero constant Int64
    IRConstantInt *LoadZeroInt64() { return CreateConstZero(IRType::INT64); }

    // Create zero constant Float 32
    IRConstantFloat32 *LoadZeroFloat32() {
      auto *constant = new IRConstantFloat32(IRType::FLOAT32, 0);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create zero constant Float 64
    IRConstantFloat64 *LoadZeroFloat64() {
      auto *constant = new IRConstantFloat64(IRType::FLOAT64, 0);
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Create zero constant Vector 128
    IRConstantVec128 *LoadZeroVec128() {
      auto *constant = new IRConstantVec128(IRType::VEC128, Base::Vector128{ 0 });
      ownedValues.emplace_back(constant);
      return constant;
    }

    // Register Access
    // ---------------

    // Create a reference to a PPC register
    IRRegister *CreateRegister(IRRegisterType regType, u32 index, IRType type) {
      auto *reg = new IRRegister(regType, index, type);
      ownedValues.emplace_back(reg);
      return reg;
    }

    /// Create a reference to a GPR (General Purpose Register)
    IRRegister *CreateGPR(u32 index) {
      return CreateRegister(IRRegisterType::GPR, index, IRType::INT64);
    }

    /// Create a reference to an FPR (Floating Point Register)
    IRRegister *CreateFPR(u32 index) {
      return CreateRegister(IRRegisterType::FPR, index, IRType::FLOAT64);
    }

    /// Create a reference to a Vector Register
    IRRegister *CreateVR(u32 index) {
      return CreateRegister(IRRegisterType::VR, index, IRType::VEC128);
    }

    // Instruction Creation Helpers
    // -----------------------------

    // Load value from a PPC register
    IRInstruction *LoadReg(IRRegister *reg) {
      auto *inst = CreateInstruction(IROp::LoadReg, reg->GetType());
      inst->AddOperand(reg);
      return inst;
    }

    /// Store value to a PPC register
    IRInstruction *StoreReg(IRRegister *reg, IRValue *value) {
      auto *inst = CreateInstruction(IROp::StoreReg, IRType::Void);
      inst->AddOperand(reg);
      inst->AddOperand(value);
      return inst;
    }

    // Load from memory
    IRInstruction *Load(IRValue *address, IRType type) {
      auto *inst = CreateInstruction(IROp::Load, type);
      inst->AddOperand(address);
      return inst;
    }

    // Store to memory
    IRInstruction *Store(IRValue *address, IRValue *value) {
      auto *inst = CreateInstruction(IROp::Store, IRType::Void);
      inst->AddOperand(address);
      inst->AddOperand(value);
      return inst;
    }

    // Instructions Creation
    // ----------------------------------

    // Create a signed integer comparison
    IRInstruction *CreateCmpS(IRCmpPredicate pred, IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::Cmp, IRType::INT8);
      inst->SetMetadata("predicate", IRCmpPredicateToString(pred));
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create an unsigned integer comparison
    IRInstruction *CreateCmpU(IRCmpPredicate pred, IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::CmpU, IRType::INT8);
      inst->SetMetadata("predicate", IRCmpPredicateToString(pred));
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create comparisons for different types

    // Compare equal
    IRInstruction *CmpEQ(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::CmpU, IRType::INT8);
      inst->SetMetadata("predicate", IRCmpPredicateToString(IRCmpPredicate::EQ));
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Compare not equal
    IRInstruction *CmpNE(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::CmpU, IRType::INT8);
      inst->SetMetadata("predicate", IRCmpPredicateToString(IRCmpPredicate::NE));
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Compare Signed Less Than
    IRInstruction *CmpSLT(IRValue *lhs, IRValue *rhs) {
      return CreateCmpS(IRCmpPredicate::SLT, lhs, rhs);
    }

    // Compare Signed Less Than Or Equal
    IRInstruction *CmpSLE(IRValue *lhs, IRValue *rhs) {
      return CreateCmpS(IRCmpPredicate::SLE, lhs, rhs);
    }

    // Compare Signed Greater Than
    IRInstruction *CmpSGT(IRValue *lhs, IRValue *rhs) {
      return CreateCmpS(IRCmpPredicate::SGT, lhs, rhs);
    }

    // Compare Signed Greater Than Or Equal
    IRInstruction *CmpSGE(IRValue *lhs, IRValue *rhs) {
      return CreateCmpS(IRCmpPredicate::SGE, lhs, rhs);
    }

    // Compare Unsigned Less Than
    IRInstruction *CmpULT(IRValue *lhs, IRValue *rhs) {
      return CreateCmpU(IRCmpPredicate::ULT, lhs, rhs);
    }

    // Compare Unsigned Less Than Or Equal
    IRInstruction *CmpULE(IRValue *lhs, IRValue *rhs) {
      return CreateCmpU(IRCmpPredicate::ULE, lhs, rhs);
    }

    // Compare Unsigned Greater Than
    IRInstruction *CmpUGT(IRValue *lhs, IRValue *rhs) {
      return CreateCmpU(IRCmpPredicate::UGT, lhs, rhs);
    }

    // Compare Unsigned Greater Than Or Equal
    IRInstruction *CmpUGE(IRValue *lhs, IRValue *rhs) {
      return CreateCmpU(IRCmpPredicate::UGE, lhs, rhs);
    }

    // Create an addition instruction
    IRInstruction *Add(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::Add, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a subtraction instruction
    IRInstruction *Sub(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::Sub, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a multiplication instruction
    IRInstruction *Mul(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::Mul, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a signed division instruction
    IRInstruction *DivS(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::DivS, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create an unsigned division instruction
    IRInstruction *DivU(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::DivU, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a signed modulo instruction
    IRInstruction *ModS(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::ModS, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create an unsigned modulo instruction
    IRInstruction *ModU(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::ModU, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a negation instruction
    IRInstruction *Neg(IRValue *operand) {
      auto *inst = CreateInstruction(IROp::Neg, operand->GetType());
      inst->AddOperand(operand);
      return inst;
    }

    // Create a floating point addition
    IRInstruction *FAdd(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::FAdd, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a floating point subtraction
    IRInstruction *FSub(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::FSub, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a floating point multiplication
    IRInstruction *FMul(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::FMul, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a floating point division
    IRInstruction *FDiv(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::FDiv, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a floating point negation
    IRInstruction *FNeg(IRValue *operand) {
      auto *inst = CreateInstruction(IROp::FNeg, operand->GetType());
      inst->AddOperand(operand);
      return inst;
    }

    // Create a floating point absolute value
    IRInstruction *FAbs(IRValue *operand) {
      auto *inst = CreateInstruction(IROp::FAbs, operand->GetType());
      inst->AddOperand(operand);
      return inst;
    }

    // Create a floating point square root
    IRInstruction *FSqrt(IRValue *operand) {
      auto *inst = CreateInstruction(IROp::FSqrt, operand->GetType());
      inst->AddOperand(operand);
      return inst;
    }

    // Create a fused multiply-add (a * b + c)
    IRInstruction *FMA(IRValue *a, IRValue *b, IRValue *c) {
      auto *inst = CreateInstruction(IROp::FMA, a->GetType());
      inst->AddOperand(a);
      inst->AddOperand(b);
      inst->AddOperand(c);
      return inst;
    }

    // Create a bitwise AND
    IRInstruction *And(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::And, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a bitwise OR
    IRInstruction *Or(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::Or, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a bitwise XOR
    IRInstruction *Xor(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::Xor, lhs->GetType());
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a bitwise NOT
    IRInstruction *Not(IRValue *operand) {
      auto *inst = CreateInstruction(IROp::Not, operand->GetType());
      inst->AddOperand(operand);
      return inst;
    }

    // Create a shift left
    IRInstruction *Shl(IRValue *value, IRValue *shift) {
      auto *inst = CreateInstruction(IROp::Shl, value->GetType());
      inst->AddOperand(value);
      inst->AddOperand(shift);
      return inst;
    }

    // Create a logical shift right
    IRInstruction *Shr(IRValue *value, IRValue *shift) {
      auto *inst = CreateInstruction(IROp::Shr, value->GetType());
      inst->AddOperand(value);
      inst->AddOperand(shift);
      return inst;
    }

    // Create an arithmetic shift right
    IRInstruction *Sar(IRValue *value, IRValue *shift) {
      auto *inst = CreateInstruction(IROp::Sar, value->GetType());
      inst->AddOperand(value);
      inst->AddOperand(shift);
      return inst;
    }

    // Create a rotate left
    IRInstruction *Rotl(IRValue *value, IRValue *shift) {
      auto *inst = CreateInstruction(IROp::Rotl, value->GetType());
      inst->AddOperand(value);
      inst->AddOperand(shift);
      return inst;
    }

    // Create a rotate right
    IRInstruction *Rotr(IRValue *value, IRValue *shift) {
      auto *inst = CreateInstruction(IROp::Rotr, value->GetType());
      inst->AddOperand(value);
      inst->AddOperand(shift);
      return inst;
    }

    // Create a zero extension
    IRInstruction *CreateZExt(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::ZExt, destType);
      inst->AddOperand(value);
      return inst;
    }

    /// Create a sign extension
    IRInstruction *CreateSExt(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::SExt, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Create a truncation
    IRInstruction *CreateTrunc(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::Trunc, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Create a float to signed integer conversion
    IRInstruction *CreateFPToSI(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::FPToSI, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Create a float to unsigned integer conversion
    IRInstruction *CreateFPToUI(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::FPToUI, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Create a signed integer to float conversion
    IRInstruction *CreateSIToFP(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::SIToFP, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Create an unsigned integer to float conversion
    IRInstruction *CreateUIToFP(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::UIToFP, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Create a float extension (f32 -> f64)
    IRInstruction *CreateFPExt(IRValue *value) {
      auto *inst = CreateInstruction(IROp::FPExt, IRType::FLOAT64);
      inst->AddOperand(value);
      return inst;
    }

    // Create a float truncation (f64 -> f32)
    IRInstruction *CreateFPTrunc(IRValue *value) {
      auto *inst = CreateInstruction(IROp::FPTrunc, IRType::FLOAT32);
      inst->AddOperand(value);
      return inst;
    }

    // Create a bitcast (reinterpret bits)
    IRInstruction *CreateBitcast(IRValue *value, IRType destType) {
      auto *inst = CreateInstruction(IROp::Bitcast, destType);
      inst->AddOperand(value);
      return inst;
    }

    // Vector Operations
    // -----------------

    // Create a vector addition
    IRInstruction *CreateVAdd(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::VAdd, IRType::VEC128);
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a vector subtraction
    IRInstruction *CreateVSub(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::VSub, IRType::VEC128);
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a vector multiplication
    IRInstruction *CreateVMul(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::VMul, IRType::VEC128);
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a vector division
    IRInstruction *CreateVDiv(IRValue *lhs, IRValue *rhs) {
      auto *inst = CreateInstruction(IROp::VDiv, IRType::VEC128);
      inst->AddOperand(lhs);
      inst->AddOperand(rhs);
      return inst;
    }

    // Create a vector splat (broadcast scalar to all elements)
    IRInstruction *CreateVSplat(IRValue *scalar) {
      auto *inst = CreateInstruction(IROp::VSplat, IRType::VEC128);
      inst->AddOperand(scalar);
      return inst;
    }

    // Create a vector element extraction
    IRInstruction *CreateVExtract(IRValue *vector, IRValue *index, IRType elemType) {
      auto *inst = CreateInstruction(IROp::VExtract, elemType);
      inst->AddOperand(vector);
      inst->AddOperand(index);
      return inst;
    }

    // Create a vector element insertion
    IRInstruction *CreateVInsert(IRValue *vector, IRValue *element, IRValue *index) {
      auto *inst = CreateInstruction(IROp::VInsert, IRType::VEC128);
      inst->AddOperand(vector);
      inst->AddOperand(element);
      inst->AddOperand(index);
      return inst;
    }

    // Control Flow
    // ------------

    // NOTE: All terminator instructions should set addToBlock as false.

    // Create an unconditional branch
    IRInstruction *CreateBranch(IRBasicBlock *target) {
      auto *inst = CreateInstruction(IROp::Branch, IRType::Void, false);
      inst->AddOperand(target);
      if (currentBlock) {
        // Transferir ownership al basic block para el terminator
        auto instPtr = TransferOwnership(inst);
        currentBlock->SetTerminator(std::move(instPtr));
        currentBlock->AddSuccessor(target);
        target->AddPredecessor(currentBlock);
      }
      return inst;
    }

    // Create a conditional branch
    IRInstruction *CreateBranchCond(IRValue *condition, IRBasicBlock *trueBlock,
      IRBasicBlock *falseBlock) {
      auto *inst = CreateInstruction(IROp::BranchCond, IRType::Void, false);
      inst->AddOperand(condition);
      inst->AddOperand(trueBlock);
      inst->AddOperand(falseBlock);
      if (currentBlock) {
        // Transferir ownership al basic block para el terminator
        auto instPtr = TransferOwnership(inst);
        currentBlock->SetTerminator(std::move(instPtr));
        currentBlock->AddSuccessor(trueBlock);
        currentBlock->AddSuccessor(falseBlock);
        trueBlock->AddPredecessor(currentBlock);
        falseBlock->AddPredecessor(currentBlock);
      }
      return inst;
    }

    // Create a return instruction
    IRInstruction *CreateReturn(IRValue *value = nullptr) {
      auto *inst = CreateInstruction(IROp::Return, IRType::Void, false);
      if (value) {
        inst->AddOperand(value);
      }
      if (currentBlock) {
        // Transferir ownership al basic block para el terminator
        auto instPtr = TransferOwnership(inst);
        currentBlock->SetTerminator(std::move(instPtr));
      }
      return inst;
    }

    // Special PPC Operations
    // ----------------------

    // Create a count leading zeros instruction
    IRInstruction *CreateCountLeadingZeros(IRValue *value) {
      auto *inst = CreateInstruction(IROp::CountLeadingZeros, value->GetType());
      inst->AddOperand(value);
      return inst;
    }

    // Create an extract bits instruction
    IRInstruction *CreateExtractBits(IRValue *value, IRValue *start, IRValue *length) {
      auto *inst = CreateInstruction(IROp::ExtractBits, value->GetType());
      inst->AddOperand(value);
      inst->AddOperand(start);
      inst->AddOperand(length);
      return inst;
    }

    // Create an insert bits instruction
    IRInstruction *CreateInsertBits(IRValue *target, IRValue *source,
      IRValue *start, IRValue *length) {
      auto *inst = CreateInstruction(IROp::InsertBits, target->GetType());
      inst->AddOperand(target);
      inst->AddOperand(source);
      inst->AddOperand(start);
      inst->AddOperand(length);
      return inst;
    }

    // Condition Register Operations
    // -----------------------------

    // Create a CR bit set instruction
    IRInstruction *CreateCRSetBit(IRValue *crIndex, IRValue *bitIndex, IRValue *value) {
      auto *inst = CreateInstruction(IROp::CRSetBit, IRType::Void);
      inst->AddOperand(crIndex);
      inst->AddOperand(bitIndex);
      inst->AddOperand(value);
      return inst;
    }

    // Create a CR bit get instruction
    IRInstruction *CreateCRGetBit(IRValue *crIndex, IRValue *bitIndex) {
      auto *inst = CreateInstruction(IROp::CRGetBit, IRType::INT8);
      inst->AddOperand(crIndex);
      inst->AddOperand(bitIndex);
      return inst;
    }

    // Create a synchronization instruction
    IRInstruction *CreateSync() {
      return CreateInstruction(IROp::Sync, IRType::Void);
    }

    // Create an instruction synchronization
    IRInstruction *CreateIsync() {
      return CreateInstruction(IROp::Isync, IRType::Void);
    }

    // Create a trap instruction
    IRInstruction *CreateTrap() {
      return CreateInstruction(IROp::Trap, IRType::Void);
    }

    // Debug and Metadata
    // ------------------

    // Create a comment instruction (for debugging)
    IRInstruction *CreateComment(const std::string &text) {
      auto *inst = CreateInstruction(IROp::Comment, IRType::Void);
      inst->SetMetadata("text", text);
      return inst;
    }

    // Set source location for the next instructions
    void SetCurrentSourceLocation(u64 address) { currentSourceAddress = address; }

  private:
    IRFunction *function;
    IRBasicBlock *currentBlock;
    u64 currentSourceAddress = 0;
    std::vector<std::unique_ptr<IRValue>> ownedValues;

    // Core instruction creation helper
    IRInstruction *CreateInstruction(IROp op, IRType type, bool addToBlock = true) {
      auto *inst = new IRInstruction(op, type);
      ownedValues.emplace_back(inst);
      if (currentSourceAddress != 0) { inst->SetSourceLocation(currentSourceAddress); }
      if (addToBlock && currentBlock) {
        auto instPtr = TransferOwnership(inst);
        currentBlock->AddInstruction(std::move(instPtr));
      }
      return inst;
    }

    // Helper for transfering ownership from ownedValues to basic block
    std::unique_ptr<IRInstruction> TransferOwnership(IRInstruction *inst) {
      // Buscar y extraer el unique_ptr del vector ownedValues
      for (auto it = ownedValues.begin(); it != ownedValues.end(); ++it) {
        if (it->get() == inst) {
          // Extraer el unique_ptr y remover del vector
          auto ptr = std::unique_ptr<IRInstruction>(
            static_cast<IRInstruction*>(it->release())
          );
          ownedValues.erase(it);
          return ptr;
        }
      }
      // Esto no debería ocurrir, pero por seguridad
      return std::unique_ptr<IRInstruction>(inst);
    }
  };

} // namespace Xe::XCPU::JIT::IR
