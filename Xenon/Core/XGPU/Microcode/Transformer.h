// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Constants.h"
#include "ASTNode.h"
#include "ASTNodeBase.h"

#include "Core/XGPU/ShaderConstants.h"

namespace Xe {
namespace Microcode {

enum class eSwizzle : u8 {
  // Read/Write
  X = 0,
  Y,
  Z,
  W,

  // Write Only
  Zero,    // 0: Component is forced to zero
  One,     // 1: Component is forced to one
  Ignored, // Don't care about this component
  Unused   // Masked out and not modified
};

enum class eVector : u32 {
  ADDv,               // 2 args
  MULv,               // 2 args
  MAXv,               // 2 args
  MINv,               // 2 args
  SETEv,              // 2 args
  SETGTv,             // 2 args
  SETGTEv,            // 2 args
  SETNEv,             // 2 args
  FRACv,              // 1 arg
  TRUNCv,             // 1 arg
  FLOORv,             // 1 arg
  MULADDv,            // 3 args
  CNDEv,              // 3 args
  CNDGTEv,            // 3 args
  CNDGTv,             // 3 args
  DOT4v,              // 2 args
  DOT3v,              // 2 args
  DOT2ADDv,           // 3 args
  CUBEv,              // 2 args
  MAX4v,              // 1 arg
  PRED_SETE_PUSHv,    // 2 args
  PRED_SETNE_PUSHv,   // 2 args
  PRED_SETGT_PUSHv,   // 2 args
  PRED_SETGTE_PUSHv,  // 2 args
  KILLEv,             // 2 args
  KILLGTv,            // 2 args
  KILLGTEv,           // 2 args
  KILLNEv,            // 2 args
  DSTv,               // 2 args
  MOVAv               // 1 arg
};

enum class eScalar : u32 {
  ADDs,              // 1 arg
  ADD_PREVs,         // 1 arg
  MULs,              // 1 arg
  MUL_PREVs,         // 1 arg
  MUL_PREV2s,        // 1 arg
  MAXs,              // 1 arg
  MINs,              // 1 arg
  SETEs,             // 1 arg
  SETGTs,            // 1 arg
  SETGTEs,           // 1 arg
  SETNEs,            // 1 arg
  FRACs,             // 1 arg
  TRUNCs,            // 1 arg
  FLOORs,            // 1 arg
  EXP_IEEE,          // 1 arg
  LOG_CLAMP,         // 1 arg
  LOG_IEEE,          // 1 arg
  RECIP_CLAMP,       // 1 arg
  RECIP_FF,          // 1 arg
  RECIP_IEEE,        // 1 arg
  RECIPSQ_CLAMP,     // 1 arg
  RECIPSQ_FF,        // 1 arg
  RECIPSQ_IEEE,      // 1 arg
  MOVAs,             // 1 arg
  MOVA_FLOORs,       // 1 arg
  SUBs,              // 1 arg
  SUB_PREVs,         // 1 arg
  PRED_SETEs,        // 1 arg
  PRED_SETNEs,       // 1 arg
  PRED_SETGTs,       // 1 arg
  PRED_SETGTEs,      // 1 arg
  PRED_SET_INVs,     // 1 arg
  PRED_SET_POPs,     // 1 arg
  PRED_SET_CLRs,     // 1 arg
  PRED_SET_RESTOREs, // 1 arg
  KILLEs,            // 1 arg
  KILLGTs,           // 1 arg
  KILLGTEs,          // 1 arg
  KILLNEs,           // 1 arg
  KILLONEs,          // 1 arg
  SQRT_IEEE,         // 1 arg
  UNKNOWN,           // Unknown
  MUL_CONST_0,       // 2 args
  MUL_CONST_1,       // 2 args
  ADD_CONST_0,       // 2 args
  ADD_CONST_1,       // 2 args
  SUB_CONST_0,       // 2 args
  SUB_CONST_1,       // 2 args
  SIN,               // 1 arg
  COS,               // 1 arg
  RETAIN_PREV        // 1 arg
};

enum class eFlowBlock : u32 {
  NOP,
  EXEC,
  EXEC_END,
  COND_EXEC,
  COND_EXEC_END,
  COND_PRED_EXEC,
  COND_PRED_EXEC_END,
  LOOP_START,
  LOOP_END,
  COND_CALL,
  RETURN,
  COND_JMP,
  ALLOC,
  COND_EXEC_PRED_CLEAN,
  COND_EXEC_PRED_CLEAN_END,
  MARK_VS_FETCH_DONE
};

enum class eFetchFormat : u32 {
  FMT_1_REVERSE = 0,
  FMT_8 = 2,
  FMT_8_8_8_8 = 6,
  FMT_2_10_10_10 = 7,
  FMT_8_8 = 10,
  FMT_16 = 24,
  FMT_16_16 = 25,
  FMT_16_16_16_16 = 26,
  FMT_16_16_FLOAT = 31,
  FMT_16_16_16_16_FLOAT = 32,
  FMT_32 = 33,
  FMT_32_32 = 34,
  FMT_32_32_32_32 = 35,
  FMT_32_FLOAT = 36,
  FMT_32_32_FLOAT = 37,
  FMT_32_32_32_32_FLOAT = 38,
  FMT_32_32_32_FLOAT = 57,
};

class Expression {
public:
  Expression() :
    node(nullptr)
  {}
  Expression(const Expression&other) {
    node = other.node ? other.node->Copy() : nullptr;
  }
  Expression(AST::NodeBase *base) :
    node(base)
  {}
  ~Expression() {
    Release();
  }

  std::string ToString() {
    return node ? node->ToString() : "<empty>";
  }

  Expression& operator=(const Expression &other) {
    if (node != other.node) {
      Release();

      if (other.node) {
        node = other.node;
      }
    }
    return *this;
  }

  template <typename T = AST::NodeBase>
  inline T* Get() {
    return reinterpret_cast<T*>(node);
  }
  inline operator bool() const {
    return node != nullptr;
  }
  inline void Release() {
    if (node) {
      node->Release();
      node = nullptr;
    }
  }
private:
  AST::NodeBase *node = nullptr;
};

class Statement {
public:
  Statement() :
    node(nullptr)
  {}
  Statement(const Statement &other) {
    node = other.node ? other.node->Copy() : nullptr;
  }
  Statement(AST::NodeBase *base) :
    node(base)
  {}
  ~Statement() {
    Release();
  }

  std::string ToString() {
    return node ? node->ToString() : "<empty>";
  }

  Statement& operator=(const Statement &other) {
    if (node != other.node) {
      Release();

      if (other.node) {
        node = other.node;
      }
    }
    return *this;
  }

  template <typename T = AST::NodeBase>
  inline T* Get() {
    return reinterpret_cast<T*>(node);
  }
  inline operator bool() const {
    return node != nullptr;
  }
  inline void Release() {
    if (node) {
      node->Release();
      node = nullptr;
    }
  }
private:
  AST::NodeBase *node = nullptr;
};

class NodeWriterBase {
public:
  virtual ~NodeWriterBase() = default;

  //
  // Building Blocks
  //

  virtual Expression EmitReadReg(u32 regIndex) = 0;
  virtual Expression EmitWriteReg(bool pixelShader, u32 exported, u32 regIndex) = 0;
  // Access boolean constant
  virtual Expression EmitBoolConst(bool pixelShader, u32 constIndex) = 0;
  // Access to float const table at given index
  virtual Expression EmitFloatConst(bool pixelShader, u32 regIndex) = 0;
  // Access to float const table at given index relative to index register (a0)
  virtual Expression EmitFloatConstRel(bool pixelShader, u32 regOffset) = 0;
  // Current predicate register
  virtual Expression EmitGetPredicate() = 0;
  virtual Expression EmitAbs(Expression code) = 0;
  virtual Expression EmitNegate(Expression code) = 0;
  virtual Expression EmitNot(Expression code) = 0;
  virtual Expression EmitReadSwizzle(Expression src, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) = 0;
  virtual Expression EmitSaturate(Expression dest) = 0;

  //
  // Vector Data Fetch
  //

  virtual Expression EmitVertexFetch(Expression src, u32 fetchSlot, u32 fetchOffset, u32 stride, eFetchFormat format, bool isFloat, bool isSigned, bool isNormalized) = 0;

  //
  // Texture Sample
  //

  virtual Expression EmitTextureSample1D(Expression src, u32 fetchSlot) = 0;
  virtual Expression EmitTextureSample2D(Expression src, u32 fetchSlot) = 0;
  virtual Expression EmitTextureSample3D(Expression src, u32 fetchSlot) = 0;
  virtual Expression EmitTextureSampleCube(Expression src, u32 fetchSlot) = 0;

  //
  // Statements
  //

  // Builds the list of statements to execute
  virtual Statement EmitMergeStatements(Statement prev, Statement next) = 0;
  // Conditional wrapper around statement
  virtual Statement EmitConditionalStatement(Expression condition, Statement code) = 0;
  // Writes the specified Expression to output with specified swizzles (the only general Expression -> statement conversion)
  virtual Statement EmitWriteWithSwizzleStatement(Expression dest, Expression src, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) = 0;
  virtual Statement EmitSetPredicateStatement(Expression value) = 0; // sets new value for predicate

  //
  // Instructions
  //

  // Wrapper for function evaluation
  virtual Expression EmitVectorInstruction1(eVector instr, Expression a) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitVectorInstruction2(eVector instr, Expression a, Expression b) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitVectorInstruction3(eVector instr, Expression a, Expression b, Expression c) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitScalarInstruction1(eScalar instr, Expression a) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitScalarInstruction2(eScalar instr, Expression a, Expression b) = 0;

  //
  // Control flow blocks (top level)
  //
 
  // NOTE: Blocks are implicitly remembered inside
  virtual void EmitNop() = 0;
  virtual void EmitExec(const u32 codeAddr, eFlowBlock type, Statement preamble, Statement code, Expression condition, const bool endOfShader) = 0;
  virtual void EmitJump(const u32 targetAddr, Statement preamble, Expression condition) = 0;
  virtual void EmitCall(const u32 targetAddr, Statement preamble, Expression condition) = 0;

  //
  // Exports
  //

  virtual void EmitExportAllocPosition() = 0;
  virtual void EmitExportAllocParam(const u32 size) = 0;
  virtual void EmitExportAllocMemExport(const u32 size) = 0;
};

} // namespace Microcode

} // namespace Xe