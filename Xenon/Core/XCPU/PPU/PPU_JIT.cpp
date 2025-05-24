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

bool callEpil(PPU *ppu) {
  // Check timebase
  ppu->CheckTimeBaseStatus();

  // Check for external interrupts
  auto &thread = ppu->ppuState->ppuThread[ppu->ppuState->currentThread];
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
  COMP->movzx(tempR, x86::byte_ptr(b->ppuState->Base(), offsetof(PPU_STATE, currentThread)));
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
  COMP->mov(temp, x86::ptr(b->ppu->Base(), offsetof(PPU, ppuHaltOn)));
  COMP->test(temp, temp);
  COMP->je(continueLabel);

  // ppuHaltOn == curThread.CIA - !guestHalt
  COMP->cmp(temp, x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, CIA)));
  COMP->jne(continueLabel);
  COMP->cmp(x86::byte_ptr(b->ppu->Base(), offsetof(PPU, guestHalt)), 0);
  COMP->jne(continueLabel);

  // Call HALT
  InvokeNode *out = nullptr;
  COMP->invoke(&out, imm((void*)callHalt), FuncSignatureT<void>());

  COMP->bind(continueLabel);

  // Update CIA NIA _instr (CI)
  COMP->mov(temp, x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, NIA)));
  COMP->mov(x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, CIA)), temp);
  COMP->add(temp, 4);
  COMP->mov(x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, NIA)), temp);
  COMP->mov(temp, instrData);
  COMP->mov(x86::dword_ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, CI.opcode)), temp);

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
  jitBuilder->ppu = new PPUPtr(compiler.newGpz("ppu"));
  jitBuilder->ppuState = new ASMJitPtr<PPU_STATE>(compiler.newGpz("ppuState"));
  jitBuilder->threadCtx = new ASMJitPtr<PPU_THREAD_REGISTERS>(compiler.newGpz("thread"));
  jitBuilder->haltBool = compiler.newGpb("enableHalt"); // bool

  FuncSignatureT<void, void*, void*, bool> signature;
  compiler.addFunc(signature);
  compiler.setArg(0, jitBuilder->ppu->Base());
  compiler.setArg(1, jitBuilder->ppuState->Base());
  compiler.setArg(2, jitBuilder->haltBool);
  setupContext(jitBuilder.get());

  std::vector<u32> instrsTemp{};

  //
  // Instruction emitters
  //
  u64 instrCount = 0;
  while (XeRunning && !XePaused) {
    u32 offset = instrCount * 4;
    u64 pc = addr + offset;

    // Fetch instruction
    curThread.CIA = curThread.NIA;
    curThread.NIA += 4;
    curThread.instrFetch = true;
    PPCOpcode op{ PPCInterpreter::MMURead32(ppuState, curThread.CIA) };
    u32 opcode = op.opcode;
    instrsTemp.push_back(opcode);
    curThread.instrFetch = false;

    // Decode and emit

    // Saves a few cycles to cache the value here
    u32 decodedInstr = PPCDecode(opcode);
    auto emitter = PPCInterpreter::ppcDecoder.getJITTable()[decodedInstr];
    static thread_local std::unordered_map<u32, u32> opcodeHashCache;
    u32 opName = opcodeHashCache.contains(opcode) ? opcodeHashCache[opcode] : opcodeHashCache[opcode] = Base::JoaatStringHash(PPCInterpreter::ppcDecoder.getNameTable()[decodedInstr]);

    // Handle skips
    bool skip = false;
    switch (curThread.CIA) {
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

    if (skip)
      break;

    bool readNextInstr = true;
    // Prol
    setupProl(jitBuilder.get(), opcode);

    if ((curThread.exceptReg & PPU_EX_INSSTOR || curThread.exceptReg & PPU_EX_INSTSEGM) || opcode == 0xFFFFFFFF)
      readNextInstr = false;

    // Call JIT emitter
    if (ppu->currentExecMode == eExecutorMode::Hybrid && emitter == &PPCInterpreter::PPCInterpreterJIT_invalid && readNextInstr) {
      auto intEmitter = PPCInterpreter::ppcDecoder.getTable()[decodedInstr];

      InvokeNode *out = nullptr;
      compiler.invoke(&out, imm((void*)intEmitter), FuncSignatureT<void, void*>());
      out->setArg(0, jitBuilder->ppuState->Base());
    }

    // Epil (check for exc and interrupts)
    InvokeNode *tbCheck = nullptr;
    x86::Gp retVal = compiler.newGpb();
    compiler.invoke(&tbCheck, imm((void*)callEpil), FuncSignatureT<bool, PPU*>());
    tbCheck->setArg(0, jitBuilder->ppu->Base());
    tbCheck->setRet(0, retVal);
    Label skipRet = compiler.newLabel();
    compiler.test(retVal, retVal);
    compiler.je(skipRet);
    compiler.ret();
    compiler.bind(skipRet);


    // If branch or block end    
    instrCount++;
    if (opName == "bclr"_j || opName == "bcctr"_j || opName == "bc"_j || opName == "b"_j || opName == "rfid"_j ||
        opName == "invalid"_j || instrCount >= maxBlockSize)
      break;
  }
#endif

  // reset CIA NIA
  curThread.CIA = addr - 4;
  curThread.NIA = addr;
  jitBuilder->size = instrCount * 4;

  compiler.ret();
  compiler.endFunc();
  compiler.finalize();

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
  while (instrsExecuted < numInstrs && active) {
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