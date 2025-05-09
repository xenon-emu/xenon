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

private:

};

// Statement node base
class StatementNode : public NodeBase {
public:

private:

};

} // namespace Xe::Microcode