/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Base/Types.h"
#include "Base/Assert.h"
#include "Base/Vector128.h"
#include "Core/XCPU/JIT/IR/IROpcodes.h"

//=============================================================================
// IR Types
// ----------------------------------------------------------------------------
// Defines various types used in the JIT Intermediate Representation
// The flow goes as follows:
// PPC Code -> IR -> Optimization Layer -> Runtime Code Emitter (ARM/X86_64)
//=============================================================================

namespace Xe::XCPU::JIT::IR {

  // Forward declarations
  class IRInstruction;
  class IRBasicBlock;
  class IRFunction;

  //=============================================================================
  // Data Types
  //=============================================================================

  enum class IRType : u8 {
    // Void type (for instructions with no return value)
    Void,

    // Integer types
    INT8,     // 8-bit integer
    INT16,    // 16-bit integer
    INT32,    // 32-bit integer
    INT64,    // 64-bit integer

    // Floating point types
    FLOAT32,  // Single precision float
    FLOAT64,  // Double precision float

    // Vector types (for VMX/AltiVec)
    VEC128,   // 128-bit vector (4x f32 or 4x i32)

    // Pointer type
    Ptr,      // Pointer to memory

    // Special types
    Label,    // Basic block label
  };

  // Returns the name of the IR Type as a string
  // Should be useful for debugging PPC -> IR later
  inline const char *IRTypeToString(IRType type) {
    switch (type) {
    case IRType::Void: return "void";
    case IRType::INT8: return "i8";
    case IRType::INT16: return "i16";
    case IRType::INT32: return "i32";
    case IRType::INT64: return "i64";
    case IRType::FLOAT32: return "f32";
    case IRType::FLOAT64: return "f64";
    case IRType::VEC128: return "v128";
    case IRType::Ptr: return "ptr";
    case IRType::Label: return "label";
    }
    return "unknown";
  }

  // Helper for determining the size of a specific IR Type
  inline size_t IRTypeSize(IRType type) {
    switch (type) {
    case IRType::Void: return 0;
    case IRType::INT8: return 1;
    case IRType::INT16: return 2;
    case IRType::INT32: return 4;
    case IRType::INT64: return 8;
    case IRType::FLOAT32: return 4;
    case IRType::FLOAT64: return 8;
    case IRType::VEC128: return 16;
    case IRType::Ptr: return 8;
    case IRType::Label: return 0;
    }
    return 0;
  }

  //=============================================================================
  // Register Types
  //=============================================================================

  // PPC register types that can be represented in IR
  enum class IRRegisterType : u8 {
    // General Purpose Registers
    GPR,    // r0-r31 (64-bit)

    // Floating Point Registers
    FPR,    // f0-f31 (64-bit double)

    // Vector Registers
    VR,     // v0-v127 (128-bit)

    // Special Purpose Registers
    SPR,    // Special Purpose Register (generic)
    XER,    // Fixed-Point Exception Register
    LR,     // Link Register
    CTR,    // Count Register
    CFAR,   // Used in Linux, unknown definition atm.
    DSISR,  // Data Storage Interrupt Status Register
    DAR,    // Data Address Register
    DEC,    // Decrementer Register
    SRR0,   // Machine Status Save/Restore Register 0
    SRR1,   // Machine Status Save/Restore Register 1
    ACCR,   // Address Compare Control Register
    VRSAVE, // VXU Register Save
    SPRG0,  // Software Use Special Purpose Register 0
    SPRG1,  // Software Use Special Purpose Register 1
    SPRG2,  // Software Use Special Purpose Register 2
    SPRG3,  // Software Use Special Purpose Register 3
    HSPRG0, // Hypervisor Software Use Special Purpose Register 0
    HSPRG1, // Hypervisor Software Use Special Purpose Register 1
    HSRR0,  // Hypervisor Save Restore Register 0
    HSRR1,  // Hypervisor Save Restore Register 1
    TSRL,   // Thread Status Register Local
    TSRR,   // Thread Status Register Remote
    PPE_TLB_Index_Hint, // PPE Translation Lookaside Buffer Index Hint Register
    DABR,   // Data Address Beakpoint Register
    DABRX,  // Data Address Beakpoint Register Extension
    MSR,    // Machine State Register
    PIR,    // Processor Identification Register
    // Virtual/temporary register
    Temp,
  };

  inline const char *IRRegisterTypeToString(IRRegisterType type) {
    switch (type) {
    case IRRegisterType::GPR: return "GPR";
    case IRRegisterType::FPR: return "FPR";
    case IRRegisterType::VR: return "VR";
    case IRRegisterType::SPR: return "Generic SPR";
    case IRRegisterType::XER: return "XER";
    case IRRegisterType::LR: return "LR";
    case IRRegisterType::CTR: return "CTR";
    case IRRegisterType::CFAR: return "CFAR";
    case IRRegisterType::DSISR: return "DSISR";
    case IRRegisterType::DAR: return "DAR";
    case IRRegisterType::DEC: return "DEC";
    case IRRegisterType::SRR0: return "SRR0";
    case IRRegisterType::SRR1: return "SRR1";
    case IRRegisterType::ACCR: return "ACCR";
    case IRRegisterType::VRSAVE: return "VRSAVE";
    case IRRegisterType::SPRG0: return "SPRG0";
    case IRRegisterType::SPRG1: return "SPRG1";
    case IRRegisterType::SPRG2: return "SPRG2";
    case IRRegisterType::SPRG3: return "SPRG3";
    case IRRegisterType::HSPRG0: return "HSPRG0";
    case IRRegisterType::HSPRG1: return "HSPRG1";
    case IRRegisterType::HSRR0: return "HSRR0";
    case IRRegisterType::HSRR1: return "HSRR1";
    case IRRegisterType::TSRL: return "TSRL";
    case IRRegisterType::TSRR: return "TSRR";
    case IRRegisterType::PPE_TLB_Index_Hint: return "PPE_TLB_Index_Hint";
    case IRRegisterType::DABR: return "DABR";
    case IRRegisterType::DABRX: return "DABRX";
    case IRRegisterType::MSR: return "MSR";
    case IRRegisterType::PIR: return "PIR";
    case IRRegisterType::Temp: return "TEMP";
    }
    return "Unknown";
  }

  //=============================================================================
  // Comparison Predicates
  //=============================================================================

  enum class IRCmpPredicate : u8 {
    // Integer comparisons
    EQ,   // Equal
    NE,   // Not equal
    SLT,  // Signed Less than
    SLE,  // Signed Less than or equal
    SGT,  // Signed Greater than
    SGE,  // Signed Greater than or equal
    ULT,  // Unsigned Less than
    ULE,  // Unsigned Less than or equal
    UGT,  // Unsigned Greater than
    UGE,  // Unsigned Greater than or equal

    // Floating point comparisons (ordered and unordered variants)
    FOEQ, // Ordered equal
    FONE, // Ordered not equal
    FOLT, // Ordered less than
    FOLE, // Ordered less than or equal
    FOGT, // Ordered greater than
    FOGE, // Ordered greater than or equal
    FUEQ, // Unordered equal
    FUNE, // Unordered not equal
    FULT, // Unordered less than
    FULE, // Unordered less than or equal
    FUGT, // Unordered greater than
    FUGE, // Unordered greater than or equal

    // Vector comparisons
    VEQ,  // Vector equal
    VSGT, // Vector signed greater than
    VSGE, // Vector signed greater than or equal
    VUGT, // Vector unsigned greter than
    VUGE, // Vector unisgned greater or equal
  };

  inline const char *IRCmpPredicateToString(IRCmpPredicate pred) {
    switch (pred) {
    case IRCmpPredicate::EQ: return "EQ";
    case IRCmpPredicate::NE: return "NE";
    case IRCmpPredicate::SLT: return "SLT";
    case IRCmpPredicate::SLE: return "SLE";
    case IRCmpPredicate::SGT: return "SGT";
    case IRCmpPredicate::SGE: return "SGE";
    case IRCmpPredicate::ULT: return "ULT";
    case IRCmpPredicate::ULE: return "ULE";
    case IRCmpPredicate::UGT: return "UGT";
    case IRCmpPredicate::UGE: return "UGE";
    case IRCmpPredicate::FOEQ: return "FOEQ";
    case IRCmpPredicate::FONE: return "FONE";
    case IRCmpPredicate::FOLT: return "FOLT";
    case IRCmpPredicate::FOLE: return "FOLE";
    case IRCmpPredicate::FOGT: return "FOGT";
    case IRCmpPredicate::FOGE: return "FOGE";
    case IRCmpPredicate::FUEQ: return "FUEQ";
    case IRCmpPredicate::FUNE: return "FUNE";
    case IRCmpPredicate::FULT: return "FULT";
    case IRCmpPredicate::FULE: return "FULE";
    case IRCmpPredicate::FUGT: return "FUGT";
    case IRCmpPredicate::FUGE: return "FUGE";
    case IRCmpPredicate::VEQ: return "VEQ";
    case IRCmpPredicate::VSGT: return "VSGT";
    case IRCmpPredicate::VSGE: return "VSGE";
    case IRCmpPredicate::VUGT: return "VUGT";
    case IRCmpPredicate::VUGE: return "VUGE";
    }
    return "Unknown";
  }

  //=============================================================================
  // IR IRValue
  //=============================================================================

  // Represents a value in the IR (SSA form)
  // All instructions in the IR return and use such value
  // This allows full SSA to be performed as well as code/comp opts later on
  class IRValue {
  public:
    // The kind of IR IRValue
    enum class ValueKind : u8 {
      Generic,          // Generic value
      Constant,         // Constant value
      Instruction,      // Result of an instruction
      Register,         // PPC register reference
      BasicBlock,       // Basic block label
      Argument,         // Function argument
    };

    typedef union {
      s8 i8;
      u8 u8;
      s16 i16;
      u16 u16;
      s32 i32;
      u32 u32;
      s64 i64;
      u64 u64;
      f32 flt32;
      f64 flt64;
      Base::Vector128 vec128;
    } ConstantValue;

    IRValue(ValueKind k = ValueKind::Generic, IRType t = IRType::Void) : kind(k), type(t), id(nextId++) {}
    virtual ~IRValue() = default;

    ValueKind GetKind() const { return kind; }
    IRType GetType() const { return type; }
    u32 GetId() const { return id; }

    // Returns wheter this is a constValue value of any given type.
    inline bool IsConstant() { return isConstantVal; }

    // Setters for constValue values

    void SetZero(IRType newType);
    void SetConstant(s8 value);
    void SetConstant(u8 value);
    void SetConstant(s16 value);
    void SetConstant(u16 value);
    void SetConstant(s32 value);
    void SetConstant(u32 value);
    void SetConstant(s64 value);
    void SetConstant(u64 value);
    void SetConstant(f32 value);
    void SetConstant(f64 value);
    void SetConstant(const Base::Vector128 &value);

    // Returns the constValue value stored in this IRValue
    ConstantValue GetValue() const { return constValue; }

    // Operations for constValue values
    
    bool IsConstantTrue() const;
    bool IsConstantFalse() const;
    bool IsConstantZero() const;
    bool IsConstantOne() const;
    bool IsConstantEQ(IRValue *operand) const;
    bool IsConstantNE(IRValue *operand) const;
    bool IsConstantSLT(IRValue *operand) const;
    bool IsConstantSLE(IRValue *operand) const;
    bool IsConstantSGT(IRValue *operand) const;
    bool IsConstantSGE(IRValue *operand) const;
    bool IsConstantULT(IRValue *operand) const;
    bool IsConstantULE(IRValue *operand) const;
    bool IsConstantUGT(IRValue *operand) const;
    bool IsConstantUGE(IRValue *operand) const;

    void Cast(IRType newType);
    void ZeroExtend(IRType newType);
    void SignExtend(IRType newType);
    void Truncate(IRType newType);
    void Convert(IRType newType);
    void Round(RoundingMode roundMode);
    void Add(IRValue *operand);
    void Sub(IRValue *operand);
    void Mul(IRValue *operand);
    void MulHi(IRValue *operand, bool isUnsigned);
    void Div(IRValue *operand, bool isUnsigned);
    void Max(IRValue *operand);
    static void MulAdd(IRValue *dest, IRValue *value1, IRValue *value2, IRValue *value3);
    static void MulSub(IRValue *dest, IRValue *value1, IRValue *value2, IRValue *value3);
    void Neg();
    void Abs();
    void Sqrt();
    void RSqrt();
    void Recip();
    void And(IRValue *operand);
    void Or(IRValue *operand);
    void Xor(IRValue *operand);
    void Not();
    void Shl(IRValue *operand);
    void Shr(IRValue *operand);
    void Sha(IRValue *operand);
    void Extract(IRValue *vec, IRValue *index);
    void Select(IRValue *operand, IRValue *ctrl);
    void Splat(IRValue *operand);
    void VectorCompareEQ(IRValue *operand, IRType type);
    void VectorCompareSGT(IRValue *operand, IRType type);
    void VectorCompareSGE(IRValue *operand, IRType type);
    void VectorCompareUGT(IRValue *operand, IRType type);
    void VectorCompareUGE(IRValue *operand, IRType type);
    void VectorConvertI2F(IRValue *operand, bool isUnsigned);
    void VectorConvertF2I(IRValue *operand, bool isUnsigned);
    void VectorShl(IRValue *operand, IRType type);
    void VectorShr(IRValue *operand, IRType type);
    void VectorRol(IRValue *operand, IRType type);
    void VectorAdd(IRValue *operand, IRType type, bool isUnsigned, bool saturate);
    void VectorSub(IRValue *operand, IRType type, bool isUnsigned, bool saturate);
    void DotProduct3(IRValue *operand);
    void DotProduct4(IRValue *operand);
    void VectorAverage(IRValue *operand, IRType type, bool isUnsigned, bool saturate);
    void ByteSwap();
    void CountLeadingZeros(const IRValue *operand);
    bool Compare(IROp opcode, IRValue *operand);


    // For use tracking
    void AddUse(IRInstruction *inst) { uses.push_back(inst); }
    const std::vector<IRInstruction *> &GetUses() const { return uses; }

    virtual std::string ToString() const {
      return std::string("%") + std::to_string(id) + " : " + IRTypeToString(type);
    }

  protected:
    // Kind of IRValue we hold
    ValueKind kind;
    // Constant value this IRValue holds
    ConstantValue constValue = {};
    // This is a constValue IRValue
    bool isConstantVal = false;
    IRType type;
    u32 id;
    std::vector<IRInstruction *> uses;

    static inline u32 nextId = 0;
  };

  //=============================================================================
  // Register Reference
  //=============================================================================

  // Represents a PPC register (GPR/FPR/SPR/etc)
  class IRRegister : public IRValue {
  public:
    IRRegister(IRRegisterType rt, u32 idx, IRType t)
      : IRValue(ValueKind::Register, t), regType(rt), regIndex(idx) {
    }

    IRRegisterType GetRegisterType() const { return regType; }
    u32 GetRegisterIndex() const { return regIndex; }

    std::string ToString() const override {
      return std::string(IRRegisterTypeToString(regType)) + std::to_string(regIndex);
    }

  private:
    IRRegisterType regType;
    u32 regIndex;
  };

  //=============================================================================
  // IR Instruction
  //=============================================================================

  // Represents an instruction in the IR
  class IRInstruction : public IRValue {
  public:
    IRInstruction(IROp o, IRType t = IRType::Void) : IRValue(ValueKind::Instruction, t), op(o) {}

    IROp GetOpcode() const { return op; }

    // Operand management
    void AddOperand(IRValue *val) {
      operands.push_back(val);
      if (val) val->AddUse(this);
    }

    size_t GetNumOperands() const { return operands.size(); }
    IRValue *GetOperand(size_t idx) const {
      return idx < operands.size() ? operands[idx] : nullptr;
    }

    // Metadata
    void SetMetadata(const std::string &key, const std::string &value) {
      metadata[key] = value;
    }

    std::string GetMetadata(const std::string &key) const {
      auto it = metadata.find(key);
      return it != metadata.end() ? it->second : "";
    }

    // Debug info
    void SetSourceLocation(u64 addr) { sourceAddress = addr; }
    u64 GetSourceLocation() const { return sourceAddress; }

    std::string ToString() const override {
      // Special formatting for high-level register operations
      switch (op) {
      case IROp::LoadGPR: {
        if (!operands.empty()) {
          if (auto *idx = dynamic_cast<IRValue *>(operands[0])) {
            return "%" + std::to_string(id) + " = LoadGPR[" + std::to_string(idx->GetValue().u32) + "]";
          }
        }
        break;
      }
      case IROp::StoreGPR: {
        if (operands.size() >= 2) {
          if (auto *idx = dynamic_cast<IRValue *>(operands[0])) {
            return "StoreGPR[" + std::to_string(idx->GetValue().u32) + "] %" + std::to_string(operands[1]->GetId());
          }
        }
        break;
      }
      case IROp::LoadFPR: {
        if (!operands.empty()) {
          if (auto *idx = dynamic_cast<IRValue *>(operands[0])) {
            return "%" + std::to_string(id) + " = LoadFPR[" + std::to_string(idx->GetValue().u32) + "]";
          }
        }
        break;
      }
      case IROp::StoreFPR: {
        if (operands.size() >= 2) {
          if (auto *idx = dynamic_cast<IRValue *>(operands[0])) {
            return "StoreFPR[" + std::to_string(idx->GetValue().u32) + "] %" + std::to_string(operands[1]->GetId());
          }
        }
        break;
      }
      case IROp::LoadVR: {
        if (!operands.empty()) {
          if (auto *idx = dynamic_cast<IRValue *>(operands[0])) {
            return "%" + std::to_string(id) + " = LoadVR[" + std::to_string(idx->GetValue().u32) + "]";
          }
        }
        break;
      }
      case IROp::StoreVR: {
        if (operands.size() >= 2) {
          if (auto *idx = dynamic_cast<IRValue *>(operands[0])) {
            return "StoreVR[" + std::to_string(idx->GetValue().u32) + "] %" + std::to_string(operands[1]->GetId());
          }
        }
        break;
      }
      case IROp::LoadSPR: {
        std::string sprName = GetMetadata("spr_name");
        if (!sprName.empty()) {
          return "%" + std::to_string(id) + " = Load" + sprName;
        }
        break;
      }
      case IROp::StoreSPR: {
        std::string sprName = GetMetadata("spr_name");
        if (!sprName.empty() && operands.size() >= 2) {
          return "Store" + sprName + " %" + std::to_string(operands[1]->GetId());
        }
        break;
      }
      default:
        break;
      }

      // Default formatting
      std::string result = "%" + std::to_string(id) + " = " + IROpToString(op);
      for (auto *operand : operands) {
        if (operand) {
          result += " %" + std::to_string(operand->GetId());
        }
      }
      return result;
    }

  protected:
    IROp op;
    std::vector<IRValue *> operands;
    std::unordered_map<std::string, std::string> metadata;
    u64 sourceAddress = 0;
  };

  //=============================================================================
  // Basic Block
  //=============================================================================

  // Contains a list of consecutive instructions 
  class IRBasicBlock : public IRValue {
  public:
    IRBasicBlock(const std::string &n)
      : IRValue(ValueKind::BasicBlock, IRType::Label), name(n) {
    }

    const std::string &GetName() const { return name; }

    // Instruction management
    IRInstruction *AddInstruction(std::unique_ptr<IRInstruction> inst) {
      auto *ptr = inst.get();
      instructions.push_back(std::move(inst));
      return ptr;
    }

    const std::vector<std::unique_ptr<IRInstruction>> &GetInstructions() const {
      return instructions;
    }

    // Predecessor/Successor management
    void AddPredecessor(IRBasicBlock *bb) { predecessors.push_back(bb); }
    void AddSuccessor(IRBasicBlock *bb) { successors.push_back(bb); }

    const std::vector<IRBasicBlock *> &GetPredecessors() const { return predecessors; }
    const std::vector<IRBasicBlock *> &GetSuccessors() const { return successors; }

    // Terminator
    void SetTerminator(std::unique_ptr<IRInstruction> inst) { terminator = std::move(inst); }

    IRInstruction *GetTerminator() const { return terminator.get(); }

    std::string ToString() const override { return name + ":"; }

  private:
    std::string name;
    std::vector<std::unique_ptr<IRInstruction>> instructions;
    std::vector<IRBasicBlock *> predecessors;
    std::vector<IRBasicBlock *> successors;
    std::unique_ptr<IRInstruction> terminator = nullptr;
  };

  //=============================================================================
  // IR Function
  //=============================================================================

  class IRFunction {
  public:
    IRFunction(const std::string &n, u64 addr) : name(n), address(addr) {}

    const std::string &GetName() const { return name; }
    u64 GetAddress() const { return address; }

    // Basic block management
    IRBasicBlock *CreateBasicBlock(const std::string &bbName) {
      auto *bb = new IRBasicBlock(bbName);
      basicBlocks.emplace_back(bb);
      return bb;
    }

    const std::vector<std::unique_ptr<IRBasicBlock>> &GetBasicBlocks() const {
      return basicBlocks;
    }

    IRBasicBlock *GetEntryBlock() const {
      return !basicBlocks.empty() ? basicBlocks[0].get() : nullptr;
    }

    // Take ownership of values from IRBuilder
    // This ensures constants and registers live as long as the function
    void TakeOwnership(std::vector<std::unique_ptr<IRValue>> &&values) {
      for (auto &v : values) {
        ownedValues.push_back(std::move(v));
      }
    }

    // Metadata
    void SetMetadata(const std::string &key, const std::string &value) {
      metadata[key] = value;
    }

    std::string ToString() const {
      std::string result = "function " + name + " {\n";
      for (const auto &bb : basicBlocks) {
        result += "  " + bb->ToString() + "\n";
        for (auto &inst : bb->GetInstructions()) {
          result += "    " + inst->ToString() + "\n";
        }
      }
      result += "}\n";
      return result;
    }

  private:
    std::string name;
    u64 address;
    std::vector<std::unique_ptr<IRBasicBlock>> basicBlocks;
    std::vector<std::unique_ptr<IRValue>> ownedValues;
    std::unordered_map<std::string, std::string> metadata;
  };

} // namespace Xe::XCPU::JIT::IR