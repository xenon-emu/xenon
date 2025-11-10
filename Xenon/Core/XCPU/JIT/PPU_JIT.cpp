/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Logging/Log.h"
#include "Base/Global.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "Core/XCPU/JIT/x86_64/JITEmitter_Helpers.h"
#endif
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Core/XCPU/PPU/PPCInternal.h"
#include "Core/XCPU/XenonCPU.h"
#include "Core/XCPU/PPU/PPU.h"
#include "PPU_JIT.h"

//
//  Trampolines for Invoke
//
void callHalt() {
  return XeMain::GetCPU()->Halt();
}

// Constructor
PPU_JIT::PPU_JIT(PPU *ppu) :
  ppu(ppu),
  ppeState(ppu->ppeState.get()) {}

// Destructor
PPU_JIT::~PPU_JIT() {
  for (auto &[hash, block] : jitBlocksCache)
    block.reset();
}

// Gets current sPPUThread and uses ppeState to get the current Thread pointer.
void PPU_JIT::SetupContext(JITBlockBuilder *b) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  x86::Gp tempR = newGP32();
  COMP->movzx(tempR, b->ppeState->scalar(&sPPEState::currentThread).Ptr<u8>());
  COMP->imul(b->threadCtx->Base(), tempR, sizeof(sPPUThread));
  // Since ppuThread[] base is at offset 0 we just need to add the offset in the array
  COMP->add(b->threadCtx->Base(), b->ppeState->Base());
#endif
}

// JIT Instruction Prologue
// * Checks for HALT at current address and calls it if it's enabled and address is a match
// * Updates instruction pointers (PIA,CIA,NIA) and instruction data
void PPU_JIT::InstrPrologue(JITBlockBuilder *b, u32 instrData) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  x86::Gp temp = newGP64();
  Label continueLabel = COMP->newLabel();

  // enableHalt
  COMP->test(b->haltBool, b->haltBool);
  COMP->je(continueLabel);

  // ppuHaltOn != NULL
  COMP->mov(temp, b->ppu->scalar(&PPU::ppuHaltOn));
  COMP->test(temp, temp);
  COMP->je(continueLabel);

  // ppuHaltOn == curThread.NIA - !guestHalt
  COMP->cmp(temp, b->threadCtx->scalar(&sPPUThread::NIA));
  COMP->jne(continueLabel);
  COMP->cmp(b->ppu->scalar(&PPU::guestHalt).Ptr<u8>(), 0);
  COMP->jne(continueLabel);

  // Call HALT
  InvokeNode *out = nullptr;
  COMP->invoke(&out, imm((void*)callHalt), FuncSignature::build<void>());

  COMP->bind(continueLabel);

  // Update PIA, CIA, NIA and CI.
  // PIA = CIA:
  COMP->mov(temp, b->threadCtx->scalar(&sPPUThread::CIA));
  COMP->mov(b->threadCtx->scalar(&sPPUThread::PIA), temp);
  // CIA = NIA:
  COMP->mov(temp, b->threadCtx->scalar(&sPPUThread::NIA));
  COMP->mov(b->threadCtx->scalar(&sPPUThread::CIA), temp);
  // NIA +=4:
  COMP->add(temp, 4);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::NIA), temp);
  // CI data.
  COMP->mov(temp, instrData);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::CI).Ptr<u32>(), temp);
#endif
}

// Instruction Epilogue
// * Checks and advances TimeBase
// * Checks for external interrupts and exceptions.
bool InstrEpilogue(PPU *ppu, sPPEState *ppeState) {
  // Check timebase and update if enabled.
  ppu->CheckTimeBaseStatus();

  // Get current thread.
  auto &thread = ppeState->ppuThread[ppeState->currentThread];
  // Check for external interrupts.
  if (thread.SPR.MSR.EE && ppu->xenonContext->xenonIIC.checkExtInterrupt(thread.SPR.PIR)) {
    thread.exceptReg |= ppuExternalEx;
  }

  // Check if exceptions are pending and process them in order.
  return ppu->PPUCheckExceptions();
}

#undef GPR
using namespace asmjit;
// Builds a JIT block starting at the given address.
std::shared_ptr<JITBlock> PPU_JIT::BuildJITBlock(u64 blockStartAddress, u64 maxBlockSize) {
  std::unique_ptr<JITBlockBuilder> jitBuilder = std::make_unique<STRIP_UNIQUE(jitBuilder)>(blockStartAddress, &jitRuntime);

#if defined(ARCH_X86) || defined(ARCH_X86_64)
  asmjit::x86::Compiler compiler(jitBuilder->Code());
  jitBuilder->compiler = &compiler;

  // Setup function and, state / thread context
  jitBuilder->ppu = new ASMJitPtr<PPU>(compiler.newGpz("ppu"));
  jitBuilder->ppeState = new ASMJitPtr<sPPEState>(compiler.newGpz("ppeState"));
  jitBuilder->threadCtx = new ASMJitPtr<sPPUThread>(compiler.newGpz("thread"));
  jitBuilder->haltBool = compiler.newGpb("enableHalt"); // bool

  FuncNode *signature = nullptr;
  compiler.addFuncNode(&signature, FuncSignature::build<void, PPU *, sPPEState *, bool>());
  signature->setArg(0, jitBuilder->ppu->Base());
  signature->setArg(1, jitBuilder->ppeState->Base());
  signature->setArg(2, jitBuilder->haltBool);
#endif

  // Temporary container holding all instructions data in the block.
  std::vector<u32> instrsTemp{};

  // Setup our block context.
  SetupContext(jitBuilder.get());

  //
  // Instruction emitter
  //

  u64 instrCount = 0;

  while (XeRunning && !XePaused) {
    auto &thread = curThread;

    // Update previous instruction address
    thread.PIA = thread.CIA;
    // Update current instruction address
    thread.CIA = thread.NIA;
    // Increase next instruction address
    thread.NIA += 4;

    // Is the instruction data valid?
    bool instrDataValid = true;
    // Fetch Instruction data.
    thread.instrFetch = true;
    uPPCInstr op{ PPCInterpreter::MMURead32(ppeState, thread.CIA) };
    thread.instrFetch = false;

    // Check for Instruction storage/segment exceptions. If found we must end the block.
    if (curThread.exceptReg & ppuInstrStorageEx || curThread.exceptReg & ppuInstrSegmentEx) {
      LOG_DEBUG(Xenon, "[JIT]: Instruction exception when creating block at CIA {:#x}, block start address {:#x}, instruction count {:#x}", 
        thread.CIA, blockStartAddress, instrCount);

      if (instrCount != 0) {
        // We're a few instructions into the block, just end the block on the last instruction and start a new block on
        // the faulting instruction. It will process the exception accordingly.
        // We clear the exception condition or else the exception handler will run on the first instruction of last the 
        // compiled block.
        thread.exceptReg &= ~(ppuInstrStorageEx | ppuInstrSegmentEx);
        break;
      } else {
        // Manually process the pending exceptions.
        ppu->PPUCheckExceptions();
        // Return from block creation. Next block will be one the handlers for instruction exceptions.
        return nullptr;
      }
    }

    u32 opcode = op.opcode;
    instrsTemp.push_back(opcode);


    // Decode and emit

    // Saves a few cycles to cache the value here
    u32 decodedInstr = PPCDecode(opcode);
    auto emitter = PPCInterpreter::ppcDecoder.getJITTable()[decodedInstr];
    static thread_local std::unordered_map<u32, u32> opcodeHashCache;
    u32 opName = opcodeHashCache.contains(opcode) ? opcodeHashCache[opcode] : opcodeHashCache[opcode] =
      Base::JoaatStringHash(PPCInterpreter::ppcDecoder.getNameTable()[decodedInstr]);



    // Setup our instruction prologue.
    InstrPrologue(jitBuilder.get(), opcode);

    // Check for ocurred Instruction access exceptions.
    if (opcode == 0xFFFFFFFF || opcode == 0xCDCDCDCD || opcode == 0x00000000) {
      instrDataValid = false;
    }

    // Call JIT Emitter on fetched instruction id instruction data is valid.
    if (instrDataValid) {
      // First perform any patches for registers at runtime.
      // Used for codeflow skips, and value patching.
#if defined(ARCH_X86) || defined(ARCH_X86_64)
      auto patchGPR = [&](s32 reg, u64 val) {
        x86::Gp temp = compiler.newGpq();
        compiler.mov(temp, val);
        compiler.mov(jitBuilder->threadCtx->array(&sPPUThread::GPR).Ptr(reg), temp);
        };

      // Patches are done using the 32 bit Kernel/Games address space.
      switch (static_cast<u32>(thread.CIA)) {
      case 0x0200C870: patchGPR(5, 0); break;
        // RGH 2 17489 in a JRunner Corona XDKBuild
      case 0x0200C7F0: patchGPR(3, 0); break;
        // VdpWriteXDVOUllong. Set r10 to 1. Skips XDVO write loop
      case 0x800EF7C0: patchGPR(10, 1); break;
        // VdpSetDisplayTimingParameter. Set r11 to 0x10. Skips ANA Check
      case 0x800F6264: patchGPR(11, 0x15E); break;
        // Needed for FSB_FUNCTION_2
      case 0x1003598ULL: patchGPR(11, 0x0E); break;
      case 0x1003644ULL: patchGPR(11, 0x02); break;
        // Bootanim load skip
      case 0x80081EA4: patchGPR(3, 0x0); break;
        // VdRetrainEDRAM return 0
      case 0x800FC288: patchGPR(3, 0x0); break;
        // VdIsHSIOTrainingSucceeded return 1
      case 0x800F9130: patchGPR(3, 0x1); break;
        // Pretend ARGON hardware is present, to avoid the call
      case 0x800819E0:
      case 0x80081A60: {
        x86::Gp temp = compiler.newGpq();
        compiler.mov(temp, jitBuilder->threadCtx->array(&sPPUThread::GPR).Ptr(11));
        compiler.or_(temp, 0x08);
        compiler.mov(jitBuilder->threadCtx->array(&sPPUThread::GPR).Ptr(11), temp);
      } break;
      }
#endif

      bool invalidInstr = emitter == &PPCInterpreter::PPCInterpreterJIT_invalid;

      // If the instruction is invalid and we're in hybrid mode, call the interpreter decoder and function lookup.
      if (ppu->currentExecMode == eExecutorMode::Hybrid && invalidInstr) {
        auto function = PPCInterpreter::ppcDecoder.decode(opcode);

#if defined(ARCH_X86) || defined(ARCH_X86_64)
        InvokeNode *out = nullptr;
        compiler.invoke(&out, imm((void *)function), FuncSignature::build<void, void *>());
        out->setArg(0, jitBuilder->ppeState->Base());
#endif
      }
      else {
        // Execute decoded instruction.
        emitter(ppeState, jitBuilder.get(), op);
      }
    }

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    // Epilogue, update the time base and check/process pending exceptions.
    // We return execution if any exception is indeed found.
    InvokeNode *returnCheck = nullptr;
    x86::Gp retVal = compiler.newGpb();

    // Call our epilogue.
    compiler.invoke(&returnCheck, imm((void *)InstrEpilogue), FuncSignature::build<bool, PPU *, sPPEState *>());
    returnCheck->setArg(0, jitBuilder->ppu->Base());
    returnCheck->setArg(1, jitBuilder->ppeState->Base());
    returnCheck->setRet(0, retVal);

    // Test for ocurred exceptions and return if any.
    Label skipRet = compiler.newLabel();

    compiler.test(retVal, retVal);  // Check for a positive result.
    compiler.je(skipRet);           // Skip return if no exceptions.
    compiler.ret();                 // Return if exceptions ocurred.
    compiler.bind(skipRet);         // Skip return Tag.
#endif

    // Check if the last instruction was a branch or a jump (rfid). We must end the block if any is found or the block
    // is at the maximum available size.
    instrCount++;
    if (opName == "bclr"_j || opName == "bcctr"_j || opName == "bc"_j || opName == "b"_j || opName == "rfid"_j ||
      opName == "invalid"_j || instrCount >= maxBlockSize)
      break;
  }

  // Reset CIA and NIA.
  curThread.CIA = blockStartAddress - 4;
  curThread.NIA = blockStartAddress;

  // Set block size in bytes.
  jitBuilder->size = instrCount * 4;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
  // Block end.
  compiler.ret();
  compiler.endFunc();
  compiler.finalize();
#endif

  // Create the final JITBlock
  std::shared_ptr<JITBlock> block = std::make_shared<STRIP_UNIQUE(block)>(&jitRuntime, blockStartAddress, jitBuilder.get());
  if (!block->Build()) {
    block.reset();
    return nullptr; // Block build failed.
  }

  // Create block hash. Needs improvement.
  block->hash = 0;
  for (auto &instr : instrsTemp) {
    block->hash += instr;
  }

  // Insert block into the block cache.
  jitBlocksCache.emplace(blockStartAddress, std::move(block));
  return jitBlocksCache.at(blockStartAddress);
}
#define GPR(x) curThread.GPR[x]

// Executes a given JIT block at a designated address.
u64 PPU_JIT::ExecuteJITBlock(u64 blockStartAddress, bool enableHalt) {
  auto &block = jitBlocksCache.at(blockStartAddress);
  block->codePtr(ppu, ppeState, enableHalt);
  return block->size / 4;
}

// Execute a given number of instructions using JIT.
void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, bool enableHalt, bool singleBlock) {
  u32 instrsExecuted = 0;
  while (instrsExecuted < numInstrs && active && (XeRunning && !XePaused)) {
    auto &thread = curThread;

    // Quick way of skiping function calls:
    // This *must *be done here simply because of how we handle JIT.
    // We run until the start of a block, which is a branch opcode (or until it's a invalid instruction),
    // but these are branch opcodes, designed to avoid calling them.
    // So, these will break under BuildJITBlock, and to avoid the issue, it's done here
    bool skipBlock = false;
    switch (thread.NIA) {
      // INIT_POWER_MODE bypass 2.0.17489.0
    case 0x80081764:
      // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
      // older console revisions
    case 0x801AF580:
      // XamSetPowerMode call to KeSetPowerMode bypass.
    case 0x817AC968:
      skipBlock = true;
      break;
    default:
      break;
    }
    // Skip to next block if needed.
    if (skipBlock) { instrsExecuted++; thread.NIA += 4; }

    // Get next block start address.
    u64 blockStartAddress = thread.NIA;
    // Attempt to find such block in the block cache.
    auto it = jitBlocksCache.find(blockStartAddress);
    if (it == jitBlocksCache.end()) {
      // Block was not found. Attempt to create a new one.
      auto block = BuildJITBlock(blockStartAddress, numInstrs - instrsExecuted);
      if (!block) { continue; } // Block build attempt failed.

      // Execute our block and increse executed instructions.
      block->codePtr(ppu, ppeState, enableHalt);
      instrsExecuted += block->size / 4;

      // For Testing and debugging purposes only.
      if (singleBlock) { break; }
    } else {
      // We have a match, check for the block hash to see if it hasn't been modified.
      auto &block = it->second;
      u64 sum = 0;
      if (block->size % 8 == 0) {
        for (u64 i = 0; i < block->size / 8; i++) {
          thread.instrFetch = true;
          u64 val = PPCInterpreter::MMURead64(ppeState, block->ppuAddress + i * 8);
          thread.instrFetch = false;
          u64 top = val >> 32;
          u64 bottom = val & 0xFFFFFFFF;
          sum += top + bottom;
        }
      } else {
        for (u64 i = 0; i != block->size / 4; i++) {
          thread.instrFetch = true;
          sum += PPCInterpreter::MMURead32(ppeState, block->ppuAddress + i * 4);
          thread.instrFetch = false;
        }
      }

      if (block->hash != sum) {
#ifdef JIT_DEBUG
        LOG_DEBUG(Xenon, "[JIT]: Block hash mismatch for block at address {:#x}", blockStartAddress);
#endif // JIT_DEBUG
        // Blocks do not match. Erase it and retry.
        block.reset();
        jitBlocksCache.erase(blockStartAddress);
        continue;
      }

      // Run block as usual.
      instrsExecuted += ExecuteJITBlock(blockStartAddress, enableHalt);
    }
  }
}
