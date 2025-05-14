// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "ASTEmitter.h"

namespace Xe::Microcode::AST {

ShaderCodeWriterSirit::ShaderCodeWriterSirit(eShaderType shaderType) : type(shaderType) {
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
  
  // Shader structure

  if (type == eShaderType::Pixel) {
    output_vars[eExportReg::COLOR0] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::COLOR1] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::COLOR2] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::COLOR3] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
  } else {
    output_vars[eExportReg::POSITION] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::POINTSIZE] = module.AddGlobalVariable(float_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP0] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP1] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP2] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP3] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP4] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP5] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP6] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
    output_vars[eExportReg::INTERP7] = module.AddGlobalVariable(vec4_type, spv::StorageClass::Output);
  }

  // Global variable
  gpr_var = module.AddGlobalVariable(gpr_array_ptr_type, spv::StorageClass::Private); // float4 R[128]
  Sirit::Id bool_type = module.TypeBool();
  Sirit::Id bool_ptr_type = module.TypePointer(spv::StorageClass::Private, bool_type);

  // Declare global predicate register
  Sirit::Id false_val = module.ConstantFalse(bool_type);
  predicate_var = module.AddGlobalVariable(bool_ptr_type, spv::StorageClass::Private, false_val);
}

void ShaderCodeWriterSirit::BeginMain() {
  LOG_DEBUG(Xenos, "[AST::Sirit] BeginMain()");
  Sirit::Id void_type = module.TypeVoid();
  Sirit::Id func_type = module.TypeFunction(void_type);
  main_func = module.OpFunction(void_type, spv::FunctionControlMask::MaskNone, func_type);
  main_label = module.AddLabel();
  if (type == eShaderType::Vertex) {
    Sirit::Id uint_type = module.TypeInt(32, false);

    // VertexID (SV_VertexID)
    Sirit::Id vertex_id_ptr = module.TypePointer(spv::StorageClass::Input, uint_type);
    Sirit::Id vertex_id = module.AddGlobalVariable(vertex_id_ptr, spv::StorageClass::Input);
    input_vars["VertexID"] = vertex_id;

    // InstanceID (SV_InstanceID)
    Sirit::Id instance_id_ptr = module.TypePointer(spv::StorageClass::Input, uint_type);
    Sirit::Id instance_id = module.AddGlobalVariable(instance_id_ptr, spv::StorageClass::Input);
    input_vars["InstanceID"] = instance_id;
  }
}

void ShaderCodeWriterSirit::FinalizeEntryPoint() {
  LOG_DEBUG(Xenos, "[AST::Sirit] FinalizeEntryPoint()");
  std::vector<Sirit::Id> interface_vars;

  if (type == eShaderType::Pixel) {
    // Fragment shader entry point
    for (const auto& [reg, var] : output_vars) {
      uint32_t location = 0;
      switch (reg) {
        case eExportReg::COLOR0: location = 0; break;
        case eExportReg::COLOR1: location = 1; break;
        case eExportReg::COLOR2: location = 2; break;
        case eExportReg::COLOR3: location = 3; break;
        default: continue; // Ignore unknowns
      }
      module.Decorate(var, spv::Decoration::Location, location);
      interface_vars.push_back(var);
    }

    module.AddExecutionMode(main_func, spv::ExecutionMode::OriginUpperLeft);
    module.AddExecutionMode(main_func, spv::ExecutionMode::PixelCenterInteger);

    module.AddEntryPoint(spv::ExecutionModel::Fragment, main_func, "main", interface_vars);
  } else {
    // Vertex shader entry point
    if (input_vars.count("VertexID")) {
      module.Decorate(input_vars["VertexID"], spv::Decoration::BuiltIn, spv::BuiltIn::VertexIndex);
      interface_vars.push_back(input_vars["VertexID"]);
    }
    if (input_vars.count("InstanceID")) {
      module.Decorate(input_vars["InstanceID"], spv::Decoration::BuiltIn, spv::BuiltIn::InstanceIndex);
      interface_vars.push_back(input_vars["InstanceID"]);
    }

    for (const auto& [reg, var] : output_vars) {
      switch (reg) {
        case eExportReg::POSITION:
          module.Decorate(var, spv::Decoration::BuiltIn, spv::BuiltIn::Position);
          break;
        case eExportReg::POINTSIZE:
          module.Decorate(var, spv::Decoration::BuiltIn, spv::BuiltIn::PointSize);
          break;
        default:
          module.Decorate(var, spv::Decoration::Location, static_cast<u32>(reg) - static_cast<u32>(eExportReg::INTERP0));
          break;
      }
      interface_vars.push_back(var);
    }

    module.AddEntryPoint(spv::ExecutionModel::Vertex, main_func, "main", interface_vars);
  }
}

void ShaderCodeWriterSirit::EndMain() {
  LOG_DEBUG(Xenos, "[AST::Sirit] EndMain()");
  module.OpReturn();
  module.OpFunctionEnd();
  FinalizeEntryPoint(); // Add this line
}

Chunk ShaderCodeWriterSirit::GetExportDest(const eExportReg reg) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetExportDest({})", (u32)reg);
  auto it = output_vars.find(reg);
  if (it == output_vars.end()) {
    LOG_ERROR(Xenos, "[AST::Sirit] Unknown export register used in GetExportDest!");
    return {};
  }
  Sirit::Id var_id = it->second;
  Sirit::Id loaded = {};
  if (type == eShaderType::Pixel) {
    Sirit::Id vec4_type = module.TypeVector(module.TypeFloat(32), 4);
    loaded = module.OpLoad(vec4_type, var_id);
  } else {
    if (reg == eExportReg::POINTSIZE) {
      Sirit::Id float_type = module.TypeFloat(32);
      loaded = module.OpLoad(float_type, var_id);
    }
    else {
      Sirit::Id vec4_type = module.TypeVector(module.TypeFloat(32), 4);
      loaded = module.OpLoad(vec4_type, var_id);
    }
  }

  return Chunk{ loaded, var_id };
}

Chunk ShaderCodeWriterSirit::GetReg(u32 regIndex) {
  LOG_DEBUG(Xenos, "[AST::Sirit] GetReg({})", regIndex);
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
  LOG_DEBUG(Xenos, "[AST::Sirit] GetBoolVal({})", boolRegIndex);
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
  LOG_DEBUG(Xenos, "[AST::Sirit] GetFloatVal({})", floatRegIndex);
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
  LOG_DEBUG(Xenos, "[AST::Sirit] GetFloatValRelative({})", floatRegOffset);
  Sirit::Id vec4_type = module.TypeVector(module.TypeFloat(32), 4);
  Sirit::Id base_ptr_type = module.TypePointer(spv::StorageClass::Private, vec4_type);

  Sirit::Id uint_type = module.TypeInt(32, false);
  Sirit::Id offset_id = module.Constant(uint_type, floatRegOffset);

  Sirit::Id base_address = module.OpAccessChain(base_ptr_type, gpr_var, offset_id);

  // Load the value (A0 + offset)
  Sirit::Id loaded_value = module.OpLoad(vec4_type, base_address);

  Sirit::Id x_value = module.OpCompositeExtract(module.TypeFloat(32), loaded_value, 0);

  return Chunk(x_value);
}

Chunk ShaderCodeWriterSirit::GetPredicate() {
  Sirit::Id bool_type = module.TypeBool();
  Sirit::Id pred_val = module.OpLoad(bool_type, predicate_var);
  return Chunk(pred_val);
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
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(FRACv)");
    return Chunk(module.OpFMod(vec4_type, a.id, module.Constant(float_type, 1.f))); // frac(a)
  case TRUNCv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(TRUNCv)");
    return Chunk(module.OpTrunc(vec4_type, a.id)); // trunc(a)
  case FLOORv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc1(FLOORv)");
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
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(ADDv)");
    return Chunk(module.OpFAdd(vec4_type, a.id, b.id));
  case MULv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(MULv)");
    return Chunk(module.OpFMul(vec4_type, a.id, b.id));
  case MAXv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(MAXv)");
    return Chunk(module.OpFMax(vec4_type, a.id, b.id));
  case MINv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc2(MINv)");
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
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(DOT4v)");
    return Chunk(module.OpDot(float_type, a.id, b.id)); // Dot product of a and b (vector type)
  case DOT3v:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(DOT3v)");
    return Chunk(module.OpDot(float_type, a.id, b.id)); // Dot product of a and b (3D version)
  case DOT2ADDv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(DOT2ADDv)");
    return Chunk(module.OpFAdd(float_type, module.OpDot(float_type, a.id, b.id), c.id)); // Compute the dot product
  case CUBEv:
    LOG_DEBUG(Xenos, "[AST::Sirit] VectorFunc3(CUBEv)");
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
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(ADDs)");
    return Chunk(module.OpFAdd(float_type, a.id, a.id)); // a + a
  case MULs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(MULs)");
    return Chunk(module.OpFMul(float_type, a.id, a.id)); // a * a
  case MAXs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(MAXs)");
    return Chunk(module.OpFMax(float_type, a.id, a.id)); // max(a, a)
  case MINs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(MINs)");
    return Chunk(module.OpFMin(float_type, a.id, a.id)); // min(a, a)
  case FRACs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(FRACs)");
    return Chunk(module.OpFMod(float_type, a.id, module.Constant(float_type, 1.0f))); // frac(a)
  case TRUNCs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(TRUNCs)");
    return Chunk(module.OpTrunc(float_type, a.id)); // trunc(a)
  case FLOORs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc1(FLOORs)");
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
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(ADDs)");
    return Chunk(module.OpFAdd(float_type, a.id, b.id)); // a + b
  case MULs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(MULs)");
    return Chunk(module.OpFMul(float_type, a.id, b.id)); // a * b
  case MAXs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(MAXs)");
    return Chunk(module.OpFMax(float_type, a.id, b.id)); // max(a, b)
  case MINs:
    LOG_DEBUG(Xenos, "[AST::Sirit] ScalarFunc2(MINs)");
    return Chunk(module.OpFMin(float_type, a.id, b.id)); // min(a, b)
  default:
    LOG_ERROR(Xenos, "[AST::Emitter] Unsupported scalar binary op!");
    return {};
  }
}

Chunk ShaderCodeWriterSirit::AllocLocalImpl(Sirit::Id type, const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalImpl({}, {})", type.value, initCode.id.value);
  Sirit::Id ptr_type = module.TypePointer(spv::StorageClass::Function, type);

  // Declare local variable
  Sirit::Id local_ptr = module.AddLocalVariable(ptr_type, spv::StorageClass::Function);

  // Store initial value
  module.OpStore(local_ptr, initCode.id);

  // Load it for use (if you want to return r-value Chunk)
  Sirit::Id loaded = module.OpLoad(type, local_ptr);

  return Chunk{ loaded, local_ptr };
}

Chunk ShaderCodeWriterSirit::AllocLocalVector(const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalVector({})", initCode.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  Sirit::Id vec4_type = module.TypeVector(float_type, 4);
  return AllocLocalImpl(vec4_type, initCode);
}

Chunk ShaderCodeWriterSirit::AllocLocalScalar(const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalScalar({})", initCode.id.value);
  Sirit::Id float_type = module.TypeFloat(32);
  return AllocLocalImpl(float_type, initCode);
}

Chunk ShaderCodeWriterSirit::AllocLocalBool(const Chunk &initCode) {
  LOG_DEBUG(Xenos, "[AST::Sirit] AllocLocalBool({})", initCode.id.value);
  Sirit::Id bool_type = module.TypeBool();
  return AllocLocalImpl(bool_type, initCode);
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
  Sirit::Id void_type = module.TypeVoid();
  Sirit::Id func_type = module.TypeFunction(void_type);
  current_function = module.OpFunction(void_type, spv::FunctionControlMask::MaskNone, func_type);

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

void ShaderCodeWriterSirit::AssignMasked(ExpressionNode *src, ExpressionNode *dst,
  std::span<const eSwizzle> dstSwizzle,
  std::span<const eSwizzle> srcSwizzle) {

}

void ShaderCodeWriterSirit::AssignImmediate(ExpressionNode *dst,
  std::span<const eSwizzle> dstSwizzle,
  std::span<const eSwizzle> immediateValues) {

}

} // namespace Xe::Microcode::AST
