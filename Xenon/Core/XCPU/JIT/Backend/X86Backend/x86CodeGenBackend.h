/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Arch.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

#include <array>
#include <mutex>

#include "asmjit/x86.h"

#include "Core/XCPU/JIT/PPU_JIT.h"
#include "Core/XCPU/JIT/HIR/Instr.h"
#include "Core/XCPU/JIT/HIR/Opcodes.h"
#include "Core/XCPU/JIT/Backend/CodeGenBackend.h"

namespace Xe::XCPU::JIT {

// Forward Declarations
class x86CodeGenBackend;

// Per-opcode emitter signature.
// Receives the backend (for compiler/context access) and the HIR instruction.
using x86HIREmitFn = void(*)(x86CodeGenBackend *backend, const HIR::Instr *instr);

// Keyed emitter entry used to populate the dispatch table at init time.
// Adding a new opcode handler is a single REGISTER_EMITTER() line.
struct x86EmitterKey {
  HIR::Opcode   opcode;
  x86HIREmitFn  handler;
};

// x86 CodeGen Backend
class x86CodeGenBackend final : public CodeGenBackend {
public:
  // |runtimeMutex| protects the shared JitRuntime across per-thread backend instances.
  explicit x86CodeGenBackend(asmjit::JitRuntime *runtime, std::mutex *runtimeMutex = nullptr);
  ~x86CodeGenBackend() override = default;

  bool Initialize() override;
  bool EmitBlock(HIR::HIRBlock *block, void **outCode, u64 *outCodeSize,
                 ePPUThreadID threadId = ePPUThread_Zero) override;
  void ReleaseCode(void *codePtr) override;
  void Reset() override;

  // Enables debug features
  void EnableGeneratedCodeDebugging() {
    printGeneratedCode = true;
    // Set Logging type on asmjit logger.
    asmLogger.set_flags(asmjit::FormatFlags::kMachineCode);
  }

  // Accessors for per-opcode emitters
  asmjit::x86::Compiler *GetCompiler() const { return compiler; }

  // Typed context pointer
  ASMJitPtr<sPPUThread> *GetThreadContext() const { return ppuThreadCtx; }

  // Typed PPE state pointer (second function argument, for interpreter fallback)
  ASMJitPtr<sPPEState> *ppeState = nullptr;

private:
  // Flat dispatch table indexed by HIR::Opcode enum value.
  // Populated from the keyed emitter list at Initialize() time.
  std::array<x86HIREmitFn, HIR::__OPCODE_MAX_VALUE> emittersTable{};

  // Builds emittersTable from the static key list.
  void BuildEmitTable();

  // Decode and dispatch a single HIR instruction.
  void EmitInstruction(const HIR::Instr *instr);

  // JIT runtime (shared across per-thread backends)
  asmjit::JitRuntime *jitRuntime = nullptr;
  std::mutex *jitRuntimeMutex = nullptr;

  // Transient per-block state
  asmjit::CodeHolder codeHolder{};
  asmjit::x86::Compiler *compiler = nullptr;

  // Pointer to the sPPUThread context struct
  ASMJitPtr<sPPUThread> *ppuThreadCtx = nullptr;

  // Debug utilities
  bool printGeneratedCode = false;

  // ASMJIT logger for debug output of generated code
  asmjit::StringLogger asmLogger;
};

// 
// Emitter registration helpers
// Each x86 emitter .cpp declares handlers and registers them via the
// REGISTER_EMITTER macro, which appends to the global key list that
// BuildEmitTable() consumes.
//

// Returns a mutable reference to the global emitter key vector.
// Filled at static-init time before Initialize() runs.
std::vector<x86EmitterKey> &GetEmitterKeyList();

// Helper that appends one key at static-init time.
struct x86EmitterRegister {
  x86EmitterRegister(HIR::Opcode op, x86HIREmitFn fn) {
    GetEmitterKeyList().push_back({ op, fn });
  }
};

// Usage in any .cpp:
//   REGISTER_EMITTER(OPCODE_ADD, Emit_ADD);
// This both forward-declares and registers the handler.
#define REGISTER_EMITTER(opcode, fn)                                   \
  static void fn(x86CodeGenBackend *backend, const HIR::Instr *instr); \
  static x86EmitterRegister _reg_##opcode(HIR::opcode, &fn);

} // namespace Xe::XCPU::JIT

#endif // ARCH_X86 || ARCH_X86_64
