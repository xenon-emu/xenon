/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Base/Types.h"
#include "Base/Vector128.h"

//=============================================================================
// IR Types
// ----------------------------------------------------------------------------
// Defines various types used in the JIT Intermediate Representation
// The flow goes as follows:
// PPC Code -> IR -> Optimization Layer -> Runtime Code Emitter (ARM/X86_64)
//=============================================================================

namespace Xe::XCPU::JIT::IR {

  // Forward declarations
  class IRValue;
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
    SPR,
    LR,     // Link Register
    CTR,    // Count Register
    XER,    // Fixed-Point Exception Register
    CR,     // Condition Register
    FPSCR,  // Floating-Point Status and Control Register
    VSCR,   // Vector Status and Control Register

    // System Registers
    MSR,    // Machine State Register
    SRR0,   // Save/Restore Register 0
    SRR1,   // Save/Restore Register 1
    DAR,    // Data Address Register
    DSISR,  // Data Storage Interrupt Status Register

    // Virtual/temporary register
    Temp,
  };

  inline const char *IRRegisterTypeToString(IRRegisterType type) {
    switch (type) {
    case IRRegisterType::GPR: return "GPR";
    case IRRegisterType::FPR: return "FPR";
    case IRRegisterType::VR: return "VR";
    case IRRegisterType::LR: return "LR";
    case IRRegisterType::CTR: return "CTR";
    case IRRegisterType::XER: return "XER";
    case IRRegisterType::CR: return "CR";
    case IRRegisterType::FPSCR: return "FPSCR";
    case IRRegisterType::VSCR: return "VSCR";
    case IRRegisterType::MSR: return "MSR";
    case IRRegisterType::SRR0: return "SRR0";
    case IRRegisterType::SRR1: return "SRR1";
    case IRRegisterType::DAR: return "DAR";
    case IRRegisterType::DSISR: return "DSISR";
    case IRRegisterType::Temp: return "TEMP";
    }
    return "Unknown";
  }

  //=============================================================================
  // IR Opcodes
  //=============================================================================

  // Basic operation performed by the IR
  // PPC Instructions should be easily mapped to this
  // TODO: Fix this, there are unnecesary ops here.
  enum class IROp : u16 {
    // Control Flow
    Comment,        // Debug Comment
    Nop,            // No operation
    Branch,         // Unconditional branch
    BranchCond,     // Conditional branch
    Call,           // Function call
    Return,         // Return from function

    // Memory Operations
    Load,           // Load from memory
    Store,          // Store to memory
    LoadReg,        // Load from PPC register
    StoreReg,       // Store to PPC register
    CondLoad,       // Conditional load (lwarx/ldwarx)
    CondStore,      // Conditional Store (stwcx/stdcx)

    // Arithmetic (Integer)
    Add,            // Addition
    Sub,            // Subtraction
    Mul,            // Multiplication
    DivS,           // Division (signed)
    DivU,           // Division (unsigned)
    ModS,           // Modulo (signed)
    ModU,           // Modulo (unsigned)
    Neg,            // Negation

    // Arithmetic (Floating Point)
    FAdd,          // FP Addition
    FSub,          // FP Subtraction
    FMul,          // FP Multiplication
    FDiv,          // FP Division
    FNeg,  // FP Negation
    FAbs,          // FP Absolute value
    FSqrt,         // FP Square root
    FMA,           // Fused multiply-add

    // Bitwise Operations
    And,   // Bitwise AND
    Or,            // Bitwise OR
    Xor,        // Bitwise XOR
    Not,           // Bitwise NOT
    Shl,    // Shift left
    Shr, // Logical shift right
    Sar,// Arithmetic shift right
    Rotl,       // Rotate left
    Rotr,    // Rotate right

    // Comparison
    Cmp,         // Compare (signed)
    CmpU,          // Compare (unsigned)
    FCmp,     // FP Compare

    // Type Conversions
    ZExt,          // Zero extend
    SExt,        // Sign extend
    Trunc,         // Truncate
    FPToSI,        // Float to signed int
    FPToUI,     // Float to unsigned int
    SIToFP,        // Signed int to float
    UIToFP,        // Unsigned int to float
    FPExt,       // FP extend (f32 -> f64)
    FPTrunc,     // FP truncate (f64 -> f32)
    Bitcast,  // Bitcast (reinterpret bits)

    // Vector Operations (VMX/AltiVec)
    VAdd,          // Vector add
    VSub,          // Vector subtract
    VMul,          // Vector multiply
    VDiv,          // Vector divide
    VAnd,    // Vector AND
    VOr,           // Vector OR
    VXor,          // Vector XOR
    VSplat,        // Vector splat (broadcast scalar)
    VExtract,      // Extract vector element
    VInsert,       // Insert vector element
    VShuffle,      // Vector shuffle

    // Special PPC Operations
    CountLeadingZeros,  // Count leading zeros (cntlzw/cntlzd)
    ExtractBits,        // Extract bit field
    InsertBits,   // Insert bit field

    // Condition Register Operations
    CRSetBit,      // Set CR bit
    CRGetBit,      // Get CR bit
    CRAnd,    // CR AND
    CROr,          // CR OR
    CRXor,         // CR XOR
    CRNand,        // CR NAND
    CRNor,         // CR NOR

    // System/Special Operations
    Syscall,       // System call
    Trap,  // Trap instruction
    Sync,          // Synchronize
    Isync,         // Instruction synchronize

    // Intrinsics (for direct PPC instruction mapping when needed)
    Intrinsic,     // Generic intrinsic call
  };

  inline const char *IROpToString(IROp op) {
    switch (op) {
    case IROp::Comment: return "comment";
    case IROp::Nop: return "nop";
    case IROp::Branch: return "br";
    case IROp::BranchCond: return "br_cond";
    case IROp::Call: return "call";
    case IROp::Return: return "ret";
    case IROp::Load: return "load";
    case IROp::Store: return "store";
    case IROp::LoadReg: return "load_reg";
    case IROp::StoreReg: return "store_reg";
    case IROp::CondLoad: return "cond_load";
    case IROp::CondStore: return "cond_store";
    case IROp::Add: return "add";
    case IROp::Sub: return "sub";
    case IROp::Mul: return "mul";
    case IROp::DivS: return "div";
    case IROp::DivU: return "divu";
    case IROp::ModS: return "mod";
    case IROp::ModU: return "modu";
    case IROp::Neg: return "neg";
    case IROp::FAdd: return "fadd";
    case IROp::FSub: return "fsub";
    case IROp::FMul: return "fmul";
    case IROp::FDiv: return "fdiv";
    case IROp::FNeg: return "fneg";
    case IROp::FAbs: return "fabs";
    case IROp::FSqrt: return "fsqrt";
    case IROp::FMA: return "fma";
    case IROp::And: return "and";
    case IROp::Or: return "or";
    case IROp::Xor: return "xor";
    case IROp::Not: return "not";
    case IROp::Shl: return "shl";
    case IROp::Shr: return "shr";
    case IROp::Sar: return "sar";
    case IROp::Rotl: return "rotl";
    case IROp::Rotr: return "rotr";
    case IROp::Cmp: return "cmp";
    case IROp::CmpU: return "cmpu";
    case IROp::FCmp: return "fcmp";
    case IROp::ZExt: return "zext";
    case IROp::SExt: return "sext";
    case IROp::Trunc: return "trunc";
    case IROp::FPToSI: return "fptosi";
    case IROp::FPToUI: return "fptoui";
    case IROp::SIToFP: return "sitofp";
    case IROp::UIToFP: return "uitofp";
    case IROp::FPExt: return "fpext";
    case IROp::FPTrunc: return "fptrunc";
    case IROp::Bitcast: return "bitcast";
    case IROp::VAdd: return "vadd";
    case IROp::VSub: return "vsub";
    case IROp::VMul: return "vmul";
    case IROp::VDiv: return "vdiv";
    case IROp::VAnd: return "vand";
    case IROp::VOr: return "vor";
    case IROp::VXor: return "vxor";
    case IROp::VSplat: return "vsplat";
    case IROp::VExtract: return "vextract";
    case IROp::VInsert: return "vinsert";
    case IROp::VShuffle: return "vshuffle";
    case IROp::CountLeadingZeros: return "clz";
    case IROp::ExtractBits: return "extract_bits";
    case IROp::InsertBits: return "insert_bits";
    case IROp::CRSetBit: return "cr_set";
    case IROp::CRGetBit: return "cr_get";
    case IROp::CRAnd: return "crand";
    case IROp::CROr: return "cror";
    case IROp::CRXor: return "crxor";
    case IROp::CRNand: return "crnand";
    case IROp::CRNor: return "crnor";
    case IROp::Syscall: return "syscall";
    case IROp::Trap: return "trap";
    case IROp::Sync: return "sync";
    case IROp::Isync: return "isync";
    case IROp::Intrinsic: return "intrinsic";
    }
    return "unknown";
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
  // IR Value
  //=============================================================================

  // Represents a value in the IR (SSA form)
  // All instructions in the IR return and use such value
  // This allows full SSA to be performed as well as code/comp opts later on
  class IRValue {
  public:
    // The kind of IR Value
    enum class ValueKind : u8 {
      Instruction,      // Result of an instruction
      ConstantInt,      // Constant int value
      ConstantFloat,    // Constant float value
      ConstantVec128,   // Constant vec value
      Register,         // PPC register reference
      BasicBlock,       // Basic block label
      Argument,         // Function argument
    };

    IRValue(ValueKind k, IRType t) : kind(k), type(t), id(nextId++) {}
    virtual ~IRValue() = default;

    ValueKind GetKind() const { return kind; }
    IRType GetType() const { return type; }
    u32 GetId() const { return id; }

    // For use tracking
    void AddUse(IRInstruction *inst) { uses.push_back(inst); }
    const std::vector<IRInstruction *> &GetUses() const { return uses; }

    virtual std::string ToString() const {
      return std::string("%") + std::to_string(id) + " : " + IRTypeToString(type);
    }

  protected:
    ValueKind kind;
    IRType type;
    u32 id;
    std::vector<IRInstruction *> uses;

    static inline u32 nextId = 0;
  };

  //=============================================================================
  // Constant Values
  //=============================================================================

  // These represent values that never change, such as instruction immediates

  // Integer Constants (8/16/32/64 bits)
  class IRConstantInt : public IRValue {
  public:
    IRConstantInt(IRType t, u64 val) : IRValue(ValueKind::ConstantInt, t), value(val) {}

    u64 GetValue() const { return value; }

    std::string ToString() const override {
      return std::string(IRTypeToString(type)) + " " + std::to_string(value);
    }

  private:
    u64 value;
  };

  // Float 32 Constants
  class IRConstantFloat32 : public IRValue {
  public:
    IRConstantFloat32(IRType t, f32 val) : IRValue(ValueKind::ConstantFloat, t), value(val) {}

    u64 GetValue() const { return value; }

    std::string ToString() const override {
      return std::string(IRTypeToString(type)) + " " + std::to_string(value);
    }

  private:
    f32 value;
  };

  // Float 64 Constants
  class IRConstantFloat64 : public IRValue {
  public:
    IRConstantFloat64(IRType t, f64 val) : IRValue(ValueKind::ConstantFloat, t), value(val) {}

    f64 GetValue() const { return value; }

    std::string ToString() const override {
      return std::string(IRTypeToString(type)) + " " + std::to_string(value);
    }

  private:
    f64 value;
  };

  // Vector 128 Constants
  class IRConstantVec128 : public IRValue {
  public:
    IRConstantVec128(IRType t, Base::Vector128 val) : IRValue(ValueKind::ConstantVec128, t), value(val) {}

    Base::Vector128 GetValue() const { return value; }

    std::string ToString() const override {
      return std::string(IRTypeToString(type)) + " X = [" + std::to_string(value.x) + "] | Y = [" + std::to_string(value.y)
        + "] | Z = [" + std::to_string(value.z) + "] | W = [" + std::to_string(value.w);
    }

  private:
    Base::Vector128 value;
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
    IRInstruction(IROp o, IRType t) : IRValue(ValueKind::Instruction, t), op(o) {}

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
      std::string result = "%" + std::to_string(id) + " = " + IROpToString(op);
      for (auto *op : operands) {
        if (op) {
          result += " %" + std::to_string(op->GetId());
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