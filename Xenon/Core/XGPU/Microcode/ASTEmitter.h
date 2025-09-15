/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <array>
#include <span>
#include <stack>
#include <unordered_set>

#include "ASTNodeBase.h"
#include "ASTNode.h"

namespace Xe::Microcode::AST {

class ShaderCodeWriterBase {
public:
  virtual ~ShaderCodeWriterBase() = default;

  virtual void BeginMain() = 0;
  virtual void EndMain() = 0;
  virtual Chunk GetExportDest(const eExportReg reg) = 0;
  // TODO(chelseab): This GetReg function should be deprecated.
  // Register access should be explicit about the type (temp, constant, input).
  virtual Chunk GetReg(u32 regIndex) = 0;
  virtual Chunk GetTemporaryReg(u32 regIndex) = 0;
  virtual Chunk GetConstantReg(u32 regIndex) = 0;
  virtual Chunk GetVertexInputReg(u32 regIndex) = 0;
  virtual Chunk GetPixelInputReg(u32 regIndex) = 0; // Maps to interpolated values from VS
  virtual Chunk GetBoolVal(const u32 boolRegIndex) = 0;
  virtual Chunk GetFloatVal(const u32 floatRegIndex) = 0;
  virtual Chunk GetFloatValRelative(const u32 floatRegOffset) = 0;
  virtual Chunk GetPredicate() = 0;

  virtual Chunk Abs(const Chunk &value) = 0;
  virtual Chunk Negate(const Chunk &value) = 0;
  virtual Chunk Not(const Chunk &value) = 0;
  virtual Chunk Saturate(const Chunk &value) = 0;
  virtual Chunk Swizzle(const Chunk &value, std::array<eSwizzle, 4> swizzle) = 0;

  virtual Chunk FetchVertex(const Chunk &src, const VertexFetch &instr, const Shader *shader) = 0;
  virtual Chunk FetchTexture(const Chunk &src, const TextureFetch &instr) = 0;

  virtual Chunk VectorFunc1(instr_vector_opc_t instr, const Chunk &a) = 0;
  virtual Chunk VectorFunc2(instr_vector_opc_t instr, const Chunk &a, const Chunk &b) = 0;
  virtual Chunk VectorFunc3(instr_vector_opc_t instr, const Chunk &a, const Chunk &b, const Chunk &c) = 0;

  virtual Chunk ScalarFunc0(instr_scalar_opc_t instr) = 0;
  virtual Chunk ScalarFunc1(instr_scalar_opc_t instr, const Chunk &a) = 0;
  virtual Chunk ScalarFunc2(instr_scalar_opc_t instr, const Chunk &a, const Chunk &b) = 0;

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
  virtual void LoopBegin(const u32 targetAddress) = 0;
  virtual void LoopEnd(const u32 targetAddress) = 0;

  virtual void SetPredicate(const Chunk &newValue) = 0;
  virtual void PushPredicate(const Chunk &newValue) = 0;
  virtual void PopPredicate() = 0;

  virtual void Assign(const Chunk &dest, const Chunk &src) = 0;
  virtual void Emit(const Chunk &src) = 0;

  virtual void AssignMasked(const Chunk &src, const Chunk &dst,
    std::span<const eSwizzle> dstSwizzle,
    std::span<const eSwizzle> srcSwizzle) = 0;

  virtual void AssignImmediate(const Chunk &dst,
    std::span<const eSwizzle> dstSwizzle,
    std::span<const eSwizzle> immediateValues) = 0;
};

class TextureInfo {
public:
  u8 runtimeSlot; // Slot we are bounded to (sampler index in shader)
  u32 fetchSlot; // Source fetch slot
  instr_dimension_t type;
};

class Shader;

#ifndef NO_GFX
class ShaderCodeWriterSirit : public ShaderCodeWriterBase {
public:
  ShaderCodeWriterSirit(eShaderType shaderType);

  void BeginMain() override;

  void FinalizeEntryPoint();

  void EndMain() override;

  Chunk GetExportDest(const eExportReg reg) override;
  Chunk GetReg(u32 regIndex) override; // Deprecated, see comment in ShaderCodeWriterBase
  Chunk GetTemporaryReg(u32 regIndex) override;
  Chunk GetConstantReg(u32 regIndex) override;
  Chunk GetVertexInputReg(u32 regIndex) override;
  Chunk GetPixelInputReg(u32 regIndex) override;
  Chunk GetBoolVal(const u32 boolRegIndex) override;
  Chunk GetFloatVal(const u32 floatRegIndex) override;
  Chunk GetFloatValRelative(const u32 floatRegOffset) override;
  Chunk GetPredicate() override;

  Chunk Abs(const Chunk &value) override;
  Chunk Negate(const Chunk &value) override;
  Chunk Not(const Chunk &value) override;
  Chunk Saturate(const Chunk &value) override;
  Chunk Swizzle(const Chunk &value, std::array<eSwizzle, 4> swizzle) override;

  Sirit::Id GetSampledImageType(instr_dimension_t dim);

  Chunk FetchVertex(const Chunk &src, const VertexFetch &instr, const Shader *shader) override;
  Chunk FetchTexture(const Chunk &src, const TextureFetch &instr) override;

  Chunk VectorFunc1(instr_vector_opc_t instr, const Chunk &a) override;
  Chunk VectorFunc2(instr_vector_opc_t instr, const Chunk &a, const Chunk &b) override;
  Chunk VectorFunc3(instr_vector_opc_t instr, const Chunk &a, const Chunk &b, const Chunk &c) override;

  Chunk ScalarFunc0(instr_scalar_opc_t instr) override;
  Chunk ScalarFunc1(instr_scalar_opc_t instr, const Chunk &a) override;
  Chunk ScalarFunc2(instr_scalar_opc_t instr, const Chunk &a, const Chunk &b) override;  

  Chunk AllocLocalImpl(Sirit::Id type, const Chunk &initCode);

  Chunk AllocLocalVector(const Chunk &initCode) override;
  Chunk AllocLocalScalar(const Chunk &initCode) override;
  Chunk AllocLocalBool(const Chunk &initCode) override;

  void BeingCondition(const Chunk &condition) override;
  void EndCondition() override;

  void BeginControlFlow(const u32 address, const bool hasJumps, const bool hasCalls, const bool called) override;
  void EndControlFlow() override;

  void BeginBlockWithAddress(const u32 address) override;
  void EndBlockWithAddress() override;

  void ControlFlowEnd() override;
  void ControlFlowReturn(const u32 targetAddress) override;
  void ControlFlowCall(const u32 targetAddress) override;
  void ControlFlowJump(const u32 targetAddress) override;
  void LoopBegin(const u32 targetAddress) override;
  void LoopEnd(const u32 targetAddress) override;

  void SetPredicate(const Chunk &newValue) override;
  void PushPredicate(const Chunk &newValue) override;
  void PopPredicate() override;

  void Assign(const Chunk &dest, const Chunk &src) override;
  void Emit(const Chunk &src) override;

  void AssignMasked(const Chunk &src, const Chunk &dst,
    std::span<const eSwizzle> dstSwizzle,
    std::span<const eSwizzle> srcSwizzle) override;

  void AssignImmediate(const Chunk &dst,
    std::span<const eSwizzle> dstSwizzle,
    std::span<const eSwizzle> immediateValues) override;

  eShaderType type{};

  std::unordered_map<u32, Chunk> texture_vars{};

  std::unordered_map<u32, Sirit::Id> temp_regs{};
  std::unordered_map<u32, Sirit::Id> vertex_input_vars{};
  std::unordered_map<u32, Sirit::Id> input_vars{}; // Changed to u32 for Location mapping
  std::unordered_map<eExportReg, Sirit::Id> output_vars{};

  Sirit::Id main_func = { 0 };
  Sirit::Id main_label = { 0 };

  Sirit::Module module{ 0x00010000 };
private:
  Sirit::Id predicate_var = { 0 };

  Sirit::Id ubo_type_v = { 0 };
  Sirit::Id ubo_var_v = { 0 };

  Sirit::Id ubo_type_b = { 0 };
  Sirit::Id ubo_var_b = { 0 };

  Sirit::Id current_function = { 0 };

  Sirit::Id entry_dispatch_func = { 0 };

  std::stack<std::pair<Sirit::Id, Sirit::Id>> loop_stack{};

  Sirit::Id merge_label = { 0 };
  Sirit::Id true_label = { 0 };
  Sirit::Id false_label = { 0 };

  bool inside_block = false;
  bool block_needs_termination = false;
  Sirit::Id current_block_label = { 0 };
  Sirit::Id next_block_label = { 0 };
  std::unordered_map<u32, Sirit::Id> address_to_label = {};

  // Helper to get export semantic index, typically from ASTNode.cpp
  // Copied here for completeness, but ideally should be part of a shared utility
  s32 GetExportSemanticIndex(const eExportReg reg) {
    switch (reg) {
    case eExportReg::POSITION: return 0;
    case eExportReg::POINTSIZE: return 1;
    case eExportReg::COLOR0: return 2;
    case eExportReg::COLOR1: return 3;
    case eExportReg::COLOR2: return 4;
    case eExportReg::COLOR3: return 5;
    case eExportReg::INTERP0: return 6;
    case eExportReg::INTERP1: return 7;
    case eExportReg::INTERP2: return 8;
    case eExportReg::INTERP3: return 9;
    case eExportReg::INTERP4: return 10;
    case eExportReg::INTERP5: return 11;
    case eExportReg::INTERP6: return 12;
    case eExportReg::INTERP7: return 13;
    // INTERP8 could be 14 if needed, but GPU_INTERPOLATORS is 16.
    default: return -1;
    }
  }
};
#endif

} // namespace Xe::Microcode::AST