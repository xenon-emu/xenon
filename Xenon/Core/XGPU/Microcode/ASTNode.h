// Copyright 2025 Xenon Emulator Project

#pragma once

#include "ASTNodeBase.h"

namespace Xe::Microcode::AST {
  
// General expression type
enum class eExprType : u8 {
  ALU,
  VFETCH,
  TFETCH,
  EXPORT
};

// Expression node base
class ExpressionNode : public NodeBase {
public:
  virtual eExprType GetType() const { return eExprType::ALU; }
private:

};

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
  std::atomic<int> references = 0;
};

} // namespace Xe::Microcode
