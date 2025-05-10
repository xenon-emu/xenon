// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Constants.h"
#include "ASTNode.h"
#include "ASTNodeBase.h"
#include "ASTNodeWriterBase.h"

namespace Xe::Microcode {

class ShaderNodeWriter : public AST::NodeWriterBase {
public:
  ShaderNodeWriter(eShaderType type);
  ~ShaderNodeWriter();
  void TransformShader(AST::NodeWriterBase &nodeWriter, const u32 *words, const u32 numWords);
private:
  void TransformBlock(AST::NodeWriterBase &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_t *cf, u32 &pc);

  AST::Statement EmitExec(AST::NodeWriterBase &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_exec_t &exec);
  AST::Statement EmitALU(AST::NodeWriterBase &nodeWriter, const instr_alu_t &alu, const bool sync);
  AST::Statement EmitVertexFetch(AST::NodeWriterBase &nodeWriter, const instr_fetch_vtx_t &vtx, const bool sync);
  AST::Statement EmitTextureFetch(AST::NodeWriterBase &nodeWriter, const instr_fetch_tex_t &tex, const bool sync);
  AST::Statement EmitPredicateTest(AST::NodeWriterBase &nodeWriter, const AST::Statement &code, const bool conditional, const u32 flowPredCondition, const u32 predSelect, const u32 predCondition);

  AST::Expression EmitSrcReg(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const u32 num, const u32 type, const u32 swiz, const u32 negate, const s32 slot);
  AST::Expression EmitSrcReg(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const u32 argIndex);
  AST::Expression EmitSrcScalarReg1(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr);
  AST::Expression EmitSrcScalarReg2(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr);
  AST::Statement EmitVectorResult(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const AST::Expression &code);
  AST::Statement EmitScalarResult(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const AST::Expression &code);

  static const u32 GetArgCount(instr_vector_opc_t instr);
  static const u32 GetArgCount(instr_scalar_opc_t instr);

  eShaderType shaderType = eShaderType::Unknown;
  u32 lastVertexStride = 0;
};

} // namespace Xe::Microcode
