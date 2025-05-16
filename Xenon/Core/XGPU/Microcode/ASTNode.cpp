// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "ASTNode.h"

#include "ASTNodeWriter.h"
#include "ASTEmitter.h"

namespace Xe::Microcode::AST {

Chunk ReadRegister::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.GetReg(regIndex);
}

Chunk WriteRegister::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.GetReg(regIndex);
}

Chunk WriteExportRegister::EmitShaderCode(ShaderCodeWriterBase &writer) {
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

Chunk BoolConstant::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.GetBoolVal(index);
}

Chunk FloatConstant::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.GetFloatVal(index);
}

Chunk FloatRelativeConstant::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.GetFloatValRelative(relativeOffset);
}

Chunk GetPredicate::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.GetPredicate();
}

Chunk Abs::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.Abs(children[0].get());
}

Chunk Negate::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.Negate(children[0].get());
}

Chunk Not::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.Not(children[0].get());
}

Chunk Saturate::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.Saturate(children[0].get());
}

Chunk Swizzle::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.Swizzle(children[0].get(), swizzle);
}

Chunk VertexFetch::EmitShaderCode(ShaderCodeWriterBase &writer) {
  Chunk src = children[0]->EmitShaderCode(writer);
  return writer.FetchVertex(src, *this);
}

Chunk TextureFetch::EmitShaderCode(ShaderCodeWriterBase &writer) {
  Chunk src = children[0]->EmitShaderCode(writer);
  return writer.FetchTexture(src, *this);
}

Chunk VectorFunc1::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.VectorFunc1(vectorInstr, children[0].get());
}

Chunk VectorFunc2::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.VectorFunc2(vectorInstr, children[0].get(), children[1].get());
}

Chunk VectorFunc3::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.VectorFunc3(vectorInstr, children[0].get(), children[1].get(), children[2].get());
}

Chunk ScalarFunc1::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.ScalarFunc1(scalarInstr, children[0].get());
}

Chunk ScalarFunc2::EmitShaderCode(ShaderCodeWriterBase &writer) {
  return writer.ScalarFunc2(scalarInstr, children[0].get(), children[1].get());
}

} // namespace Xe::Microcode::AST
