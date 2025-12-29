/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "PPCTranslator.h"
#include "Core/XCPU/JIT/IR/IRTypes.h"
#include "Core/XCPU/JIT/IR/IRBuilder.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <format>

//=============================================================================
// IR Printer
// ----------------------------------------------------------------------------
// Utility for printing IR in human-readable format for debugging
//=============================================================================

namespace Xe::XCPU::JIT {

  // Prints IR function to a string
  class IRPrinter {
  public:
    // Print an IR function
    static std::string PrintFunction(const IR::IRFunction *function) {
      if (!function) {
        return "nullptr function";
      }

      std::string output;
      output += FMT("function {} @ {:#x} {{\n", function->GetName(), function->GetAddress());

      for (const auto &bb : function->GetBasicBlocks()) {
        output += PrintBasicBlock(bb.get());
      }

      output += "}\n";
      return output;
    }

    // Print a basic block
    static std::string PrintBasicBlock(const IR::IRBasicBlock *block) {
      if (!block) {
        return "";
      }

      std::string output;
      output += "  " + block->GetName() + ":\n";

      // Print predecessors
      const auto &preds = block->GetPredecessors();
      if (!preds.empty()) {
        output += "    ; predecessors: ";
        for (size_t i = 0; i < preds.size(); ++i) {
          if (i > 0) output += ", ";
          output += preds[i]->GetName();
        }
        output += "\n";
      }

      // Print instructions
      for (const auto &inst : block->GetInstructions()) {
        output += PrintInstruction(inst.get());
      }

      // Print terminator
      if (block->GetTerminator()) {
        output += PrintInstruction(block->GetTerminator());
      }

      // Print successors
      const auto &succs = block->GetSuccessors();
      if (!succs.empty()) {
        output += "    ; successors: ";
        for (size_t i = 0; i < succs.size(); ++i) {
          if (i > 0) output += ", ";
          output += succs[i]->GetName();
        }
        output += "\n";
      }

      output += "\n";
      return output;
    }

    // Print an instruction
    static std::string PrintInstruction(const IR::IRInstruction *inst) {
      if (!inst) {
        return "";
      }

      std::string output = "    ";

      // Add source location if available
      u64 srcLoc = inst->GetSourceLocation();
      if (srcLoc != 0) {
        output += FMT("[{:#x}] ", srcLoc);
      }

      // Print value name for non-void instructions
      if (inst->GetType() != IR::IRType::Void) {
        output += FMT("%{} = ", inst->GetId());
      }

      // Print opcode
      output += IR::IROpToString(inst->GetOpcode());

      // Print operands
      for (size_t i = 0; i < inst->GetNumOperands(); ++i) {
        output += " ";
        auto *operand = inst->GetOperand(i);
        if (operand) {
          output += PrintValue(operand);
        }
        else {
          output += "null";
        }
      }

      // Print metadata
      std::string pred = inst->GetMetadata("predicate");
      if (!pred.empty()) {
        output += " [pred=" + pred + "]";
      }

      std::string text = inst->GetMetadata("text");
      if (!text.empty()) {
        output += " ; " + text;
      }

      output += "\n";
      return output;
    }

    // Print a value
    static std::string PrintValue(const IR::IRValue *value) {
      if (!value) {
        return "null";
      }

      switch (value->GetKind()) {
      case IR::IRValue::ValueKind::Constant: {
        auto *constant = static_cast<const IR::IRValue *>(value);
        return FMT("{} {:#x}", IR::IRTypeToString(constant->GetType()), constant->GetValue().u32);
      }

      case IR::IRValue::ValueKind::Register: {
        auto *reg = static_cast<const IR::IRRegister *>(value);
        return FMT("{}[{}]", IR::IRRegisterTypeToString(reg->GetRegisterType()), reg->GetRegisterIndex());
      }

      case IR::IRValue::ValueKind::Instruction:
        return FMT("%{}", value->GetId());

      case IR::IRValue::ValueKind::BasicBlock: {
        auto *block = static_cast<const IR::IRBasicBlock *>(value);
        return block->GetName();
      }

      default:
        return FMT("%{}", value->GetId());
      }
    }
  };

} // namespace Xe::XCPU::JIT
