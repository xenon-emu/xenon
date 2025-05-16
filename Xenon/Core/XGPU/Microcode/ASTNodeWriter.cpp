// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "ASTNode.h"
#include "ASTNodeWriter.h"

namespace Xe::Microcode::AST {

Expression NodeWriter::EmitReadReg(u32 idx) {
  return { std::make_shared<ReadRegister>(idx) };
}

Expression NodeWriter::EmitWriteReg(bool pixelShader, u32 exported, u32 idx) {
  if (exported) {
    if (pixelShader) {
      switch (idx) {
      #define COLOR(x) case x: return { std::make_shared<WriteExportRegister>(eExportReg::COLOR##x) };
      COLOR(0);
      COLOR(1);
      COLOR(2);
      COLOR(3);
      }
    } else {
      switch (idx) {
      #define INTERP(x) case x: return { std::make_shared<WriteExportRegister>(eExportReg::INTERP##x) };
      INTERP(0);
      INTERP(1);
      INTERP(2);
      INTERP(3);
      INTERP(4);
      INTERP(5);
      INTERP(6);
      INTERP(7);
      case 62: return { std::make_shared<WriteExportRegister>(eExportReg::POSITION) };
      case 63: return { std::make_shared<WriteExportRegister>(eExportReg::POINTSIZE) };
      }
    }
  }
  return { std::make_shared<WriteRegister>(idx) };
}

Expression NodeWriter::EmitBoolConst(bool pixelShader, u32 idx) {
  return { std::make_shared<BoolConstant>(pixelShader, idx) };
}

Expression NodeWriter::EmitFloatConst(bool pixelShader, u32 idx) {
  return { std::make_shared<FloatConstant>(pixelShader, idx) };
}

Expression NodeWriter::EmitFloatConstRel(bool pixelShader, u32 regOffset) {
  return { std::make_shared<FloatRelativeConstant>(pixelShader, regOffset) };
}

Expression NodeWriter::EmitGetPredicate() {
  return { std::make_shared<GetPredicate>() };
}

Expression NodeWriter::EmitAbs(Expression code) {
  return { std::make_shared<Abs>(code.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitNegate(Expression code) {
  return { std::make_shared<Negate>(code.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitNot(Expression code) {
  return { std::make_shared<Not>(code.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitReadSwizzle(Expression src, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) {
  return { std::make_shared<Swizzle>(src.Get<ExpressionNode>(), x, y, z, w) };
}

Expression NodeWriter::EmitSaturate(Expression dest) {
  return { std::make_shared<Saturate>(dest.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitVertexFetch(Expression src, u32 slot, u32 offset, u32 stride, instr_surf_fmt_t fmt, bool isFloat, bool isSigned, bool isNormalized) {
  return { std::make_shared<VertexFetch>(src.Get<ExpressionNode>(), slot, offset, stride, fmt, isFloat, isSigned, isNormalized) };
}

Expression NodeWriter::EmitTextureSample1D(Expression src, u32 slot) {
  return { std::make_shared<TextureFetch>(src.Get<ExpressionNode>(), slot, DIMENSION_1D) };
}

Expression NodeWriter::EmitTextureSample2D(Expression src, u32 slot) {
  return { std::make_shared<TextureFetch>(src.Get<ExpressionNode>(), slot, DIMENSION_2D) };
}

Expression NodeWriter::EmitTextureSample3D(Expression src, u32 slot) {
  return { std::make_shared<TextureFetch>(src.Get<ExpressionNode>(), slot, DIMENSION_3D) };
}

Expression NodeWriter::EmitTextureSampleCube(Expression src, u32 slot) {
  return { std::make_shared<TextureFetch>(src.Get<ExpressionNode>(), slot, DIMENSION_CUBE) };
}

Statement NodeWriter::EmitMergeStatements(Statement prev, Statement next) {
  if (!prev)
    return next;
  if (!next)
    return prev;
  return { std::make_shared<ListStatement>(prev.Get<StatementNode>(), next.Get<StatementNode>()) };
}

Statement NodeWriter::EmitConditionalStatement(Expression condition, Statement code) {
  if (!condition)
    return code;
  if (!code)
    return {};
  return { std::make_shared<ConditionalStatement>(code.Get<StatementNode>(), condition.Get<ExpressionNode>()) };
}

Statement NodeWriter::EmitWriteWithSwizzleStatement(Expression dest, Expression src, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) {
  if (!dest)
    return {};
  if (!src)
    return {};
  return { std::make_shared<WriteWithMaskStatement>(dest.Get<ExpressionNode>(), src.Get<ExpressionNode>(), x, y, z, w) };
}

Statement NodeWriter::EmitSetPredicateStatement(Expression value) {
  if (!value)
    return {};
  return { std::make_shared<SetPredicateStatement>(value.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitVectorInstruction1(instr_vector_opc_t instr, Expression a) {
  if (!a)
    return {};
  return { std::make_shared<VectorFunc1>(instr, a.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitVectorInstruction2(instr_vector_opc_t instr, Expression a, Expression b) {
  if (!a || !b)
    return {};
  return { std::make_shared<VectorFunc2>(instr, a.Get<ExpressionNode>(), b.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitVectorInstruction3(instr_vector_opc_t instr, Expression a, Expression b, Expression c) {
  if (!a || !b || !c)
    return {};
  return { std::make_shared<VectorFunc3>(instr, a.Get<ExpressionNode>(), b.Get<ExpressionNode>(), c.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitScalarInstruction1(instr_scalar_opc_t instr, Expression a) {
  if (!a)
    return {};
  return { std::make_shared<ScalarFunc1>(instr, a.Get<ExpressionNode>()) };
}

Expression NodeWriter::EmitScalarInstruction2(instr_scalar_opc_t instr, Expression a, Expression b) {
  if (!a || !b)
    return {};
  return { std::make_shared<ScalarFunc2>(instr, a.Get<ExpressionNode>(), b.Get<ExpressionNode>()) };
}

void NodeWriter::EmitNop() {
  // Just do nothing
}

void NodeWriter::EmitExec(const u32 addr, instr_cf_opc_t type, Statement preamble, Statement code, Expression condition, const bool endOfShader) {
  if (!code)
    return;
  Block *block = new Block(addr, preamble.Get<StatementNode>(), code.Get<StatementNode>(), condition.Get<ExpressionNode>());
  createdBlocks.push_back(block);
  if (endOfShader) {
    createdBlocks.push_back(new Block(nullptr, 0, eBlockType::END));
  }
}

void NodeWriter::EmitJump(const u32 addr, Statement preamble, Expression condition) {
  Block *block = new Block(condition.Get<ExpressionNode>(), addr, eBlockType::JUMP);
  createdBlocks.push_back(block);
}

void NodeWriter::EmitLoopStart(const u32 addr, Statement preamble, Expression condition) {
  Block *block = new Block(
    condition ? condition.Get<ExpressionNode>() : nullptr,
    preamble ? preamble.Get<StatementNode>() : nullptr,
    addr,
    eBlockType::LOOP_BEGIN
  );
  createdBlocks.push_back(block);
}

void NodeWriter::EmitLoopEnd(const u32 addr, Expression condition) {
  Block *block = new Block(
    condition ? condition.Get<ExpressionNode>() : nullptr,
    addr,
    eBlockType::LOOP_END
  );
  createdBlocks.push_back(block);
}

void NodeWriter::EmitCall(const u32 addr, Statement preamble, Expression condition) {
  Block *block = new Block(condition.Get<ExpressionNode>(), addr, eBlockType::JUMP);
  createdBlocks.push_back(block);
}

void NodeWriter::EmitExportAllocPosition() {
  positionExported = true;
}

void NodeWriter::EmitExportAllocParam(const u32 size) {
  numParamExports += size;
}

void NodeWriter::EmitExportAllocMemExport(const u32 size) {
  numMemoryExports += size;
}

} // namespace Xe::Microcode::AST
