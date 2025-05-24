// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Global.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "Core/XCPU/Interpreter/JIT/x86_64/JITEmitter_Helpers.h"
#endif
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/Interpreter/PPCInternal.h"
#include "Core/XCPU/Xenon.h"
#include "PPU.h"
#include "PPU_JIT.h"

static u32 ComputeBlockHash(const std::vector<u32> &instrs) {
  u32 hash = 0;
  for (u32 instr : instrs)
    hash += instr;
  return hash;
}

//
//  Trampolines for Invoke
//
void callHalt() {
  return XeMain::GetCPU()->Halt();
}

bool callEpil(PPU *ppu, PPU_STATE *ppuState) {
  // Check timebase
  // Increase Time Base Counter
  if (ppu->xenonContext->timeBaseActive) {
    // HID6[15]: Time-base and decrementer facility enable.
    // 0 -> TBU, TBL, DEC, HDEC, and the hang-detection logic do not
    // update. 1 -> TBU, TBL, DEC, HDEC, and the hang-detection logic
    // are enabled to update
    if (ppuState->SPR.HID6 & 0x1000000000000) {
      // The Decrementer and the Time Base are driven by the same time frequency.
      u32 newDec = 0;
      u32 dec = 0;
      // Update the Time Base.
      ppuState->SPR.TB += ppu->clocksPerInstruction;
      // Get the decrementer value.
      dec = curThread.SPR.DEC;
      newDec = dec - ppu->clocksPerInstruction;
      // Update the new decrementer value.
      curThread.SPR.DEC = newDec;
      // Check if Previous decrementer measurement is smaller than current and a
      // decrementer exception is not pending.
      if (newDec > dec && !(_ex & PPU_EX_DEC)) {
        // The decrementer must issue an interrupt.
        _ex |= PPU_EX_DEC;
      }
    }
  }

  // Get current thread
  auto &thread = ppuState->ppuThread[ppuState->currentThread];
  // Check for external interrupts
  if (thread.SPR.MSR.EE && ppu->xenonContext->xenonIIC.checkExtInterrupt(thread.SPR.PIR)) {
    thread.exceptReg |= PPU_EX_EXT;
  }

  // Handle pending exceptions
  u16 &exceptions = thread.exceptReg;
  if (exceptions != PPU_EX_NONE) {
    // Non Maskable:
    //
    // 1. System Reset
    //
    if (exceptions & PPU_EX_RESET) {
      PPCInterpreter::ppcResetException(ppuState);
      exceptions &= ~PPU_EX_RESET;
      return true;
    }
    //
    // 2. Machine Check
    //
    if (exceptions & PPU_EX_MC) {
      if (thread.SPR.MSR.ME) {
        PPCInterpreter::ppcResetException(ppuState);
        exceptions &= ~PPU_EX_MC;
        return true;
      } else {
        // Checkstop Mode. Hard Fault.
        // A checkstop is a full - stop of the processor that requires a System
        // Reset to recover.
        LOG_CRITICAL(Xenon, "{}: CHECKSTOP!", ppuState->ppuName);
        XeMain::ShutdownCPU();
      }
    }
    // Maskable:
    //
    // 3. Instruction-Dependent
    //
    // A. Program - Illegal Instruction
    if (exceptions & PPU_EX_PROG && thread.progExceptionType == PROGRAM_EXCEPTION_TYPE_ILL) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Illegal Instruction.", ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_PROG;
      return true;
    }
    // B. Floating-Point Unavailable
    if (exceptions & PPU_EX_FPU) {
      PPCInterpreter::ppcFPUnavailableException(ppuState);
      exceptions &= ~PPU_EX_FPU;
      return true;
    }
    // C. Data Storage, Data Segment, or Alignment
    // Data Storage
    if (exceptions & PPU_EX_DATASTOR) {
      PPCInterpreter::ppcDataStorageException(ppuState);
      exceptions &= ~PPU_EX_DATASTOR;
      return true;
    }
    // Data Segment
    if (exceptions & PPU_EX_DATASEGM) {
      PPCInterpreter::ppcDataSegmentException(ppuState);
      exceptions &= ~PPU_EX_DATASEGM;
      return true;
    }
    // Alignment
    if (exceptions & PPU_EX_ALIGNM) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Alignment.", ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_ALIGNM;
      return true;
    }
    // D. Trace
    if (exceptions & PPU_EX_TRACE) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Trace.", ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_TRACE;
      return true;
    }
    // E. Program Trap, System Call, Program Priv Inst, Program Illegal Inst
    // Program Trap
    if (exceptions & PPU_EX_PROG && thread.progExceptionType == PROGRAM_EXCEPTION_TYPE_TRAP) {
      PPCInterpreter::ppcProgramException(ppuState);
      exceptions &= ~PPU_EX_PROG;
      return true;
    }
    // System Call
    if (exceptions & PPU_EX_SC) {
      PPCInterpreter::ppcSystemCallException(ppuState);
      exceptions &= ~PPU_EX_SC;
      return true;
    }
    // Program - Privileged Instruction
    if (exceptions & PPU_EX_PROG && thread.progExceptionType == PROGRAM_EXCEPTION_TYPE_PRIV) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Privileged Instruction.", ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_PROG;
      return true;
    }
    // F. Instruction Storage and Instruction Segment
    // Instruction Storage
    if (exceptions & PPU_EX_INSSTOR) {
      PPCInterpreter::ppcInstStorageException(ppuState);
      exceptions &= ~PPU_EX_INSSTOR;
      return true;
    }
    // Instruction Segment
    if (exceptions & PPU_EX_INSTSEGM) {
      PPCInterpreter::ppcInstSegmentException(ppuState);
      exceptions &= ~PPU_EX_INSTSEGM;
      return true;
    }
    //
    // 4. Program - Imprecise Mode Floating-Point Enabled Exception
    //
    if (exceptions & PPU_EX_PROG) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Imprecise Mode Floating-Point Enabled Exception.", ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_PROG;
      return true;
    }
    //
    // 5. External, Decrementer, and Hypervisor Decrementer
    //
    // External
    if (exceptions & PPU_EX_EXT && thread.SPR.MSR.EE) {
      PPCInterpreter::ppcExternalException(ppuState);
      exceptions &= ~PPU_EX_EXT;
      return true;
    }
    // Decrementer. A dec exception may be present but will only be taken when
    // the EE bit of MSR is set.
    if (exceptions & PPU_EX_DEC && thread.SPR.MSR.EE) {
      PPCInterpreter::ppcDecrementerException(ppuState);
      exceptions &= ~PPU_EX_DEC;
      return true;
    }
    // Hypervisor Decrementer
    if (exceptions & PPU_EX_HDEC) {
      LOG_ERROR(Xenon, "{}(Thrd{:#d}): Unhandled Exception: Hypervisor Decrementer.", ppuState->ppuName, static_cast<u8>(ppuState->currentThread));
      exceptions &= ~PPU_EX_HDEC;
      return true;
    }
    // VX Unavailable.
    if (exceptions & PPU_EX_VXU) {
      PPCInterpreter::ppcVXUnavailableException(ppuState);
      exceptions &= ~PPU_EX_VXU;
      return true;
    }
  }

  return false;
}

PPU_JIT::PPU_JIT(PPU *ppu) :
  ppu(ppu),
  ppuState(ppu->ppuState.get())
{}

PPU_JIT::~PPU_JIT() {
  for (auto &[hash, block] : jitBlocks)
    block.reset();
}

u64 PPU_JIT::ExecuteJITBlock(u64 addr, bool enableHalt) {
  auto &block = jitBlocks.at(addr);
  block->codePtr(ppu, ppuState, enableHalt);
  return block->size / 4;
}

// get current PPU_THREAD_REGISTERS, use ppuState to get the current Thread
void PPU_JIT::setupContext(JITBlockBuilder *b) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  x86::Gp tempR = newGP32();
  COMP->movzx(tempR, b->ppuState->Ptr<u8>());
  COMP->imul(b->threadCtx->Base(), tempR, sizeof(PPU_THREAD_REGISTERS));
  // Since ppuThread[] base is at offset 0 we just need to add the offset in the array
  COMP->add(b->threadCtx->Base(), b->ppuState->Base());
#endif
}

void PPU_JIT::setupProl(JITBlockBuilder *b, u32 instrData) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  COMP->nop();
  COMP->nop();

  x86::Gp temp = newGP64();
  Label continueLabel = COMP->newLabel();

  // enableHalt
  COMP->test(b->haltBool, b->haltBool);
  COMP->je(continueLabel);

  // ppuHaltOn != NULL
  COMP->mov(temp, b->ppu->scalar(&PPU::ppuHaltOn));
  COMP->test(temp, temp);
  COMP->je(continueLabel);

  // ppuHaltOn == curThread.CIA - !guestHalt
  COMP->cmp(temp, b->threadCtx->scalar(&PPU_THREAD_REGISTERS::CIA));
  COMP->jne(continueLabel);
  COMP->cmp(b->ppu->scalar(&PPU::guestHalt).Ptr<u8>(), 0);
  COMP->jne(continueLabel);

  // Call HALT
  InvokeNode *out = nullptr;
  COMP->invoke(&out, imm((void*)callHalt), FuncSignature::build<void>());

  COMP->bind(continueLabel);

  // Update CIA NIA _instr (CI)
  COMP->mov(temp, b->threadCtx->scalar(&PPU_THREAD_REGISTERS::NIA));
  COMP->mov(b->threadCtx->scalar(&PPU_THREAD_REGISTERS::CIA), temp);
  COMP->add(temp, 4);
  COMP->mov(b->threadCtx->scalar(&PPU_THREAD_REGISTERS::NIA), temp);
  COMP->mov(temp, instrData);
  COMP->mov(b->threadCtx->scalar(&PPU_THREAD_REGISTERS::CI).Ptr<u32>(), temp);

  COMP->nop();
  COMP->nop();
#endif
}

void PPU_JIT::patchSkips(JITBlockBuilder *b, u32 addr) {

}

#undef GPR
using namespace asmjit;
std::shared_ptr<JITBlock> PPU_JIT::BuildJITBlock(u64 addr, u64 maxBlockSize) {
  std::unique_ptr<JITBlockBuilder> jitBuilder = std::make_unique<STRIP_UNIQUE(jitBuilder)>(addr, &jitRuntime);

#if defined(ARCH_X86) || defined(ARCH_X86_64)
  //
  // x86
  //
  asmjit::x86::Compiler compiler(jitBuilder->Code());
  jitBuilder->compiler = &compiler;

  // Setup function and, state / thread context
  jitBuilder->ppu = new ASMJitPtr<PPU>(compiler.newGpz("ppu"));
  jitBuilder->ppuState = new ASMJitPtr<PPU_STATE>(compiler.newGpz("ppuState"));
  jitBuilder->threadCtx = new ASMJitPtr<PPU_THREAD_REGISTERS>(compiler.newGpz("thread"));
  jitBuilder->haltBool = compiler.newGpb("enableHalt"); // bool

  FuncNode *signature = nullptr;
  compiler.addFuncNode(&signature, FuncSignature::build<void, PPU *, PPU_STATE *, bool>());
  signature->setArg(0, jitBuilder->ppu->Base());
  signature->setArg(1, jitBuilder->ppuState->Base());
  signature->setArg(2, jitBuilder->haltBool);
#endif

  std::vector<u32> instrsTemp{};

  setupContext(jitBuilder.get());

  //
  // Instruction emitters
  //
  u64 instrCount = 0;
  while (XeRunning && !XePaused) {
    u32 offset = instrCount * 4;
    u64 pc = addr + offset;

    // Fetch instruction
    auto &thread = curThread;
    thread.CIA = thread.NIA;
    thread.NIA += 4;
    thread.instrFetch = true;
    PPCOpcode op{ PPCInterpreter::MMURead32(ppuState, thread.CIA) };
    thread.instrFetch = false;
    u32 opcode = op.opcode;
    instrsTemp.push_back(opcode);

    // Decode and emit

    // Saves a few cycles to cache the value here
    u32 decodedInstr = PPCDecode(opcode);
    auto emitter = PPCInterpreter::ppcDecoder.getJITTable()[decodedInstr];
    static thread_local std::unordered_map<u32, u32> opcodeHashCache;
    u32 opName = opcodeHashCache.contains(opcode) ? opcodeHashCache[opcode] : opcodeHashCache[opcode] = Base::JoaatStringHash(PPCInterpreter::ppcDecoder.getNameTable()[decodedInstr]);

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    // Handle skips
    bool skip = false;
    switch (thread.CIA) {
    case 0x0200C870: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, 0);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(5), temp);
    } break;
    // RGH 2 17489 in a JRunner Corona XDKBuild.
    case 0x0200C7F0: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, 0);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(3), temp);
    } break;
    case 0x1003598ULL: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, 0x0E);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    case 0x1003644ULL: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, 0x02);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    // INIT_POWER_MODE bypass 2.0.17489.0.
    case 0x80081764:
    // XAudioRenderDriverInitialize bypass 2.0.17489.0.
    case 0x80081830:
    // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
    // older console revisions.
    case 0x801AF580:
      skip = true;
      instrCount++;
      break;
    }

    switch (static_cast<u32>(thread.CIA)) {
    // INIT_POWER_MODE bypass 2.0.17489.0.
    case 0x80081764:
    // XAudioRenderDriverInitialize bypass 2.0.17489.0.
    case 0x80081830:
    // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
    // older console revisions.
    case 0x801AF580:
      skip = true;
      instrCount++;
      break;
    // VdpWriteXDVOUllong. Set r10 to 1. Skips XDVO write loop.
    case 0x800EF7C0: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, 0x01);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(10), temp);
    } break;
    // VdpSetDisplayTimingParameter. Set r11 to 0x10. Skips ANA Check.
    case 0x800F6264: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, 0x15E);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    // Pretend ARGON hardware is present, to avoid the call
    case 0x800819E0:
    case 0x80081A60: {
      // Set bit 3 (ARGON present)
      x86::Gp temp = compiler.newGpq();
      // thread.GPR[11] |= 0x08; // Set bit 3 (ARGON present)
      compiler.mov(temp, jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11));
      compiler.or_(temp, 0x08);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    }

    if (skip)
      break;
#endif

    bool readNextInstr = true;
    // Prol
    setupProl(jitBuilder.get(), opcode);

    if ((curThread.exceptReg & PPU_EX_INSSTOR || curThread.exceptReg & PPU_EX_INSTSEGM) || opcode == 0xFFFFFFFF)
      readNextInstr = false;

    // Call JIT emitter
    if (ppu->currentExecMode == eExecutorMode::Hybrid && emitter == &PPCInterpreter::PPCInterpreterJIT_invalid && readNextInstr) {
      auto intEmitter = PPCInterpreter::ppcDecoder.getTable()[decodedInstr];

#if defined(ARCH_X86) || defined(ARCH_X86_64)
      InvokeNode *out = nullptr;
      compiler.invoke(&out, imm((void*)intEmitter), FuncSignature::build<void, void*>());
      out->setArg(0, jitBuilder->ppuState->Base());
#endif
    }

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    // Epil (check for exc and interrupts)
    InvokeNode *tbCheck = nullptr;
    x86::Gp retVal = compiler.newGpb();
    compiler.invoke(&tbCheck, imm((void*)callEpil), FuncSignature::build<bool, PPU*, PPU_STATE*>());
    tbCheck->setArg(0, jitBuilder->ppu->Base());
    tbCheck->setArg(1, jitBuilder->ppuState->Base());
    tbCheck->setRet(0, retVal);
    Label skipRet = compiler.newLabel();
    compiler.test(retVal, retVal);
    compiler.je(skipRet);
    compiler.ret();
    compiler.bind(skipRet);
#endif

    // If branch or block end
    instrCount++;
    if (opName == "bclr"_j || opName == "bcctr"_j || opName == "bc"_j || opName == "b"_j || opName == "rfid"_j ||
        opName == "invalid"_j || instrCount >= maxBlockSize)
      break;
  }

  // reset CIA NIA
  curThread.CIA = addr - 4;
  curThread.NIA = addr;
  jitBuilder->size = instrCount * 4;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
  compiler.ret();
  compiler.endFunc();
  compiler.finalize();
#endif

  // Create the final JITBlock
  std::shared_ptr<JITBlock> block = std::make_shared<STRIP_UNIQUE(block)>(&jitRuntime, addr, jitBuilder.get());
  if (!block->Build()) {
    block.reset();
    return nullptr;
  }

  // Create block hash
  block->hash = 0;
  for (auto &instr : instrsTemp) {
    block->hash += instr;
  }
  
  // Insert block into cache
  jitBlocks.emplace(addr, std::move(block));
  return jitBlocks.at(addr);
}
#define GPR(x) curThread.GPR[x]

// TODO: Invalidate blocks
void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt) {
  u32 instrsExecuted = 0;
  while (instrsExecuted < numInstrs && active && (XeRunning && !XePaused)) {
    u64 blockStart = curThread.NIA;
    auto it = jitBlocks.find(blockStart);
    if (it == jitBlocks.end()) {
      if (it != jitBlocks.end())
        jitBlocks.erase(blockStart);
      auto block = BuildJITBlock(blockStart, numInstrs - instrsExecuted);
      if (!block)
        break; // Failed to build block, abort
      block->codePtr(ppu, ppuState, enableHalt);
      instrsExecuted += block->size / 4;
    } else {
      auto &block = it->second;
      u64 sum = 0;
      if (block->size % 8 == 0) {
        for (u64 i = 0; i < block->size / 8; i++) {
          curThread.instrFetch = true;
          u64 val = PPCInterpreter::MMURead64(ppuState, block->ppuAddress + i * 8);
          curThread.instrFetch = false;
          u64 top = val >> 32;
          u64 bottom = val & 0xFFFFFFFF;
          sum += top + bottom;
        }
      } else {
        for (u64 i = 0; i != block->size / 4; i++) {
          curThread.instrFetch = true;
          sum += PPCInterpreter::MMURead32(ppuState, block->ppuAddress + i * 4);
          curThread.instrFetch = false;
        }
      }

      if (block->hash != sum) {
        block.reset();
        jitBlocks.erase(blockStart);
        return;
      }

      instrsExecuted += ExecuteJITBlock(blockStart, enableHalt);
    }
  }
}

bool PPU_JIT::isBlockCached(u64 addr) {
  return jitBlocks.find(addr) != jitBlocks.end();
}