/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/JIT/IR/Backend/X86/X86Backend.h"


namespace Xe::XCPU::JIT::IR {
  
  using namespace asmjit;




  void comment_emit(X86Backend* backEnd, IRInstruction* instr, X86EmitterContext*) {
    // do nothing
    return;
  }

  void load_gpr_emit(X86Backend* backEnd, IRInstruction* instr, X86EmitterContext* context) {
    
    // dst value should be appended to Context virtual register map
    x86::Gp dst = context->MapToGp(backEnd, instr);

    // Operand 0 is IRRegister
    IRRegister* reg = (IRRegister*)instr->GetOperand(0);
    COMP.mov(dst, GPRPtr(reg->GetRegisterIndex()));
  }

  void store_gpr_emit(X86Backend* backEnd, IRInstruction* instr, X86EmitterContext* context) {
    // Operand 0 is an IRRegister
    IRRegister* dstReg = (IRRegister*)instr->GetOperand(0);
    x86::Gp src = context->MapToGp(backEnd, instr->GetOperand(1));
    COMP.mov(GPRPtr(dstReg->GetRegisterIndex()), src);
  }


  void add_emit(X86Backend* backEnd, IRInstruction* instr, X86EmitterContext* context) {

    x86::Gp dst = context->MapToGp(backEnd, instr);
    x86::Gp lhs = context->MapToGp(backEnd, instr->GetOperand(0)); // LHS = op 0
    x86::Gp rhs = context->MapToGp(backEnd, instr->GetOperand(1)); // LHS = op 1

    x86::Gp temp1 = context->MakeGpOfType(backEnd, instr->GetOperand(0)->GetType());
    COMP.mov(temp1, lhs);
    COMP.add(temp1, rhs);
    COMP.mov(dst, temp1);
  }


  // TODO: maybe it's better to use a std::map instead

  X86CodeEmitter dispatchCodeEmitter(IROp instructionOpcode) {
    switch (instructionOpcode) {
    case IROp::Comment: return comment_emit;
    case IROp::LoadGPR: return load_gpr_emit;
    case IROp::StoreGPR: return store_gpr_emit;
    case IROp::Add: return add_emit;

    default:
      LOG_ERROR(JIT, "Unable to dispatch CodeEmitter for: '{}'", IROpToString(instructionOpcode));
      return nullptr;
    }
  }

}
