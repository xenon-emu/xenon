/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <map>
#include <vector>

#include "Constants.h"
#include "ASTNodeBase.h"
#include "ASTBlock.h"

namespace Xe::Microcode::AST {

class Expression : public std::enable_shared_from_this<Expression> {
public:
  using Ptr = std::shared_ptr<NodeBase>;

  virtual ~Expression() = default;

  Expression() :
    node(nullptr)
  {}

  template <typename T>
  Expression(std::shared_ptr<T> base) :
    node(std::move(base))
  {}

  Expression(const Expression &other)
    :
    node(other.node ? other.node->Clone() : nullptr)
  {}

  Expression& operator=(const Expression &other) {
    if (this != &other) {
      node = other.node ? other.node->Clone() : nullptr;
    }
    return *this;
  }

  template <typename T = NodeBase>
  std::shared_ptr<T> Get() {
    return std::dynamic_pointer_cast<T>(node);
  }

  explicit operator bool() const {
    return static_cast<bool>(node);
  }

private:
  std::shared_ptr<NodeBase> node;
};

class Statement : public std::enable_shared_from_this<Statement> {
public:
  using Ptr = std::shared_ptr<NodeBase>;

  virtual ~Statement() = default;

  Statement() :
    node(nullptr)
  {}

  template <typename T>
  Statement(std::shared_ptr<T> base) : node(std::move(base)) {}

  Statement(const Statement &other)
    : node(other.node ? other.node->Clone() : nullptr) {}

  Statement& operator=(const Statement &other) {
    if (this != &other) {
      node = other.node ? other.node->Clone() : nullptr;
    }
    return *this;
  }

  template <typename T = NodeBase>
  std::shared_ptr<T> Get() {
    return std::dynamic_pointer_cast<T>(node);
  }

  explicit operator bool() const {
    return static_cast<bool>(node);
  }

private:
  std::shared_ptr<NodeBase> node;
};

class NodeWriter {
public:
  NodeWriter() = default;
  ~NodeWriter() = default;
  //
  // Building Blocks
  //
  Expression EmitReadReg(u32 idx, eRegisterType type);
  Expression EmitWriteReg(bool pixelShader, u32 exported, u32 idx, eRegisterType type);
  // Access boolean constant
  Expression EmitBoolConst(bool pixelShader, u32 idx);
  // Access to float const table at given index
  Expression EmitFloatConst(bool pixelShader, u32 idx);
  // Access to float const table at given index relative to index register (a0)
  Expression EmitFloatConstRel(bool pixelShader, u32 regOffset);
  // Current predicate register
  Expression EmitGetPredicate();
  Expression EmitAbs(Expression code);
  Expression EmitNegate(Expression code);
  Expression EmitNot(Expression code);
  Expression EmitReadSwizzle(Expression src, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w);
  Expression EmitSaturate(Expression dest);

  //
  // Vector Data Fetch
  //
  Expression EmitVertexFetch(Expression src, u32 slot, u32 offset, u32 stride, instr_surf_fmt_t fmt, bool isFloat, bool isSigned, bool isNormalized);

  //
  // Texture Sample
  //
  Expression EmitTextureSample1D(Expression src, u32 slot);
  Expression EmitTextureSample2D(Expression src, u32 slot);
  Expression EmitTextureSample3D(Expression src, u32 slot);
  Expression EmitTextureSampleCube(Expression src, u32 slot);

  //
  // Statements
  //
  // Builds the list of statements to execute
  Statement EmitMergeStatements(Statement prev, Statement next);
  // Conditional wrapper around statement
  Statement EmitConditionalStatement(Expression condition, Statement code);
  // Writes the specified Expression to output with specified swizzles (the only general Expression -> statement conversion)
  Statement EmitWriteWithSwizzleStatement(Expression dest, Expression src, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w);
  Statement EmitSetPredicateStatement(Expression value); // sets new value for predicate

  //
  // Instructions
  //
  // Wrapper for function evaluation
  Expression EmitVectorInstruction1(instr_vector_opc_t instr, Expression a);
  // Wrapper for function evaluation
  Expression EmitVectorInstruction2(instr_vector_opc_t instr, Expression a, Expression b);
  // Wrapper for function evaluation
  Expression EmitVectorInstruction3(instr_vector_opc_t instr, Expression a, Expression b, Expression c);
  // Wrapper for function evaluation
  Expression EmitScalarInstruction0(instr_scalar_opc_t instr);
  // Wrapper for function evaluation
  Expression EmitScalarInstruction1(instr_scalar_opc_t instr, Expression a);
  // Wrapper for function evaluation
  Expression EmitScalarInstruction2(instr_scalar_opc_t instr, Expression a, Expression b);

  //
  // Control flow blocks (top level)
  //

  // NOTE: Blocks are implicitly remembered inside
  void EmitNop();
  void EmitExec(const u32 addr, instr_cf_opc_t type, Statement preamble, Statement code, Expression condition, const bool endOfShader);
  void EmitJump(const u32 addr, Statement preamble, Expression condition);
  void EmitCall(const u32 addr, Statement preamble, Expression condition);
  void EmitLoopStart(const u32 addr, Statement preamble, Expression condition);
  void EmitLoopEnd(const u32 addr, Expression condition);

  //
  // Exports
  //
  void EmitExportAllocPosition();
  void EmitExportAllocParam(const u32 size);
  void EmitExportAllocMemExport(const u32 size);
  u32 GetNumCreatedBlocks() { return createdBlocks.size(); }
  Block* GetCreatedBlock(u64 i) { return createdBlocks[i]; }
private:
  std::vector<Block*> createdBlocks = {};

  bool positionExported = false;
  u32 numParamExports = 0;
  u32 numMemoryExports = 0;
};

} // namespace Xe::Microcode::AST
