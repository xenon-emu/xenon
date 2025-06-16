// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Assert.h"

#include "ASTBlock.h"
#include "ASTEmitter.h"

#ifndef NO_GFX
namespace Xe::Microcode::AST {

ShaderCodeWriterSirit::ShaderCodeWriterSirit(eShaderType shaderType) :
  type(shaderType) {
  module.AddCapability(spv::Capability::Shader);
  module.AddExtension("SPV_KHR_storage_buffer_storage_class");
  module.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);

  std::string shaderTypeName = type == eShaderType::Pixel ? "Pixel" : "Vertex";

  // Declare types
  Sirit::Id uint_type = module.TypeInt(32, false);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id vec4_ptr_output = module.TypePointer(spv::StorageClass::Output, vec4_type);
  Sirit::Id float_ptr_output = module.TypePointer(spv::StorageClass::Output, float_type);

  Sirit::Id bool_type = module.TypeBool();
  Sirit::Id bool_ptr_type = module.TypePointer(spv::StorageClass::Private, bool_type);
  Sirit::Id false_val = module.ConstantFalse(bool_type);
  predicate_var = module.AddGlobalVariable(bool_ptr_type, spv::StorageClass::Private, false_val);
 
  // Float Constants
  Sirit::Id const_512 = module.Constant(uint_type, 512);
  Sirit::Id vec4_array_512 = module.TypeArray(vec4_type, const_512);
  // Struct and UBO
  Sirit::Id ubo_struct_v = module.TypeStruct(vec4_array_512);
  module.Decorate(ubo_struct_v, spv::Decoration::Block);
  module.Name(ubo_struct_v, "ALUConsts");
  module.MemberName(ubo_struct_v, 0, "FloatConsts");
  ubo_type_v = module.TypePointer(spv::StorageClass::StorageBuffer, ubo_struct_v);
  ubo_var_v = module.AddGlobalVariable(ubo_type_v, spv::StorageClass::StorageBuffer);
  // For vec4 array inside struct
  module.Decorate(vec4_array_512, spv::Decoration::ArrayStride, 16);
  module.MemberDecorate(ubo_struct_v, 0, spv::Decoration::Offset, 0);

  // For uint array inside struct
  Sirit::Id const_32 = module.Constant(uint_type, 32);
  Sirit::Id uint_array_32 = module.TypeArray(uint_type, const_32);
  Sirit::Id ubo_struct_b = module.TypeStruct(uint_array_32);
  module.Decorate(ubo_struct_b, spv::Decoration::Block);
  module.Name(ubo_struct_b, "CommonBoolConsts");
  module.MemberName(ubo_struct_b, 0, "BoolConsts");
  ubo_type_b = module.TypePointer(spv::StorageClass::StorageBuffer, ubo_struct_b);
  ubo_var_b = module.AddGlobalVariable(ubo_type_b, spv::StorageClass::StorageBuffer);
  module.Decorate(uint_array_32, spv::Decoration::ArrayStride, 4);
  module.MemberDecorate(ubo_struct_b, 0, spv::Decoration::Offset, 0);

  // Decorate UBO variable (binding, descriptor set)
  module.Decorate(ubo_var_v, spv::Decoration::DescriptorSet, 0);
  u32 floatConstBinding = 0;
  module.Decorate(ubo_var_v, spv::Decoration::Binding, floatConstBinding);

  module.Decorate(ubo_var_b, spv::Decoration::DescriptorSet, 0);
  module.Decorate(ubo_var_b, spv::Decoration::Binding, 1);
}

void ShaderCodeWriterSirit::BeginMain() {
  LOG_DEBUG(Xenos, "[AST::Sirit] BeginMain()");
  Sirit::Id func_type = module.TypeFunction(module.TypeVoid());
  main_func = module.OpFunction(module.TypeVoid(), spv::FunctionControlMask::MaskNone, func_type);
  main_label = module.OpLabel();
  module.AddLabel(main_label);

  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id const_128 = module.Constant(module.TypeInt(32, false), 128);

  Sirit::Id uint_type = module.TypeInt(32, false);
}

void ShaderCodeWriterSirit::FinalizeEntryPoint() {
  LOG_DEBUG(Xenos, "[AST::Sirit] FinalizeEntryPoint()");
  std::vector<Sirit::Id> interface_vars;

  spv::ExecutionModel execModel = type == eShaderType::Pixel ? spv::ExecutionModel::Fragment : spv::ExecutionModel::Vertex;
  if (type == eShaderType::Pixel) {
    module.AddExecutionMode(main_func, spv::ExecutionMode::OriginUpperLeft);
  } else {
    for (const auto &[slot, var] : vertex_input_vars) {
      interface_vars.push_back(var);
    }
  }
  for (const auto &[reg, var] : output_vars) {
    u32 location = -1;
    spv::Decoration type = spv::Decoration::Location;
    switch (reg) {
    case eExportReg::POSITION: {
      type = spv::Decoration::BuiltIn;
      location = static_cast<u32>(spv::BuiltIn::Position);
    } break;
    case eExportReg::POINTSIZE: {
      type = spv::Decoration::BuiltIn;
      location = static_cast<u32>(spv::BuiltIn::PointSize);
    } break;
    default:
      if (reg >= eExportReg::COLOR0 && reg <= eExportReg::COLOR3) {
        location = vertex_input_vars.size() + (static_cast<u32>(reg) - static_cast<u32>(eExportReg::COLOR0));
      } else if (reg >= eExportReg::INTERP0 && reg <= eExportReg::INTERP7) {
        location = vertex_input_vars.size() + (static_cast<u32>(reg) - static_cast<u32>(eExportReg::INTERP0));
      }
      break;
    }
    module.Decorate(var, type, location);
    interface_vars.push_back(var);
  }
  module.AddEntryPoint(execModel, main_func, "main", interface_vars);
}

void ShaderCodeWriterSirit::EndMain() {
  LOG_DEBUG(Xenos, "[AST::Sirit] EndMain()");
  if (entry_dispatch_func.value) {
    module.OpFunctionCall(module.TypeVoid(), entry_dispatch_func, {});
  }
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  module.OpReturn();
  module.OpFunctionEnd();
  FinalizeEntryPoint();
}

Chunk ShaderCodeWriterSirit::GetExportDest(const eExportReg reg) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetExportDest({})", (u32)reg);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id vec4_ptr_output = module.TypePointer(spv::StorageClass::Output, vec4_type);
  Sirit::Id float_ptr_output = module.TypePointer(spv::StorageClass::Output, float_type);
  bool colorReg = false;
  auto it = output_vars.find(reg);
  if (it == output_vars.end()) {
    auto &var = output_vars[reg];
    switch (reg) {
    case eExportReg::POSITION:
      output_vars[eExportReg::POSITION] = module.AddGlobalVariable(vec4_ptr_output, spv::StorageClass::Output);
      module.Name(output_vars[eExportReg::POSITION], "POSITION");
      break;
    case eExportReg::POINTSIZE:
      output_vars[eExportReg::POINTSIZE] = module.AddGlobalVariable(float_ptr_output, spv::StorageClass::Output);
      module.Name(output_vars[eExportReg::POINTSIZE], "POINTSIZE");
      break;
    default:
      if (reg >= eExportReg::COLOR0 && reg <= eExportReg::COLOR3) {
        colorReg = true;
        var = module.AddGlobalVariable(vec4_ptr_output, spv::StorageClass::Output);
        module.Name(var, FMT("COLOR{}", static_cast<u32>(reg) - static_cast<u32>(eExportReg::COLOR0)));
      } else if (reg >= eExportReg::INTERP0 && reg <= eExportReg::INTERP7) {
        var = module.AddGlobalVariable(vec4_ptr_output, spv::StorageClass::Output);
        module.Name(var, FMT("INTERP{}", static_cast<u32>(reg) - static_cast<u32>(eExportReg::INTERP0)));
      }
      break;
    }
    it = output_vars.find(reg);
  }
  Sirit::Id var_id = it->second;
  Sirit::Id loaded = {};

  eChunkType chunkType = eChunkType::Vector;
  if (type == eShaderType::Pixel) {
    loaded = module.OpLoad(vec4_type, var_id);
  } else {
    if (reg == eExportReg::POINTSIZE) {
      chunkType = eChunkType::Scalar;
      loaded = module.OpLoad(float_type, var_id);
    }
    else {
      loaded = module.OpLoad(vec4_type, var_id);
    }
  }

  return Chunk(loaded, var_id, chunkType);
}

Chunk ShaderCodeWriterSirit::GetReg(u32 regIndex) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetReg({})", regIndex);
  Sirit::Id uint_type = module.TypeInt(32, false);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id vec4_ptr_func = module.TypePointer(spv::StorageClass::Function, vec4_type);
  Sirit::Id vec4_ptr_ubo = module.TypePointer(spv::StorageClass::StorageBuffer, vec4_type);

  // Check for existing temp
  if (temp_regs.contains(regIndex)) {
    Sirit::Id ptr = temp_regs[regIndex];
    return AllocLocalVector(Chunk{ {}, ptr });
  }

  // Load from UBO (storage buffer)
  Sirit::Id index_id = module.Constant(uint_type, regIndex);
  Sirit::Id ubo_ptr = module.OpAccessChain(vec4_ptr_ubo, ubo_var_v, module.Constant(uint_type, 0), index_id);
  Sirit::Id initial_val = module.OpLoad(vec4_type, ubo_ptr);

  // Allocate local variable
  Sirit::Id local_ptr = module.AddLocalVariable(vec4_ptr_func, spv::StorageClass::Function);
  module.OpStore(local_ptr, initial_val);

  temp_regs[regIndex] = local_ptr;

  Sirit::Id val = module.OpLoad(vec4_type, local_ptr);
  return Chunk(val, local_ptr, eChunkType::Vector);
}

Chunk ShaderCodeWriterSirit::GetBoolVal(const u32 boolRegIndex) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetBoolVal({})", boolRegIndex);
  Sirit::Id uint_type = module.TypeInt(32, false);
  Sirit::Id bool_type = module.TypeBool();
  Sirit::Id int_const_32 = module.Constant(uint_type, 32);

  Sirit::Id reg_idx = module.Constant(uint_type, boolRegIndex);
  Sirit::Id word_idx = module.OpUDiv(uint_type, reg_idx, int_const_32);
  Sirit::Id bit_idx = module.OpUMod(uint_type, reg_idx, int_const_32);
  Sirit::Id bit_mask = module.OpShiftLeftLogical(uint_type, module.Constant(uint_type, 1), bit_idx);

  Sirit::Id ptr_type = module.TypePointer(spv::StorageClass::StorageBuffer, uint_type);
  Sirit::Id word_ptr = module.OpAccessChain(ptr_type, ubo_var_b, module.Constant(uint_type, 0), word_idx);
  Sirit::Id word_val = module.OpLoad(uint_type, word_ptr);

  Sirit::Id and_val = module.OpBitwiseAnd(uint_type, word_val, bit_mask);
  Sirit::Id result = module.OpINotEqual(bool_type, and_val, module.Constant(uint_type, 0));
  return Chunk(result, word_ptr, eChunkType::Boolean);
}

Chunk ShaderCodeWriterSirit::GetFloatVal(const u32 floatRegIndex) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetFloatVal({})", floatRegIndex);
  Chunk chunk = GetReg(floatRegIndex);
  return Chunk(chunk.id, chunk.ptr, chunk.type);
}

Chunk ShaderCodeWriterSirit::GetFloatValRelative(const u32 floatRegOffset) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetFloatValRelative({})", floatRegOffset);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id vec4_ptr_type = module.TypePointer(spv::StorageClass::Function, vec4_type);

  Sirit::Id uint_type = module.TypeInt(32, false);
  Sirit::Id offset_id = module.Constant(uint_type, floatRegOffset);

  Sirit::Id reg_ptr = module.OpAccessChain(vec4_ptr_type, ubo_var_v, offset_id);

  // Load the value (A0 + offset)
  Sirit::Id reg_val = module.OpLoad(vec4_type, reg_ptr);

  Sirit::Id x_value = module.OpCompositeExtract(float_type, reg_val, 0);

  return Chunk(x_value, reg_ptr, eChunkType::Scalar);
}

Chunk ShaderCodeWriterSirit::GetPredicate() {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetPredicate()");
  Sirit::Id bool_type = module.TypeBool();
  Sirit::Id pred_val = module.OpLoad(bool_type, predicate_var);
  return Chunk(pred_val, eChunkType::Boolean);
}

Chunk ShaderCodeWriterSirit::Abs(const Chunk &value) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Abs({})", value.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  return Chunk(module.OpFAbs(float_type, value.id), eChunkType::Scalar);
}

Chunk ShaderCodeWriterSirit::Negate(const Chunk &value) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Negate({})", value.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  return Chunk(module.OpFNegate(float_type, value.id), eChunkType::Scalar);
}

Chunk ShaderCodeWriterSirit::Not(const Chunk &value) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Not({})", value.id.value);
  Sirit::Id bool_type = module.TypeBool();
  return Chunk(module.OpNot(bool_type, value.id), eChunkType::Boolean);
}

Chunk ShaderCodeWriterSirit::Saturate(const Chunk &value) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Saturate({})", value.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id zero = module.Constant(float_type, 0.f);
  Sirit::Id one = module.Constant(float_type, 1.f);

  return Chunk(module.OpFClamp(float_type, value.id, zero, one), eChunkType::Scalar);
}

Chunk ShaderCodeWriterSirit::Swizzle(const Chunk &value, std::array<eSwizzle, 4> swizzle) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Swizzle({})", value.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  // If scalar, just broadcast the value to all components
  if (value.type == eChunkType::Scalar) {
    std::array<Sirit::Id, 4> scalar_comps{};
    for (u8 i = 0; i != 4; ++i) {
      scalar_comps[i] = value.id;
    }
    return Chunk(module.OpCompositeConstruct(vec4_type, scalar_comps), eChunkType::Vector);
  }
  // Otherwise assume it's a proper vector
  std::array<u32, 4> components{};
  for (u8 i = 0; i != 4; ++i) {
    components[i] = static_cast<u32>(swizzle[i]);
  }
  return Chunk(module.OpVectorShuffle(vec4_type, value.id, value.id, components[0], components[1], components[2], components[3]), eChunkType::Vector);
}

Sirit::Id ShaderCodeWriterSirit::GetSampledImageType(instr_dimension_t dim) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id image_type = {};

  switch (dim) {
  case DIMENSION_1D:
    image_type = module.TypeImage(float_type, spv::Dim::Dim1D, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    break;
  case DIMENSION_2D:
    image_type = module.TypeImage(float_type, spv::Dim::Dim2D, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    break;
  case DIMENSION_3D:
    image_type = module.TypeImage(float_type, spv::Dim::Dim3D, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    break;
  case DIMENSION_CUBE:
    image_type = module.TypeImage(float_type, spv::Dim::Cube, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    break;
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported texture sample '{}'!", static_cast<u32>(dim));
    return {};
  }

  return module.TypeSampledImage(image_type);
}

Chunk ShaderCodeWriterSirit::FetchTexture(const Chunk &src, const TextureFetch &instr) {
  const u32 slot = instr.fetchSlot;
  Sirit::Id coord = src.id;
  LOG_DEBUG(Xenos, "[AST::Sirit] FetchTexture({}, {})", coord.value, slot);

  Sirit::Id sampled_type = GetSampledImageType(instr.textureType);
  Sirit::Id ptr_type = module.TypePointer(spv::StorageClass::UniformConstant, sampled_type);
  Sirit::Id var = module.AddGlobalVariable(ptr_type, spv::StorageClass::UniformConstant);
  module.Decorate(var, spv::Decoration::DescriptorSet, 0);
  module.Decorate(var, spv::Decoration::Binding, slot);
  module.Name(var, FMT("TextureSlot{}", slot));

  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id sampled_image_val = module.OpLoad(sampled_type, var);
  Sirit::Id sampled_result = module.OpImageSampleImplicitLod(vec4_type, sampled_image_val, coord);
  return Chunk(sampled_result, { 0 }, eChunkType::Fetch);
}

Chunk ShaderCodeWriterSirit::FetchVertex(const Chunk &src, const VertexFetch &instr) {
  const u32 slot = instr.fetchSlot;
  const u32 location = slot;

  const u32 componentCount = instr.GetComponentCount();
  const bool isFloat = instr.isFloat;
  const bool isSigned = instr.isSigned;

  Sirit::Id scalar_type = isFloat ? module.TypeFloat(32) : module.TypeInt(32, isSigned);
  Sirit::Id input_type = componentCount == 1 ? scalar_type  : module.TypeVector(scalar_type, componentCount);

  if (!vertex_input_vars.contains(slot)) {
    Sirit::Id ptr_type = module.TypePointer(spv::StorageClass::Input, input_type);

    u32 glLocation = static_cast<u32>(vertex_input_vars.size());
    if (glLocation >= 32) {
      LOG_ERROR(Xenos, "Too many vertex input slots! Max supported is 32.");
      return {};
    }

    Sirit::Id input_var = module.AddGlobalVariable(ptr_type, spv::StorageClass::Input);
    module.Decorate(input_var, spv::Decoration::Location, glLocation);
    module.Name(input_var, FMT("INPUT_{}", glLocation));

    vertex_input_vars[slot] = input_var;
  }

  Sirit::Id var = vertex_input_vars[slot];
  Sirit::Id val = module.OpLoad(input_type, var);

  return Chunk(val, var, componentCount == 1 ? eChunkType::Scalar : eChunkType::Fetch);
}

Chunk ShaderCodeWriterSirit::VectorFunc1(instr_vector_opc_t instr, const Chunk &a) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  switch (instr) {
  case FRACv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(FRACv)");
    return Chunk(module.OpFMod(vec4_type, a.id, module.Constant(float_type, 1.f)), eChunkType::Vector); // frac(a)
  case TRUNCv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(TRUNCv)");
    return Chunk(module.OpTrunc(vec4_type, a.id), eChunkType::Vector); // trunc(a)
  case FLOORv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(FLOORv)");
    return Chunk(module.OpFloor(vec4_type, a.id), eChunkType::Vector); // floor(a)
  case MAX4v: {
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(MAX4v)");
    // max4(a): max(a.x, a.y, a.z, a.w)
    Sirit::Id x = module.OpCompositeExtract(float_type, a.id, 0);
    Sirit::Id y = module.OpCompositeExtract(float_type, a.id, 1);
    Sirit::Id z = module.OpCompositeExtract(float_type, a.id, 2);
    Sirit::Id w = module.OpCompositeExtract(float_type, a.id, 3);
    Sirit::Id max1 = module.OpFMax(float_type, x, y);
    Sirit::Id max2 = module.OpFMax(float_type, z, w);
    Sirit::Id max4 = module.OpFMax(float_type, max1, max2);
    return Chunk(module.OpCompositeConstruct(vec4_type, max4, max4, max4, max4), eChunkType::Vector);
  }
  case MOVAv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(MOVAv)");
    return Chunk(module.OpFloor(vec4_type, a.id), eChunkType::Vector); // Often MOVA means integer floor for address computation
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported vector unary op!");
    return Chunk({ 0 }, eChunkType::Vector);
  }
}

Chunk ShaderCodeWriterSirit::VectorFunc2(instr_vector_opc_t instr, const Chunk &a, const Chunk &b) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  switch (instr) {
  case ADDv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(ADDv)");
    return Chunk(module.OpFAdd(vec4_type, a.id, b.id), eChunkType::Vector);
  case MULv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(MULv)");
    return Chunk(module.OpFMul(vec4_type, a.id, b.id), eChunkType::Vector);
  case MAXv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(MAXv)");
    return Chunk(module.OpFMax(vec4_type, a.id, b.id), eChunkType::Vector);
  case MINv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(MINv)");
    return Chunk(module.OpFMin(vec4_type, a.id, b.id), eChunkType::Vector);
  case SETEv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(SETEv)");
    return Chunk(module.OpFOrdEqual(vec4_type, a.id, b.id), eChunkType::Vector);
  case SETGTv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(SETGTv)");
    return Chunk(module.OpFOrdGreaterThan(vec4_type, a.id, b.id), eChunkType::Vector);
  case SETGTEv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(SETGTEv)");
    return Chunk(module.OpFOrdGreaterThanEqual(vec4_type, a.id, b.id), eChunkType::Vector);
  case SETNEv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(SETNEv)");
    return Chunk(module.OpFOrdNotEqual(vec4_type, a.id, b.id), eChunkType::Vector);
  case DOT4v: {
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(DOT4v)");
    Sirit::Id dot = module.OpDot(float_type, a.id, b.id);
    Sirit::Id broadcast = module.OpCompositeConstruct(vec4_type, dot, dot, dot, dot);
    return Chunk(broadcast, eChunkType::Vector); // Dot product of a and b (vector type)
  }
  case DOT3v: {
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(DOT3v)");
    Sirit::Id dot = module.OpDot(float_type, a.id, b.id);
    Sirit::Id broadcast = module.OpCompositeConstruct(vec4_type, dot, dot, dot, dot);
    return Chunk(broadcast, eChunkType::Vector); // Dot product of a and b (3D version)
  }
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported vector binary op '{}'!", static_cast<u32>(instr));
    return Chunk({ 0 }, eChunkType::Vector);
  }
}

Chunk ShaderCodeWriterSirit::VectorFunc3(instr_vector_opc_t instr, const Chunk &a, const Chunk &b, const Chunk &c) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);

  switch (instr) {
  case DOT2ADDv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(DOT2ADDv)");
    return Chunk(module.OpFAdd(float_type, module.OpDot(float_type, a.id, b.id), c.id), eChunkType::Vector); // Compute the dot product
  case CUBEv: {
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(CUBEv)");
    Sirit::Id squared = module.OpFMul(vec4_type, a.id, a.id);
    Sirit::Id cubed = module.OpFMul(vec4_type, squared, a.id);
    return Chunk(cubed, eChunkType::Vector);
  }
  case MULADDv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(MULADDv)");
    return Chunk(module.OpFAdd(vec4_type, module.OpFMul(vec4_type, a.id, b.id), c.id), eChunkType::Vector);
  case CNDEv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(CNDEv)");
    return Chunk(module.OpSelect(vec4_type, module.OpFOrdGreaterThanEqual(vec4_type, a.id, module.Constant(float_type, 0.5f)), b.id, c.id), eChunkType::Vector);
  case CNDGTEv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(CNDGTEv)");
    return Chunk(module.OpSelect(vec4_type, module.OpFOrdGreaterThanEqual(vec4_type, a.id, b.id), b.id, c.id), eChunkType::Vector);
  case CNDGTv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(CNDGTv)");
    return Chunk(module.OpSelect(vec4_type, module.OpFOrdGreaterThan(vec4_type, a.id, b.id), b.id, c.id), eChunkType::Vector);
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported vector '{}' operation in VectorFunc3!", static_cast<u32>(instr));
    return Chunk({ 0 }, eChunkType::Vector);
  }
}

Chunk ShaderCodeWriterSirit::ScalarFunc0(instr_scalar_opc_t instr) {
  Sirit::Id float_type = module.TypeFloat(32);
  switch (instr) {
  case RETAIN_PREV:
  default:
    LOG_ERROR(Xenos, "Unsupported 0-arg scalar instruction: {}", static_cast<u32>(instr));
    return AllocLocalScalar(Chunk(module.Constant(float_type, 0.f)));
  }
}

Chunk ShaderCodeWriterSirit::ScalarFunc1(instr_scalar_opc_t instr, const Chunk &a) {
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id type = a.type == eChunkType::Vector ? vec4_type : float_type;

  switch (instr) {
  case ADDs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(ADDs)");
    return Chunk(module.OpFAdd(type, a.id, a.id), eChunkType::Scalar); // a + a
  case MULs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(MULs)");
    return Chunk(module.OpFMul(type, a.id, a.id), eChunkType::Scalar); // a * a
  case MAXs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(MAXs)");
    return Chunk(module.OpFMax(type, a.id, a.id), eChunkType::Scalar); // max(a, a)
  case MINs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(MINs)");
    return Chunk(module.OpFMin(type, a.id, a.id), eChunkType::Scalar); // min(a, a)
  case FRACs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(FRACs)");
    return Chunk(module.OpFMod(type, a.id, module.Constant(float_type, 1.f)), eChunkType::Scalar); // frac(a)
  case TRUNCs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(TRUNCs)");
    return Chunk(module.OpTrunc(type, a.id), eChunkType::Scalar); // trunc(a)
  case FLOORs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(FLOORs)");
    return Chunk(module.OpFloor(type, a.id), eChunkType::Scalar); // floor(a)
  case EXP_IEEE:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(EXP_IEEE)");
    return Chunk(module.OpExp2(type, a.id), eChunkType::Scalar); // 2^x
  case LOG_CLAMP: {
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(LOG_CLAMP)");
    Sirit::Id abs_x = module.OpFAbs(type, a.id);
    Sirit::Id epsilon = module.Constant(type, 1e-8f); // or smallest denormal > 0
    Sirit::Id clamped = module.OpFMax(type, abs_x, epsilon);
    return Chunk(module.OpLog2(type, clamped)); // log2(max(abs(x), 1e-8f))
  }
  case LOG_IEEE:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(LOG_IEEE)");
    return Chunk(module.OpLog2(float_type, a.id)); // log2(x)
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported scalar unary op '{}'!", static_cast<u32>(instr));
    return Chunk({ 0 }, eChunkType::Scalar);
  }
}

Chunk ShaderCodeWriterSirit::ScalarFunc2(instr_scalar_opc_t instr, const Chunk &a, const Chunk &b) {
  Sirit::Id float_type = module.TypeFloat(32);

  switch (instr) {
  case ADDs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(ADDs)");
    return Chunk(module.OpFAdd(float_type, a.id, b.id), eChunkType::Scalar); // a + b
  case MULs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(MULs)");
    return Chunk(module.OpFMul(float_type, a.id, b.id), eChunkType::Scalar); // a * b
  case MAXs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(MAXs)");
    return Chunk(module.OpFMax(float_type, a.id, b.id), eChunkType::Scalar); // max(a, b)
  case MINs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(MINs)");
    return Chunk(module.OpFMin(float_type, a.id, b.id), eChunkType::Scalar); // min(a, b)
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported scalar binary op!");
    return Chunk({ 0 }, eChunkType::Scalar);
  }
}

Chunk ShaderCodeWriterSirit::AllocLocalImpl(Sirit::Id type, const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalImpl({}, {})", type.value, initCode.id.value);
  Sirit::Id ptr_type = module.TypePointer(spv::StorageClass::Function, type);

  // Declare local variable
  Sirit::Id local_ptr = module.AddLocalVariable(ptr_type, spv::StorageClass::Function);

  // Ensure the value is loaded if initCode.id is a pointer
  Sirit::Id value = initCode.HasPointer() ? module.OpLoad(type, initCode.ptr) : initCode.id;

  // Store initial value
  module.OpStore(local_ptr, value);

  // Load value for usage
  Sirit::Id loaded = module.OpLoad(type, local_ptr);

  return Chunk{ loaded, local_ptr };
}

Chunk ShaderCodeWriterSirit::AllocLocalVector(const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalVector({})", initCode.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  return Chunk(AllocLocalImpl(vec4_type, initCode), eChunkType::Vector);
}

Chunk ShaderCodeWriterSirit::AllocLocalScalar(const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalScalar({})", initCode.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  return Chunk(AllocLocalImpl(float_type, initCode), eChunkType::Scalar);
}

Chunk ShaderCodeWriterSirit::AllocLocalBool(const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalBool({})", initCode.id.value);
  Sirit::Id bool_type = module.TypeBool();
  return AllocLocalImpl(bool_type, initCode);
  return Chunk(AllocLocalImpl(bool_type, initCode), eChunkType::Boolean);
}

void ShaderCodeWriterSirit::BeingCondition(const Chunk &condition) {
  LOG_DEBUG(Xenos, "[AST::Sirit] BeingCondition({})", condition.id.value);
  // Pre-allocate labels, do not emit yet
  true_label = module.OpLabel();
  false_label = module.OpLabel();
  merge_label = module.OpLabel();

  // Emit selection merge and conditional branch
  module.OpSelectionMerge(merge_label, spv::SelectionControlMask::MaskNone);
  module.OpBranchConditional(condition.id, true_label, false_label);

  module.AddLabel(true_label); // Begin "true" block
}

void ShaderCodeWriterSirit::EndCondition() {
  LOG_DEBUG(Xenos, "[AST::Sirit] EndCondition()");
  module.OpBranch(merge_label); // End of true or false block

  module.AddLabel(false_label); // Begin "false" block
  // Unsure what to do here right now.
  module.OpBranch(merge_label);

  module.AddLabel(merge_label); // Merge block
}

void ShaderCodeWriterSirit::BeginControlFlow(const u32 address, const bool hasJumps, const bool hasCalls, const bool called) {
  LOG_DEBUG(Xenos, "[AST::Sirit] BeginControlFlow(0x{:X}, {}, {}, {})", address, hasJumps, hasCalls, called);

  // Create function type and function
  Sirit::Id func_type = module.TypeFunction(module.TypeVoid());
  current_function = module.OpFunction(module.TypeVoid(), spv::FunctionControlMask::MaskNone, func_type);

  // Save the first control flow function to call from main
  if (!entry_dispatch_func.value) {
    entry_dispatch_func = current_function;
  }

  // Create entry block label
  current_block_label = module.OpLabel();
  module.AddLabel(current_block_label);
  // Tell it to terminate after
  block_needs_termination = true;
}

void ShaderCodeWriterSirit::EndControlFlow() {
  LOG_DEBUG(Xenos, "[AST::Sirit] EndControlFlow()");

  if (inside_block && block_needs_termination) {
    module.OpReturn();
    block_needs_termination = false;
  }

  module.OpFunctionEnd();
  inside_block = false;
  current_function = {};
  current_block_label = {};
}

void ShaderCodeWriterSirit::BeginBlockWithAddress(const u32 address) {
  LOG_DEBUG(Xenos, "[AST::Sirit] BeginBlockWithAddress(0x{:X})", address);

  if (block_needs_termination) {
    // End the previous block by branching to the new one
    auto it = address_to_label.find(address);
    Sirit::Id target_label = (it != address_to_label.end()) ? it->second : module.OpLabel();

    address_to_label[address] = target_label;
    module.OpBranch(target_label);
    block_needs_termination = false;
    module.AddLabel(target_label);
    current_block_label = target_label;
  } else {
    auto it = address_to_label.find(address);
    current_block_label = (it != address_to_label.end()) ? it->second : module.OpLabel();
    address_to_label[address] = current_block_label;
    module.AddLabel(current_block_label);
  }

  inside_block = true;
  block_needs_termination = true;
}

void ShaderCodeWriterSirit::EndBlockWithAddress() {
  LOG_DEBUG(Xenos, "[AST::Sirit] EndBlockWithAddress()");
  // Do nothing here
}

void ShaderCodeWriterSirit::ControlFlowEnd() {
  LOG_DEBUG(Xenos, "[AST::Sirit] ControlFlowEnd()");
  // No-op currently
}

void ShaderCodeWriterSirit::ControlFlowReturn(const u32 targetAddress) {
  LOG_DEBUG(Xenos, "[AST::Sirit] ControlFlowReturn(0x{:X})", targetAddress);
  module.OpReturn();
}

void ShaderCodeWriterSirit::ControlFlowCall(const u32 targetAddress) {
  LOG_DEBUG(Xenos, "[AST::Sirit] ControlFlowCall(0x{:X})", targetAddress);
  // TODO: Fix the arg for ControlFlowCall
  Sirit::Id target_function = module.OpFunctionCall(current_function, { current_block_label });
  module.OpBranch(target_function);
}

void ShaderCodeWriterSirit::ControlFlowJump(const u32 targetAddress) {
  LOG_DEBUG(Xenos, "[AST::Sirit] ControlFlowJump(0x{:X})", targetAddress);
  Sirit::Id target;
  auto it = address_to_label.find(targetAddress);
  if (it != address_to_label.end()) {
    target = it->second;
  } else {
    target = module.OpLabel();
  }
  module.OpBranch(current_block_label);
}

void ShaderCodeWriterSirit::LoopBegin(const u32 targetAddress) {
  LOG_DEBUG(Xenos, "[AST::Sirit] LoopBegin(0x{:X})", targetAddress);
  Sirit::Id merge_label = module.OpLabel();
  Sirit::Id continue_label = module.OpLabel();
  Sirit::Id loop_label = module.OpLabel();

  loop_stack.push({ merge_label, continue_label });

  module.AddLabel(loop_label);

  // Declare as a loop merge block (continue and merge)
  module.OpLoopMerge(merge_label, continue_label, spv::LoopControlMask::MaskNone);
  module.OpBranch(continue_label);

  // Add the continue block (begin of the loop body)
  module.AddLabel(continue_label);
}

void ShaderCodeWriterSirit::LoopEnd(const u32 targetAddress) {
  LOG_DEBUG(Xenos, "[AST::Sirit] LoopEnd(0x{:X})", targetAddress);
  auto [merge_label, continue_label] = loop_stack.top();
  loop_stack.pop();

  // Jump back to loop label
  module.OpBranch(continue_label);

  // Add the merge label for exiting the loop
  module.AddLabel(merge_label);
}

void ShaderCodeWriterSirit::SetPredicate(const Chunk &newValue) {
  LOG_DEBUG(Xenos, "[AST::Sirit] SetPredicate({})", newValue.id.value);
  module.OpStore(predicate_var, newValue.id);
}

void ShaderCodeWriterSirit::PushPredicate(const Chunk &newValue) {
  LOG_DEBUG(Xenos, "[AST::Sirit] PushPredicate({})", newValue.id.value);
}

void ShaderCodeWriterSirit::PopPredicate() {
  LOG_DEBUG(Xenos, "[AST::Sirit] PopPredicate()");
}

void ShaderCodeWriterSirit::Assign(const Chunk &dest, const Chunk &src) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Assign({}, {})", dest.id.value, src.id.value);
  if (!dest.HasPointer()) {
    LOG_ERROR(Xenos, "[AST::Sirit] Attempted to assign to a non-addressable Chunk!");
    return;
  }

  module.OpStore(dest.ptr, src);
}

void ShaderCodeWriterSirit::Emit(const Chunk &src) {
  LOG_DEBUG(Xenos, "[AST::Sirit] Emit({})", src.id.value);

}

void ShaderCodeWriterSirit::AssignMasked(const Chunk &src, const Chunk &dst,
  std::span<const eSwizzle> dstSwizzle,
  std::span<const eSwizzle> srcSwizzle) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AssignMasked({}, {})", src.id.value, dst.id.value);
  Sirit::Id float_type = module.TypeFloat(32);

  // Extract current dest components
  std::array<Sirit::Id, 4> components = {};
  for (u8 i = 0; i != 4; ++i) {
    components[i] = module.OpCompositeExtract(float_type, dst.id, i);
  }

  // Apply masked assignment from src
  for (u64 i = 0; i < dstSwizzle.size(); ++i) {
    u32 srcIndex = static_cast<u32>(srcSwizzle[i]);
    u32 dstIndex = static_cast<u32>(dstSwizzle[i]);
    if (srcIndex >= 4 || dstIndex >= 4) {
      LOG_ERROR(Xenos, "Invalid swizzle index: srcIndex={}, dstIndex={}", srcIndex, dstIndex);
      continue;
    }
    
    if (src.type == eChunkType::Vector || src.type == eChunkType::Fetch) {
      // Extract src vector component
      components[dstIndex] = module.OpCompositeExtract(float_type, src.id, srcIndex);
    } else if (src.type == eChunkType::Scalar) {
      // Use scalar directly
      components[dstIndex] = src.id;
    } else {
      // Default to 0 if something is off
      components[dstIndex] = module.Constant(float_type, 0.f);
    }
  }

  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id newVec = module.OpCompositeConstruct(vec4_type, components);
  module.OpStore(dst.ptr, newVec);
}

void ShaderCodeWriterSirit::AssignImmediate(const Chunk &dst,
  std::span<const eSwizzle> dstSwizzle,
  std::span<const eSwizzle> immediateValues) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AssignImmediate()");
  Sirit::Id float_type = module.TypeFloat(32);

  std::array<Sirit::Id, 4> components = {};
  for (u8 i = 0; i != 4; ++i) {
    components[i] = module.OpCompositeExtract(float_type, dst.id, i);
  }

  for (u64 i = 0; i != dstSwizzle.size(); ++i) {
    f32 value = 0.f;
    switch (immediateValues[i]) {
    case eSwizzle::One: value = 1.f; break;
    case eSwizzle::Zero: value = 0.f; break;
    case eSwizzle::Unused: value = 0.f; break;
    default:
      LOG_ERROR(Xenos, "[AST::Sirit] Invalid immediate swizzle");
      continue;
    }

    Sirit::Id imm = module.Constant(float_type, value);
    u32 dstIndex = static_cast<u32>(dstSwizzle[i]);
    components[dstIndex] = imm;
  }

  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  Sirit::Id newVec = module.OpCompositeConstruct(vec4_type, components);
  module.OpStore(dst.ptr, newVec);
}

} // namespace Xe::Microcode::AST
#endif
