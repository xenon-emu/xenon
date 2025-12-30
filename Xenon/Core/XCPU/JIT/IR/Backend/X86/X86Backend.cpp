/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "X86Backend.h"
#include "X86CodeGen.h"
#include "Core/XCPU/PPU/PPU.h"

namespace Xe::XCPU::JIT::IR {

  using namespace asmjit;

  // Initialize the backend
  bool X86Backend::Initialize(const CodeGenOptions& options) {
    // Initialise Holder
    mHolder.init(mRuntime.environment(), mRuntime.cpuFeatures());
    mHolder.setErrorHandler(&mErrorHandler);

    // Attach Holder to Compiler
    mHolder.attach(&mCompiler);

    // Attach the logger
    mHolder.setLogger(&mLogger);

    // Get our CodeGen options
    codeGenOpts = options;

    if (!mHolder.isInitialized() || !mCompiler.isInitialized()) {
      LOG_ERROR(JIT, "[Backend]: Failed to initialize X86 Backend");
      return false;
    }

    LOG_INFO(JIT, "[Backend]: Initilized X86 Backend using asmjit");

    return true;
  }

  void X86Backend::Shutdown() {
    LOG_INFO(JIT, "[Backend]: Shuting down X86 Backend");
  }

  // Sets up the context for the current thread
  void X86Backend::SetupContext(X86EmitterContext* context) {
    x86::Gp tempR = mCompiler.newGpz(); // 32 bit
    mCompiler.movzx(tempR, context->ppeState->scalar(&sPPEState::currentThread).Ptr<u8>());
    mCompiler.imul(context->threadCtx->Base(), tempR, sizeof(sPPUThread));
    // Since ppuThread[] base is at offset 0 we just need to add the offset in the array
    mCompiler.add(context->threadCtx->Base(), context->ppeState->Base());
  }


  // Creates a new compiled code block from an IR Function
  CodeBlock X86Backend::Compile(IRFunction* function) {
    X86EmitterContext emitContext;
   
    // Setup PPU and thread contexts pointers
    emitContext.ppu = std::make_unique<ASMJitPtr<PPU>>(mCompiler.newGpz("ppu"));
    emitContext.ppeState = std::make_unique<ASMJitPtr<sPPEState>>(mCompiler.newGpz("ppeState"));
    emitContext.threadCtx = std::make_unique<ASMJitPtr<sPPUThread>>(mCompiler.newGpz("thread"));

    // TODO:
    // we could use signature->ArgCount() for easier arg addition instead of raw magic numbers 0, 1, 2 ...
    // also the function signature should be set in another way cause if we change arguments we may forget
    // to update it here
    FuncNode* signature = nullptr;
    mCompiler.addFuncNode(&signature, FuncSignature::build<void, sPPEState*>());
    signature->setArg(0, emitContext.ppeState->Base());

    // Enable AVX support if enabled
    if (codeGenOpts.enableAVX2) {
      signature->frame().setAvxEnabled();
    }

    // Setup thread context using x86 instrs.
    SetupContext(&emitContext);

    // TODO: Handle multiple blocks code gen, as some functions may have several blocks inside of them.
    IRBasicBlock* bb = function->GetEntryBlock();

    // Call all instruction emitters in the current block.
    for (int i = 0; i < bb->GetInstructions().size(); i++) {
      // Get instruction.
      IRInstruction* instr = bb->GetInstructions()[i].get();
      // Create the corresponding emitter.
      X86CodeEmitter emitter = dispatchCodeEmitter(instr->GetOpcode());
      // Emit.
      if (emitter) { emitter(this, instr, &emitContext); }
    }

    // We must handle this differently when more than one block is found or in traps/etc.
    // The function should at least return.
    mCompiler.ret();

    // Finalize code.
    mCompiler.endFunc();
    mCompiler.finalize();

    // Dump generated assembly if enabled.
    if (codeGenOpts.enableAssemblyPrint) {
      LOG_INFO(JIT, "Dumping generated x86 assembly");
      LOG_INFO(JIT, "==============================");
      LOG_INFO(JIT, "{}", mLogger.data());
      LOG_INFO(JIT, "==============================");
    }

    // Initialize the code block with the generated function.
    void* fnPtr = nullptr;
    mRuntime.add(&fnPtr, &mHolder);
    
    CodeBlock codeBlock;
    codeBlock.codeAddress = function->GetAddress(); // PPC address this function is located at (may be Virtual or Physical).
    codeBlock.codePtr = reinterpret_cast<decltype(codeBlock.codePtr)>(fnPtr);
    codeBlock.codeSize = mHolder.codeSize(); // Size of the generated code in bytes.

    // Reinit the code holder for the next function.
    mHolder.reinit();

    // Clear logger
    mLogger.clear();

    // Return generated code block.
    return codeBlock;
  }

  // TODO: Release all allocated data and code.
  void X86Backend::Release(CodeBlock& block) {
  }

  // Backend name
  const char* X86Backend::GetName() const {
    return "X86 Backend using ASMJIT";
  }

  bool X86Backend::IsSupported() const {
    return true;
  }

  // Helper for creating a new GP reg of the specified IRType
  asmjit::x86::Gp X86EmitterContext::MakeGpOfType(X86Backend* backEnd, IRType type) {
    switch (type) {
    case IRType::INT8: return newGP8();
    case IRType::INT16: return newGP16();
    case IRType::INT32: return newGP32();
    case IRType::INT64: return newGP64();
    case IRType::Ptr: return newGPptr();
    default:
      LOG_CRITICAL(JIT, "TYPE NOT SUPPORTED AS GP: {}", IRTypeToString(type));
    }
  }
}
