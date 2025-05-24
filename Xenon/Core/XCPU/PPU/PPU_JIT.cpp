// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"
#include "Base/Global.h"

#include "Core/XCPU/Interpreter/JIT/x86_64/JITEmitter_Helpers.h"
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

bool callEpil(PPU *ppu) {
  // Check timebase
  ppu->CheckTimeBaseStatus();

  // Check for external interrupts
  if (ppu->ppuState->ppuThread[ppu->ppuState->currentThread].SPR.MSR.EE && ppu->xenonContext->xenonIIC.checkExtInterrupt(ppu->ppuState->ppuThread[ppu->ppuState->currentThread].SPR.PIR)) {
    ppu->ppuState->ppuThread[ppu->ppuState->currentThread].exceptReg |= PPU_EX_EXT;
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
  std::shared_ptr<JITBlock> &entry = jitBlocks.at(addr);
  entry->codePtr(ppu, ppuState, enableHalt);
  return entry->size / 4;
}

// get current PPU_THREAD_REGISTERS, use ppuState to get the current Thread
void PPU_JIT::setupContext(JITBlockBuilder *b) {
  x86::Gp tempR = newGP32();
  COMP->movzx(tempR, x86::byte_ptr(b->ppuState->Base(), offsetof(PPU_STATE, currentThread)));
  COMP->imul(b->threadCtx->Base(), tempR, sizeof(PPU_THREAD_REGISTERS));
  // Since ppuThread[] base is at offset 0 we just need to add the offset in the array
  COMP->add(b->threadCtx->Base(), b->ppuState->Base());
}

void PPU_JIT::setupProl(JITBlockBuilder *b, u32 instrData) {
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

  // call HALT
  InvokeNode *out = nullptr;
  COMP->invoke(&out, imm((void*)callHalt), FuncSignatureT<void>());

  COMP->bind(continueLabel);

  // update CIA NIA _instr (CI)
  COMP->mov(temp, x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, NIA)));
  COMP->mov(x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, CIA)), temp);
  COMP->add(temp, 4);
  COMP->mov(x86::ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, NIA)), temp);
  COMP->mov(temp, instrData);
  COMP->mov(x86::dword_ptr(b->threadCtx->Base(), offsetof(PPU_THREAD_REGISTERS, CI.opcode)), temp);
  
  COMP->nop();
  COMP->nop();
}

void PPU_JIT::patchSkips(JITBlockBuilder *b, u32 addr) {

}

#undef GPR
using namespace asmjit;
std::shared_ptr<JITBlock> PPU_JIT::BuildJITBlock(u64 addr, u64 maxBlockSize) {
  std::unique_ptr<JITBlockBuilder> jitBuilder = std::make_unique<STRIP_UNIQUE(jitBuilder)>(addr, &jitRuntime);

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
  while (true) {
    u32 offset = instrCount * 4;
    u64 pc = addr + offset;

    // Fetch instruction
    curThread.CIA = curThread.NIA;
    curThread.NIA += 4;
    curThread.instrFetch = true;
    PPCOpcode op{ PPCInterpreter::MMURead32(ppuState, curThread.CIA) };
	  instrsTemp.push_back(op.opcode);
    curThread.instrFetch = false;


    // Decode and emit
    PPCInterpreter::instructionHandlerJIT emitter = PPCInterpreter::ppcDecoder.decodeJIT(op.opcode);
    u32 opName = Base::JoaatStringHash(PPCInterpreter::ppcDecoder.decodeName(op.opcode));

    // Handle skips
    x86::Gp temp = compiler.newGpq();
    bool skip = false;
    switch (curThread.CIA) {
    case 0x0200C870: {
      compiler.mov(temp, 0);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(5), temp);
    } break;
    // RGH 2 17489 in a JRunner Corona XDKBuild.
    case 0x0200C7F0: {
      compiler.mov(temp, 0);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(3), temp);
    } break;
    case 0x1003598ULL: {
      compiler.mov(temp, 0x0E);
      compiler.mov(jitBuilder->threadCtx->array(&PPU_THREAD_REGISTERS::GPR).Ptr(11), temp);
    } break;
    case 0x1003644ULL: {
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
    setupProl(jitBuilder.get(), op.opcode);

    if ((curThread.exceptReg & PPU_EX_INSSTOR || curThread.exceptReg & PPU_EX_INSTSEGM) || op.opcode == 0xFFFFFFFF) {
      readNextInstr = false;
    }

    // Call JIT emitter
    if (ppu->currentExecMode == eExecutorMode::Hybrid && emitter == &PPCInterpreter::PPCInterpreterJIT_invalid && readNextInstr) {
      PPCInterpreter::instructionHandler intEmitter = PPCInterpreter::ppcDecoder.decode(op.opcode);

      InvokeNode *out;
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

    instrCount++;

    // If branch or block end
    if (opName == "bclr"_j || opName == "bcctr"_j || opName == "bc"_j || opName == "b"_j || opName == "rfid"_j) {
      break;
    }

    if (opName == "invalid"_j) {
      break;
    }

    if (instrCount >= maxBlockSize) {
      break;
    }
  }

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
  jitBlocks.emplace(addr, block);
  return block;
}
#define GPR(x) curThread.GPR[x]

// TODO: Invalidate blocks
void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt) {
  u32 instrsExecuted = 0;
  while (instrsExecuted < numInstrs && active) {
    u64 blockStart = curThread.NIA;
    if (!isBlockCached(blockStart)) {
      if (isBlockCached(blockStart))
        jitBlocks.erase(blockStart);
      std::shared_ptr<JITBlock>block = BuildJITBlock(blockStart, numInstrs - instrsExecuted);
      // Failed to build block, abort
      if (!block)
        break;

      block->codePtr(ppu, ppuState, enableHalt);
      instrsExecuted += block->size / 4;
    } else {
      u64 sum = 0;
      std::shared_ptr<JITBlock> &block = jitBlocks.at(blockStart);
      std::vector<u32> values{};
      values.resize(block->size / 4);

      curThread.instrFetch = true;
      PPCInterpreter::MMURead(PPCInterpreter::CPUContext, ppuState, block->ppuAddress, values.size(), reinterpret_cast<u8*>(values.data()));
      curThread.instrFetch = false;

      for (auto &v : values) {
        sum += v;
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