// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <array>
#include <span>

#include "Core/XGPU/ShaderConstants.h"

#include "ASTNodeBase.h"
#include "ASTEmitter.h"
#include "ASTNode.h"

namespace Xe::Microcode::AST {

// Statement type
enum class eStatementType : u8 {
  List,
  Conditional,
  Write
};

// Statement node base
class StatementNode : public NodeBase, public std::enable_shared_from_this<StatementNode> {
public:
  class Visitor {
  public:
    virtual ~Visitor() = default;
    virtual void OnWrite(ExpressionNode::Ptr dest, ExpressionNode::Ptr src, std::array<eSwizzle, 4> mask) {}
    virtual void OnConditionPush(ExpressionNode::Ptr condition) {}
    virtual void OnConditionPop() {}
  };
  StatementNode()
  {}
  virtual ~StatementNode() = default;
  using Ptr = std::shared_ptr<StatementNode>;
  virtual eStatementType GetType() const = 0;
  virtual void Visit(Visitor &vistor) const = 0;
  virtual void EmitShaderCode(ShaderCodeWriterBase &writer) = 0;
  virtual std::shared_ptr<StatementNode> CloneStatement() const = 0;
  std::shared_ptr<NodeBase> Clone() const override {
    return CloneStatement();
  }
};

class ListStatement : public StatementNode {
public:
  ListStatement(StatementNode::Ptr a, StatementNode::Ptr b) {
    statementA = std::move(a->CloneAs<StatementNode>());
    statementB = std::move(b->CloneAs<StatementNode>());
  }
  eStatementType GetType() const override final { return eStatementType::List; }
  void Visit(Visitor &vistor) const override;
  void EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<StatementNode> CloneStatement() const override {
    return std::make_unique<ListStatement>(statementA, statementB);
  }
protected:
  StatementNode::Ptr statementA = nullptr;
  StatementNode::Ptr statementB = nullptr;
};

class ConditionalStatement : public StatementNode {
public:
  ConditionalStatement(StatementNode::Ptr _statement, ExpressionNode::Ptr cond) {
    statement = std::move(_statement->CloneAs<StatementNode>());
    condition = std::move(cond->CloneAs<ExpressionNode>());
  }
  eStatementType GetType() const override final { return eStatementType::Conditional; }
  void Visit(Visitor &vistor) const override;
  void EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<StatementNode> CloneStatement() const override {
    return std::make_unique<ConditionalStatement>(statement, condition);
  }
protected:
  StatementNode::Ptr statement = nullptr;
  ExpressionNode::Ptr condition = nullptr;
};

class SetPredicateStatement : public StatementNode {
public:
  SetPredicateStatement(ExpressionNode::Ptr expr) {
    expression = std::move(expr->CloneAs<ExpressionNode>());
  }
  eStatementType GetType() const override final { return eStatementType::Write; }
  void Visit(Visitor &vistor) const override;
  void EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<StatementNode> CloneStatement() const override {
    return std::make_unique<SetPredicateStatement>(expression);
  }
protected:
  ExpressionNode::Ptr expression = nullptr;
};

class WriteWithMaskStatement : public StatementNode {
public:
  WriteWithMaskStatement(ExpressionNode::Ptr t, ExpressionNode::Ptr s, eSwizzle x, eSwizzle y, eSwizzle z, eSwizzle w) {
    target = std::move(t->CloneAs<ExpressionNode>());
    source = std::move(s->CloneAs<ExpressionNode>());
    mask[0] = x;
    mask[1] = y;
    mask[2] = z;
    mask[3] = w;
  }
  eStatementType GetType() const override final { return eStatementType::Write; }
  void Visit(Visitor &vistor) const override;
  void EmitShaderCode(ShaderCodeWriterBase &writer) override;
  std::shared_ptr<StatementNode> CloneStatement() const override {
    return std::make_unique<WriteWithMaskStatement>(target, source, mask[0],  mask[1],  mask[2],  mask[3]);
  }
protected:
  ExpressionNode::Ptr target = nullptr;
  ExpressionNode::Ptr source = nullptr;
  std::array<eSwizzle, 4> mask = {};
};

} // namespace Xe::Microcode
