// Copyright 2025 Xenon Emulator Project

#pragma once

#include <vector>

#include "Base/Types.h"

#include "ASTNode.h"
#include "ASTStatement.h"

namespace Xe::Microcode::AST {

class ShaderCodeWriterBase;

enum class eBlockType : u8 {
  EXEC,
  JUMP,
  CALL,
  END, // Shader end
  RET  // Function return
};

// Generalized block in the shader
class Block {
public:
  Block(u32 address, StatementNode::Ptr preamble, StatementNode::Ptr code, ExpressionNode::Ptr cond);
  Block(ExpressionNode::Ptr cond, u32 target, eBlockType type);
  ~Block();
  // JMP/CALL target
  void ConnectTarget(Block *targetBlock);
  // Next block to execute (NULL only for END and RET)
  void ConnectContinuation(Block *nextBlock);
  void EmitShaderCode(AST::ShaderCodeWriterBase& writer) const;
  inline u32 GetAddress() {
    return address;
  }
  inline u32 GetTargetAddress() {
    return targetAddress;
  }
private:
  // Block type
  eBlockType type;
  // Generalized address (used to link blocks together)
  u32 address = 0;
  // Target address - only for JUMP and CALL
  u32 targetAddress = 0;
  // Condition for this block of code
  std::unique_ptr<AST::ExpressionNode> condition = nullptr;
  // Code for this block (executed inside conditional branch)
  std::unique_ptr<AST::StatementNode> codeStatement = nullptr;
  // Part of code executed outside the conditional branch
  std::unique_ptr<AST::StatementNode> preambleStatement = nullptr;
  // Blocks jumping to this block
  std::vector<Block*> sources = {};
  // Resolved target block, only for JUMP and CALL
  Block *target = nullptr;
  // Continuation block (branchless case, simply put, the next block to execute)
  Block *continuation = nullptr;
};

} // namespace Xe::Microcode
