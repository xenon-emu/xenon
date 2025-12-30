/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/JIT/IR/IRPrinter.h"
#include "Core/XCPU/JIT/IR/PPCTranslator.h"
#include "IR/Backend/CodeGenBackend.h"

/*
SSA PROPERTIES:
===============

The translator maintains SSA (Static Single Assignment) form:
- Each value is assigned exactly once
- Use-def chains are automatically maintained
- Register values are tracked per-instruction
- PHI nodes will be inserted at join points (future work)


OPTIMIZATION OPPORTUNITIES:
===========================

The IR enables various optimizations:
1. Constant propagation: r3 = 10, r4 = 20 could be folded
2. Constant folding: add 10, 20 -> 30
3. Dead code elimination: Unused loads/stores
4. Common subexpression elimination
5. Loop optimizations
6. Register allocation for native code generation


TRANSLATION PIPELINE:
=====================

PPC Binary -> Decoder -> IR Builder -> Optimizer -> Code Generator -> Native Code

NEXT STEPS:
===========
1. Add PHI node insertion for proper SSA at join points
2. Implement optimization passes
3. Create backend for x86_64/ARM64 code generation
*/

namespace Xe::XCPU::JIT {

  // Example function showing how to translate and print IR
  inline void TranslateAndPrintExample(sPPEState *ppeState, u64 address) {
    PPCTranslator translator;

    // Translate the block
    auto irFunction = translator.TranslateBlock(ppeState, address, 50);

    if (irFunction) {
      // Print the IR
      std::string irText = IRPrinter::PrintFunction(irFunction.get());

      // Log or print it
      LOG_INFO(JIT, "Generated IR:\n{}", irText);
    } else {
       LOG_ERROR(JIT, "Failed to translate block at {:#x}", address);
    }

    
    // Compile and execute our IR inside the backend.
    if (irFunction) {
      std::unique_ptr<IR::CodeGenBackend> bck = IR::CreateCodeGenBackend();

      // Setup our code gen options
      // TODO: We must first check if the host supports AVX/AVX512/etc...
      IR::CodeGenOptions opts = { 0 };
      opts.enableAssemblyPrint = true; // This is the basic example
      opts.enableAVX2 = true;

      if (!bck->Initialize(opts)) {
        LOG_ERROR(JIT, "Failer to Initialise BackEnd!");
        return;
      }
      // Compile block
      IR::CodeBlock block = bck->Compile(irFunction.get());

      // Run Jitted code
      if (block.codePtr)
        block.codePtr(ppeState);
    }
  }

} // namespace Xe::XCPU::JIT
