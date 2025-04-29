// Copyright 2025 Xenon Emulator Project

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "asmjit/asmjit.h"

#include "Base/Arch.h"
#include "Base/Types.h"

#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/RootBus/RootBus.h"

using JITFunc = fptr<void(PPU_STATE*, PPU_THREAD_REGISTERS*)>;

using namespace asmjit;
class JITBlockBuilder {
public:
  JITBlockBuilder(u64 addr, asmjit::JitRuntime *rt) :
    ppuAddr(addr), runtime(rt)
  {
    code.init(runtime->environment(), runtime->cpuFeatures());
  }
  ~JITBlockBuilder() {
    ppuAddr = 0;
    size = 0;
  }
  u64 ppuAddr = 0; // Start Instruction Address
  u64 size = 0;   // PPC code size in bytes

  asmjit::CodeHolder* Code() {
    return &code;
  }
#ifdef ARCH_X86_64
  // x86_64
  x86::Gp ppuAddrPointer{}; // Context pointer (PPU_STATE*)
  x86::Compiler *compiler = nullptr; // asmjit Compiler
  x86::Gp threadCtx{}; // Current thread context (PPU_THREAD_REGISTERS*)
#endif
private:
  asmjit::CodeHolder code{};
  asmjit::JitRuntime* runtime{};
};

class JITBlock {
public:  
  JITBlock(asmjit::JitRuntime *rt, u64 ppuAddr, JITBlockBuilder *builder) :
    runtime(rt), ppuAddress(ppuAddr), builder(builder), size(builder->size)
  {
    isDirty = false;
  }
  ~JITBlock() {
    // Release (delete) the code pointer allocated by asmjit
    if (codePtr) {
      runtime->release(codePtr);
    }
  }

  bool Build() {
    void *fnPtr = nullptr;
    asmjit::CodeHolder *code = builder->Code();
    runtime->add(&fnPtr, code);
    codePtr = reinterpret_cast<decltype(codePtr)>(fnPtr);
    codeSize = code->codeSize();
    return true;
  }

  JITBlockBuilder *builder;    // JIT block builder
  JITFunc codePtr = nullptr;   // Pointer to compiled assembly code
  u64 codeSize = 0;            // Size of the compiled code
  u64 ppuAddress = 0;          // Address of the PPC block
  u64 size = 0;                // PPC code size in bytes
  bool isDirty;                // Tracks validation of the block, if Dirty the block is discarded and recompiled
  asmjit::JitRuntime *runtime; // Reference to JIT runtime
};

class PPU_JIT {
public:
  PPU_JIT(PPU_STATE *ppuState);
  ~PPU_JIT();

  void ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt = true);
  u64 ExecuteJITBlock(u64 addr); // returns step count
  void* CodeGenBlock(JITBlockBuilder *block);
  JITBlock* BuildJITBlock(u64 addr, u64 maxBlockSize);
private:
  bool isBlockCached(u64 addr);

  PPU_STATE *ppuState = nullptr;
  asmjit::JitRuntime jitRuntime;
  std::unordered_map<u64, JITBlock*> jitBlocks = {};
};