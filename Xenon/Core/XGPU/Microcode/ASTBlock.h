// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <set>
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
  inline Block* GetTarget() const { return target; }
  inline std::vector<Block*> GetSources() const { return sources; }
  inline Block* GetContinuation() const { return continuation; }
  inline AST::ExpressionNode* GetCondition() const { return condition.get(); }
  inline AST::StatementNode* GetCode() const { return codeStatement.get(); }
  inline AST::StatementNode* GetPreamble() const { return preambleStatement.get(); }
  inline u32 GetAddress() const { return address; }
  inline u32 GetTargetAddress() const { return targetAddress; }
  inline eBlockType GetType() const { return type; }
private:
  // Block type
  eBlockType type;
  // Generalized address (used to link blocks together)
  u32 address = 0;
  // Target address - only for JUMP and CALL
  u32 targetAddress = 0;
  // Condition for this block of code
  std::shared_ptr<AST::ExpressionNode> condition = nullptr;
  // Code for this block (executed inside conditional branch)
  std::shared_ptr<AST::StatementNode> codeStatement = nullptr;
  // Part of code executed outside the conditional branch
  std::shared_ptr<AST::StatementNode> preambleStatement = nullptr;
  // Blocks jumping to this block
  std::vector<Block*> sources = {};
  // Resolved target block, only for JUMP and CALL
  Block *target = nullptr;
  // Continuation block (branchless case, simply put, the next block to execute)
  Block *continuation = nullptr;
};

class ControlFlowGraph {
public:
  ControlFlowGraph() = default;
  ~ControlFlowGraph() {
    for (auto* block : blocks) {
      delete block;
    }
  }

  Block* GetStartBlock() const { return blocks.front(); }
  u32 GetNumBlocks() const { return static_cast<u32>(blocks.size()); }
  Block* GetBlock(u32 index) const { return blocks[index]; }
  u32 GetEntryPointAddress() const {
    return roots[0]->GetAddress();
  }

  static ControlFlowGraph* DecompileMicroCode(const void* code, u32 codeLength, eShaderType shaderType);

  void EmitShaderCode(AST::ShaderCodeWriterBase& writer) const;

private:
  std::vector<Block*> blocks;
  std::vector<Block*> roots;

  static void ExtractBlocks(const Block* block, std::vector<const Block*>& out, std::set<const Block*>& visited);
};

} // namespace Xe::Microcode
