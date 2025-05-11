// Copyright 2025 Xenon Emulator Project

#pragma once

#include <array>

#include "Core/XGPU/ShaderConstants.h"

#include "ASTNodeBase.h"
#include "Constants.h"

namespace Xe::Microcode::AST {

class ShaderCodeWriter;
class ExpressionNode;

class ExpressionNodeVisitor {
public:
  virtual ~ExpressionNodeVisitor() = default;
  virtual void OnExprStart(const ExpressionNode *node) = 0;
  virtual void OnExprEnd(const ExpressionNode *node) = 0;
};

// Expression node base
class ExpressionNode : public std::enable_shared_from_this<ExpressionNode> {
public:
  using Ptr = std::shared_ptr<ExpressionNode>;
  using Children = std::array<Ptr, 4>;

  virtual ~ExpressionNode() = default;

  virtual eExprType GetType() const { return eExprType::ALU; }
  virtual int GetRegisterIndex() const { return -1; }
  virtual Chunk EmitShaderCode(ShaderCodeWriter&) const = 0;

  virtual void Visit(ExpressionNodeVisitor &vistor) const {
    vistor.OnExprStart(this);
    for (const auto &child : children) {
      if (child)
        child->Visit(vistor);
    }
    vistor.OnExprEnd(this);
  }

protected:
  Children children{};
};

class ReadRegister : public ExpressionNode {
public:
  explicit ReadRegister(s32 index) :
    regIndex(index)
  {}
  s32 GetRegisterIndex() const override { return regIndex; }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  s32 regIndex = 0;
};

class WriteRegister : public ExpressionNode {
public:
  explicit WriteRegister(s32 index) :
    regIndex(index)
  {}
  s32 GetRegisterIndex() const override { return regIndex; }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  s32 regIndex = 0;
};

// Exported Register Write
class WriteExportRegister : public ExpressionNode {
public:
  explicit WriteExportRegister(eExportReg reg) :
    exportReg(reg)
  {}
  eExprType GetType() const override { return eExprType::EXPORT; }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;

  eExportReg GetExportReg() const { return exportReg; }
  static std::string GetExporRegName(const eExportReg reg);
  static std::string GetExporSemanticName(const eExportReg reg);
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
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  bool pixelShader = false;
  s32 index = 0;
};

// Float Constant
class FloatConstant : public ExpressionNode {
public:
  FloatConstant(bool ps, s32 idx) :
    pixelShader(ps), index(idx)
  {}
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  bool pixelShader = false;
  s32 index = 0;
};

class FloatRelativeConstant : public ExpressionNode {
public:
  FloatRelativeConstant(bool ps, s32 rel) :
    pixelShader(ps), relativeOffset(rel)
  {}
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  bool pixelShader = false;
  s32 relativeOffset = 0;
};

// Unary Operations
class GetPredicate : public ExpressionNode {
public:
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
};

class Abs : public ExpressionNode {
public:
  explicit Abs(Ptr expr)   {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
};

class Negate : public ExpressionNode {
public:
  explicit Negate(Ptr expr) {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
};

class Not : public ExpressionNode {
public:
  explicit Not(Ptr expr) {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
};

class Saturate : public ExpressionNode {
public:
  explicit Saturate(Ptr expr) {
    children[0] = std::move(expr);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
};

class Swizzle : public ExpressionNode {
public:
  Swizzle(Ptr base, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) {
    children[0] = std::move(base);
    swizzles[0] = x;
    swizzles[1] = y;
    swizzles[2] = z;
    swizzles[3] = w;
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  std::array<eSwizzle, 4> swizzles = {};
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
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;

private:
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
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;

private:
  u32 fetchSlot = 0;
  instr_dimension_t textureType{};
};

// Function Calls

class VectorFunc1 : public ExpressionNode {
public:
  VectorFunc1(const std::string_view &name, Ptr a) : funcName(name) {
    children[0] = std::move(a);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  std::string_view funcName = {};
};

class VectorFunc2 : public ExpressionNode {
public:
  VectorFunc2(const std::string_view &name, Ptr a, Ptr b) : funcName(name) {
    children[0] = std::move(a);
    children[1] = std::move(b);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  std::string_view funcName = {};
};

class VectorFunc3 : public ExpressionNode {
public:
  VectorFunc3(const std::string_view &name, Ptr a, Ptr b, Ptr c) : funcName(name) {
    children[0] = std::move(a);
    children[1] = std::move(b);
    children[2] = std::move(c);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  std::string_view funcName = {};
};

class ScalarFunc1 : public ExpressionNode {
public:
  ScalarFunc1(const std::string_view &name, Ptr a) : funcName(name) {
    children[0] = std::move(a);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  std::string_view funcName = {};
};

class ScalarFunc2 : public ExpressionNode {
public:
  ScalarFunc2(const std::string_view &name, Ptr a, Ptr b) : funcName(name) {
    children[0] = std::move(a);
    children[1] = std::move(b);
  }
  Chunk EmitShaderCode(ShaderCodeWriter&) const override;
private:
  std::string_view funcName = {};
};

} // namespace Xe::Microcode
