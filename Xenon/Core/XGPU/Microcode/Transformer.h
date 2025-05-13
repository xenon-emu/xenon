// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Constants.h"
#include "ASTNode.h"
#include "ASTNodeBase.h"
#include "ASTNodeWriter.h"

namespace Xe::Microcode {

class ShaderNodeWriter : public AST::NodeWriter {
public:
  ShaderNodeWriter(eShaderType type);
  ~ShaderNodeWriter();
  void TransformShader(AST::NodeWriter &nodeWriter, const u32 *words, const u32 numWords);
private:
  void TransformBlock(AST::NodeWriter &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_t &cf, u32 &pc);

  AST::Statement EmitExec(AST::NodeWriter &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_exec_t &exec);
  AST::Statement EmitALU(AST::NodeWriter &nodeWriter, const instr_alu_t &alu, const bool sync);
  AST::Statement EmitVertexFetch(AST::NodeWriter &nodeWriter, const instr_fetch_vtx_t &vtx, const bool sync);
  AST::Statement EmitTextureFetch(AST::NodeWriter &nodeWriter, const instr_fetch_tex_t &tex, const bool sync);
  AST::Statement EmitPredicateTest(AST::NodeWriter &nodeWriter, const AST::Statement &code, const bool conditional, const u32 flowPredCondition, const u32 predSelect, const u32 predCondition);

  AST::Expression EmitSrcReg(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const u32 num, const u32 type, const u32 swizzle, const u32 negate, const s32 slot);
  AST::Expression EmitSrcReg(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const u32 argIndex);
  AST::Expression EmitSrcScalarReg1(AST::NodeWriter &nodeWriter, const instr_alu_t &instr);
  AST::Expression EmitSrcScalarReg2(AST::NodeWriter &nodeWriter, const instr_alu_t &instr);
  AST::Statement EmitVectorResult(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const AST::Expression &code);
  AST::Statement EmitScalarResult(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const AST::Expression &code);

  static const u32 GetArgCount(instr_vector_opc_t instr);
  static const u32 GetArgCount(instr_scalar_opc_t instr);

  eShaderType shaderType = eShaderType::Unknown;
  u32 lastVertexStride = 0;
};

} // namespace Xe::Microcode
