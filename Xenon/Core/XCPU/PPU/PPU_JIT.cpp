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

//
//  Trampolines for Invoke
//
void callHalt() {
  return XeMain::GetCPU()->Halt();
}

bool callEpil(PPU *ppu, PPU_STATE *ppuState) {
  // Check timebase
  ppu->CheckTimeBaseStatus();

  // Get current thread
  auto &thread = ppuState->ppuThread[ppuState->currentThread];
  // Check for external interrupts
  if (thread.SPR.MSR.EE && ppu->xenonContext->xenonIIC.checkExtInterrupt(thread.SPR.PIR)) {
    thread.exceptReg |= PPU_EX_EXT;
  }

  return ppu->PPUCheckExceptions();
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
  COMP->movzx(tempR, b->ppuState->scalar(&PPU_STATE::currentThread).Ptr<u8>());
  COMP->imul(b->threadCtx->Base(), tempR, sizeof(PPU_THREAD_REGISTERS));
  // Since ppuThread[] base is at offset 0 we just need to add the offset in the array
  COMP->add(b->threadCtx->Base(), b->ppuState->Base());
#endif
}

void PPU_JIT::setupProl(JITBlockBuilder *b, u32 instrData, u32 decoded) {
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
  auto emitter = PPCInterpreter::ppcDecoder.getJITTable()[decoded];
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
  compiler.addFuncNode(&signature, FuncSignature::build<void, PPU*, PPU_STATE*, bool>());
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

    // Handle skips
    bool skip = false;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    auto patchGPR = [&](s32 reg, u64 val) {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, val);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(reg), temp);
    };
    switch (thread.CIA) {
    case 0x80081830: skip = true; break;
    case 0x0200C870: patchGPR(5, 0); break;
    // RGH 2 17489 in a JRunner Corona XDKBuild
    case 0x0200C7F0: patchGPR(3, 0); break;
    // VdpWriteXDVOUllong. Set r10 to 1. Skips XDVO write loop
    case 0x800EF7C0: patchGPR(10, 1); break;
    // VdpSetDisplayTimingParameter. Set r11 to 0x10. Skips ANA Check
    case 0x800F6264: patchGPR(11, 0x15E); break;
    // Needed for FSB_FUNCTION_2
    case 0x1003598ULL: patchGPR(11, 0x0E); break;
    case 0x1003644ULL: patchGPR(11, 0x02); break;
    // Pretend ARGON hardware is present, to avoid the call
    case 0x800819E0:
    case 0x80081A60: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11));
      compiler.or_(temp, 0x08);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    }
#endif

    bool readNextInstr = true;

    // Prol
    setupProl(jitBuilder.get(), opcode, decodedInstr);

    if ((curThread.exceptReg & PPU_EX_INSSTOR || curThread.exceptReg & PPU_EX_INSTSEGM) || opcode == 0xFFFFFFFF)
      readNextInstr = false;

    // Call JIT emitter
    if (!skip && readNextInstr) {
      bool invalidInstr = emitter == &PPCInterpreter::PPCInterpreterJIT_invalid;
      if (ppu->currentExecMode == eExecutorMode::Hybrid && invalidInstr) {
        auto intEmitter = PPCInterpreter::ppcDecoder.getTable()[decodedInstr];

#if defined(ARCH_X86) || defined(ARCH_X86_64)
        InvokeNode *out = nullptr;
        compiler.invoke(&out, imm((void *)intEmitter), FuncSignature::build<void, void *>());
        out->setArg(0, jitBuilder->ppuState->Base());
#endif
      } else {
        emitter(ppuState, jitBuilder.get(), op);
      }
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

void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt) {
  u32 instrsExecuted = 0;
  while (instrsExecuted < numInstrs && active && (XeRunning && !XePaused)) {
    auto &thread = curThread;
    // This *must* be done here simply because of how we handle JIT.
    // We run until the start of a block, which is a branch opcode (or until it's a invalid instruction),
    // but these are branch opcodes, designed to avoid calling them.
    // So, these will break under BuildJITBlock, and tp avoid the issue, it's done here
    bool skipBlock = false;
    switch (thread.NIA) {
    // INIT_POWER_MODE bypass 2.0.17489.0
    case 0x80081764:
    // XAudioRenderDriverInitialize bypass 2.0.17489.0
    case 0x8018B0EC:
    // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
    // older console revisions
    case 0x801AF580:
      skipBlock = true;
      break;
    default:
      break;
    }
    if (skipBlock) {
      instrsExecuted++;
      thread.NIA += 4;
    }
    u64 blockStart = thread.NIA;
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
          thread.instrFetch = true;
          u64 val = PPCInterpreter::MMURead64(ppuState, block->ppuAddress + i * 8);
          thread.instrFetch = false;
          u64 top = val >> 32;
          u64 bottom = val & 0xFFFFFFFF;
          sum += top + bottom;
        }
      } else {
        for (u64 i = 0; i != block->size / 4; i++) {
          thread.instrFetch = true;
          sum += PPCInterpreter::MMURead32(ppuState, block->ppuAddress + i * 4);
          thread.instrFetch = false;
        }
      }

      if (block->hash != sum) {
        block.reset();
        jitBlocks.erase(blockStart);
        continue;
      }

      instrsExecuted += ExecuteJITBlock(blockStart, enableHalt);
    }
  }
}
