// Copyright 2025 Xenon Emulator Project

#include "Base/Assert.h"

#include "Transformer.h"

namespace Xe::Microcode {

ShaderNodeWriter::ShaderNodeWriter(eShaderType type) :
  shaderType(type)
{}

ShaderNodeWriter::~ShaderNodeWriter()
{}

void ShaderNodeWriter::TransformShader(AST::NodeWriterBase &nodeWriter, const u32 *words, const u32 numWords) {
  instr_cf_t cfa;
  instr_cf_t cfb;

  u32 pc = 0;
  for (u32 i = 0; i != numWords; i += 3) {
    cfa.dword_0 = words[i];
    cfa.dword_1 = words[i+1] & 0xFFFF;
    cfb.dword_0 = (words[i+1] >> 16) | (words[i+2] << 16);
    cfb.dword_1 = words[i+2] >> 16;

    TransformBlock(nodeWriter, words, numWords, &cfa, pc);
    TransformBlock(nodeWriter, words, numWords, &cfb, pc);

    if (cfa.opc == EXEC_END || cfb.opc == EXEC_END)
      break;
  }
}

void ShaderNodeWriter::TransformBlock(AST::NodeWriterBase &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_t *cf, u32 &pc) {
  const instr_cf_opc_t cfType = static_cast<instr_cf_opc_t>(cf->opc);
  switch (cfType) {
  case NOP: {
    nodeWriter.EmitNop();
  } break;
  case ALLOC: {
    const instr_cf_alloc_t &alloc = cf->alloc;
    switch (alloc.buffer_select) {
    case 1: {
      nodeWriter.EmitExportAllocPosition();
    } break;
    case 2: {
      u32 count = 1 + alloc.size;
      nodeWriter.EmitExportAllocParam(count);
    } break;
    case 3: {
      u32 count = 1 + alloc.size;
      nodeWriter.EmitExportAllocMemExport(count);
    } break;
    }
  } break;
  case COND_EXEC:
  case COND_EXEC_END:
  case COND_PRED_EXEC:
  case COND_PRED_EXEC_END:
  case COND_EXEC_PRED_CLEAN:
  case COND_EXEC_PRED_CLEAN_END: {
    const instr_cf_exec_t &exec = cf->exec;
    // Condition argument
    AST::Statement preamble = {};
    // Evaluate condition
    AST::Expression condition = {};
    if ((cfType == COND_EXEC_PRED_CLEAN) || (cfType == COND_EXEC_PRED_CLEAN_END)) {
      const bool pixelShader = shaderType == eShaderType::Pixel;
      condition = nodeWriter.EmitBoolConst(pixelShader, exec.bool_addr); // set new predication
      preamble = nodeWriter.EmitSetPredicateStatement(condition);
    } else {
      condition = nodeWriter.EmitGetPredicate();
    }
    // Invert condition
    if (!exec.pred_condition) {
      condition = nodeWriter.EmitNot(condition);
    }
    // Evaluate code
    const AST::Statement code = EmitExec(nodeWriter, words, numWords, exec);
    // Emit execution block with that code (no condition)
    const bool endOfShader = (cfType == COND_EXEC_END) || (cfType == COND_PRED_EXEC_END) || (cfType == COND_EXEC_PRED_CLEAN_END);
    nodeWriter.EmitExec(exec.address, cfType, preamble, code, condition, endOfShader);
    // Update PC
    pc = exec.address + exec.count;
  } break;
  // Execution block
  case EXEC:
  case EXEC_END: {
    const instr_cf_exec_t &exec = cf->exec;
    // Blank statements
    AST::Statement preamble = {};
    AST::Expression condition = {};
    // Emit execution block with that code (no condition)
    const AST::Statement code = EmitExec(nodeWriter, words, numWords, exec);
    const bool endOfShader = (cfType == COND_EXEC_END) || (cfType == COND_PRED_EXEC_END) || (cfType == EXEC_END);
    nodeWriter.EmitExec(exec.address, cfType, preamble, code, condition, endOfShader);
    // Update PC
    pc = exec.address + exec.count;
  } break;
  // Conditional flow control change
  case COND_CALL:
  case COND_JMP: {
    const instr_cf_jmp_call_t &jmp = cf->jmp_call;
    u32 targetAddr = jmp.address;
    if (jmp.address_mode != ABSOLUTE_ADDR) {
      if (jmp.direction == 0) {
        LOG_ERROR(Xenos, "[UCode] Jump had a invalid direction of '{}'", targetAddr);
      }
      targetAddr += pc;
    }
    // Evaluate condition
    AST::Statement preamble = {};
    AST::Expression condition = {};
    if (jmp.force_call) {
      // Always jump
    } else if (jmp.predicated_jmp) {
      // Use existing predication
      condition = nodeWriter.EmitGetPredicate();
    } else {
      const bool pixelShader = (shaderType == eShaderType::Pixel);
      condition = nodeWriter.EmitBoolConst(pixelShader, jmp.bool_addr);
      preamble = nodeWriter.EmitSetPredicateStatement(condition);
    }
    // Invert condition
    if (!jmp.condition)
      condition = nodeWriter.EmitNot(condition);
    // Emit jump instruction
    if (cfType == COND_CALL)
      nodeWriter.EmitCall(targetAddr, preamble, condition);
    else
      nodeWriter.EmitJump(targetAddr, preamble, condition);
  } break;
  case RETURN:
  case LOOP_START:
  case LOOP_END:
  case MARK_VS_FETCH_DONE:
    LOG_ERROR(Xenos, "[UCode] Failed to translate block! Unsupported control flow '{}'", cf->opc);
    break;
  }
}

AST::Statement ShaderNodeWriter::EmitExec(AST::NodeWriterBase &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_exec_t &exec) {
  // Check if execution has a condition
  const bool conditional = exec.is_cond_exec();
  // Sequence bytes
  const u32 sequence = exec.serialize;
  // Reset VertexStride for VFetch groups
  lastVertexStride = 0;
  // Return code (statement list)
  AST::Statement statement = {};
  // Process instructions
  for (u32 i = 0; i != exec.count; ++i) {
    // Get actual ALU data for the instruction
    const u32 offset = exec.address + i;
    // Decode instruction type 
    const u32 seqCode = sequence >> (i * 2);
    const bool iFetch = (0 != (seqCode & 0x1));
    const bool sync = (0 != (seqCode & 0x2));
    const u32 *aluPtr = words + offset * 3;
    if (iFetch) {
      const instr_fetch_t *fetch = reinterpret_cast<const instr_fetch_t*>(aluPtr);
      switch (fetch->opc) {
      case VTX_FETCH: {
        // Evaluate vertex fetch
        instr_fetch_vtx_t vtx = fetch->vtx;
        AST::Statement code = EmitVertexFetch(nodeWriter, vtx, sync);
        code = EmitPredicateTest(nodeWriter, code, conditional, exec.pred_condition, vtx.pred_select, vtx.pred_condition);
        statement = nodeWriter.EmitMergeStatements(statement, code);
      } break;
      case TEX_FETCH: {
        // Evaluate texture fetch
        instr_fetch_tex_t tex = fetch->tex;
        AST::Statement code = EmitTextureFetch(nodeWriter, tex, sync);
        code = EmitPredicateTest(nodeWriter, code, conditional, exec.pred_condition, tex.pred_select, tex.pred_condition);
        statement = nodeWriter.EmitMergeStatements(statement, code);
      } break;
      case TEX_GET_BORDER_COLOR_FRAC:
      case TEX_GET_COMP_TEX_LOD:
      case TEX_GET_GRADIENTS:
      case TEX_GET_WEIGHTS:
      case TEX_SET_TEX_LOD:
      case TEX_SET_GRADIENTS_H:
      case TEX_SET_GRADIENTS_V:
      default:
        LOG_ERROR(Xenos, "[UCode] Failed to translate block! Unsupported fetch type '{}'", fetch->opc);
        break;
      }
    } else {
      const instr_alu_t *alu = reinterpret_cast<const instr_alu_t*>(aluPtr);
      AST::Statement code = EmitALU(nodeWriter, *alu, sync);
      code = EmitPredicateTest(nodeWriter, code, conditional, exec.pred_condition, alu->pred_select, alu->pred_condition);
      statement = nodeWriter.EmitMergeStatements(statement, code);
    }
  }

  return statement;
}

AST::Statement ShaderNodeWriter::EmitALU(AST::NodeWriterBase &nodeWriter, const instr_alu_t &alu, const bool sync) {
  return {};
}

AST::Statement ShaderNodeWriter::EmitVertexFetch(AST::NodeWriterBase &nodeWriter, const instr_fetch_vtx_t &vtx, const bool sync) {
  return {};
}

AST::Statement ShaderNodeWriter::EmitTextureFetch(AST::NodeWriterBase &nodeWriter, const instr_fetch_tex_t &tex, const bool sync) {
  return {};
}

AST::Statement ShaderNodeWriter::EmitPredicateTest(AST::NodeWriterBase &nodeWriter, const AST::Statement &code, const bool conditional, const u32 flowPredCondition, const u32 predSelect, const u32 predCondition) {
  if (predSelect && (!conditional || flowPredCondition != predCondition)) {
    // Get predicate register (invert if needed) and wrap in a condition
    return nodeWriter.EmitConditionalStatement(predCondition ? nodeWriter.EmitGetPredicate() : nodeWriter.EmitNot(nodeWriter.EmitGetPredicate()), code);
  }
  
  return code;
}

AST::Expression ShaderNodeWriter::EmitSrcReg(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const u32 num, const u32 type, const u32 swiz, const u32 negate, const s32 slot) {
  return {};
}

AST::Expression ShaderNodeWriter::EmitSrcReg(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const u32 argIndex) {
  return {};
}

AST::Expression ShaderNodeWriter::EmitSrcScalarReg1(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr) {
  return {};
}

AST::Expression ShaderNodeWriter::EmitSrcScalarReg2(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr) {
  return {};
}

AST::Statement ShaderNodeWriter::EmitVectorResult(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const AST::Expression &code) {
  return {};
}

AST::Statement ShaderNodeWriter::EmitScalarResult(AST::NodeWriterBase &nodeWriter, const instr_alu_t &instr, const AST::Expression &code) {
  return {};
}

const u32 ShaderNodeWriter::GetArgCount(instr_vector_opc_t instr) {
  switch (instr) {
  case ADDv: return 2;
  case MULv: return 2;
  case MAXv: return 2;
  case MINv: return 2;
  case SETEv: return 2;
  case SETGTv: return 2;
  case SETGTEv: return 2;
  case SETNEv: return 2;
  case FRACv: return 1;
  case TRUNCv: return 1;
  case FLOORv: return 1;
  case MULADDv: return 3;
  case CNDEv: return 3;
  case CNDGTEv: return 3;
  case CNDGTv: return 3;
  case DOT4v: return 2;
  case DOT3v: return 2;
  case DOT2ADDv: return 3;
  case CUBEv: return 2;
  case MAX4v: return 1;
  case PRED_SETE_PUSHv: return 2;
  case PRED_SETNE_PUSHv: return 2;
  case PRED_SETGT_PUSHv: return 2;
  case PRED_SETGTE_PUSHv: return 2;
  case KILLEv: return 2;
  case KILLGTv: return 2;
  case KILLGTEv: return 2;
  case KILLNEv: return 2;
  case DSTv: return 2;
  case MOVAv: return 1;
  }

  THROW_MSG(true, "Unknown vector instruction!");
  return 0;
}

const u32 ShaderNodeWriter::GetArgCount(instr_scalar_opc_t instr) {
  switch (instr) {
  case ADDs: return 1;
  case ADD_PREVs: return 1;
  case MULs: return 1;
  case MUL_PREVs: return 1;
  case MUL_PREV2s: return 1;
  case MAXs: return 1;
  case MINs: return 1;
  case SETEs: return 1;
  case SETGTs: return 1;
  case SETGTEs: return 1;
  case SETNEs: return 1;
  case FRACs: return 1;
  case TRUNCs: return 1;
  case FLOORs: return 1;
  case EXP_IEEE: return 1;
  case LOG_CLAMP: return 1;
  case LOG_IEEE: return 1;
  case RECIP_CLAMP: return 1;
  case RECIP_FF: return 1;
  case RECIP_IEEE: return 1;
  case RECIPSQ_CLAMP: return 1;
  case RECIPSQ_FF: return 1;
  case RECIPSQ_IEEE: return 1;
  case MOVAs: return 1;
  case MOVA_FLOORs: return 1;
  case SUBs: return 1;
  case SUB_PREVs: return 1;
  case PRED_SETEs: return 1;
  case PRED_SETNEs: return 1;
  case PRED_SETGTs: return 1;
  case PRED_SETGTEs: return 1;
  case PRED_SET_INVs: return 1;
  case PRED_SET_POPs: return 1;
  case PRED_SET_CLRs: return 1;
  case PRED_SET_RESTOREs: return 1;
  case KILLEs: return 1;
  case KILLGTs: return 1;
  case KILLGTEs: return 1;
  case KILLNEs: return 1;
  case KILLONEs: return 1;
  case SQRT_IEEE: return 1;
  case MUL_CONST_0: return 2;
  case MUL_CONST_1: return 2;
  case ADD_CONST_0: return 2;
  case ADD_CONST_1: return 2;
  case SUB_CONST_0: return 2;
  case SUB_CONST_1: return 2;
  case SIN: return 1;
  case COS: return 1;
  case RETAIN_PREV: return 1;
  }

  THROW_MSG(true, "Unknown scalar instruction!");
  return 0;
}

} // namespace Xe::Microcode
