// Copyright 2025 Xenon Emulator Project

#pragma once

#include <array>
#include <memory>

#include "../ShaderConstants.h"

#include "ASTNodeBase.h"
#include "Constants.h"

namespace Xe::Microcode::AST {

// Forward declare
class ShaderCodeWriterBase;

// Expression node base
class ExpressionNode : public NodeBase, public std::enable_shared_from_this<ExpressionNode> {
public:
  using Ptr = std::shared_ptr<ExpressionNode>;
  using Children = std::array<Ptr, 4>;
  class Visitor {
  public:
    virtual ~Visitor() = default;
    virtual void OnExprStart(Ptr node) = 0;
    virtual void OnExprEnd(Ptr node) = 0;
  };

  virtual ~ExpressionNode() = default;
  virtual eExprType GetType() const { return eExprType::ALU; }
  virtual int GetRegisterIndex() const { return -1; }
  virtual Chunk EmitShaderCode(ShaderCodeWriterBase &writer) = 0;
  virtual std::shared_ptr<ExpressionNode> CloneExpr() const = 0;
  std::shared_ptr<NodeBase> Clone() const override {
    return CloneExpr();
  }

  virtual void Visit(Visitor &vistor) const {
    Ptr self = std::const_pointer_cast<ExpressionNode>(shared_from_this());
    vistor.OnExprStart(self);
    for (const auto &child : children) {
      if (child)
        child->Visit(vistor);
    }
    vistor.OnExprEnd(self);
  }

protected:
  Children children{};
};

class ReadRegister : public ExpressionNode {
public:
  ReadRegister(s32 index) :
    regIndex(index)
  {}
  s32 GetRegisterIndex() const override { return regIndex; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<ReadRegister>(*this);
  }

  s32 regIndex = 0;
};

class WriteRegister : public ExpressionNode {
public:
  WriteRegister(s32 index) :
    regIndex(index)
  {}
  s32 GetRegisterIndex() const override { return regIndex; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<WriteRegister>(*this);
  }
private:
  s32 regIndex = 0;
};

// Exported Register Write
class WriteExportRegister : public ExpressionNode {
public:
  WriteExportRegister(eExportReg reg) :
    exportReg(reg)
  {}
  eExprType GetType() const override { return eExprType::EXPORT; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<WriteExportRegister>(*this);
  }

  eExportReg GetExportReg() const { return exportReg; }
  static s32 GetExportSemanticIndex(const eExportReg reg);
  static s32 GetExportInterpolatorIndex(const eExportReg reg);
private:
  eExportReg exportReg{};
};

// Boolean Constant
class BoolConstant : public ExpressionNode {
public:
  BoolConstant(bool ps, s32 idx) :
    pixelShader(ps), index(idx)
  {}
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<BoolConstant>(*this);
  }

  bool pixelShader = false;
  s32 index = 0;
};

// Float Constant
class FloatConstant : public ExpressionNode {
public:
  FloatConstant(bool ps, s32 idx) :
    pixelShader(ps), index(idx)
  {}
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<FloatConstant>(*this);
  }

  bool pixelShader = false;
  s32 index = 0;
};

class FloatRelativeConstant : public ExpressionNode {
public:
  FloatRelativeConstant(bool ps, s32 rel) :
    pixelShader(ps), relativeOffset(rel)
  {}
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<FloatRelativeConstant>(*this);
  }

  bool pixelShader = false;
  s32 relativeOffset = 0;
};

// Unary Operations
class GetPredicate : public ExpressionNode {
public:
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<GetPredicate>(*this);
  }
};

class Abs : public ExpressionNode {
public:
  Abs(Ptr expr)   {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<Abs>(*this);
  }
};

class Negate : public ExpressionNode {
public:
  Negate(Ptr expr) {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<Negate>(*this);
  }
};

class Not : public ExpressionNode {
public:
  Not(Ptr expr) {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<Not>(*this);
  }
};

class Saturate : public ExpressionNode {
public:
  Saturate(Ptr expr) {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<Saturate>(*this);
  }
};

class Swizzle : public ExpressionNode {
public:
  Swizzle(Ptr base, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) {
    children[0] = std::move(base);
    swizzle[0] = x;
    swizzle[1] = y;
    swizzle[2] = z;
    swizzle[3] = w;
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<Swizzle>(*this);
  }

  std::array<eSwizzle, 4> swizzle = {};
};

// Vertex/Texture Fetch
class VertexFetch : public ExpressionNode {
public:
  VertexFetch(Ptr src, u32 slot, u32 offset, u32 stride, instr_surf_fmt_t fmt, bool isF, bool isS, bool isN) :
    fetchSlot(slot), fetchOffset(offset), fetchStride(stride),
    format(fmt), isFloat(isF), isSigned(isS), isNormalized(isN) {
    children[0] = std::move(src);
  }

  eExprType GetType() const override { return eExprType::VFETCH; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<VertexFetch>(*this);
  }

  u32 fetchSlot = 0, fetchOffset = 0, fetchStride = 0;
  instr_surf_fmt_t format{};
  bool isFloat = false, isSigned = false, isNormalized = false;
};

class TextureFetch : public ExpressionNode {
public:
  TextureFetch(Ptr src, u32 slot, instr_dimension_t type) :
    fetchSlot(slot), textureType(type) {
    children[0] = std::move(src);
  }

  eExprType GetType() const override { return eExprType::TFETCH; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<TextureFetch>(*this);
  }

  u32 fetchSlot = 0;
  instr_dimension_t textureType{};
};

// Function Calls

class VectorFunc1 : public ExpressionNode {
public:
  VectorFunc1(instr_vector_opc_t instr, Ptr a) : vectorInstr(instr) {
    children[0] = std::move(a);
  }

  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<VectorFunc1>(*this);
  }

  instr_vector_opc_t vectorInstr = {};
};

class VectorFunc2 : public ExpressionNode {
public:
  VectorFunc2(instr_vector_opc_t instr, Ptr a, Ptr b) : vectorInstr(instr) {
    children[0] = std::move(a);
    children[1] = std::move(b);
  }

  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<VectorFunc2>(*this);
  }

  instr_vector_opc_t vectorInstr = {};
};

class VectorFunc3 : public ExpressionNode {
public:
  VectorFunc3(instr_vector_opc_t instr, Ptr a, Ptr b, Ptr c) : vectorInstr(instr) {
    children[0] = std::move(a);
    children[1] = std::move(b);
    children[2] = std::move(c);
  }

  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<VectorFunc3>(*this);
  }

  instr_vector_opc_t vectorInstr = {};
};

class ScalarFunc1 : public ExpressionNode {
public:
  ScalarFunc1(instr_scalar_opc_t instr, Ptr a) : scalarInstr(instr) {
    children[0] = std::move(a);
  }

  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<ScalarFunc1>(*this);
  }

  instr_scalar_opc_t scalarInstr = {};
};

class ScalarFunc2 : public ExpressionNode {
public:
  ScalarFunc2(instr_scalar_opc_t instr, Ptr a, Ptr b) : scalarInstr(instr) {
    children[0] = std::move(a);
    children[1] = std::move(b);
  }

  Chunk EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<ScalarFunc2>(*this);
  }

  instr_scalar_opc_t scalarInstr = {};
};

} // namespace Xe::Microcode::AST
