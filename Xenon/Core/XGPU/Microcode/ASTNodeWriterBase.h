// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Constants.h"
#include "ASTNode.h"
#include "ASTNodeBase.h"

namespace Xe::Microcode::AST {

class Expression {
public:
  Expression() = default;
  Expression(std::unique_ptr<NodeBase> base) : node(std::move(base)) {}
  Expression(const Expression& other)
    : node(other.node ? other.node->Clone() : nullptr) {}
  Expression& operator=(const Expression& other) {
    if (this != &other) {
      node = other.node ? other.node->Clone() : nullptr;
    }
    return *this;
  }

  std::string ToString() const {
    return node ? node->ToString() : "<empty>";
  }

  template <typename T = NodeBase>
  T* Get() const {
    return static_cast<T*>(node.get());
  }

  explicit operator bool() const {
    return static_cast<bool>(node);
  }

private:
  std::unique_ptr<NodeBase> node;
};

class Statement {
public:
  Statement() = default;
  Statement(std::unique_ptr<NodeBase> base) : node(std::move(base)) {}
  Statement(const Statement& other)
    : node(other.node ? other.node->Clone() : nullptr) {}
  Statement& operator=(const Statement& other) {
    if (this != &other) {
      node = other.node ? other.node->Clone() : nullptr;
    }
    return *this;
  }

  std::string ToString() const {
    return node ? node->ToString() : "<empty>";
  }

  template <typename T = NodeBase>
  T* Get() const {
    return static_cast<T*>(node.get());
  }

  explicit operator bool() const {
    return static_cast<bool>(node);
  }

private:
  std::unique_ptr<NodeBase> node;
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

  virtual Expression EmitVertexFetch(Expression src, u32 fetchSlot, u32 fetchOffset, u32 stride, instr_surf_fmt_t format, bool isFloat, bool isSigned, bool isNormalized) = 0;

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
  virtual Expression EmitVectorInstruction1(instr_vector_opc_t instr, Expression a) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitVectorInstruction2(instr_vector_opc_t instr, Expression a, Expression b) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitVectorInstruction3(instr_vector_opc_t instr, Expression a, Expression b, Expression c) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitScalarInstruction1(instr_scalar_opc_t instr, Expression a) = 0;
  // Wrapper for function evaluation
  virtual Expression EmitScalarInstruction2(instr_scalar_opc_t instr, Expression a, Expression b) = 0;

  //
  // Control flow blocks (top level)
  //
 
  // NOTE: Blocks are implicitly remembered inside
  virtual void EmitNop() = 0;
  virtual void EmitExec(const u32 codeAddr, instr_cf_opc_t type, Statement preamble, Statement code, Expression condition, const bool endOfShader) = 0;
  virtual void EmitJump(const u32 targetAddr, Statement preamble, Expression condition) = 0;
  virtual void EmitCall(const u32 targetAddr, Statement preamble, Expression condition) = 0;

  //
  // Exports
  //

  virtual void EmitExportAllocPosition() = 0;
  virtual void EmitExportAllocParam(const u32 size) = 0;
  virtual void EmitExportAllocMemExport(const u32 size) = 0;
};

} // namespace Xe::AST
