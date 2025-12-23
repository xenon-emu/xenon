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

//=============================================================================
// IR Printer
// ----------------------------------------------------------------------------
// Utility for printing IR in human-readable format for debugging
//=============================================================================

namespace Xe::XCPU::JIT {

  /// Prints IR function to a string
  class IRPrinter {
  public:
    /// Print an IR function
    static std::string PrintFunction(const IR::IRFunction *function) {
      if (!function) {
        return "nullptr function";
      }

      std::string output;
      output += "function " + function->GetName();
      output += " @ 0x" + ToHex(function->GetAddress());
      output += " {\n";

      for (const auto &bb : function->GetBasicBlocks()) {
        output += PrintBasicBlock(bb.get());
      }

      output += "}\n";
      return output;
    }

    /// Print a basic block
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

    /// Print an instruction
    static std::string PrintInstruction(const IR::IRInstruction *inst) {
      if (!inst) {
        return "";
      }

      std::string output = "    ";

      // Add source location if available
      u64 srcLoc = inst->GetSourceLocation();
      if (srcLoc != 0) {
        output += "[0x" + ToHex(srcLoc) + "] ";
      }

      // Print value name for non-void instructions
      if (inst->GetType() != IR::IRType::Void) {
        output += "%" + ToHex(inst->GetId()) + " = ";
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

    /// Print a value
    static std::string PrintValue(const IR::IRValue *value) {
      if (!value) {
        return "null";
      }

      switch (value->GetKind()) {
      case IR::IRValue::ValueKind::ConstantInt: {
        auto *constant = static_cast<const IR::IRConstantInt *>(value);
        return std::string(IR::IRTypeToString(constant->GetType())) + " 0x" +
          ToHex(constant->GetValue());
      }

      case IR::IRValue::ValueKind::Register: {
        auto *reg = static_cast<const IR::IRRegister *>(value);
        return std::string(IR::IRRegisterTypeToString(reg->GetRegisterType())) +
          std::to_string(reg->GetRegisterIndex());
      }

      case IR::IRValue::ValueKind::Instruction:
        return "%" + std::to_string(value->GetId());

      case IR::IRValue::ValueKind::BasicBlock: {
        auto *block = static_cast<const IR::IRBasicBlock *>(value);
        return block->GetName();
      }

      default:
        return "%" + std::to_string(value->GetId());
      }
    }

  private:
    /// Helper function to convert a value to hexadecimal string
    template<typename T>
    static std::string ToHex(T value) {
      std::ostringstream oss;
      oss << std::hex << std::uppercase << value;
      return oss.str();
    }
  };

} // namespace Xe::XCPU::JIT
