// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "ASTEmitter.h"

namespace Xe::Microcode::AST {

ShaderCodeWriterSirit::ShaderCodeWriterSirit() {
  module.AddCapability(spv::Capability::Shader);
  module.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);

  // Types
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id uint_type = module.TypeInt(32, false);
  Sirit::Id const_128 = module.Constant(uint_type, 128);

  // vec4[128]
  Sirit::Id gpr_array_type = module.TypeArray(vec4_type, const_128);
  Sirit::Id gpr_array_ptr_type = module.TypePointer(spv::StorageClass::Private, gpr_array_type);

  // Global variable
  gpr_var = module.AddGlobalVariable(gpr_array_ptr_type, spv::StorageClass::Private); // float4 R[128]
}

Chunk ShaderCodeWriterSirit::GetExportDest(const eExportReg reg) {

  return {};
}

Chunk ShaderCodeWriterSirit::GetReg(u32 regIndex) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id uint_type = module.TypeInt(32, false);

  Sirit::Id index_id = module.Constant(uint_type, regIndex);
  // Build access chain
  Sirit::Id vec4_ptr_type = module.TypePointer(spv::StorageClass::Private, vec4_type);
  Sirit::Id reg_ptr = module.OpAccessChain(vec4_ptr_type, gpr_var, index_id);
  Sirit::Id reg_val = module.OpLoad(vec4_type, reg_ptr);
  return Chunk(reg_val);
}

Chunk ShaderCodeWriterSirit::GetBoolVal(const u32 boolRegIndex) {
  Sirit::Id bool_type = module.TypeBool();
  Sirit::Id uint_type = module.TypeInt(32, false);

  // Constant for the index
  Sirit::Id index_id = module.Constant(uint_type, boolRegIndex);

  Sirit::Id bool_ptr_type = module.TypePointer(spv::StorageClass::Private, bool_type);

  Sirit::Id reg_ptr = module.OpAccessChain(bool_ptr_type, gpr_var, index_id);

  // Load the value
  Sirit::Id reg_val = module.OpLoad(bool_type, reg_ptr);

  return Chunk(reg_val);
}

Chunk ShaderCodeWriterSirit::GetFloatVal(const u32 floatRegIndex) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id uint_type = module.TypeInt(32, false);

  Sirit::Id index_id = module.Constant(uint_type, floatRegIndex);
  Sirit::Id vec4_ptr_type = module.TypePointer(spv::StorageClass::Private, module.TypeVector(float_type, 4));
  Sirit::Id reg_ptr = module.OpAccessChain(vec4_ptr_type, gpr_var, index_id);
  Sirit::Id reg_val = module.OpLoad(module.TypeVector(float_type, 4), reg_ptr);

  // Extract .x (index 0)
  Sirit::Id x_val = module.OpCompositeExtract(float_type, reg_val, 0);
  return Chunk(x_val);
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
  Chunk a = arg1->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  switch (instr) {
  case FRACv:
    return Chunk(module.OpFMod(vec4_type, a.id, module.Constant(float_type, 1.f))); // frac(a)
  case TRUNCv:
    return Chunk(module.OpTrunc(vec4_type, a.id)); // trunc(a)
  case FLOORv:
    return Chunk(module.OpFloor(vec4_type, a.id)); // floor(a)
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported vector unary op!");
    return {};
  }
}

Chunk ShaderCodeWriterSirit::VectorFunc2(instr_vector_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2) {
  Chunk a = arg1->EmitShaderCode(*this);
  Chunk b = arg2->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  switch (instr) {
  case ADDv:
    return Chunk(module.OpFAdd(vec4_type, a.id, b.id));
  case MULv:
    return Chunk(module.OpFMul(vec4_type, a.id, b.id));
  case MAXv:
    return Chunk(module.OpFMax(vec4_type, a.id, b.id));
  case MINv:
    return Chunk(module.OpFMin(vec4_type, a.id, b.id));
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported vector binary op!");
    return {};
  }
}

Chunk ShaderCodeWriterSirit::VectorFunc3(instr_vector_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2, ExpressionNode *arg3) {
  Chunk a = arg1->EmitShaderCode(*this);
  Chunk b = arg2->EmitShaderCode(*this);
  Chunk c = arg3->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  switch (instr) {
  case DOT4v:
    return Chunk(module.OpDot(float_type, a.id, b.id)); // Dot product of a and b (vector type)
  case DOT3v:
    return Chunk(module.OpDot(float_type, a.id, b.id)); // Dot product of a and b (3D version)
  case DOT2ADDv:
    return Chunk(module.OpFAdd(float_type, module.OpDot(float_type, a.id, b.id), c.id)); // Compute the dot product
  case CUBEv:
    return Chunk(module.OpFMul(vec4_type, a.id, module.Constant(float_type, 3.f))); // Assume ^ 3 for now
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported vector operation in VectorFunc3!");
    return {};
  }
}

Chunk ShaderCodeWriterSirit::ScalarFunc1(instr_scalar_opc_t instr, ExpressionNode *arg1) {
  Chunk a = arg1->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);
  switch (instr) {
  case ADDs:
    return Chunk(module.OpFAdd(float_type, a.id, a.id)); // a + a
  case MULs:
    return Chunk(module.OpFMul(float_type, a.id, a.id)); // a * a
  case MAXs:
    return Chunk(module.OpFMax(float_type, a.id, a.id)); // max(a, a)
  case MINs:
    return Chunk(module.OpFMin(float_type, a.id, a.id)); // min(a, a)
  case FRACs:
    return Chunk(module.OpFMod(float_type, a.id, module.Constant(float_type, 1.0f))); // frac(a)
  case TRUNCs:
    return Chunk(module.OpTrunc(float_type, a.id)); // trunc(a)
  case FLOORs:
    return Chunk(module.OpFloor(float_type, a.id)); // floor(a)
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported scalar unary op!");
    return {};
  }
}

Chunk ShaderCodeWriterSirit::ScalarFunc2(instr_scalar_opc_t instr, ExpressionNode *arg1, ExpressionNode *arg2) {
  Chunk a = arg1->EmitShaderCode(*this);
  Chunk b = arg2->EmitShaderCode(*this);
  Sirit::Id float_type = module.TypeFloat(32);

  switch (instr) {
  case ADDs:
    return Chunk(module.OpFAdd(float_type, a.id, b.id)); // a + b
  case MULs:
    return Chunk(module.OpFMul(float_type, a.id, b.id)); // a * b
  case MAXs:
    return Chunk(module.OpFMax(float_type, a.id, b.id)); // max(a, b)
  case MINs:
    return Chunk(module.OpFMin(float_type, a.id, b.id)); // min(a, b)
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported scalar binary op!");
    return {};
  }
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
