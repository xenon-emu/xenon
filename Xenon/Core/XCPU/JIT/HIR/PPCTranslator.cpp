/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Logging/Log.h"
#include "Core/XeMain.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/JIT/PPU_JIT.h"
#include "Core/XCPU/JIT/HIR/PPCTranslator.h"
#include "Core/XCPU/JIT/HIR/HIREmitters/HIREmitters.h"
#include "Core/XCPU/PPU/PPCInternal.h"
#include "Core/XCPU/PPU/PPU.h"
#include "Core/XCPU/JIT/Compiler/Passes/ContextPromotionPass.h"
#include "Core/XCPU/JIT/Compiler/Passes/ConditionalGroupPass.h"
#include "Core/XCPU/JIT/Compiler/Passes/SimplificationPass.h"
#include "Core/XCPU/JIT/Compiler/Passes/ConstantPropagationPass.h"
#include "Core/XCPU/JIT/Compiler/Passes/DeadCodeEliminationPass.h"
#include "Core/XCPU/Interpreter/InstructionProfiler.h"

namespace Xe::XCPU::JIT {

PPCTranslator::PPCTranslator(PPU *inPPU, sPPEState *inPPEState, CodeGenBackend *inBackend)
  : ppu(inPPU), ppeState(inPPEState), backend(inBackend) {
  Reset();
  compiler_.reset(new Compiler::Compiler());

  compiler_->AddPass(std::make_unique<Compiler::Passes::ContextPromotionPass>());
  // Grouped simplification + constant propagation.
  // Loops until no changes are made.
  auto sap = std::make_unique<Compiler::Passes::ConditionalGroupPass>();
  sap->AddPass(std::make_unique<Compiler::Passes::SimplificationPass>());
  sap->AddPass(std::make_unique<Compiler::Passes::ConstantPropagationPass>());
  compiler_->AddPass(std::move(sap));
  compiler_->AddPass(std::make_unique<Compiler::Passes::DeadCodeEliminationPass>());
}

void PPCTranslator::Reset() {
  builder.Reset();
}

bool PPCTranslator::IsBlockEndingInstruction(u32 instrData) {
  // Check the primary opcode field (bits 0..5) for branch instructions.
  u32 primary = (instrData >> 26) & 0x3F;
  switch (primary) {
  case 0x12: // b/bl/ba/bla
    return true;
  case 0x10: // bc/bcl/bca/bcla
    return true;
  case 0x11: // sc
    return true;
  case 0x13: {
    // Extended opcode group - check XO field (bits 21..30)
    u32 xo = (instrData >> 1) & 0x3FF;
    switch (xo) {
    case 0x010: // bclr/bclrl
    case 0x210: // bcctr/bcctrl
    case 0x012: // rfid
      return true;
    default:
      break;
    }
    break;
  }
  default:
    break;
  }
  return false;
}

const Core::HLE::sHLEFunction *PPCTranslator::FindHLEFunction(u64 guestAddress) const {
  if (!hleFunctionTable) {
    return nullptr;
  }
  for (const auto &entry : *hleFunctionTable) {
    if (entry.valid && entry.guestAddress == static_cast<u32>(guestAddress)) {
      return &entry;
    }
  }
  return nullptr;
}

// Full pipeline: PPC -> HIR -> Optimized HIR -> Backend -> native code pointer.
void *PPCTranslator::TranslateBlock(u64 blockStartAddress, sPPEState *inPPEState,
                                    u64 maxInstrs, u64 *outCodeSize,
                                    HIRBlockMetadata *outMeta,
                                    ePPUThreadID threadId) {
  // Check backend
  if (!backend) {
    LOG_ERROR(Xenon, "[PPCTranslator]: TranslateBlock called without a backend");
    return nullptr;
  }

  // Use the caller-provided state for this translation.
  ppeState = inPPEState;

  // When POST 0x2E HW_INIT fires, hwInitPosted is set and hwReturnAddress.
  // We make use of that to grab the LR and set it here, thus completely
  // avoiding the hardware-init busy-loop (same mechanism used by the
  // interpreter and legacy x86 JIT — no per-console-revision address
  // matching needed).
  // NOTE: whoever wires a caching HIR dispatch loop (mirroring
  // PPU_JIT::ExecuteJITInstrs) must repeat this check once per dispatch,
  // not just at translation time — a cached block would otherwise bypass it.
  if (XeMain::GetCPU()->HasHWINITPosted()) {
    auto &thread = ppeState->ppuThread[static_cast<u8>(threadId)];
    XeMain::GetCPU()->SetHWINITPosted(false);
    PPCInterpreter::ppuSetCR(ppeState, 0, false, false, true, false);
    thread.NIA = XeMain::GetCPU()->GetHWINITReturnAddress() & ~3ULL;
    LOG_INFO(Xenon, "[HIR Translator]: HwInit intercepted. Returning to {:#x}.", thread.NIA);
    return nullptr;
  }

  //
  // Translate PPC -> HIR
  //

  // Reset block builder
  builder.Reset();

  // Get thread by explicit ID to avoid racing on shared ppeState->currentThread.
  auto &thread = ppeState->ppuThread[static_cast<u8>(threadId)];

  builder.CommentFormat("Block start: {:#x}", blockStartAddress);

  u64 instrCount = 0;
  if (outMeta) {
    outMeta->instrCount = 0;
    outMeta->hash = 0;
    outMeta->lastWasBranch = false;
  }

  while (XeRunning && !XePaused) {

    // Update previous instruction address
    thread.PIA = thread.CIA;
    // Update current instruction address
    thread.CIA = thread.NIA;
    // Increase next instruction address
    thread.NIA += 4;

    // HLE dispatch: check if this guest address maps to a host HLE handler.
    if (const Core::HLE::sHLEFunction *hleEntry = FindHLEFunction(thread.CIA)) {
      builder.CommentFormat("{:#x}: HLE dispatch -> {} ({})",
        thread.CIA, hleEntry->functionName, reinterpret_cast<const void *>(hleEntry->functionPtr));

      // Emit the CIA/NIA source annotation so metadata is correct.
      builder.SourceOffset(thread.CIA, 0, true);

      // Call the host HLE handler.
      builder.CallHLEFunction(hleEntry->functionPtr);

      // Synthesize a bclr (branch to LR, unconditional: BO=20, BI=0, LK=0).
      uPPCInstr blrInstr{ 0x4E800020 };
      builder.BranchConditionalToLR(blrInstr);
      builder.CommentFormat("Block end (HLE)");
      instrCount++;
      break;
    }

    // Is the instruction data valid?
    bool instrDataValid = true;
    // Fetch Instruction data.
    thread.instrFetch = true;
    uPPCInstr op{ PPCInterpreter::MMURead32(ppeState, thread.CIA) };
    thread.instrFetch = false;

    // Check for Instruction storage/segment exceptions. If found we must end the block.
    if (thread.exceptReg & ppuInstrStorageEx || thread.exceptReg & ppuInstrSegmentEx) {
#ifdef JIT_DEBUG
      LOG_DEBUG(Xenon, "[HIR Translator]: Instruction exception when creating block at CIA {:#x}, block start address {:#x}, instruction count {:#x}",
        thread.CIA, blockStartAddress, instrCount);
#endif
      if (instrCount != 0) {
        // We're a few instructions into the block, just end the block on the last instruction and start a new block on
        // the faulting instruction. It will process the exception accordingly.
        // We clear the exception condition or else the exception handler will run on the first instruction of last the
        // compiled block.
        thread.exceptReg &= ~(ppuInstrStorageEx | ppuInstrSegmentEx);
        break;
      } else {
        // Manually process the pending exceptions.
        ppu->PPUProcessSyncExceptions(ppeState);
        // Return from block creation. Next block will be one the handlers for instruction exceptions.
        return nullptr;
      }
    }

    u32 opcode = op.opcode;

    u32 decodedInstr = PPCDecode(opcode);

    // Compute instruction name hash - use direct computation instead of thread_local map
    // The hash is only needed for block termination check, so compute it efficiently
    u32 opName = Base::JoaatStringHash(PPCInterpreter::ppcDecoder.decodeName(opcode));

    // Check if this instruction is a block-ending branch
    bool isBranchInstr = (opName == JITOpcodeHashes::B || opName == JITOpcodeHashes::BC ||
      opName == JITOpcodeHashes::BCLR || opName == JITOpcodeHashes::BCCTR ||
      opName == JITOpcodeHashes::RFID);

    // Check if this instruction can cause sync exceptions
    bool canCauseException = InstrCanCauseSyncException(opName);

    // Branch instructions and sync exception-capable instructions need CIA/NIA updated
    // Safe instructions only need CI data written
    builder.SourceOffset(thread.CIA, opcode, true);

    // Check for ocurred Instruction access exceptions.
    if (opcode == 0xFFFFFFFF || opcode == 0xCDCDCDCD || opcode == 0x00000000) {
      LOG_CRITICAL(Xenon, "[HIR Translator]: Invalid instruction data {:#010x} at {:#x}", opcode, thread.CIA);
      instrDataValid = false;
    }

    if (instrDataValid) {
      // Apply GPR patches
      for (const auto &patch : kJITPatchTable) {
        if (static_cast<u32>(thread.CIA) == patch.address) {
          if (patch.isOr) {
            builder.StoreGPR(patch.reg, builder.Or(builder.LoadGPR(patch.reg), builder.LoadConstantUint64(patch.value)));
          }
          else {
            builder.StoreGPR(patch.reg, builder.LoadConstantUint64(patch.value));
          }

          // Let the user know the patch executed successfully.
          LOG_INFO(Xenon, "[HIR Translator]: Patched GPR[{:#d}] at address {:#x}. New Value = {:#x}. Purpose: {}", patch.reg, patch.address, patch.value, patch.patchName);
          // Exit out
          break;
        }
      }

      // Decode using PPCDecode and look up the HIR emitter.
      HIR::instructionHandlerHIR emitter = decoder.decode(opcode);

      // Get the instruction name for annotation.
      const std::string instrName = PPCInterpreter::ppcDecoder.getNameTable()[decodedInstr];
      builder.CommentFormat("{:#x}: {} ({:#010x})", thread.CIA, instrName, opcode);

      if (emitter != &HIR::HIRInstrEmit_invalid) {
        // Call the HIR emitter to generate IR for this instruction.
        emitter(builder, op);
      } else {
        LOG_WARNING(Xenon, "[HIR Translator]: No Emitter for '{}' at {:#x}", instrName, thread.CIA);
        // Fall back to interpreter
        builder.CallInterpreter(op, reinterpret_cast<void *>(PPCInterpreter::ppcDecoder.decode(op.opcode)));
      }
    }

    // Emit exception check if the instruction can cause a synchronous exception
    if (canCauseException) {
      builder.SyncExceptionCheck();
    }

    instrCount++;

    // Check for block-ending instructions.
    bool isBlockEnd = false;
    if (opName == JITOpcodeHashes::B) {
      isBlockEnd = true;
    } else if (opName == JITOpcodeHashes::BC) {
      isBlockEnd = true;
    } else if (opName == JITOpcodeHashes::BCLR || opName == JITOpcodeHashes::BCCTR ||
      opName == JITOpcodeHashes::RFID) {
      isBlockEnd = true;
    }

    if (!instrDataValid) {
      isBlockEnd = true;
      LOG_ERROR(Xenon, "[HIR Translator]: Invalid instruction data {:#x}, found at {:#x}", opcode, thread.CIA);
    }

    if (isBlockEnd) {
      builder.CommentFormat("Block end (branch)");
      break;
    }

    if (instrCount >= maxInstrs) {
      builder.CommentFormat("Block end (max instructions)");
      break;
    }
  }

  // Reset CIA and NIA.
  thread.CIA = blockStartAddress - 4;
  thread.NIA = blockStartAddress;

  // Compile/optimize/etc.
  if (!compiler_->Compile(&builder)) {
    LOG_CRITICAL(Xenon, "[PPCTranslator]: Optimization passes failed!");
    return nullptr;
  }

  // Compute block metadata by walking the HIR SOURCE_OFFSET instructions.
  if (outMeta) {
    outMeta->instrCount = 0;
    outMeta->hash = 0;
    outMeta->lastWasBranch = false;

    HIR::HIRBlock *hirBlock = builder.getCurrentBlock();
    if (hirBlock) {
      u32 lastRawWord = 0;
      for (const HIR::Instr *instr = hirBlock->instrHead; instr; instr = instr->next) {
        if (instr->opcode && instr->opcode->num == HIR::OPCODE_SOURCE_OFFSET) {
          outMeta->instrCount++;
          u32 opcodeData = instr->currentInstrData.opcode;
          outMeta->hash += opcodeData;
          lastRawWord = opcodeData;
        }
      }

      // Check if the last instruction was a branch
      if (lastRawWord != 0 && IsBlockEndingInstruction(lastRawWord)) {
        outMeta->lastWasBranch = true;
      }
    }
  }

  // Step 2: HIR -> backend native code
  HIR::HIRBlock *block = builder.getCurrentBlock();
  if (!block) {
    LOG_WARNING(Xenon, "[PPCTranslator]: No HIR block produced for {:#x}", blockStartAddress);
    return nullptr;
  }

  void *code = nullptr;
  u64 codeSize = 0;
  if (!backend->EmitBlock(block, &code, &codeSize, threadId)) {
    LOG_WARNING(Xenon, "[PPCTranslator]: Backend compilation failed for {:#x}", blockStartAddress);
    return nullptr;
  }

  if (outCodeSize) {
    *outCodeSize = codeSize;
  }
  return code;
}

// Dumps the generated HIR in a human readable form
void PPCTranslator::DumpHIR() const {
  using namespace HIR;

  // Get current block
  HIRBlock *block = builder.getCurrentBlock();

  if (!block) {
    LOG_INFO(Xenon, "[HIR Dump]: No HIR block generated");
    return;
  }

  // Formats a constant value as a decimal integer or float literal.
  auto fmtConst = [](const Value *v) -> std::string {
    switch (v->type) {
    case INT8_TYPE:    return FMT("{:#04x}", static_cast<u8>(v->constant.i8));
    case INT16_TYPE:   return FMT("{:#06x}", static_cast<u16>(v->constant.i16));
    case INT32_TYPE:   return FMT("{:#010x}", static_cast<u32>(v->constant.i32));
    case INT64_TYPE:   return FMT("{:#018x}", static_cast<u64>(v->constant.i64));
    case FLOAT32_TYPE: return FMT("{}", v->constant.f32);
    case FLOAT64_TYPE: return FMT("{}", v->constant.f64);
    case VEC128_TYPE:
      return FMT("[{:#010x}, {:#010x}, {:#010x}, {:#010x}]",
        v->constant.v128.dsword[0], v->constant.v128.dsword[1],
        v->constant.v128.dsword[2], v->constant.v128.dsword[3]);
    default: return "WTF?";
    }
  };

  // Formats a Value as "v<N>" for SSA values or the constant literal for constants.
  auto fmtVal = [&](const Value *v) -> std::string {
    if (!v) return "(null)";
    if (v->flags & VALUE_IS_CONSTANT) {
      return fmtConst(v);
    }
    return FMT("v{}", v->ordinal);
  };

  LOG_INFO(Xenon, "[HIR Dump]: === Begin HIR ===");

  const Instr *instr = block->instrHead;
  u32 instrCount = 0;
  while (instr) {
    if (!instr->opcode) {
      instr = instr->next;
      continue;
    }

    if (instr->opcode->num == OPCODE_COMMENT) {
      const char *text = reinterpret_cast<const char *>(instr->src1.offset);
      LOG_INFO(Xenon, "  // {}", text ? text : "");
    } else if (instr->opcode->num == OPCODE_NOP) {
      LOG_INFO(Xenon, "  nop");
    } else {
      const char *opName = instr->opcode->name ? instr->opcode->name : "???";

      u32 sig = instr->opcode->signature;
      u32 src1Type = (sig >> 3) & 0x7;
      u32 src2Type = (sig >> 6) & 0x7;
      u32 src3Type = (sig >> 9) & 0x7;

      // Build the operand list.
      std::string operands;
      auto appendOperand = [&](std::string_view op) {
        if (!operands.empty()) operands += ", ";
        operands += op;
      };

      if (src1Type == OPCODE_SIG_TYPE_V && instr->src1.value) {
        appendOperand(fmtVal(instr->src1.value));
      } else if (src1Type == OPCODE_SIG_TYPE_O) {
        appendOperand(FMT("+{:#x}", instr->src1.offset));
      } else if (src1Type == OPCODE_SIG_TYPE_S && instr->src1.label) {
        appendOperand(FMT("label@{}", static_cast<const void *>(instr->src1.label)));
      } else if (src1Type == OPCODE_SIG_TYPE_L) {
        appendOperand(FMT("label@{}", static_cast<const void *>(instr->src1.label)));
      }

      if (src2Type == OPCODE_SIG_TYPE_V && instr->src2.value) {
        appendOperand(fmtVal(instr->src2.value));
      } else if (src2Type == OPCODE_SIG_TYPE_O) {
        appendOperand(FMT("+{:#x}", instr->src2.offset));
      } else if (src2Type == OPCODE_SIG_TYPE_L) {
        appendOperand(FMT("label@{}", static_cast<const void *>(instr->src2.label)));
      }

      if (src3Type == OPCODE_SIG_TYPE_V && instr->src3.value) {
        appendOperand(fmtVal(instr->src3.value));
      } else if (src3Type == OPCODE_SIG_TYPE_O) {
        appendOperand(FMT("+{:#x}", instr->src3.offset));
      }

      // Emit: "  [vN = ]opname [operands]"
      if (instr->dest) {
        if (!operands.empty()) {
          LOG_INFO(Xenon, "  v{} = {} {}", instr->dest->ordinal, opName, operands);
        } else {
          LOG_INFO(Xenon, "  v{} = {}", instr->dest->ordinal, opName);
        }
      } else {
        if (!operands.empty()) {
          LOG_INFO(Xenon, "  {} {}", opName, operands);
        } else {
          LOG_INFO(Xenon, "  {}", opName);
        }
      }
    }

    ++instrCount;
    instr = instr->next;
  }

  LOG_INFO(Xenon, "[HIR Dump]: === End HIR ({} instructions) ===", instrCount);
}

} // namespace Xe::XCPU::JIT
