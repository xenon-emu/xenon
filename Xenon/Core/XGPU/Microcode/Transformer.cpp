// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Assert.h"
#include "Base/Logging/Log.h"

#include "Transformer.h"

namespace Xe::Microcode {

ShaderNodeWriter::ShaderNodeWriter(eShaderType type) :
  shaderType(type)
{}

ShaderNodeWriter::~ShaderNodeWriter()
{}

void ShaderNodeWriter::TransformShader(AST::NodeWriter &nodeWriter, const u32 *words, const u32 numWords) {
  u32 pc = 0;
  for (u32 i = 0; i != numWords; i += 3) {
    instr_cf_t cfa;
    instr_cf_t cfb;

    cfa.dword_0 = words[i];
    cfb.dword_0 = (words[i+1] >> 16) | (words[i+2] << 16);
    cfa.dword_1 = words[i+1] & 0xFFFF;
    cfb.dword_1 = words[i+2] >> 16;

    /*LOG_DEBUG(Xenos, "[ShaderNodeWriter::TransformShader] Opcode: {}, 0x{:X}",
      GetCFOpcodeName(static_cast<instr_cf_opc_t>(cfa.opc)), static_cast<u32>(cfa.opc));
    LOG_DEBUG(Xenos, "[ShaderNodeWriter::TransformShader] Opcode: {}, 0x{:X}",
      GetCFOpcodeName(static_cast<instr_cf_opc_t>(cfb.opc)), static_cast<u32>(cfb.opc));*/

    TransformBlock(nodeWriter, words, numWords, cfa, pc);
    TransformBlock(nodeWriter, words, numWords, cfb, pc);

    if (cfa.opc == EXEC_END || cfb.opc == EXEC_END)
      break;
  }
}

void ShaderNodeWriter::TransformBlock(AST::NodeWriter &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_t &cf, u32 &pc) {
  const instr_cf_opc_t cfType = static_cast<instr_cf_opc_t>(cf.opc);
  switch (cfType) {
  case NOP: {
    nodeWriter.EmitNop();
  } break;
  case ALLOC: {
    const instr_cf_alloc_t &alloc = cf.alloc;
    switch (static_cast<instr_alloc_type_t>(alloc.buffer_select)) {
    case SQ_NO_ALLOC: {
      LOG_ERROR(Xenos, "[AST::TransformBlock] ALLOC with NO_ALLOC, what.");
    } break;
    case SQ_POSITION: {
      nodeWriter.EmitExportAllocPosition();
    } break;
    case SQ_PARAMETER_PIXEL: {
      u32 count = 1 + alloc.size;
      nodeWriter.EmitExportAllocParam(count);
    } break;
    case SQ_MEMORY: {
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
    const instr_cf_exec_t &exec = cf.exec;
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
    const instr_cf_exec_t &exec = cf.exec;
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
    const instr_cf_jmp_call_t &jmp = cf.jmp_call;
    u32 targetAddr = jmp.address;
    if (jmp.address_mode != ABSOLUTE_ADDR) {
      // Relative addressing: direction = 0 (backward), 1 (forward)
      if (jmp.direction == 0) {
        targetAddr -= pc;
      } else {
        targetAddr += pc;
      }
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
  case LOOP_START: {
    const instr_cf_loop_t &loop = cf.loop;
    // Compute loop target address
    u32 targetAddr = loop.address;
    if (loop.address_mode != ABSOLUTE_ADDR)
      targetAddr += pc;

    // Optional predicate check
    AST::Expression condition = {};
    AST::Statement preamble = {};
    if (loop.pred_break) {
      // Conditional loop start
      const bool pixelShader = (shaderType == eShaderType::Pixel);
      condition = nodeWriter.EmitBoolConst(pixelShader, loop.condition);
      preamble = nodeWriter.EmitSetPredicateStatement(condition);
    }
    nodeWriter.EmitLoopStart(targetAddr, preamble, condition);
  } break;
  case LOOP_END: {
    const instr_cf_loop_t &loop = cf.loop;
    AST::Expression condition = {};
    if (loop.pred_break) {
      condition = nodeWriter.EmitGetPredicate();
      if (!loop.condition)
        condition = nodeWriter.EmitNot(condition);
    }
    nodeWriter.EmitLoopEnd(loop.address, condition);
  } break;
  case RETURN:
  case MARK_VS_FETCH_DONE:
    LOG_ERROR(Xenos, "[UCode] Failed to translate block! Unsupported control flow '{}'", cf.opc);
    break;
  }
}

AST::Statement ShaderNodeWriter::EmitExec(AST::NodeWriter &nodeWriter, const u32 *words, const u32 numWords, const instr_cf_exec_t &exec) {
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

AST::Statement ShaderNodeWriter::EmitALU(AST::NodeWriter &nodeWriter, const instr_alu_t &alu, const bool sync) {
  AST::Statement vector, scalar, predicate;
  // Fast case, no magic stuff around
  if (alu.vector_write_mask || (alu.export_data && alu.scalar_dest_rel)) {
    const instr_vector_opc_t &vectorInstr = static_cast<const instr_vector_opc_t>(alu.vector_opc);
    const u32 argCount = GetArgCount(vectorInstr);
    // Process function depending on the argument count
    switch (argCount) {
    case 1: {
      AST::Expression arg1 = EmitSrcReg(nodeWriter, alu, 0);
      AST::Expression func = nodeWriter.EmitVectorInstruction1(vectorInstr, arg1);
      vector = EmitVectorResult(nodeWriter, alu, func);
    } break;
    case 2: {
      AST::Expression arg1 = EmitSrcReg(nodeWriter, alu, 0);
      AST::Expression arg2 = EmitSrcReg(nodeWriter, alu, 1);
      AST::Expression func = nodeWriter.EmitVectorInstruction2(vectorInstr, arg1, arg2);
      vector = EmitVectorResult(nodeWriter, alu, func);
    } break;
    case 3: {
      AST::Expression arg1 = EmitSrcReg(nodeWriter, alu, 0);
      AST::Expression arg2 = EmitSrcReg(nodeWriter, alu, 1);
      AST::Expression arg3 = EmitSrcReg(nodeWriter, alu, 2);
      AST::Expression func = nodeWriter.EmitVectorInstruction3(vectorInstr, arg1, arg2, arg3);
      vector = EmitVectorResult(nodeWriter, alu, func);
    } break;
    default: {
      LOG_ERROR(Xenos, "[UCode::ALU] Failed to emit Vector code! Unsupported argument count '{}'", argCount);
    } break;
    }
  }
  // Additional scalar instruction
  if (alu.scalar_write_mask || !alu.vector_write_mask) {
    const instr_scalar_opc_t& scalarInstr = static_cast<const instr_scalar_opc_t>(alu.scalar_opc);
    const u32 argCount = GetArgCount(scalarInstr);
    // Process function depending on the argument count
    switch (argCount) {
  case 0: {
    AST::Expression func = nodeWriter.EmitScalarInstruction0(scalarInstr);
    scalar = EmitScalarResult(nodeWriter, alu, func);
  } break;
    case 1: {
      AST::Expression arg1 = EmitSrcReg(nodeWriter, alu, 2);
      AST::Expression func = nodeWriter.EmitScalarInstruction1(scalarInstr, arg1);
      if (scalarInstr == PRED_SETNEs ||
          scalarInstr == PRED_SETEs ||
          scalarInstr == PRED_SETGTEs ||
          scalarInstr == PRED_SETGTs) {
        predicate = nodeWriter.EmitSetPredicateStatement(func);
      }
      scalar = EmitScalarResult(nodeWriter, alu, func);
    } break;
    case 2: {
      AST::Expression arg1, arg2;
      if (scalarInstr == MUL_CONST_0 ||
          scalarInstr == MUL_CONST_1 ||
          scalarInstr == ADD_CONST_0 ||
          scalarInstr == ADD_CONST_1 ||
          scalarInstr == SUB_CONST_0 ||
          scalarInstr == SUB_CONST_1) {
        const u32 src3 = alu.src3_swiz & ~0x3C;
        const u32 regB = (alu.scalar_opc & 1) | (alu.src3_swiz & 0x3C) | (alu.src3_sel << 1);
        const s32 slot = (alu.src1_sel || alu.src2_sel) ? 1 : 0;
        const eSwizzle a = static_cast<eSwizzle>(((src3 >> 6) - 1) & 0x3);
        const eSwizzle b = static_cast<eSwizzle>((src3 & 0x3));
        arg1 = nodeWriter.EmitReadSwizzle(EmitSrcReg(nodeWriter, alu, alu.src3_reg, 0, 0, alu.src3_reg_negate, 0), a, a, a, a);
        arg2 = nodeWriter.EmitReadSwizzle(EmitSrcReg(nodeWriter, alu, regB, 1, 0, 0, slot), b, b, b, b);
      } else {
        arg1 = EmitSrcReg(nodeWriter, alu, 0);
        arg2 = EmitSrcReg(nodeWriter, alu, 1);
      }
      AST::Expression func = nodeWriter.EmitScalarInstruction2(scalarInstr, arg1, arg2);
      scalar = EmitScalarResult(nodeWriter, alu, func);
    } break;
    default: {
      LOG_ERROR(Xenos, "[UCode::ALU] Failed to emit Scalar code! Unsupported argument count '{}'", argCount);
    } break;
    }
  }
  // Concat both operations
  return nodeWriter.EmitMergeStatements(vector, nodeWriter.EmitMergeStatements(predicate, scalar));
}

AST::Statement ShaderNodeWriter::EmitVertexFetch(AST::NodeWriter &nodeWriter, const instr_fetch_vtx_t &vtx, const bool sync) {
  const u32 fetchSlot = vtx.const_index * 3 + vtx.const_index_sel;
  const u32 fetchOffset = vtx.offset;
  const u32 fetchStride = vtx.stride ? vtx.stride : lastVertexStride;
  const instr_surf_fmt_t fetchFormat = (const instr_surf_fmt_t)vtx.format;
  // Update vertex stride
  if (vtx.stride)
    lastVertexStride = vtx.stride;
  // Fetch formats
  const bool isFloat = (fetchFormat == FMT_32_FLOAT ||
                        fetchFormat == FMT_32_32_FLOAT ||
                        fetchFormat == FMT_32_32_32_FLOAT ||
                        fetchFormat == FMT_32_32_32_32_FLOAT ||
                        fetchFormat == FMT_16_FLOAT ||
                        fetchFormat == FMT_16_16_FLOAT ||
                        fetchFormat == FMT_16_16_16_16_FLOAT);
  const bool isSigned = vtx.format_comp_all != 0;
  const bool isNormalized = !vtx.num_format_all;
  // Get the source register
  bool isPixel = shaderType == eShaderType::Pixel;
  AST::eRegisterType regType = isPixel ? AST::eRegisterType::PixelInput : AST::eRegisterType::VertexInput;
  AST::Expression source = nodeWriter.EmitReadReg(vtx.src_reg, regType);
  // create the value fetcher (returns single expression with fetch result)
  const AST::Expression fetch = nodeWriter.EmitVertexFetch(source, fetchSlot, fetchOffset, fetchStride, fetchFormat, isFloat, isSigned, isNormalized);
  const AST::Expression dest = nodeWriter.EmitWriteReg(isPixel, false, vtx.dst_reg, AST::eRegisterType::Constant);
  // Setup target swizzle
  eSwizzle swizzle[4] = { eSwizzle::Unused, eSwizzle::Unused, eSwizzle::Unused, eSwizzle::Unused };
  for (u32 i = 0; i < 4; i++) {
    const u32 swizzleMask = (vtx.dst_swiz >> (3 * i)) & 0x7;
    switch (swizzleMask) {
    case 1:
    case 2:
    case 3: {
      swizzle[i] = static_cast<eSwizzle>(swizzleMask);
    } break;
    case 4: {
      swizzle[i] = eSwizzle::Zero;
    } break;
    case 5: {
      swizzle[i] = eSwizzle::One;
    } break;
    case 6: {
      swizzle[i] = eSwizzle::Ignored;
    } break;
    case 7: {
      swizzle[i] = eSwizzle::Unused;
    } break;
    }
  }
  return nodeWriter.EmitWriteWithSwizzleStatement(dest, fetch, swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
}

AST::Statement ShaderNodeWriter::EmitTextureFetch(AST::NodeWriter &nodeWriter, const instr_fetch_tex_t &tex, const bool sync) {
  AST::eRegisterType regType = shaderType == eShaderType::Pixel ? AST::eRegisterType::PixelInput : AST::eRegisterType::VertexInput;
  const AST::Expression dest = nodeWriter.EmitWriteReg(shaderType == eShaderType::Pixel, false, tex.dst_reg, AST::eRegisterType::Constant);
  const AST::Expression src = nodeWriter.EmitReadReg(tex.src_reg, regType);
  const eSwizzle srcX = static_cast<const eSwizzle>((tex.src_swiz >> 0) & 3);
  const eSwizzle srcY = static_cast<const eSwizzle>((tex.src_swiz >> 2) & 3);
  const eSwizzle srcZ = static_cast<const eSwizzle>((tex.src_swiz >> 4) & 3);
  const eSwizzle srcW = static_cast<const eSwizzle>((tex.src_swiz >> 6) & 3);
  const AST::Expression srcSwizzle = nodeWriter.EmitReadSwizzle(src, srcX, srcY, srcZ, srcW);
  AST::Expression sample;
  switch (static_cast<instr_dimension_t>(tex.dimension)) {
  case DIMENSION_1D: {
    sample = nodeWriter.EmitTextureSample1D(srcSwizzle, tex.const_idx);
  } break;
  case DIMENSION_2D: {
    sample = nodeWriter.EmitTextureSample2D(srcSwizzle, tex.const_idx);
  } break;
  case DIMENSION_3D: {
    sample = nodeWriter.EmitTextureSample3D(srcSwizzle, tex.const_idx);
  } break;
  case DIMENSION_CUBE: {
    sample = nodeWriter.EmitTextureSampleCube(srcSwizzle, tex.const_idx);
  } break;
  }
  // Write back swizzled
  const eSwizzle destX = static_cast<const eSwizzle>((tex.dst_swiz >> 0) & 7);
  const eSwizzle destY = static_cast<const eSwizzle>((tex.dst_swiz >> 3) & 7);
  const eSwizzle destZ = static_cast<const eSwizzle>((tex.dst_swiz >> 6) & 7);
  const eSwizzle destW = static_cast<const eSwizzle>((tex.dst_swiz >> 9) & 7);
  return nodeWriter.EmitWriteWithSwizzleStatement(dest, sample, destX, destY, destZ, destW);
}

AST::Statement ShaderNodeWriter::EmitPredicateTest(AST::NodeWriter &nodeWriter, const AST::Statement &code, const bool conditional, const u32 flowPredCondition, const u32 predSelect, const u32 predCondition) {
  if (predSelect && (!conditional || flowPredCondition != predCondition)) {
    // Get predicate register (invert if needed) and wrap in a condition
    return nodeWriter.EmitConditionalStatement(predCondition ? nodeWriter.EmitGetPredicate() : nodeWriter.EmitNot(nodeWriter.EmitGetPredicate()), code);
  }
  
  return code;
}

AST::Expression ShaderNodeWriter::EmitSrcReg(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const u32 num, const u32 type, const u32 swizzle, const u32 negate, const s32 slot) {
  AST::Expression reg = {};
  if (type) {
    // Runtime register
    const u32 regIndex = num & 0x7F;
    reg = nodeWriter.EmitReadReg(regIndex, AST::eRegisterType::Constant);
    // Take abs value
    if (num & 0x80)
      reg = nodeWriter.EmitAbs(reg);
  } else {
    if ((slot == 0 && instr.const_0_rel_abs) || (slot == 1 && instr.const_1_rel_abs)) {
      // consts[relative ? a0 + num : a0]
      reg = nodeWriter.EmitFloatConstRel(shaderType == eShaderType::Pixel, instr.relative_addr ? num : 0);
    } else {
      // consts[num]
      reg = nodeWriter.EmitFloatConst(shaderType == eShaderType::Pixel, num);
    }
    // Take abs value
    if (instr.abs_constants)
      reg = nodeWriter.EmitAbs(reg);
  }
  // Negate the result
  if (negate)
    reg = nodeWriter.EmitNegate(reg);
  // Add swizzle select
  if (swizzle) {
    // NOTE: A neutral (zero) swizzle pattern represents XYZW swizzle and the whole numbering is wrapped around
    const eSwizzle x = static_cast<const eSwizzle>(((swizzle >> 0) + 0) & 0x3);
    const eSwizzle y = static_cast<const eSwizzle>(((swizzle >> 2) + 1) & 0x3);
    const eSwizzle z = static_cast<const eSwizzle>(((swizzle >> 4) + 2) & 0x3);
    const eSwizzle w = static_cast<const eSwizzle>(((swizzle >> 6) + 3) & 0x3);
    reg = nodeWriter.EmitReadSwizzle(reg, x, y, z, w);
  }
  return reg;
}

AST::Expression ShaderNodeWriter::EmitSrcReg(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const u32 argIndex) {
  switch (argIndex) {
  case 0: {
    return EmitSrcReg(nodeWriter, instr, instr.src1_reg, instr.src1_sel, instr.src1_swiz, instr.src1_reg_negate, 0);
  } break;
  case 1: {
    return EmitSrcReg(nodeWriter, instr, instr.src2_reg, instr.src2_sel, instr.src2_swiz, instr.src2_reg_negate, instr.src1_sel ? 1 : 0);
  } break;
  case 2: {
    return EmitSrcReg(nodeWriter, instr, instr.src3_reg, instr.src3_sel, instr.src3_swiz, instr.src3_reg_negate, (instr.src1_sel || instr.src2_sel) ? 1 : 0);
  } break;
  }
  LOG_ERROR(Xenos, "[UCode::Reg] Failed to emit a source register! Invaid arg index '{}'", argIndex);
  return {};
}

#define SWIZZLE(x, y, z, w) ((x & 3) << 0) | ((y & 3) << 0) | ((z & 3) << 0) | ((w & 3) << 0)
AST::Expression ShaderNodeWriter::EmitSrcScalarReg1(AST::NodeWriter &nodeWriter, const instr_alu_t &instr) {
  return EmitSrcReg(nodeWriter, instr, instr.src3_reg, instr.src3_sel, SWIZZLE(0, 0, 0, 0), instr.src3_reg_negate, (instr.src1_sel || instr.src2_sel) ? 1 : 0);
}

AST::Expression ShaderNodeWriter::EmitSrcScalarReg2(AST::NodeWriter &nodeWriter, const instr_alu_t &instr) {
  return EmitSrcReg(nodeWriter, instr, instr.src3_reg, instr.src3_sel, SWIZZLE(1, 1, 1, 1), instr.src3_reg_negate, (instr.src1_sel || instr.src2_sel) ? 1 : 0);
}

AST::Statement ShaderNodeWriter::EmitVectorResult(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const AST::Expression &code) {
  // Clamp value to 0-1 range
  AST::Expression input = instr.vector_clamp ? nodeWriter.EmitSaturate(code) : code;
  AST::Expression dest = nodeWriter.EmitWriteReg(shaderType == eShaderType::Pixel, instr.export_data, instr.vector_dest, AST::eRegisterType::Constant);
  // Prepare to write swizzle
  eSwizzle swizzle[4] = { eSwizzle::Unused, eSwizzle::Unused, eSwizzle::Unused, eSwizzle::Unused };
  if (instr.export_data) {
    u32 writeMask = instr.vector_write_mask;
    u32 scalarMask = instr.scalar_write_mask;
    for (u32 i = 0; i != 4; ++i) {
      const u32 channelMask = 1 << i;
      if (writeMask & channelMask) {
        if (writeMask & scalarMask)
          swizzle[i] = eSwizzle::One;
        else
          swizzle[i] = static_cast<eSwizzle>(i);
      }
      else if (instr.scalar_dest_rel) {
        swizzle[i] = eSwizzle::Zero;
      }
    }
  } else {
    // Normal swizzle
    u32 writeMask = instr.vector_write_mask;
    for (u32 i = 0; i != 4; ++i) {
      const u32 channelMask = 1 << i;
      if (writeMask & channelMask)
        swizzle[i] = static_cast<eSwizzle>(i);
      else
        swizzle[i] = eSwizzle::Unused;
    }
  }
  return nodeWriter.EmitWriteWithSwizzleStatement(dest, input, swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
}

AST::Statement ShaderNodeWriter::EmitScalarResult(AST::NodeWriter &nodeWriter, const instr_alu_t &instr, const AST::Expression &code) {
  // Clamp value to 0-1 range
  AST::Expression input = instr.vector_clamp ? nodeWriter.EmitSaturate(code) : code;
  // During export scalar operation can still write into the vector, so we check if it's a pure scalar operation, or a vector operation
  AST::Expression dest = nodeWriter.EmitWriteReg(shaderType == eShaderType::Pixel, instr.export_data, instr.export_data ? instr.vector_dest : instr.scalar_dest, AST::eRegisterType::Constant);
  u32 writeMask = instr.export_data ? instr.scalar_write_mask & ~instr.vector_write_mask : instr.scalar_write_mask;
  // Prepare to write swizzle
  eSwizzle swizzle[4] = { eSwizzle::Unused, eSwizzle::Unused, eSwizzle::Unused, eSwizzle::Unused };
  for (u32 i = 0; i != 4; ++i) {
    const u32 channelMask = 1 << i;
    if (writeMask & channelMask)
      swizzle[i] = static_cast<eSwizzle>(i);
    else
      swizzle[i] = eSwizzle::Unused;
  }
  // Emit output
  return nodeWriter.EmitWriteWithSwizzleStatement(dest, input, swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
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
