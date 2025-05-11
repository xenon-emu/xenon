// Copyright 2025 Xenon Emulator Project

#pragma once

#include <array>

#include "ASTNodeBase.h"
#include "ASTEmitter.h"
#include "ASTNode.h"

namespace Xe::Microcode::AST {

// Statement type
enum class eStatementType : u8 {
  LIST,
  CONDITIONAL,
  WRITE
};

// Statement node base
class StatementNode : public NodeBase {
public:
  StatementNode();
  virtual eStatementType GetType() const = 0;
protected:
  virtual ~StatementNode() = default;
  std::atomic<int> references{ 0 };
};

} // namespace Xe::Microcode
