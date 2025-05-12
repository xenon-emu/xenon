// Copyright 2025 Xenon Emulator Project

#include "ASTEmitter.h"

namespace Xe::Microcode::AST {

Chunk ShaderCodeWriterSirit::GetExportDest(const eExportReg reg) {
  return {};
}

Chunk ShaderCodeWriterSirit::GetReg(u32 regIndex) {
  return {};
}

Chunk ShaderCodeWriterSirit::GetBoolVal(const u32 boolRegIndex) {
  return {};
}

Chunk ShaderCodeWriterSirit::GetFloatVal(const u32 floatRegIndex) {
  return {};
}

Chunk ShaderCodeWriterSirit::GetFloatValRelative(const u32 floatRegOffset) {
  return {};
}

Chunk ShaderCodeWriterSirit::GetPredicate() {
  return {};
}

Chunk ShaderCodeWriterSirit::Abs(ExpressionNode *value) {
  Chunk input = value->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);
  return Chunk(module.OpFAbs(float_type, input.id));
}

Chunk ShaderCodeWriterSirit::Negate(ExpressionNode *value) {
  Chunk input = value->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);
  return Chunk(module.OpFNegate(float_type, input.id));
}

Chunk ShaderCodeWriterSirit::Not(ExpressionNode *value) {
  Chunk input = value->EmitShaderCode(*this);
  Sirit::Id bool_type = module.TypeBool();
  return Chunk(module.OpNot(bool_type, input.id));
}

Chunk ShaderCodeWriterSirit::Saturate(ExpressionNode *value) {
  auto input = value->EmitShaderCode(*this);

  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id zero = module.Constant(float_type, 0.f);
  Sirit::Id one = module.Constant(float_type, 1.f);

  return Chunk(module.OpFClamp(float_type, input.id, zero, one));
}

Chunk ShaderCodeWriterSirit::Swizzle(ExpressionNode *value, std::array<eSwizzle, 4> swizzle) {
  auto input = value->EmitShaderCode(*this);

  // Assumes input is a float4 vector
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  std::array<u32, 4> components = {
    static_cast<u32>(swizzle[0]),
    static_cast<u32>(swizzle[1]),
    static_cast<u32>(swizzle[2]),
    static_cast<u32>(swizzle[3])
  };

  return Chunk(module.OpVectorShuffle(vec4_type, input.id, input.id, components[0], components[1], components[2], components[3]));
}

Chunk ShaderCodeWriterSirit::FetchTexture(const Chunk &src, const TextureFetch &instr) {
  const u32 slot = instr.fetchSlot;
  Sirit::Id coord = src.id;

  // Load the sampled image (pointer is to OpTypeSampledImage)

  // Determine coordinate type and dimensionality
  Sirit::Id coord_type = { 0 };
  spv::Dim dim{};
  bool is_array = false;
  switch (instr.textureType) {
  case DIMENSION_1D: {

  } break;
  case DIMENSION_2D: {

  } break;
  case DIMENSION_3D: {
   
  } break;
  case DIMENSION_CUBE: {
   
  } break;
  }

  return {};
}

Chunk ShaderCodeWriterSirit::FetchVertex(const Chunk &src, const VertexFetch &instr) {
  return {};
}

Chunk ShaderCodeWriterSirit::VectorFunc1(instr_vector_opc_t instr, ExpressionNode *arg1) {
  return {};
}

Chunk ShaderCodeWriterSirit::VectorFunc2(instr_vector_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2) {
  return {};
}

Chunk ShaderCodeWriterSirit::VectorFunc3(instr_vector_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2, ExpressionNode *arg3) {
  return {};
}

Chunk ShaderCodeWriterSirit::ScalarFunc1(instr_scalar_opc_t instr, ExpressionNode *arg1) {
  return {};
}

Chunk ShaderCodeWriterSirit::ScalarFunc2(instr_scalar_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2) {
  return {};
}

Chunk ShaderCodeWriterSirit::AllocLocalVector(const Chunk &initCode) {
  return {};
}

Chunk ShaderCodeWriterSirit::AllocLocalScalar(const Chunk &initCode) {
  return {};
}

Chunk ShaderCodeWriterSirit::AllocLocalBool(const Chunk &initCode) {
  return {};
}

void ShaderCodeWriterSirit::BeingCondition(const Chunk &condition) {

}

void ShaderCodeWriterSirit::EndCondition() {

}

void ShaderCodeWriterSirit::BeginControlFlow(const u32 address, const bool hasJumps, const bool hasCalls, const bool called) {

}

void ShaderCodeWriterSirit::EndControlFlow() {

}

void ShaderCodeWriterSirit::BeginBlockWithAddress(const u32 address) {

}

void ShaderCodeWriterSirit::EndBlockWithAddress() {

}

void ShaderCodeWriterSirit::ControlFlowEnd() {

}

void ShaderCodeWriterSirit::ControlFlowReturn(const u32 targetAddress) {

}

void ShaderCodeWriterSirit::ControlFlowCall(const u32 targetAddress) {

}

void ShaderCodeWriterSirit::ControlFlowJump(const u32 targetAddress) {

}

void ShaderCodeWriterSirit::SetPredicate(const Chunk &newValue) {

}

void ShaderCodeWriterSirit::PushPredicate(const Chunk &newValue) {

}

void ShaderCodeWriterSirit::PopPredicate() {

}

void ShaderCodeWriterSirit::Assign(const Chunk &dest, const Chunk &src) {

}

void ShaderCodeWriterSirit::Emit(const Chunk &src) {

}

void ShaderCodeWriterSirit::AssignMasked(ExpressionNode *src, ExpressionNode *dst,
  std::span<const eSwizzle> dstSwizzle,
  std::span<const eSwizzle> srcSwizzle) {

}

void ShaderCodeWriterSirit::AssignImmediate(ExpressionNode *dst,
  std::span<const eSwizzle> dstSwizzle,
  std::span<const eSwizzle> immediateValues) {

}

} // namespace Xe::Microcode::AST
