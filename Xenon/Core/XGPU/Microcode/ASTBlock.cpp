// Copyright 2025 Xenon Emulator Project. All rights reserved.

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

Block::Block(ExpressionNode::Ptr cond, StatementNode::Ptr preamble, u32 target, eBlockType type) :
  targetAddress(target), type(type) {
  condition = cond ? cond->CloneAs<ExpressionNode>() : nullptr;
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
  case eBlockType::LOOP_BEGIN: {
    writer.LoopBegin(target->GetAddress());
  } break;
  case eBlockType::LOOP_END: {
    writer.LoopEnd(target->GetAddress());
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

  std::stack<Block*> loopStartStack = {};
  for (u32 i = 0; i < numBlocks; ++i) {
    Block* block = graph->blocks[i];
    if (block->GetType() == eBlockType::LOOP_BEGIN) {
      loopStartStack.push(block);
    } else if (block->GetType() == eBlockType::LOOP_END) {
      if (loopStartStack.empty()) {
        LOG_ERROR(Render, "[AST::CFG] Unmatched LOOP_END at block {}", i);
        return nullptr;
      }
      Block* start = loopStartStack.top();
      loopStartStack.pop();
      block->ConnectTarget(start); // Back-edge
      start->ConnectTarget(block); // Optional: fallthrough or structural
    }
  }

  if (!loopStartStack.empty()) {
    LOG_ERROR(Render, "[AST::CFG] Unmatched LOOP_START ({} blocks unclosed)", loopStartStack.size());
    return nullptr;
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
  writer.BeginMain();
  writer.EndMain();
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

class AllExpressionVisitor : public StatementNode::Visitor {
public:
  AllExpressionVisitor(ExpressionNode::Visitor *exprVisitor)
    : exprVisitor(exprVisitor)
  {}

  virtual void OnWrite(ExpressionNode::Ptr dest, ExpressionNode::Ptr src, std::array<eSwizzle, 4> mask) override final {
    dest->Visit(*exprVisitor);
    src->Visit(*exprVisitor);
  }

  virtual void OnConditionPush(Microcode::AST::ExpressionNode::Ptr condition) override final {
    condition->Visit(*exprVisitor);
  }

  virtual void OnConditionPop() override final {}

private:
  ExpressionNode::Visitor *exprVisitor = nullptr;
};

class GlobalInstructionExtractor : public ExpressionNode::Visitor {
public:
  virtual void OnExprStart(const ExpressionNode::Ptr n) override final {
    if (n->GetType() == eExprType::VFETCH) {
      vfetch.push_back(reinterpret_cast<const VertexFetch*>(n.get()));
    }
    else if (n->GetType() == eExprType::TFETCH) {
      tfetch.push_back(reinterpret_cast<const TextureFetch *>(n.get()));
    }
    else if (n->GetType() == eExprType::EXPORT) {
      exports.push_back(reinterpret_cast<const WriteExportRegister *>(n.get()));
    }
    else {
      const s32 regIndex = n->GetRegisterIndex();
      if (regIndex != -1) {
        usedRegisters.insert(static_cast<u32>(regIndex));
      }
    }
  }

  virtual void OnExprEnd(const ExpressionNode::Ptr n) override final
  {}

  std::vector<const VertexFetch*> vfetch{};
  std::vector<const TextureFetch*> tfetch{};
  std::vector<const WriteExportRegister*> exports{};
  std::set<u32> usedRegisters{};
};

void VisitAll(const Block *b, StatementNode::Visitor &v) {
  if (b->GetCondition())
    v.OnConditionPush(b->GetCondition());

  if (b->GetCode())
    b->GetCode()->Visit(v);

  if (b->GetCondition())
    v.OnConditionPop();
}

void VisitAll(const ControlFlowGraph *cf, StatementNode::Visitor &v) {
  for (u32 i = 0; i != cf->GetNumBlocks(); ++i) {
    const Block *b = cf->GetBlock(i);
    VisitAll(b, v);
  }
}

Shader::~Shader() {
  if (controlFlow) {
    controlFlow->~ControlFlowGraph();
  }
}

Shader* Shader::DecompileMicroCode(const void *code, const u32 codeLength, eShaderType shaderType) {
  ControlFlowGraph *cf = ControlFlowGraph::DecompileMicroCode(code, codeLength, shaderType);
  if (!cf)
    return nullptr;
  Shader *shader = new Shader();
  shader->controlFlow = cf;

  GlobalInstructionExtractor instructionExtractor;
  AllExpressionVisitor vistor{ &instructionExtractor };
  VisitAll(cf, vistor);

  shader->vertexFetches = std::move(instructionExtractor.vfetch);
  shader->exports = std::move(instructionExtractor.exports);

  shader->textureFetchSlotMask = 0;
  for (auto &tfetch : instructionExtractor.tfetch) {
    // find in existing list
    bool found = false;
    for (const auto &usedTexture : shader->usedTextures) {
      if (usedTexture.slot == tfetch->fetchSlot) {
        found = true;
        break;
      }
    }

    // New texture
    if (!found) {
      TextureRef ref = {};
      ref.slot = tfetch->fetchSlot;
      ref.type = tfetch->textureType;
      shader->usedTextures.push_back(ref);
      // Merge the fetch slot mask
      shader->textureFetchSlotMask |= (1 << ref.slot);
    }
  }
  // Extract used registers
  shader->usedRegisters.reserve(instructionExtractor.usedRegisters.size());
  for (const u32 reg : instructionExtractor.usedRegisters)
    shader->usedRegisters.push_back(reg);
  std::sort(shader->usedRegisters.begin(), shader->usedRegisters.end());
  // Scan exports
  shader->numUsedInterpolators = 0;
  for (const auto &exp : shader->exports) {
    const s32 index = exp->GetExportInterpolatorIndex(exp->GetExportReg());
    if (index >= 0) {
      shader->numUsedInterpolators = std::max(static_cast<u32>(index + 1), shader->numUsedInterpolators);
    }
  }
  return shader;
}

void Shader::EmitShaderCode(AST::ShaderCodeWriterBase &writer) const {
  controlFlow->EmitShaderCode(writer);
}


} // namespace Xe::Microcode