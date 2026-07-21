/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/JIT/HIR/HIRBuilder.h"
#include "Core/XCPU/JIT/Compiler/Compiler.h"
#include "Core/XCPU/JIT/Backend/CodeGenBackend.h"
#include "Core/XCPU/JIT/HIR/HIREmitters/HIRDecoder.h"
#include "Core/HLE/HLEFunction.h"

// Forward declaration to avoid circular include
struct HIRBlockMetadata;
class PPU;

namespace Xe::XCPU::JIT {
// PowerPC translator:
// Translates a block of guest code.
// PPC -> HIR -> Optimized HIR -> backend code emission
class PPCTranslator {
public:
  // |ppu| is required for exception processing during translation.
  // |backend| is optional. When provided the full-pipeline TranslateBlock
  // overload becomes available. The translator does NOT own the backend.
  PPCTranslator(PPU *ppu, sPPEState *inPPEState, CodeGenBackend *backend = nullptr);
  ~PPCTranslator() {};

  // Full pipeline: PPC -> HIR -> Backend native code.
  // Takes a ppeState pointer so the same translator instance can be
  // reused across different states / threads.
  // Returns a pointer to compiled code ready to be associated to a
  // JITBlock, or nullptr on failure. Caller owns the pointer and must
  // release it via backend->ReleaseCode().
  // |outCodeSize| receives the compiled code size (optional).
  // |outMeta| receives block metadata (instruction count, hash, link info).
  // |threadId| is passed through to the backend for thread context derivation.
  void *TranslateBlock(u64 blockStartAddress, sPPEState *inPPEState,
                       u64 maxInstrs = 64, u64 *outCodeSize = nullptr,
                       HIRBlockMetadata *outMeta = nullptr,
                       ePPUThreadID threadId = ePPUThread_Zero);

  // Access the builder for optimization passes or backend emission.
  HIR::HIRBuilder &GetBuilder() { return builder; }

  // Reset the translator for reuse (arena + builder reset).
  void Reset();

  // Dumps the generated HIR to the log output.
  void DumpHIR() const;

  // Sets the HLE function table pointer from XenonContext.
  void SetHLEFunctionTable(const std::vector<Core::HLE::sHLEFunction> *table) {
    hleFunctionTable = table;
  }

private:
  // Returns true if the given raw instruction word is a block-ending instruction.
  static bool IsBlockEndingInstruction(u32 instrData);
  const Core::HLE::sHLEFunction *FindHLEFunction(u64 guestAddress) const;
  PPU *ppu;
  sPPEState *ppeState;
  CodeGenBackend *backend;
  HIR::HIRBuilder builder;
  HIR::HIRDecoder decoder;
  std::unique_ptr<Compiler::Compiler> compiler_;

  // HLE function table: pointer to XenonContext's vector
  const std::vector<Core::HLE::sHLEFunction> *hleFunctionTable = nullptr;

  u32 opcodeCounts[HIR::Opcode::__OPCODE_MAX_VALUE] = { 0 };
};

} // namespace Xe::XCPU::JIT
