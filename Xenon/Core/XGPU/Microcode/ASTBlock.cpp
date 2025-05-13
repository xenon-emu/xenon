// Copyright 2025 Xenon Emulator Project

#include <algorithm>

#include "Base/Logging/Log.h"

#include "ASTBlock.h"
#include "ASTNodeWriter.h"
#include "Transformer.h"

namespace Xe::Microcode::AST {

Block::Block(u32 address, StatementNode::Ptr preamble, StatementNode::Ptr code, ExpressionNode::Ptr cond) :
  address(address), type(eBlockType::EXEC) {

  condition = cond ? cond->CloneAs<ExpressionNode>() : nullptr;
  codeStatement = code ? code->CloneAs<StatementNode>() : nullptr;
  preambleStatement = preamble ? preamble->CloneAs<StatementNode>() : nullptr;
}

Block::Block(ExpressionNode::Ptr cond, u32 target, eBlockType type) :
  targetAddress(target), type(type) {
  condition = cond ? cond->CloneAs<ExpressionNode>() : nullptr;
}

Block::~Block() {
  condition.reset();
}

void Block::ConnectTarget(Block *targetBlock) {
  targetBlock->sources.push_back(this);
  target = targetBlock;
}

void Block::ConnectContinuation(Block *nextBlock) {
  continuation = nextBlock;
}
void Block::EmitShaderCode(ShaderCodeWriterBase &writer) const {
  // Emit preamble (outside the conditional block)
  if (preambleStatement)
    preambleStatement->EmitShaderCode(writer);

  // each block can have condition
  if (condition)
    writer.BeingCondition(condition->EmitShaderCode(writer));
  switch (type) {
  case eBlockType::JUMP: {
    writer.ControlFlowCall(target->GetAddress());
  } break;
  case eBlockType::CALL: {
    writer.ControlFlowJump(target->GetAddress());
  } break;
  case eBlockType::RET: {
    writer.ControlFlowReturn(target->GetAddress());
  } break;
  case eBlockType::END: {
    writer.ControlFlowEnd();
  } break;
  case eBlockType::EXEC: {
    codeStatement->EmitShaderCode(writer);
  } break;
  }
  // Each block can have condition
  if (condition)
    writer.EndCondition();
}

ControlFlowGraph* ControlFlowGraph::DecompileMicroCode(const void *code, u32 codeLength, eShaderType shaderType) {
  if (codeLength % 4 != 0)
    return nullptr;

  ShaderNodeWriter transformer(shaderType);
  NodeWriter blockTranslator;
  transformer.TransformShader(blockTranslator, reinterpret_cast<const u32*>(code), codeLength / 4);

  if (!blockTranslator.GetNumCreatedBlocks())
    return nullptr;

  ControlFlowGraph *graph = new ControlFlowGraph();
  const u32 numBlocks = blockTranslator.GetNumCreatedBlocks();
  graph->blocks.reserve(numBlocks);

  for (u32 i = 0; i < numBlocks; ++i) {
    graph->blocks.push_back(blockTranslator.GetCreatedBlock(i));
  }

  for (u32 i = 0; i + 1 < numBlocks; ++i) {
    Block *curr = graph->blocks[i];
    Block *next = graph->blocks[i + 1];
    eBlockType type = curr->GetType();

    if (type != eBlockType::END && type != eBlockType::RET) {
      if (type == eBlockType::JUMP || type == eBlockType::CALL) {

      }
      curr->ConnectContinuation(next);
    }
  }

  std::map<u32, Block*> addrToBlock = {};
  for (Block *block : graph->blocks) {
    if (block->GetType() == eBlockType::EXEC) {
      if (addrToBlock.contains(block->GetAddress())) {
        LOG_ERROR(Render, "[AST::ControlFlowGraph] Two blocks with the same address! 0x{:X}", block->GetAddress());
        return nullptr;
      }
      addrToBlock[block->GetAddress()] = block;
    }
  }

  std::set<Block*> functionRoots = {};
  for (Block *block : graph->blocks) {
    if (block->GetType() == eBlockType::JUMP || block->GetType() == eBlockType::CALL) {
      u32 targetAddr = block->GetTargetAddress();
      Block *target = addrToBlock[targetAddr];
      if (!target) {
        LOG_ERROR(Render, "[AST::ControlFlowGraph] Missing target block at address 0x{:X}", targetAddr);
        return nullptr;
      }
      block->ConnectTarget(target);
      if (block->GetType() == eBlockType::CALL) {
        functionRoots.insert(target);
      }
    }
  }

  graph->roots.push_back(graph->blocks[0]);
  graph->roots.insert(graph->roots.end(), functionRoots.begin(), functionRoots.end());

  return graph;
}

void ControlFlowGraph::EmitShaderCode(AST::ShaderCodeWriterBase &writer) const {
  for (Block *root : roots) {
    u32 entryAddr = root->GetAddress();

    std::vector<const Block*> usedBlocks;
    {
      std::set<const Block*> visited;
      ExtractBlocks(root, usedBlocks, visited);
    }

    bool hasJumps = false, hasCalls = false;
    for (const Block *b : usedBlocks) {
      if (b->GetType() == eBlockType::JUMP)
        hasJumps = true;
      if (b->GetType() == eBlockType::CALL)
        hasCalls = true;
    }

    bool isCalled = !root->GetSources().empty();
    writer.BeginControlFlow(entryAddr, hasJumps, hasCalls, isCalled);

    std::vector<const Block*> addrBlocks;
    for (const Block *b : usedBlocks) {
      if (b->GetType() == eBlockType::EXEC && (!b->GetSources().empty() || b == root))
        addrBlocks.push_back(b);
    }

    std::sort(addrBlocks.begin(), addrBlocks.end(), [](auto* a, auto* b) {
      return a->GetAddress() < b->GetAddress();
    });

    for (u64 i = 0; i < addrBlocks.size(); ++i) {
      const Block *base = addrBlocks[i];
      const Block *next = (i + 1 < addrBlocks.size()) ? addrBlocks[i + 1] : nullptr;

      if (!base)
        continue;

      if (base)
        writer.BeginBlockWithAddress(base->GetAddress());

      const Block *curr = base;
      while (curr) {
        if (next && curr->GetAddress() == next->GetAddress()) break;
        curr->EmitShaderCode(writer);
        curr = curr->GetContinuation();
      }

      writer.EndBlockWithAddress();
    }

    writer.EndControlFlow();
  }
}

void ControlFlowGraph::ExtractBlocks(const Block *block, std::vector<const Block*>& out, std::set<const Block*> &visited) {
  if (!block || visited.contains(block))
    return;

  visited.insert(block);
  out.push_back(block);

  if (block->GetType() == eBlockType::JUMP)
    ExtractBlocks(block->GetTarget(), out, visited);
  if (block->GetContinuation())
    ExtractBlocks(block->GetContinuation(), out, visited);
}


} // namespace Xe::Microcode