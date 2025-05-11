// Copyright 2025 Xenon Emulator Project

#include "ASTBlock.h"
#include "ASTNodeWriter.h"

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

} // namespace Xe::Microcode