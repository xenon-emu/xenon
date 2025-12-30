/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/JIT/IR/Backend/CodeGenBackend.h"
#include "X86JITHelpers.h"

//=============================================================================
// X86_64 Backend code generator per PPU Core
// ----------------------------------------------------------------------------

namespace Xe::XCPU::JIT::IR {

    class PPU;
    class X86Backend;
    struct X86EmitterContext;

    using X86CodeEmitter = fptr<void(X86Backend*, IRInstruction*, X86EmitterContext*)>;

    struct X86EmitterContext : EmitterContext {
      // Current PPU
      std::unique_ptr<ASMJitPtr<PPU>> ppu;
      // Context pointer
      std::unique_ptr<ASMJitPtr<sPPEState>> ppeState;
      // Current thread context
      std::unique_ptr<ASMJitPtr<sPPUThread>> threadCtx;
      // Gp Regs map
      std::unordered_map<IRValue*, asmjit::x86::Gp> mVirtGpRegs;

      // Allocate a asmjit Gp register from instruction type
      asmjit::x86::Gp MakeGpOfType(X86Backend* backEnd, IRType type);

      asmjit::x86::Gp MapToGp(X86Backend* backEnd, IRValue* val) {
        auto it = mVirtGpRegs.find(val);
        if (it != mVirtGpRegs.end())
          return it->second;

        asmjit::x86::Gp reg = MakeGpOfType(backEnd, val->GetType());
        mVirtGpRegs.emplace(val, reg);
        return reg;
      }

    };


    class X86ErrorHandler : public asmjit::ErrorHandler {
    public:

      // TODO: expand the handler
      void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
        printf("AsmJit error: %s\n", message);
        LOG_ERROR(JIT, "!!ASMJIT THREW AN ERROR!! {}", message);
      }
    };

    class X86Backend : public CodeGenBackend {
    public:

        ~X86Backend() = default;

        // Initialize the backend
        bool Initialize(const CodeGenOptions& options = {}) override;

        // Shutdown and release resources
        void Shutdown() override;

        // Compile an IR function to native code
        CodeBlock Compile(IRFunction* function) override;

        // Release a previously compiled block
        void Release(CodeBlock& block) override;

        // Get backend name for debugging
        const char* GetName() const override;

        // Check if the backend supports the current platform
        bool IsSupported() const override;

        void SetupContext(X86EmitterContext*);

    public: 
      asmjit::CodeHolder mHolder;       // Holds current function code
      asmjit::x86::Compiler mCompiler;  // X86 Compiler interface
      asmjit::JitRuntime mRuntime;      // The Runtime
      X86ErrorHandler mErrorHandler;
      asmjit::StringLogger mLogger;
    };

} // namespace Xe::XCPU::JIT::IR