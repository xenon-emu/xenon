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

u32 ComputeBlockHash(const std::vector<u32> &instrs) {
  u32 hash = 0x811C9DC5; // FNV offset basis
  for (u32 instr : instrs) {
    for (int i = 0; i < 4; ++i) {
      u8 byte = (instr >> (i * 8)) & 0xFF;
      hash ^= byte;
      hash *= 0x01000193; // FNV prime
    }
  }
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
  ppu->CheckTimeBaseStatus();

  // Get current thread
  auto &thread = ppuState->ppuThread[ppuState->currentThread];
  // Check for external interrupts
  if (thread.SPR.MSR.EE && ppu->xenonContext->xenonIIC.checkExtInterrupt(thread.SPR.PIR)) {
    thread.exceptReg |= PPU_EX_EXT;
  }

  // Handle pending exceptions
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

  setupContext(jitBuilder.get());

  //
  // Instruction emitters
  //
  // Lookup all instructions
  auto &thread = curThread;
  u64 prevCIA = thread.CIA;
  u64 prevNIA = thread.NIA;
  std::vector<u32> fetchedOpcodes(maxBlockSize);
  thread.instrFetch = true;
  PPCInterpreter::MMURead(
    PPCInterpreter::CPUContext, ppuState,
    addr, maxBlockSize * sizeof(u32),
    reinterpret_cast<u8*>(fetchedOpcodes.data()),
    ppuState->currentThread
  );
  thread.instrFetch = false;
  // Check things over, and decide when to cutoff opcodes
  for (jitBuilder->size = 0; jitBuilder->size < maxBlockSize; ++jitBuilder->size) {
    u32 opcode = fetchedOpcodes[jitBuilder->size];
    if (opcode == 0xFFFFFFFF)
      break;

    u32 decoded = PPCDecode(opcode);
    auto emitter = PPCInterpreter::ppcDecoder.getJITTable()[decoded];
    if (emitter == &PPCInterpreter::PPCInterpreterJIT_invalid ||
      PPCInterpreter::ppcDecoder.isBranch(decoded))
      break;
  }
  for (u64 i = 0; i < jitBuilder->size; ++i) {
    u32 offset = jitBuilder->size * 4;
    u64 pc = addr + offset;

    // Fetch instruction
    thread.CIA = thread.NIA;
    thread.NIA += 4;
    u32 opcode = fetchedOpcodes[jitBuilder->size];
    u32 decoded = PPCDecode(opcode);
    auto emitter = PPCInterpreter::ppcDecoder.getJITTable()[decoded];

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    InvokeNode *tbCheck = nullptr;
    x86::Gp retVal{};
    Label skipRet{};
    // Handle skips
    auto patchGPR = [&](int reg, u64 val) {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, val);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(reg), temp);
    };
    switch (thread.CIA) {
    case 0x0200C870: patchGPR(5, 0); break;
    // RGH 2 17489 in a JRunner Corona XDKBuild
    case 0x0200C7F0: patchGPR(3, 0); break;
    // VdpWriteXDVOUllong. Set r10 to 1. Skips XDVO write loop
    case 0x800EF7C0: patchGPR(10, 1); break;
    // VdpSetDisplayTimingParameter. Set r11 to 0x10. Skips ANA Check
    case 0x800F6264: patchGPR(11, 0x15E); break;
    // Pretend ARGON hardware is present, to avoid the call
    case 0x800819E0:
    case 0x80081A60: {
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11));
      compiler.or_(temp, 0x08);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    // INIT_POWER_MODE bypass 2.0.17489.0
    case 0x80081764:
    // XAudioRenderDriverInitialize bypass 2.0.17489.0
    case 0x80081830:
    // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
    // older console revisions
    case 0x801AF580:
      jitBuilder->size++;
      goto skip_emit;
    }
#endif
    // Prol
    setupProl(jitBuilder.get(), opcode);

    // Call JIT emitter
    if (ppu->currentExecMode == eExecutorMode::Hybrid && emitter == &PPCInterpreter::PPCInterpreterJIT_invalid && !(curThread.exceptReg & PPU_EX_INSSTOR || curThread.exceptReg & PPU_EX_INSTSEGM)) {
      auto intEmitter = PPCInterpreter::ppcDecoder.getTable()[decoded];

#if defined(ARCH_X86) || defined(ARCH_X86_64)
      InvokeNode *out = nullptr;
      compiler.invoke(&out, imm((void*)intEmitter), FuncSignature::build<void, void*>());
      out->setArg(0, jitBuilder->ppuState->Base());
#endif
    }
skip_emit:;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    // Epil (check for exc and interrupts)
    retVal = compiler.newGpb();
    compiler.invoke(&tbCheck, imm((void*)callEpil), FuncSignature::build<bool, PPU*, PPU_STATE*>());
    tbCheck->setArg(0, jitBuilder->ppu->Base());
    tbCheck->setArg(1, jitBuilder->ppuState->Base());
    tbCheck->setRet(0, retVal);
    skipRet = compiler.newLabel();
    compiler.test(retVal, retVal);
    compiler.je(skipRet);
    compiler.ret();
    compiler.bind(skipRet);
#endif
  }
  jitBuilder->size *= 4;

  // reset CIA NIA
  thread.CIA = prevCIA;
  thread.NIA = prevNIA;
  jitBuilder->size *= 4;

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
  block->opcodes = std::move(fetchedOpcodes);
  // Insert block into cache
  jitBlocks.emplace(addr, std::move(block));
  return jitBlocks.at(addr);
}
#define GPR(x) curThread.GPR[x]

void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt) {
  u32 instrsExecuted = 0;

  while (instrsExecuted < numInstrs && active && XeRunning && !XePaused) {
    auto &thread = curThread;
    u64 blockStart = thread.NIA;
    auto it = jitBlocks.find(blockStart);
    if (it == jitBlocks.end()) {
      auto block = BuildJITBlock(blockStart, numInstrs - instrsExecuted);
      if (!block)
        break;
      block->codePtr(ppu, ppuState, enableHalt);
      instrsExecuted += block->opcodes.size();
      continue;
    }

    auto &block = it->second;
    std::vector<u32> opcodes(block->opcodes.size());

    thread.instrFetch = true;
    PPCInterpreter::MMURead(PPCInterpreter::CPUContext, ppuState, blockStart, block->size, reinterpret_cast<u8*>(opcodes.data()), ppuState->currentThread);
    thread.instrFetch = false;

    if (memcmp(opcodes.data(), block->opcodes.data(), block->size) != 0) {
      jitBlocks.erase(blockStart);
      continue;
    }

    instrsExecuted += ExecuteJITBlock(blockStart, enableHalt);
  }
}
