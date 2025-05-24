// Copyright 2025 Xenon Emulator Project. All rights reserved.

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

class PPU;
using JITFunc = fptr<void(PPU*, PPU_STATE*, bool)>;

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

  asmjit::x86::Gp &Base() const {
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

class PPUPtr {
public:
  PPUPtr(asmjit::x86::Gp baseReg) :
    base(baseReg)
  {}
  PPUPtr() :
    PPUPtr(asmjit::x86::Gp{})
  {}
  u64 Offset() {
    return offset;
  }

  operator asmjit::x86::Gp() const {
    return base;
  }

  asmjit::x86::Gp &Base() {
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
    code.init(runtime->environment(), runtime->cpuFeatures());
  }
  ~JITBlockBuilder() {
    delete ppu;
    delete ppuState;
    delete threadCtx;
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
  PPUPtr *ppu = nullptr;
  // Context pointer
  ASMJitPtr<PPU_STATE> *ppuState = nullptr;
  // Current thread context
  ASMJitPtr<PPU_THREAD_REGISTERS> *threadCtx = nullptr;
  x86::Gp haltBool{}; // enableHalt flag
  x86::Compiler *compiler = nullptr; // asmjit Compiler
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
  u64 hash;
};

class PPU_JIT {
public:
  PPU_JIT(PPU *ppu);
  ~PPU_JIT();

  void ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt = true);
  u64 ExecuteJITBlock(u64 addr, bool enableHalt); // returns step count
  std::shared_ptr<JITBlock> BuildJITBlock(u64 addr, u64 maxBlockSize);
  void setupContext(JITBlockBuilder *b);
  void setupProl(JITBlockBuilder *b, u32 addr);
  void patchSkips(JITBlockBuilder *b, u32 addr);
private:
  bool isBlockCached(u64 addr);

  PPU *ppu = nullptr; // "Linked" PPU
  PPU_STATE *ppuState = nullptr; // For easier thread access
  asmjit::JitRuntime jitRuntime;
  std::unordered_map<u64, std::shared_ptr<JITBlock>> jitBlocks = {};
};