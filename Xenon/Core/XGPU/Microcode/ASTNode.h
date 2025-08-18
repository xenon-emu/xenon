// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <array>
#include <memory>

#include "Core/XGPU/ShaderConstants.h"

#include "ASTNodeBase.h"
#include "Constants.h"

namespace Xe::Microcode::AST {

// A key to uniquely identify a vertex attribute definition from a VTX_FETCH instruction.
struct VertexFetchKey {
  u32 slot;
  u32 offset;
  u32 stride;
  instr_surf_fmt_t format;

  bool operator<(const VertexFetchKey &other) const {
    return std::tie(slot, offset, stride, format) < std::tie(other.slot, other.offset, other.stride, other.format);
  }
};

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
  virtual std::string GetName() const { return "ExpressionNode"; }
  virtual s32 GetRegisterIndex() const { return -1; }
  virtual Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) = 0;
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

enum class eRegisterType : u8 {
  Temporary,
  Constant,
  VertexInput,
  PixelInput,
  Invalid
};

class ReadRegister : public ExpressionNode {
public:
  ReadRegister(s32 index, eRegisterType type = eRegisterType::Temporary) :
    regIndex(index), regType(type)
  {}
  s32 GetRegisterIndex() const override { return regIndex; }
  std::string GetName() const override {
    return "ReadRegister";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<ReadRegister>(regIndex);
  }

  s32 regIndex = 0;
  eRegisterType regType = eRegisterType::Temporary;
};

class WriteRegister : public ExpressionNode {
public:
  WriteRegister(s32 index, eRegisterType type = eRegisterType::Temporary) :
    regIndex(index), regType(type)
  {}
  s32 GetRegisterIndex() const override { return regIndex; }
  std::string GetName() const override {
    return "WriteRegister";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<WriteRegister>(regIndex, regType);
  }

  s32 regIndex = 0;
  eRegisterType regType = eRegisterType::Temporary;
};

// Exported Register Write
class WriteExportRegister : public ExpressionNode {
public:
  WriteExportRegister(eExportReg reg) :
    exportReg(reg)
  {}
  eExprType GetType() const override { return eExprType::EXPORT; }
  std::string GetName() const override {
    return "WriteExportRegister";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<WriteExportRegister>(exportReg);
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
  std::string GetName() const override {
    return "BoolConstant";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<BoolConstant>(pixelShader, index);
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
  std::string GetName() const override {
    return "FloatConstant";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<FloatConstant>(pixelShader, index);
  }

  bool pixelShader = false;
  s32 index = 0;
};

class FloatRelativeConstant : public ExpressionNode {
public:
  FloatRelativeConstant(bool ps, s32 rel) :
    pixelShader(ps), relativeOffset(rel)
  {}
  std::string GetName() const override {
    return "FloatRelativeConstant";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<FloatRelativeConstant>(pixelShader, relativeOffset);
  }

  bool pixelShader = false;
  s32 relativeOffset = 0;
};

// Unary Operations
class GetPredicate : public ExpressionNode {
public:
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_unique<GetPredicate>(*this);
  }
};

class Abs : public ExpressionNode {
public:
  Abs(Ptr expr)   {
    children[0] = std::move(expr);
  }
  std::string GetName() const override {
    return "Abs";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<Abs>(children[0]->CloneExpr());
  }
};

class Negate : public ExpressionNode {
public:
  Negate(Ptr expr) {
    children[0] = std::move(expr);
  }
  std::string GetName() const override {
    return "Negate";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<Negate>(children[0]->CloneExpr());
  }
};

class Not : public ExpressionNode {
public:
  Not(Ptr expr) {
    children[0] = std::move(expr);
  }
  std::string GetName() const override {
    return "Not";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<Not>(children[0]->CloneExpr());
  }
};

class Saturate : public ExpressionNode {
public:
  Saturate(Ptr expr) {
    children[0] = std::move(expr);
  }
  std::string GetName() const override {
    return "Saturate";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<Saturate>(children[0]->CloneExpr());
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
  std::string GetName() const override {
    return "Swizzle";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<Swizzle>(children[0]->CloneExpr(), swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
  }

  std::array<eSwizzle, 4> swizzle = {};
};

enum class VertexFetchResultType {
  Float,
  Int,
  UInt,
  Unknown
};

// Vertex/Texture Fetch
class VertexFetch : public ExpressionNode {
public:
  VertexFetch(Ptr src, u32 slot, u32 offset, u32 stride, instr_surf_fmt_t fmt, bool isF, bool isS, bool isN) :
    fetchSlot(slot), fetchOffset(offset), fetchStride(stride),
    format(fmt), isFloat(isF), isSigned(isS), isNormalized(isN) {
    children[0] = std::move(src);
  }

  std::string GetName() const override {
    return "VertexFetch";
  }
  eExprType GetType() const override { return eExprType::VFETCH; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<VertexFetch>(children[0]->CloneExpr(), fetchSlot, fetchOffset, fetchStride, format, isFloat, isSigned, isNormalized);
  }

  u32 GetComponentCount() const {
    switch (format) {
      case FMT_8:
      case FMT_16:
      case FMT_32:
      case FMT_16_FLOAT:
      case FMT_32_FLOAT:
        return 1;
      case FMT_8_8:
      case FMT_16_16:
      case FMT_32_32:
        return 2;
      case FMT_8_8_8_8:
      case FMT_2_10_10_10:
      case FMT_16_16_16_16:
      case FMT_32_32_32_32:
        return 4;
      case FMT_32_32_32_FLOAT:
        return 3;
      default:
        return 4;
    }
  }

  VertexFetchResultType GetResultType() const {
    if (isFloat)
      return VertexFetchResultType::Float;
    if (isSigned)
      return VertexFetchResultType::Int;
    return VertexFetchResultType::UInt;
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

  std::string GetName() const override {
    return "TextureFetch";
  }
  eExprType GetType() const override { return eExprType::TFETCH; }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<TextureFetch>(children[0]->CloneExpr(), fetchSlot, textureType);
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

  std::string GetName() const override {
    return "VectorFunc1";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<VectorFunc1>(vectorInstr, children[0]->CloneExpr());
  }

  instr_vector_opc_t vectorInstr = {};
};

class VectorFunc2 : public ExpressionNode {
public:
  VectorFunc2(instr_vector_opc_t instr, Ptr a, Ptr b) : vectorInstr(instr) {
    children[0] = std::move(a);
    children[1] = std::move(b);
  }

  std::string GetName() const override {
    return "VectorFunc2";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<VectorFunc2>(vectorInstr, children[0]->CloneExpr(), children[1]->CloneExpr());
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

  std::string GetName() const override {
    return "VectorFunc3";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<VectorFunc3>(vectorInstr, children[0]->CloneExpr(), children[1]->CloneExpr(), children[2]->CloneExpr());
  }

  instr_vector_opc_t vectorInstr = {};
};

class ScalarFunc0 : public ExpressionNode {
public:
  ScalarFunc0(instr_scalar_opc_t instr) :
    scalarInstr(instr)
  {}

  std::string GetName() const override {
    return "ScalarFunc0";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<ScalarFunc0>(scalarInstr);
  }

  instr_scalar_opc_t scalarInstr = {};
};

class ScalarFunc1 : public ExpressionNode {
public:
  ScalarFunc1(instr_scalar_opc_t instr, Ptr a) : scalarInstr(instr) {
    children[0] = std::move(a);
  }

  std::string GetName() const override {
    return "ScalarFunc1";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<ScalarFunc1>(scalarInstr, children[0]->CloneExpr());
  }

  instr_scalar_opc_t scalarInstr = {};
};

class ScalarFunc2 : public ExpressionNode {
public:
  ScalarFunc2(instr_scalar_opc_t instr, Ptr a, Ptr b) : scalarInstr(instr) {
    children[0] = std::move(a);
    children[1] = std::move(b);
  }

  std::string GetName() const override {
    return "ScalarFunc2";
  }
  Chunk EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) override;
  std::shared_ptr<ExpressionNode> CloneExpr() const override {
    return std::make_shared<ScalarFunc2>(scalarInstr, children[0]->CloneExpr(), children[1]->CloneExpr());
  }

  instr_scalar_opc_t scalarInstr = {};
};

} // namespace Xe::Microcode::AST
