/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "X86Backend.h"
#include "X86CodeGen.h"
#include "Core/XCPU/PPU/PPU.h"

namespace Xe::XCPU::JIT::IR {

  using namespace asmjit;

  bool X86Backend::Initialize(const CodeGenOptions& options) {
    // Initialise Holder
    mHolder.init(mRuntime.environment(), mRuntime.cpuFeatures());
    mHolder.setErrorHandler(&mErrorHandler);

    // Attach Holder to Compiler
    mHolder.attach(&mCompiler);

    // Attach string logger
    mHolder.setLogger(&mLogger);

    if (!mHolder.isInitialized() || !mCompiler.isInitialized())
      return false;

    return true;
  }

  void X86Backend::Shutdown() {

  }




  void X86Backend::SetupContext(X86EmitterContext* context) {
    x86::Gp tempR = mCompiler.newGpz(); // 32 bit
    mCompiler.movzx(tempR, context->ppeState->scalar(&sPPEState::currentThread).Ptr<u8>());
    mCompiler.imul(context->threadCtx->Base(), tempR, sizeof(sPPUThread));
    // Since ppuThread[] base is at offset 0 we just need to add the offset in the array
    mCompiler.add(context->threadCtx->Base(), context->ppeState->Base());
  }

  CodeBlock X86Backend::Compile(IRFunction* function) {
    X86EmitterContext emitContext;
   
   
    emitContext.ppu = std::make_unique<ASMJitPtr<PPU>>(mCompiler.newGpz("ppu"));
    emitContext.ppeState = std::make_unique<ASMJitPtr<sPPEState>>(mCompiler.newGpz("ppeState"));
    emitContext.threadCtx = std::make_unique<ASMJitPtr<sPPUThread>>(mCompiler.newGpz("thread"));

    // TODO:
    // we could use signature->ArgCount() for easier arg addition instead of raw magic numbers 0, 1, 2 ...
    // also the function signature should be set in another way cause if we change arguments we may forget
    // to update it here
    FuncNode* signature = nullptr;
    mCompiler.addFuncNode(&signature, FuncSignature::build<void, sPPEState*>());
    //signature->setArg(0, emitContext.ppu->Base());
    signature->setArg(0, emitContext.ppeState->Base());

    // Enable AVX support
    signature->frame().setAvxEnabled();

    SetupContext(&emitContext);

    // TODO: Handle multiple blocks code gen
    IRBasicBlock* bb = function->GetEntryBlock();


    // call all instruction emitters
    for (int i = 0; i < bb->GetInstructions().size(); i++) {
      IRInstruction* instr = bb->GetInstructions()[i].get();
      X86CodeEmitter emitter = dispatchCodeEmitter(instr->GetOpcode());
      if(emitter)
       emitter(this, instr, &emitContext);
    }


    // The function should at least return
    mCompiler.ret();

    // finalise code
    mCompiler.endFunc();
    mCompiler.finalize();

    // Test!!! dump assembly gen from string logger
    LOG_INFO(JIT, "\n{}", mLogger.data());

    void* fnPtr = nullptr;
    mRuntime.add(&fnPtr, &mHolder);
    
    CodeBlock codeBlock;
    codeBlock.codeAddress = function->GetAddress();
    codeBlock.codePtr = reinterpret_cast<decltype(codeBlock.codePtr)>(fnPtr);
    codeBlock.codeSize = mHolder.codeSize();

    // Reinit the code holder for the next function
    mHolder.reinit();

    // Clear logger
    mLogger.clear();

    return codeBlock;
  }

  void X86Backend::Release(CodeBlock& block) {
  }

  const char* X86Backend::GetName() const {
    return "X86 Backend";
  }

  bool X86Backend::IsSupported() const {
    return true;
  }

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
