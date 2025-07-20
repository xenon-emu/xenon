// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"

#include "ASTStatement.h"

namespace Xe::Microcode::AST {

void ListStatement::Visit(Visitor &visitor) const {
  statementA->Visit(visitor);
  statementB->Visit(visitor);
}

void ListStatement::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  statementA->EmitShaderCode(writer, shader);
  statementB->EmitShaderCode(writer, shader);
}

void ConditionalStatement::Visit(Visitor &visitor) const {
  visitor.OnConditionPush(condition);
  statement->Visit(visitor);
  visitor.OnConditionPop();
}

void ConditionalStatement::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  if (condition) {
    Chunk cond = writer.AllocLocalBool(condition->EmitShaderCode(writer, shader));
    writer.BeingCondition(cond);
  }

  statement->EmitShaderCode(writer, shader);

  if (condition) {
    writer.EndCondition();
  }
}

void SetPredicateStatement::Visit(Visitor &visitor) const
{}

void SetPredicateStatement::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  Chunk value = expression->EmitShaderCode(writer, shader);
  LOG_DEBUG(Xenos, "[AST::Sirit] SetPredicateStatement::EmitShaderCode({})", value.id.value);
  writer.SetPredicate(value);
}

void WriteWithMaskStatement::Visit(Visitor &visitor) const {
  visitor.OnWrite(target, source, mask);
}

void WriteWithMaskStatement::EmitShaderCode(ShaderCodeWriterBase &writer, const Shader *shader) {
  // evaluate destination register
  Chunk destChunk = target->EmitShaderCode(writer, shader);

  // Check if we have any values copied from source
  std::array<eSwizzle, 4> src = {}, dest = {};
  u8 numSourceSwizzles = 0, numSpecialChannels = 0;
  for (u8 i = 0; i < mask.size(); ++i) {
    switch (mask[i]) {
    case eSwizzle::X:
    case eSwizzle::Y:
    case eSwizzle::Z:
    case eSwizzle::W: {
      src[numSourceSwizzles] = mask[i];
      dest[numSourceSwizzles] = static_cast<eSwizzle>(i);
      numSourceSwizzles += 1;
    } break;
    case eSwizzle::Zero:
    case eSwizzle::One: {
      numSpecialChannels += 1;
    } break;
    case eSwizzle::Unused: {
      numSpecialChannels += 1;
    } break;
    default: {
      LOG_ERROR(Render, "[AST::Statement] Unknown swizzle type '{}'!", static_cast<u32>(mask[i]));
    } break;
    }
  }

  // Nothing to output
  if (!numSpecialChannels && !numSourceSwizzles) {
    // We still need to evaluate the source
    Chunk src = source->EmitShaderCode(writer, shader);
    writer.Emit(src);
    return;
  }
  
  if (numSourceSwizzles > 0) {
    std::span<const eSwizzle> dstSpan(dest.data(), numSourceSwizzles);
    std::span<const eSwizzle> srcSpan(src.data(), numSourceSwizzles);
    Chunk src = source->EmitShaderCode(writer, shader);
    Chunk dst = target->EmitShaderCode(writer, shader);
    LOG_DEBUG(Xenos, "[AST::WriteWithMaskStatement::Masked]: {} -> {}", source->GetName(), target->GetName());
    writer.AssignMasked(src, dst, dstSpan, srcSpan);
  }

  if (numSpecialChannels > 0) {
    std::array<eSwizzle, 4> immediateSwizzle = {};
    std::array<eSwizzle, 4> destImmediateSwizzle = {};
    u8 index = 0;

    for (u8 i = 0; i < mask.size(); ++i) {
      if (mask[i] == eSwizzle::Zero || mask[i] == eSwizzle::One) {
        destImmediateSwizzle[index] = static_cast<eSwizzle>(i);
        immediateSwizzle[index] = mask[i];
        ++index;
      }
    }

    Chunk dst = target->EmitShaderCode(writer, shader);
    LOG_DEBUG(Xenos, "[WriteWithMaskStatement::Immediate]: {}", target->GetName());
    writer.AssignImmediate(dst,
                           std::span(destImmediateSwizzle.data(), index),
                           std::span(immediateSwizzle.data(), index));
  }
}

} // namespace Xe::Microcode::AST