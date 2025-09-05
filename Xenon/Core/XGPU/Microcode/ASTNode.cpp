// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"

#include "ASTNode.h"

#include "ASTNodeWriter.h"
#include "ASTEmitter.h"

namespace Xe::Microcode::AST {

Chunk ReadRegister::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  switch (regType) {
  case eRegisterType::Temporary:
    return writer.GetTemporaryReg(regIndex);
  case eRegisterType::Constant:
    return writer.GetConstantReg(regIndex);
  case eRegisterType::VertexInput:
    return writer.GetVertexInputReg(regIndex);
  case eRegisterType::PixelInput:
    return writer.GetPixelInputReg(regIndex);
  default:
    return writer.GetTemporaryReg(regIndex);
  }
}

Chunk WriteRegister::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  switch (regType) {
  case eRegisterType::Temporary:
    return writer.GetTemporaryReg(regIndex);
  case eRegisterType::Constant:
    return writer.GetConstantReg(regIndex);
  case eRegisterType::VertexInput:
    return writer.GetVertexInputReg(regIndex);
  case eRegisterType::PixelInput:
    return writer.GetPixelInputReg(regIndex);
  default:
    return writer.GetTemporaryReg(regIndex);
  }
}

Chunk WriteExportRegister::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  return writer.GetExportDest(exportReg);
}

s32 WriteExportRegister::GetExportSemanticIndex(const eExportReg reg) {
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
  default: return 100;
  }
}

s32 WriteExportRegister::GetExportInterpolatorIndex(const eExportReg reg) {
  switch (reg) {
  case eExportReg::INTERP0: return 0;
  case eExportReg::INTERP1: return 1;
  case eExportReg::INTERP2: return 2;
  case eExportReg::INTERP3: return 3;
  case eExportReg::INTERP4: return 4;
  case eExportReg::INTERP5: return 5;
  case eExportReg::INTERP6: return 6;
  case eExportReg::INTERP7: return 7;
  default: return -1;
  }
}

Chunk BoolConstant::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  return writer.GetBoolVal(index);
}

Chunk FloatConstant::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  return writer.GetFloatVal(index);
}

Chunk FloatRelativeConstant::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  return writer.GetFloatValRelative(relativeOffset);
}

Chunk GetPredicate::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  return writer.GetPredicate();
}

Chunk Abs::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[Abs::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.Abs(src);
}

Chunk Negate::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[Negate::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.Negate(src);
}

Chunk Not::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[Not::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.Not(src);
}

Chunk Saturate::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[Saturate::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.Saturate(src);
}

Chunk Swizzle::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[Swizzle::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.Swizzle(src, swizzle);
}

Chunk VertexFetch::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[VertexFetch::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.FetchVertex(src, *this, shader);
}

Chunk TextureFetch::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk src = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[TextureFetch::EmitShaderCode]: src = {}", children[0]->GetName());
  return writer.FetchTexture(src, *this);
}

Chunk VectorFunc1::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk a = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[VectorFunc1::EmitShaderCode]: a = {}", children[0]->GetName());
  return writer.VectorFunc1(vectorInstr, a);
}

Chunk VectorFunc2::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk a = children[0]->EmitShaderCode(writer, shader);
  Chunk b = children[1]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[VectorFunc2::EmitShaderCode]: a = {}, b = {}", children[0]->GetName(), children[1]->GetName());
  return writer.VectorFunc2(vectorInstr, a, b);
}

Chunk VectorFunc3::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk a = children[0]->EmitShaderCode(writer, shader);
  Chunk b = children[1]->EmitShaderCode(writer, shader);
  Chunk c = children[2]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[VectorFunc3::EmitShaderCode]: a = {}, b = {}, c = {}", children[0]->GetName(), children[1]->GetName(), children[2]->GetName());
  return writer.VectorFunc3(vectorInstr, a, b, c);
}

Chunk ScalarFunc0::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  return writer.ScalarFunc0(scalarInstr);
}

Chunk ScalarFunc1::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk a = children[0]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[ScalarFunc1::EmitShaderCode]: a = {}", children[0]->GetName());
  return writer.ScalarFunc1(scalarInstr, a);
}

Chunk ScalarFunc2::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk a = children[0]->EmitShaderCode(writer, shader);
  Chunk b = children[1]->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[ScalarFunc2::EmitShaderCode]: a = {}, b = {}", children[0]->GetName(), children[1]->GetName());
  return writer.ScalarFunc2(scalarInstr, a, b);
}

} // namespace Xe::Microcode::AST
