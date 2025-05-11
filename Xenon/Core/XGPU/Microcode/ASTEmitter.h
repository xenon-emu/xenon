// Copyright 2025 Xenon Emulator Project

#pragma once

#include <array>
#include <span>

#include "ASTNodeBase.h"
#include "ASTNode.h"

namespace Xe::Microcode::AST {

class ShaderCodeWriterBase {
public:
  virtual ~ShaderCodeWriterBase() = default;

  virtual Chunk GetExportDest(const eExportReg reg) = 0;
  virtual Chunk GetReg(u32 regIndex) = 0;
  virtual Chunk GetBoolVal(const u32 boolRegIndex) = 0;
  virtual Chunk GetFloatVal(const u32 floatRegIndex) = 0;
  virtual Chunk GetFloatValRelative(const u32 floatRegOffset) = 0;
  virtual Chunk GetPredicate() = 0;

  virtual Chunk Abs(ExpressionNode *value) = 0;
  virtual Chunk Negate(ExpressionNode *value) = 0;
  virtual Chunk Not(ExpressionNode *value) = 0;
  virtual Chunk Saturate(ExpressionNode *value) = 0;
  virtual Chunk Swizzle(ExpressionNode *value, std::array<eSwizzle, 4> swizzle) = 0;

  virtual Chunk FetchVertex(const Chunk &src, const VertexFetch &instr) = 0;
  virtual Chunk FetchTexture(const Chunk &src, const TextureFetch &instr) = 0;

  virtual Chunk VectorFunc1(instr_vector_opc_t instr, ExpressionNode *arg1) = 0;
  virtual Chunk VectorFunc2(instr_vector_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2) = 0;
  virtual Chunk VectorFunc3(instr_vector_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2, ExpressionNode *arg3) = 0;

  virtual Chunk ScalarFunc1(instr_scalar_opc_t instr, ExpressionNode *arg1) = 0;
  virtual Chunk ScalarFunc2(instr_scalar_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2) = 0;

  virtual Chunk AllocLocalVector(const Chunk &initCode) = 0;
  virtual Chunk AllocLocalScalar(const Chunk &initCode) = 0;
  virtual Chunk AllocLocalBool(const Chunk &initCode) = 0;

  virtual void BeingCondition(const Chunk &condition) = 0;
  virtual void EndCondition() = 0;

  virtual void BeginControlFlow(const u32 address, const bool hasJumps, const bool hasCalls, const bool called) = 0;
  virtual void EndControlFlow() = 0;

  virtual void BeginBlockWithAddress(const u32 address) = 0;
  virtual void EndBlockWithAddress() = 0;

  virtual void ControlFlowEnd() = 0;
  virtual void ControlFlowReturn(const u32 targetAddress) = 0;
  virtual void ControlFlowCall(const u32 targetAddress) = 0;
  virtual void ControlFlowJump(const u32 targetAddress) = 0;

  virtual void SetPredicate(const Chunk &newValue) = 0;
  virtual void PushPredicate(const Chunk &newValue) = 0;
  virtual void PopPredicate() = 0;

  virtual void Assign(const Chunk &dest, const Chunk &src) = 0;
  virtual void Emit(const Chunk &src) = 0;

  virtual void AssignMasked(ExpressionNode* src, ExpressionNode* dst,
    std::span<const eSwizzle> dstSwizzle,
    std::span<const eSwizzle> srcSwizzle) = 0;

  virtual void AssignImmediate(ExpressionNode* dst,
    std::span<const eSwizzle> dstSwizzle,
    std::span<const eSwizzle> immediateValues) = 0;
};

} // namespace Xe::Microcode::AST
