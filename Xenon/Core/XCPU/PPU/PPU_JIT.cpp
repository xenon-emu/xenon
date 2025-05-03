// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/Interpreter/PPCInternal.h"
#include "PPU.h"
#include "PPU_JIT.h"
#include "../Interpreter/JIT/x86_64/JITEmitter_Helpers.h"
#include <Core/Xe_Main.h>


//
//  trampolines for Invoke
//
void callHalt()
{
    return Xe_Main->getCPU()->Halt();
}

bool callEpil(PPU* ppu)
{
    ppu->CheckTimeBaseStatus();

    // Check for external interrupts  
    if (ppu->ppuState->ppuThread[ppu->ppuState->currentThread].SPR.MSR.EE && ppu->xenonContext->xenonIIC.checkExtInterrupt(ppu->ppuState->ppuThread[ppu->ppuState->currentThread].SPR.PIR)) {
        ppu->ppuState->ppuThread[ppu->ppuState->currentThread].exceptReg |= PPU_EX_EXT;
    }

    // Handle pending exceptions  
    bool ret = ppu->PPUCheckExceptions();
    if (ret) __debugbreak();
    return ret;
}

PPU_JIT::PPU_JIT(PPU *_ppu) :
  ppu(_ppu),
  ppuState(ppu->ppuState.get())
{}

PPU_JIT::~PPU_JIT() {
  for (auto &block : jitBlocks)
    delete block.second;
}

u64 PPU_JIT::ExecuteJITBlock(u64 addr, bool enableHalt) {
   auto* entry = jitBlocks.at(addr);
   entry->codePtr(ppu, ppuState, enableHalt);
   return entry->size / 4;
}

// get current PPU_THREAD_REGISTERS, use ppuState to get the current Thread
void PPU_JIT::setupContext(JITBlockBuilder* b)
{
    x86::Gp tempR = newGP32();
    COMP->movzx(tempR, x86::byte_ptr(b->ppuState, offsetof(PPU_STATE, currentThread)));
    COMP->imul(b->threadCtx, tempR, sizeof(PPU_THREAD_REGISTERS));
    COMP->add(b->threadCtx, b->ppuState);   // since ppuThread[] base is at offset 0 we just need to add the offset in the array 
}

void PPU_JIT::setupProl(JITBlockBuilder* b, u64 addr)
{
    /*   if (enableHalt && (ppuHaltOn && ppuHaltOn == curThread.CIA) && !guestHalt) */

    COMP->nop();
    COMP->nop();

    x86::Gp temp = newGP64();
    Label continueLabel = COMP->newLabel();

    // enableHalt
    COMP->test(b->haltBool, b->haltBool);
    COMP->je(continueLabel);

    // ppuHaltOn != NULL
    COMP->mov(temp, x86::ptr(b->ppu, offsetof(PPU, ppuHaltOn)));
    COMP->test(temp, temp);
    COMP->je(continueLabel);

    // ppuHaltOn == curThread.CIA - !guestHalt
    COMP->cmp(temp, x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, CIA)));
    COMP->jne(continueLabel);
    COMP->cmp(x86::byte_ptr(b->ppu, offsetof(PPU, guestHalt)), 0);
    COMP->jne(continueLabel);

    // call HALT
    InvokeNode* out;
    COMP->invoke(&out, imm((void*)callHalt), FuncSignatureT<void>());

    COMP->bind(continueLabel);

    // update CIA NIA _instr (CI)
    COMP->mov(temp, x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, NIA)));
    COMP->mov(x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, CIA)), temp);
    COMP->add(temp, 4);
    COMP->mov(x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, NIA)), temp);
    COMP->mov(temp, b->opcodesDataCache[addr]);
    COMP->mov(x86::ptr(b->threadCtx, offsetof(PPU_THREAD_REGISTERS, CI.opcode)), temp);

    COMP->nop();
    COMP->nop();
}

using namespace asmjit;
JITBlock* PPU_JIT::BuildJITBlock(u64 addr, u64 maxBlockSize) {
  auto jitBuilder = std::make_unique<JITBlockBuilder>(addr, &jitRuntime);

  //
  // x86
  //
  asmjit::x86::Compiler compiler(jitBuilder->Code());
  jitBuilder->compiler = &compiler;

  // Setup function and, state / thread context
  jitBuilder->ppu = compiler.newGpz("ppu");
  jitBuilder->ppuState = compiler.newGpz("ppuState");
  jitBuilder->threadCtx = compiler.newGpz("thread");
  jitBuilder->haltBool = compiler.newGpb("enableHalt"); // bool

  FuncSignatureT<void, void*, void*, bool> signature;
  compiler.addFunc(signature);
  compiler.setArg(0, jitBuilder->ppu);
  compiler.setArg(1, jitBuilder->ppuState);
  compiler.setArg(2, jitBuilder->haltBool);
  setupContext(jitBuilder.get());


 

  //
  // Instruction emitters
  //
  u64 instrCount = 0;
  while (true) {
    u32 offset = instrCount * 4;
    u64 pc = addr + offset;

    // Fetch instruction
    curThread.CIA = pc;
    curThread.NIA = pc + 4;
    curThread.iFetch = true;
    PPCOpcode op{ PPCInterpreter::MMURead32(ppuState, pc) };
    curThread.iFetch = false;
    jitBuilder->opcodesDataCache.emplace(pc, op.opcode);

    // Decode and emit
    auto emitter = PPCInterpreter::ppcDecoder.decodeJIT(op.opcode);
    std::string opName = PPCInterpreter::ppcDecoder.decodeName(op.opcode);

    // Prol
    setupProl(jitBuilder.get(), pc);

    x86::Gp temp = compiler.newGpq();
    if (pc == 0x0200C870) {
        compiler.mov(temp, 0);
        compiler.mov(x86::ptr(jitBuilder->threadCtx, offsetof(PPU_THREAD_REGISTERS, GPR) + 5 * 8), temp);
    }

    // RGH 2 for CB_A 9188 in a JRunner Normal Build.
    if (pc == 0x0200C820) {
        //GPR(3) = 0;
    }

    // RGH 2 17489 in a JRunner Corona XDKBuild.
    if (pc == 0x0200C7F0) {
        compiler.mov(temp, 0);
        compiler.mov(x86::ptr(jitBuilder->threadCtx, offsetof(PPU_THREAD_REGISTERS, GPR) + 3 * 8), temp);
    }

    // Call emitter
    emitter(ppuState, jitBuilder.get(), op);
    if(ppu->currentExecMode == eExecutorMode::Hybrid && emitter == &PPCInterpreter::PPCInterpreterJIT_invalid)
    {
        auto intEmitter = PPCInterpreter::ppcDecoder.decode(op.opcode);

        InvokeNode* out;
        compiler.invoke(&out, imm((void*)intEmitter), FuncSignatureT<void, void*>());
        out->setArg(0, jitBuilder->ppuState);
    }

    // Epil (check for exc and interrupts)
    InvokeNode* tbCheck;
    x86::Gp retVal = compiler.newGpb();
    compiler.invoke(&tbCheck, imm((void*)callEpil), FuncSignatureT<bool, PPU*>());
    tbCheck->setArg(0, jitBuilder->ppu);
    tbCheck->setRet(0, retVal);  
    Label skipRet = compiler.newLabel();
    compiler.test(retVal, retVal);        
    compiler.je(skipRet);                
    compiler.ret();
    compiler.bind(skipRet);

    instrCount++;

    // If branch or block end
    if (opName == "bclr" || opName == "bcctr" || opName == "bc" || opName == "b" || opName == "rfid") {
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
  JITBlock* block = new JITBlock(&jitRuntime, addr, jitBuilder.get());
  if (!block->Build()) {
    delete block;
    return nullptr;
  }

  // Insert block into cache
  jitBlocks.emplace(addr, block);
  return block;
}

// TODO: Invalidate blocks
void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt) {
  u32 instrsExecuted = 0;
  while (instrsExecuted < numInstrs && active) {

    u64 addr = curThread.NIA;
    if (!isBlockCached(addr)) {
      JITBlock* block = BuildJITBlock(addr, numInstrs - instrsExecuted);
      if (!block)
        break; // Failed to build block, abort

      block->codePtr(ppu, ppuState, enableHalt);
      instrsExecuted += block->size / 4;
    }
    else {
      instrsExecuted += ExecuteJITBlock(addr, enableHalt);
    }
  }
}

bool PPU_JIT::isBlockCached(u64 addr) {
  return jitBlocks.find(addr) != jitBlocks.end();
}