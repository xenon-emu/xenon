/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Base/Arch.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "asmjit/x86.h"
#else
#include "asmjit/core.h"
#endif

#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/RootBus/RootBus.h"

//#define JIT_DEBUG

class PPU;
using JITFunc = fptr<void(PPU*, sPPEState*, bool)>;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
template <typename T, typename fT>
class ArrayFieldProxy {
public:
  ArrayFieldProxy(const asmjit::x86::Gp &base, u64 offset = 0) :
    base(base), offset(offset)
  {}
  ArrayFieldProxy(const ArrayFieldProxy &other)
    : base(other.base), offset(other.offset)
  {}

  asmjit::x86::Mem operator[](u64 index) const {
    using eT = typename std::remove_extent<fT>::type;
    return asmjit::x86::ptr(base, offset + index * sizeof(eT));
  }

  asmjit::x86::Mem Ptr(u64 index) {
    using eT = typename std::remove_extent<fT>::type;
    return asmjit::x86::ptr(base, offset + index * sizeof(eT));
  }

  asmjit::x86::Gp Base() const {
    return base;
  }

  u64 Offset() const {
    return offset;
  }
private:
  asmjit::x86::Gp base;
  u64 offset = 0;
};

template <typename T, typename fT>
class ScalarFieldProxy {
public:
  ScalarFieldProxy(const asmjit::x86::Gp &base, u64 offset)
    : base(base), offset(offset)
  {}
  ScalarFieldProxy(const ScalarFieldProxy &other)
    : base(other.base), offset(other.offset)
  {}

  operator asmjit::x86::Mem() const {
    return asmjit::x86::ptr(base, offset);
  }

  template <typename pT = u8>
  asmjit::x86::Mem Ptr(u64 size = sizeof(pT)) const {
    return asmjit::x86::ptr(base, offset, size);
  }

  asmjit::x86::Gp Base() const {
    return base;
  }

  u64 Offset() const {
    return offset;
  }
private:
  asmjit::x86::Gp base;
  u64 offset;
};

template <typename T>
class ASMJitPtr {
public:
  ASMJitPtr(const asmjit::x86::Gp &baseReg, u64 offset = 0) :
    base(baseReg), offset(offset)
  {}

  template<typename fT>
  ScalarFieldProxy<T, fT> scalar(fT T::*member) const {
    u64 off = reinterpret_cast<u64>(&(reinterpret_cast<T*>(0)->*member));
    return ScalarFieldProxy<T, fT>(base, offset + off);
  }

  template<typename fT>
  ArrayFieldProxy<T, fT> array(fT T::*member) const {
    u64 off = reinterpret_cast<u64>(&(reinterpret_cast<T*>(0)->*member));
    return ArrayFieldProxy<T, fT>(base, offset + off);
  }

  template<typename sT>
  ASMJitPtr<sT> substruct(sT T::*member) const {
    u64 off = reinterpret_cast<u64>(&(reinterpret_cast<T*>(0)->*member));
    return ASMJitPtr<sT>(base, offset + off);
  }

  template <typename pT = u8>
  asmjit::x86::Mem Ptr(u64 size = sizeof(pT)) const {
    return asmjit::x86::ptr(base, offset, size);
  }

  operator asmjit::x86::Gp() const {
    return base;
  }

  u64 Offset() {
    return offset;
  }

  asmjit::x86::Gp Base() const {
    return base;
  }
private:
  asmjit::x86::Gp base;
  u64 offset = 0;
};
#endif

using namespace asmjit;
class JITBlockBuilder {
public:
  JITBlockBuilder(u64 addr, asmjit::JitRuntime *rt) :
    ppuAddr(addr), runtime(rt)
  {
    code.init(runtime->environment(), runtime->cpu_features());
  }
  ~JITBlockBuilder() {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
    delete ppu;
    delete ppeState;
    delete threadCtx;
#endif
    ppuAddr = 0;
    size = 0;
  }
  u64 ppuAddr = 0; // Start Instruction Address
  u64 size = 0;   // PPC code size in bytes
  std::unordered_map<u64, u32> opcodesDataCache = {};

  asmjit::CodeHolder* Code() {
    return &code;
  }
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  // x86_64
  // Current PPU
  ASMJitPtr<PPU> *ppu = nullptr;
  // Context pointer
  ASMJitPtr<sPPEState> *ppeState = nullptr;
  // Current thread context
  ASMJitPtr<sPPUThread> *threadCtx = nullptr;
  // EnableHalt flag
  x86::Gp haltBool{};
  // asmjit Compiler
  x86::Compiler *compiler = nullptr;
#endif
private:
  asmjit::CodeHolder code{};
  asmjit::JitRuntime *runtime = nullptr;
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
    codeSize = code->code_size();
    return true;
  }

  // JIT block builder
  JITBlockBuilder *builder;
  // Pointer to compiled assembly code
  JITFunc codePtr = nullptr;
  // Size of the compiled code
  u64 codeSize = 0;
  // Address of the PPC block
  u64 ppuAddress = 0;
  // PPC code size in bytes
  u64 size = 0;
  // Tracks validation of the block, if Dirty the block is discarded and recompiled
  bool isDirty = false;
  // Reference to JIT runtime
  asmjit::JitRuntime *runtime = nullptr;
  // Hash of all opcodes
  u64 hash = 0;
};

class PPU_JIT {
public:
  PPU_JIT(PPU *ppu);
  ~PPU_JIT();

  void ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt = true, bool singleBlock = false);
  u64 ExecuteJITBlock(u64 blockStartAddress, bool enableHalt); // returns step count
  std::shared_ptr<JITBlock> BuildJITBlock(u64 blockStartAddress, u64 maxBlockSize);
  void SetupContext(JITBlockBuilder *b);
  void SetupPrologue(JITBlockBuilder *b, u32 instrData, u32 decoded);
private:
  PPU *ppu = nullptr; // "Linked" PPU
  sPPEState *ppeState = nullptr; // For easier thread access
  asmjit::JitRuntime jitRuntime;
  // Block Cache, contains all created and valid JIT'ed blocks.
  std::unordered_map<u64, std::shared_ptr<JITBlock>> jitBlocksCache = {};
};