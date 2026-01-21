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
  std::lock_guard<std::mutex> lock(jitCacheMutex);
  for (auto &[hash, block] : jitBlocksCache)
    block.reset();
  jitBlocksCache.clear();
  pageBlockIndex.clear();
  blockPageList.clear();
}

void PPU_JIT::RegisterBlockPages(u64 blockStart, u64 blockSize) {
  constexpr u64 pageSize = 4096ULL;
  if (blockSize == 0) return;

  u64 pageCount = (blockSize + pageSize - 1) / pageSize;
  u64 firstPage = blockStart & ~(pageSize - 1ULL);

  std::lock_guard<std::mutex> lock(jitCacheMutex);
  std::vector<u64> pages;
  pages.reserve(static_cast<size_t>(pageCount));
  for (u64 i = 0; i < pageCount; ++i) {
    u64 pageBase = firstPage + i * pageSize;
    pageBlockIndex[pageBase].insert(blockStart);
    pages.push_back(pageBase);
  }
  blockPageList[blockStart] = std::move(pages);

#ifdef JIT_DEBUG
  LOG_DEBUG(Xenon, "[JIT]: Registered block {:#x} size {:#x} -> pages: {:#x}..{:#x}", blockStart, blockSize,
    firstPage, firstPage + pageCount * pageSize - 1);
#endif
}

void PPU_JIT::UnregisterBlock(u64 blockStart) {
  std::lock_guard<std::mutex> lock(jitCacheMutex);
  auto it = blockPageList.find(blockStart);
  if (it == blockPageList.end()) { return; }
  for (u64 pageBase : it->second) {
    auto pit = pageBlockIndex.find(pageBase);
    if (pit != pageBlockIndex.end()) {
      pit->second.erase(blockStart);
      if (pit->second.empty()) {
        pageBlockIndex.erase(pit);
      }
    }
  }
  blockPageList.erase(it);

#ifdef JIT_DEBUG
  LOG_DEBUG(Xenon, "[JIT]: Unregistered block {:#x} from page index", blockStart);
#endif
}

// Try to link a block to its target if the target block exists
void PPU_JIT::TryLinkBlock(JITBlock *block) {
  if (!block || !block->canLink || block->linkTargetAddr == 0) {
    return;
  }

  // Check if target block exists in cache
  auto it = jitBlocksCache.find(block->linkTargetAddr);
  if (it != jitBlocksCache.end() && it->second) {
    block->linkedBlock = it->second.get();
#ifdef JIT_DEBUG
    LOG_DEBUG(Xenon, "[JIT]: Linked block {:#x} -> {:#x}", block->ppuAddress, block->linkTargetAddr);
#endif
  }
}

// Unlink all blocks that link to a specific target address (called when invalidating)
void PPU_JIT::UnlinkBlocksTo(u64 targetAddr) {
  for (auto &[addr, block] : jitBlocksCache) {
    if (block && block->linkTargetAddr == targetAddr) {
      block->linkedBlock = nullptr;
#ifdef JIT_DEBUG
      LOG_DEBUG(Xenon, "[JIT]: Unlinked block {:#x} (target {:#x} invalidated)", addr, targetAddr);
#endif
    }
  }
}

void PPU_JIT::InvalidateBlocksForRange(u64 startAddr, u64 endAddr) {
  constexpr u64 pageSize = 4096ULL; // 4k is the minimum page size, can be easily increased to match p bit of tlbie/l.
  if (startAddr >= endAddr) return;

  u64 startPage = startAddr & ~(pageSize - 1ULL);
  u64 endPage = (endAddr + pageSize - 1) & ~(pageSize - 1ULL);

  std::vector<u64> blocksToInvalidate;
  {
    std::lock_guard<std::mutex> lock(jitCacheMutex);
    for (u64 page = startPage; page < endPage; page += pageSize) {
      auto pit = pageBlockIndex.find(page);
      if (pit == pageBlockIndex.end()) continue;
      for (u64 blk : pit->second) blocksToInvalidate.push_back(blk);
    }
  }

  std::sort(blocksToInvalidate.begin(), blocksToInvalidate.end());
  blocksToInvalidate.erase(std::unique(blocksToInvalidate.begin(), blocksToInvalidate.end()), blocksToInvalidate.end());

  if (blocksToInvalidate.empty()) {
#ifdef JIT_DEBUG
    LOG_DEBUG(Xenon, "[JIT]: No JIT blocks to invalidate for range {:#x}-{:#x}", startAddr, endAddr);
#endif
    return;
  }

  for (u64 blkAddr : blocksToInvalidate) {
    // First unlink any blocks that point to this one
    UnlinkBlocksTo(blkAddr);

    auto it = jitBlocksCache.find(blkAddr);
    if (it != jitBlocksCache.end()) {
#ifdef JIT_DEBUG
      LOG_DEBUG(Xenon, "[JIT]: Invalidating block at {:#x} due to page invalidation range {:#x}-{:#x}", blkAddr, startAddr, endAddr);
#endif
      // Release resources
      it->second.reset();
      jitBlocksCache.erase(it);
    }
    // Unregister from page index (will clean mappings)
    UnregisterBlock(blkAddr);
  }
}

void PPU_JIT::InvalidateBlockAt(u64 blockAddr) {
  InvalidateBlocksForRange(blockAddr, blockAddr + 1);
}

void PPU_JIT::InvalidateAllBlocks() {
  std::lock_guard<std::mutex> lock(jitCacheMutex);
#ifdef JIT_DEBUG
  LOG_DEBUG(Xenon, "[JIT]: Invalidating ALL JIT blocks");
#endif
  for (auto &p : jitBlocksCache) {
    p.second.reset();
  }
  jitBlocksCache.clear();
  pageBlockIndex.clear();
  blockPageList.clear();
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
// * Checks for external interrupts and exceptions.
bool InstrEpilogue(PPU *ppu, sPPEState *ppeState) {
  // Check if exceptions are pending and process them in order.
  return ppu->PPUCheckExceptions();
}

// Pre-computed instruction name hashes for fast comparison during block building
namespace JITOpcodeHashes {
  // Branch instructions that end blocks
  static constexpr u32 BCLR = "bclr"_j;
  static constexpr u32 BCCTR = "bcctr"_j;
  static constexpr u32 BC = "bc"_j;
  static constexpr u32 B = "b"_j;
  static constexpr u32 RFID = "rfid"_j;
  static constexpr u32 INVALID = "invalid"_j;
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

  // Enable AVX support
  signature->frame().setAvxEnabled();

#endif

  // Temporary container holding all instructions data in the block.
  // Pre-allocate to reduce reallocations during block building
  std::vector<u32> instrsTemp{};
  instrsTemp.reserve(maxBlockSize > 64 ? 64 : static_cast<size_t>(maxBlockSize));

  // Setup our block context.
  SetupContext(jitBuilder.get());

  //
  // Instruction emitter
  //

  u64 instrCount = 0;

  // Block linking info - track if this block ends with an unconditional branch
  bool blockCanLink = false;
  u64 blockLinkTarget = 0;

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
#ifdef JIT_DEBUG
      LOG_DEBUG(Xenon, "[JIT]: Instruction exception when creating block at CIA {:#x}, block start address {:#x}, instruction count {:#x}",
        thread.CIA, blockStartAddress, instrCount);
#endif
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
    auto emitter = PPCInterpreter::ppcDecoder.decodeJIT(opcode);

    // Compute instruction name hash - use direct computation instead of thread_local map
    // The hash is only needed for block termination check, so compute it efficiently
    u32 opName = Base::JoaatStringHash(PPCInterpreter::ppcDecoder.getNameTable()[decodedInstr]);

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
        // Set XAM Debug Output Level to Trace
      case 0x81743B20: patchGPR(10, 4); break;
      case 0x0200C870: patchGPR(5, 0); break;
        // CNicEmac::NicDoTimer trap, 17489
      //case 0x801086a8: patchGPR(10, 2); break;
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
        // SATA SSC Speed patch (until I can get proper code pages working in ODD)
      case 0x800C5B58: patchGPR(11, 0x3); break;
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

    // Check for block-ending instructions and determine if we can link
    bool isBlockEnd = false;
    if (opName == JITOpcodeHashes::B) {
      isBlockEnd = true;
      // Unconditional branch - can link if not a function call (LK=0)
      if (!op.lk) {
        s64 offset = EXTS(op.li, 24) << 2;
        if (op.aa) {
          // Absolute address
          blockLinkTarget = static_cast<u64>(offset);
        } else {
          // Relative address
          blockLinkTarget = thread.CIA + offset;
        }
        blockCanLink = true;
      }
    } else if (opName == JITOpcodeHashes::BCLR || opName == JITOpcodeHashes::BCCTR ||
      opName == JITOpcodeHashes::BC || opName == JITOpcodeHashes::RFID ||
      opName == JITOpcodeHashes::INVALID) {
      isBlockEnd = true;
      // These are conditional or indirect branches - cannot link
      blockCanLink = false;
    }

    if (isBlockEnd || instrCount >= maxBlockSize)
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

  // Create block hash
  u64 hash = 0;
  for (const auto &instr : instrsTemp) { hash += instr; }
  block->hash = hash;

  // Set up block linking info
  block->canLink = blockCanLink;
  block->linkTargetAddr = blockLinkTarget;
  block->linkedBlock = nullptr; // Will be linked later if target exists

  // Insert block into the block cache.
  {
    std::lock_guard<std::mutex> lock(jitCacheMutex);
    jitBlocksCache.emplace(blockStartAddress, block);

    // Try to link this block to its target
    if (blockCanLink && blockLinkTarget != 0) {
      TryLinkBlock(block.get());
    }

    // Check if any existing blocks want to link to this new block
    for (auto &[addr, existingBlock] : jitBlocksCache) {
      if (existingBlock && existingBlock->canLink &&
        existingBlock->linkTargetAddr == blockStartAddress &&
        existingBlock->linkedBlock == nullptr) {
        existingBlock->linkedBlock = block.get();
#ifdef JIT_DEBUG
        LOG_DEBUG(Xenon, "[JIT]: Linked existing block {:#x} -> {:#x}", addr, blockStartAddress);
#endif
      }
    }
  }

  // Register pages used by the block.
  RegisterBlockPages(blockStartAddress, block->size);

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
      // XDK 17.489.0 AudioChipCorder Device Detect bypass. This is not needed for
      // older console revisions
    case 0x801AF580:
      skipBlock = true;
      break;
    default:
      break;
    }

    // Skip to next block if needed.
    if (skipBlock) {
      instrsExecuted++;
      thread.NIA += 4;
    }

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
      if (singleBlock)
        break;
    } else {
      bool realMode = false;
      realMode = !thread.SPR.MSR.DR || !thread.SPR.MSR.IR;
      if (realMode) { // When in real mode TLB is disabled. Fallback to old approach.
        // We have a match, check for the block hash to see if it hasn't been modified.
        auto &block = it->second;
        u64 sum = 0;
        const u64 blockSize = block->size;
        const u64 blockAddr = block->ppuAddress;

        // Optimized hash verification - read 64-bits at a time when possible
        if (blockSize % 8 == 0) {
          const u64 count = blockSize / 8;
          for (u64 i = 0; i < count; i++) {
            thread.instrFetch = true;
            u64 val = PPCInterpreter::MMURead64(ppeState, blockAddr + i * 8);
            thread.instrFetch = false;
            sum += (val >> 32) + (val & 0xFFFFFFFF);
          }
        } else {
          const u64 count = blockSize / 4;
          for (u64 i = 0; i < count; i++) {
            thread.instrFetch = true;
            sum += PPCInterpreter::MMURead32(ppeState, blockAddr + i * 4);
            thread.instrFetch = false;
          }
        }

        if (block->hash != sum) {
#ifdef JIT_DEBUG
          LOG_DEBUG(Xenon, "[JIT]: Block hash mismatch for block at address {:#x}", blockStartAddress);
#endif // JIT_DEBUG
          // Blocks do not match. Erase it and retry.
          block.reset();
          // Clean up page index mapping
          UnregisterBlock(blockStartAddress);
          jitBlocksCache.erase(blockStartAddress);
          continue;
        }
      }

      // Run block as usual.
      JITBlock *currentBlock = it->second.get();
      currentBlock->codePtr(ppu, ppeState, enableHalt);
      instrsExecuted += currentBlock->size / 4;

      // Block linking optimization: follow linked blocks without returning to dispatcher
      // Only do this if we're not in single-block mode and have instructions remaining
      while (!singleBlock && currentBlock->linkedBlock != nullptr &&
        instrsExecuted < numInstrs && (XeRunning && !XePaused)) {
        // Verify that NIA matches the linked block's address
        // (exception handlers or interrupts may have changed NIA)
        if (thread.NIA != currentBlock->linkTargetAddr) {
          break;
        }

        // Check thread suspension
        if (ppeState->currentThread == 0 && !ppeState->SPR.CTRL.TE0) {
          break;
        }

        if (ppeState->currentThread == 1 && !ppeState->SPR.CTRL.TE1) {
          break;
        }

        // Execute linked block
        currentBlock = currentBlock->linkedBlock;
        currentBlock->codePtr(ppu, ppeState, enableHalt);
        instrsExecuted += currentBlock->size / 4;
      }

      // If the thread was suspended due to CTRL being written, we must end execution on said thread.
      if (ppeState->currentThread == 0 && !ppeState->SPR.CTRL.TE0) {
        break;
      }

      if (ppeState->currentThread == 1 && !ppeState->SPR.CTRL.TE1) {
        break;
      }
    }
  }
}
