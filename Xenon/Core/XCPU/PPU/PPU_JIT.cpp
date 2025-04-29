// Copyright 2025 Xenon Emulator Project

#include "Base/Logging/Log.h"

#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/Interpreter/PPCInternal.h"

#include "PPU_JIT.h"

PPU_JIT::PPU_JIT(PPU_STATE *ppuState) :
  ppuState(ppuState)
{}

PPU_JIT::~PPU_JIT() {
  for (auto &block : jitBlocks)
    delete block.second;
}

u64 PPU_JIT::ExecuteJITBlock(u64 addr) {
  if (isBlockCached(addr)) {
    auto* entry = jitBlocks.at(addr);
    entry->codePtr(ppuState, &curThread);
    return entry->size / 4;
  }
  return 0;
}

using namespace asmjit;
JITBlock* PPU_JIT::BuildJITBlock(u64 addr, u64 maxBlockSize) {
  auto jitBuilder = std::make_unique<JITBlockBuilder>(addr, &jitRuntime);

  //
  // x86
  //
  asmjit::x86::Compiler compiler(jitBuilder->Code());
  jitBuilder->compiler = &compiler;

  FuncSignatureT<void, void*, void*> signature;
  compiler.addFunc(signature);

  jitBuilder->ppuAddrPointer = compiler.newGpz("ppu"); // rdi
  jitBuilder->threadCtx = compiler.newGpz("ctx"); // rsi
  compiler.setArg(0, jitBuilder->ppuAddrPointer);
  compiler.setArg(1, jitBuilder->threadCtx);

  //
  // Instruction emitters
  //
  u64 instrCount = 0;
  while (true) {
    u32 offset = instrCount * 4;
    u64 pc = addr + offset;

    // Fetch instruction
    curThread.iFetch = true;
    PPCOpcode op{ PPCInterpreter::MMURead32(ppuState, pc) };
    curThread.iFetch = false;

    // Decode and emit
    auto emitter = PPCInterpreter::ppcDecoder.decodeJIT(op.opcode);
    std::string opName = PPCInterpreter::ppcDecoder.decodeName(op.opcode);

    // Call emitter
    curThread.CIA = pc;
    curThread.NIA = pc + 4;
    emitter(ppuState, jitBuilder.get(), op);

    instrCount++;

    // If branch or block end
    if (opName == "bctr" || opName == "bcctr" || opName == "bc" || opName == "b") {
      break;
    }

    if (instrCount >= maxBlockSize) {
      break;
    }
  }

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
    curThread.CIA = curThread.NIA;
    u64 addr = curThread.CIA;
    if (!isBlockCached(addr)) {
      JITBlock* block = BuildJITBlock(addr, numInstrs - instrsExecuted);
      if (!block)
        break; // Failed to build block, abort

      block->codePtr(ppuState, &curThread);
      instrsExecuted += block->size / 4;
    }
    else {
      instrsExecuted += ExecuteJITBlock(addr);
    }
  }
}

bool PPU_JIT::isBlockCached(u64 addr) {
  return jitBlocks.find(addr) != jitBlocks.end();
}