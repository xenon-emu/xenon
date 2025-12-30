/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>

#include "Base/Types.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/JIT/IR/IRTypes.h"
#include "Core/XCPU/PPU/PowerPC.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "asmjit/x86.h"
#else
#include "asmjit/core.h"
#endif

//=============================================================================
// Code Generation Backend Interface
// ----------------------------------------------------------------------------
// Abstract interface for generating native code from IR.
// Implementations provide target-specific code generation (x86_64, ARM64, etc.)
//=============================================================================

namespace Xe::XCPU::JIT::IR {
  class CodeGenBackend;
  struct CodeBlock;

  using JITFunc = fptr<void(sPPEState*)>;
  
  // Compiled code block result
  struct CodeBlock {
    JITFunc codePtr = nullptr;  // Pointer to executable code
    u64 codeSize = 0;           // Size of generated code in bytes
    u64 codeAddress = 0;        // Original PPC address
  };

  // Backend compilation options
  struct CodeGenOptions {
    // x86_64 Specific Options
    bool enableAVX2 = false;    // AVX2 for FPU and VXU
    bool enableAVX512 = false;  // Future AVX512 support
  };

  // Holds Context for emission of current IRFunction
  struct EmitterContext {
  };

  //=============================================================================
  // CodeGenBackend - Abstract Backend Interface
  //=============================================================================

  class CodeGenBackend {
  public:
    virtual ~CodeGenBackend() = default;

    // Initialize the backend
    virtual bool Initialize(const CodeGenOptions &options = {}) = 0;

    // Shutdown and release resources
    virtual void Shutdown() = 0;

    // Compile an IR function to native code
    virtual CodeBlock Compile(IRFunction *function) = 0;

    // Release a previously compiled block
    virtual void Release(CodeBlock &block) = 0;

    // Get backend name for debugging
    virtual const char *GetName() const = 0;

    // Check if the backend supports the current platform
    virtual bool IsSupported() const = 0;
 
  protected:
    CodeGenOptions options;
  };

  //=============================================================================
  // Backend Factory
  //=============================================================================

  // Creates the appropriate backend for the current platform
  std::unique_ptr<CodeGenBackend> CreateCodeGenBackend();

} // namespace Xe::XCPU::JIT::IR