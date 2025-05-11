// Copyright 2025 Xenon Emulator Project

#pragma once

#include "ASTNodeBase.h"
#include "ASTNode.h"

namespace Xe::Microcode::AST {

class ShaderCodeWriter {
public:
  virtual ~ShaderCodeWriter() = default;

  virtual Chunk GetExportDest(const eExportReg reg) = 0;
  virtual Chunk GetReg(u32 regIndex) = 0;
  virtual Chunk GetBoolVal(const u32 boolRegIndex) = 0;
  virtual Chunk GetFloatVal(const u32 floatRegIndex) = 0;
  virtual Chunk GetFloatValRelative(const u32 floatRegOffset) = 0;
  virtual Chunk GetPredicate() = 0;

  virtual Chunk FetchVertex(Chunk src, const VertexFetch &instr) = 0;
  virtual Chunk FetchTexture(Chunk src, const TextureFetch &instr) = 0;

  virtual Chunk AllocLocalVector(Chunk initCode) = 0;
  virtual Chunk AllocLocalScalar(Chunk initCode) = 0;
  virtual Chunk AllocLocalBool(Chunk initCode) = 0;

  virtual void BeingCondition(Chunk condition) = 0;
  virtual void EndCondition() = 0;

  virtual void BeginControlFlow(const u32 address, const bool hasJumps, const bool hasCalls, const bool called) = 0;
  virtual void EndControlFlow() = 0;

  virtual void BeginBlockWithAddress(const u32 address) = 0;
  virtual void EndBlockWithAddress() = 0;

  virtual void ControlFlowEnd() = 0;
  virtual void ControlFlowReturn(const u32 targetAddress) = 0;
  virtual void ControlFlowCall(const u32 targetAddress) = 0;
  virtual void ControlFlowJump(const u32 targetAddress) = 0;

  virtual void SetPredicate(Chunk newValue) = 0;
  virtual void PushPredicate(Chunk newValue) = 0;
  virtual void PopPredicate() = 0;

  virtual void Assign(Chunk dest, Chunk src) = 0;
  virtual void Emit(Chunk src) = 0;
};

} // namespace Xe::Microcode
