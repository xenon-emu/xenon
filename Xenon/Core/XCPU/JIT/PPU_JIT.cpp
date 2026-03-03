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
#include "Core/XeMain.h"
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
  for (u8 t = 0; t < 2; t++) {
    auto &cache = threadCaches[t];
    std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
    for (auto &[hash, block] : cache.jitBlocksCache)
      block.reset();
    cache.jitBlocksCache.clear();
    cache.pageBlockIndex.clear();
    cache.blockPageList.clear();
  }
}

void PPU_JIT::RegisterBlockPages(u64 blockStart, u64 blockSize, ePPUThreadID threadId) {
  constexpr u64 pageSize = 4096ULL;
  if (blockSize == 0) return;

  u64 pageCount = (blockSize + pageSize - 1) / pageSize;
  u64 firstPage = blockStart & ~(pageSize - 1ULL);

  auto &cache = threadCaches[static_cast<u8>(threadId)];
  std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
  std::vector<u64> pages;
  pages.reserve(static_cast<size_t>(pageCount));
  for (u64 i = 0; i < pageCount; ++i) {
    u64 pageBase = firstPage + i * pageSize;
    cache.pageBlockIndex[pageBase].insert(blockStart);
    pages.push_back(pageBase);
  }
  cache.blockPageList[blockStart] = std::move(pages);

#ifdef JIT_DEBUG
  LOG_DEBUG(Xenon, "[JIT]: Registered block {:#x} size {:#x} -> pages: {:#x}..{:#x} (thread {})", blockStart, blockSize,
    firstPage, firstPage + pageCount * pageSize - 1, static_cast<u8>(threadId));
#endif
}

void PPU_JIT::UnregisterBlock(u64 blockStart, ePPUThreadID threadId) {
  auto &cache = threadCaches[static_cast<u8>(threadId)];
  std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
  UnregisterBlockLocked(blockStart, cache);
}

void PPU_JIT::UnregisterBlockLocked(u64 blockStart, ThreadJITCache &cache) {
  auto it = cache.blockPageList.find(blockStart);
  if (it == cache.blockPageList.end()) { return; }
  for (u64 pageBase : it->second) {
    auto pit = cache.pageBlockIndex.find(pageBase);
    if (pit != cache.pageBlockIndex.end()) {
      pit->second.erase(blockStart);
      if (pit->second.empty()) {
        cache.pageBlockIndex.erase(pit);
      }
    }
  }
  cache.blockPageList.erase(it);

#ifdef JIT_DEBUG
  LOG_DEBUG(Xenon, "[JIT]: Unregistered block {:#x} from page index", blockStart);
#endif
}

// Try to link a block to its target if the target block exists
void PPU_JIT::TryLinkBlock(JITBlock *block, ePPUThreadID threadId) {
  if (!block || !block->canLink || block->linkTargetAddr == 0) {
    return;
  }

  auto &cache = threadCaches[static_cast<u8>(threadId)];
  // Check if target block exists in cache
  auto it = cache.jitBlocksCache.find(block->linkTargetAddr);
  if (it != cache.jitBlocksCache.end() && it->second) {
    block->linkedBlock.store(it->second.get(), std::memory_order_release);
#ifdef JIT_DEBUG
    LOG_DEBUG(Xenon, "[JIT]: Linked block {:#x} -> {:#x} (thread {})", block->ppuAddress, block->linkTargetAddr, static_cast<u8>(threadId));
#endif
  }
}

// Unlink all blocks that link to a specific target address (called when invalidating)
void PPU_JIT::UnlinkBlocksTo(u64 targetAddr, ePPUThreadID threadId) {
  auto &cache = threadCaches[static_cast<u8>(threadId)];
  for (auto &[addr, block] : cache.jitBlocksCache) {
    if (block && block->linkTargetAddr == targetAddr) {
      block->linkedBlock.store(nullptr, std::memory_order_release);
#ifdef JIT_DEBUG
      LOG_DEBUG(Xenon, "[JIT]: Unlinked block {:#x} (target {:#x} invalidated, thread {})", addr, targetAddr, static_cast<u8>(threadId));
#endif
    }
  }
}

void PPU_JIT::InvalidateBlocksForRange(u64 startAddr, u64 endAddr) {
  constexpr u64 pageSize = 4096ULL; // 4k is the minimum page size, can be easily increased to match p bit of tlbie/l.
  if (startAddr >= endAddr) return;

  u64 startPage = startAddr & ~(pageSize - 1ULL);
  u64 endPage = (endAddr + pageSize - 1) & ~(pageSize - 1ULL);

  // Invalidate across both thread caches
  for (u8 t = 0; t < 2; t++) {
    auto &cache = threadCaches[t];

    // Hold the lock for the entire invalidation sequence to prevent the executor
    // from seeing partially-invalidated entries (shared_ptr reset but not yet erased).
    std::lock_guard<std::mutex> lock(cache.jitCacheMutex);

    std::vector<u64> blocksToInvalidate;
    for (u64 page = startPage; page < endPage; page += pageSize) {
      auto pit = cache.pageBlockIndex.find(page);
      if (pit == cache.pageBlockIndex.end()) continue;
      for (u64 blk : pit->second) blocksToInvalidate.push_back(blk);
    }

    std::sort(blocksToInvalidate.begin(), blocksToInvalidate.end());
    blocksToInvalidate.erase(std::unique(blocksToInvalidate.begin(), blocksToInvalidate.end()), blocksToInvalidate.end());

    if (blocksToInvalidate.empty()) {
#ifdef JIT_DEBUG
      LOG_DEBUG(Xenon, "[JIT]: No JIT blocks to invalidate for range {:#x}-{:#x} (thread {})", startAddr, endAddr, t);
#endif
      continue;
    }

    for (u64 blkAddr : blocksToInvalidate) {
      // Unlink any blocks that point to this one (inline, lock already held)
      for (auto &[addr, block] : cache.jitBlocksCache) {
        if (block && block->linkTargetAddr == blkAddr) {
          block->linkedBlock.store(nullptr, std::memory_order_release);
        }
      }

      auto it = cache.jitBlocksCache.find(blkAddr);
      if (it != cache.jitBlocksCache.end()) {
#ifdef JIT_DEBUG
        LOG_DEBUG(Xenon, "[JIT]: Invalidating block at {:#x} due to page invalidation range {:#x}-{:#x} (thread {})", blkAddr, startAddr, endAddr, t);
#endif
        // Release resources and erase atomically (from the lock's perspective)
        it->second.reset();
        cache.jitBlocksCache.erase(it);
      }
      // Unregister from page index (lock already held)
      UnregisterBlockLocked(blkAddr, cache);
    }
  }
}

void PPU_JIT::InvalidateBlockAt(u64 blockAddr) {
  InvalidateBlocksForRange(blockAddr, blockAddr + 1);
}

void PPU_JIT::InvalidateAllBlocks() {
  for (u8 t = 0; t < 2; t++) {
    auto &cache = threadCaches[t];
    std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
#ifdef JIT_DEBUG
    LOG_DEBUG(Xenon, "[JIT]: Invalidating ALL JIT blocks (thread {})", t);
#endif
    for (auto &p : cache.jitBlocksCache) {
      p.second.reset();
    }
    cache.jitBlocksCache.clear();
    cache.pageBlockIndex.clear();
    cache.blockPageList.clear();
  }
}

// Gets current sPPUThread using the compile-time known thread ID.
// Instead of reading ppeState->currentThread at runtime (which races between
// host threads), we bake the thread offset directly into the JIT code.
void PPU_JIT::SetupContext(JITBlockBuilder *b, ePPUThreadID threadId) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  // Compute the fixed offset for this thread's sPPUThread within sPPEState
  u64 threadOffset = static_cast<u8>(threadId) * sizeof(sPPUThread);
  COMP->lea(b->threadCtx->Base(), asmjit::x86::ptr(b->ppeState->Base(), static_cast<s32>(threadOffset)));
#endif
}

// JIT Instruction Prologue (Constant Address)
// * Uses compile-time known CIA value instead of reading NIA from memory
// * Used for instructions that may cause sync exceptions or are branches
void PPU_JIT::InstrPrologueConst(JITBlockBuilder *b, u64 cia, u32 instrData) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  x86::Gp temp = newGP64();
  // CIA
  COMP->mov(temp, cia);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::CIA), temp);
  // NIA = CIA + 4
  COMP->mov(temp, cia + 4);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::NIA), temp);
  // CI data
  COMP->mov(temp, instrData);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::CI).Ptr<u32>(), temp);
#endif
}

// JIT Instruction Prologue (Minimal)
// * Only writes the CI data. CIA and NIA are not updated. Saves 4 x86 instructions
// * Used for instructions that cannot cause sync exceptions and are not branches
void PPU_JIT::InstrPrologueMinimal(JITBlockBuilder *b, u32 instrData) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
  x86::Gp temp = newGP64();
  // CI data only
  COMP->mov(temp, instrData);
  COMP->mov(b->threadCtx->scalar(&sPPUThread::CI).Ptr<u32>(), temp);
#endif
}


bool InstrEpilogue(PPU *ppu, sPPEState *ppeState) {
  // Check if exceptions are pending and process them in order.
  // Uses ppeState->currentThread which is set by the calling host thread
  return ppu->PPUCheckExceptions(ppeState->currentThread);
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

// Determine if an instruction (by name hash) can raise synchronous exceptions.
static bool InstrCanCauseSyncException(u32 opNameHash) {
  switch (opNameHash) {
    // These dont cause exceptions:
  case "mulli"_j: case "subfic"_j: case "cmpli"_j: case "cmpi"_j: case "addic"_j:
  case "addi"_j: case "addis"_j: case "bc"_j: case "b"_j: case "rlwimix"_j: case "rlwinmx"_j:
  case "rlwnmx"_j: case "ori"_j: case "oris"_j: case "xori"_j: case "xoris"_j: case "andi"_j:
  case "andis"_j: case "mcrf"_j: case "bclr"_j: case "rfid"_j: case "crnor"_j: case "crandc"_j:
  case "isync"_j: case "crxor"_j: case "crnand"_j: case "crand"_j: case "creqv"_j: case "crorc"_j:
  case "cror"_j: case "bcctr"_j: case "rldiclx"_j: case "rldicrx"_j: case "rldicx"_j:
  case "rldimix"_j: case "rldclx"_j: case "rldcrx"_j: case "cmp"_j: case "subfcx"_j: case "subfcox"_j:
  case "mulhdux"_j: case "addcx"_j: case "addcox"_j: case "mulhwux"_j: case "mfocrf"_j: case "slwx"_j:
  case "cntlzwx"_j: case "sldx"_j: case "andx"_j: case "cmpl"_j: case "subfx"_j: case "subfox"_j:
  case "dcbst"_j: case "cntlzdx"_j: case "andcx"_j: case "mulhdx"_j: case "mulhwx"_j: case "mfmsr"_j:
  case "dcbf"_j: case "negx"_j: case "negox"_j: case "norx"_j: case "subfex"_j: case "addex"_j:
  case "addeox"_j: case "mtocrf"_j: case "mtmsr"_j: case "mtmsrd"_j: case "subfze"_j: case "subfzeo"_j:
  case "addzex"_j: case "addzeox"_j: case "subfmex"_j: case "subfmeox"_j: case "mulldx"_j: case "mulldox"_j:
  case "addmex"_j: case "addmeox"_j: case "mullwx"_j: case "mullwox"_j: case "dcbtst"_j: case "addx"_j:
  case "addox"_j: case "dcbt"_j: case "eqvx"_j: case "tlbiel"_j: case "tlbie"_j: case "eciwx"_j:
  case "xorx"_j: case "mfspr"_j: case "dst"_j: case "dstst"_j: case "slbmte"_j: case "orcx"_j:
  case "slbie"_j: case "ecowx"_j: case "orx"_j: case "divdux"_j: case "divduox"_j: case "divwux"_j:
  case "divwuox"_j: case "mtspr"_j: case "dcbi"_j: case "nandx"_j: case "slbia"_j: case "divdx"_j:
  case "divdox"_j: case "divwx"_j: case "divwox"_j: case "srwx"_j: case "srdx"_j: case "tlbsync"_j:
  case "mfsrin"_j: case "mfsr"_j: case "sync"_j: case "srawx"_j: case "sradx"_j: case "dss"_j:
  case "srawix"_j: case "sradix"_j: case "slbmfev"_j: case "eieio"_j: case "slbmfee"_j: case "extshx"_j:
  case "extsbx"_j: case "extswx"_j: case "icbi"_j:
    return false;

  // Everything else can potentially fault:
  default:
    return true;
  }
}

#undef GPR
using namespace asmjit;
// Builds a JIT block starting at the given address.
std::shared_ptr<JITBlock> PPU_JIT::BuildJITBlock(u64 blockStartAddress, u64 maxBlockSize, ePPUThreadID threadId) {
  auto &cache = threadCaches[static_cast<u8>(threadId)];

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
  SetupContext(jitBuilder.get(), threadId);

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
        ppu->PPUProcessSyncExceptions(ppeState);
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

    // Check if this instruction is a block-ending branch
    bool isBranchInstr = (opName == JITOpcodeHashes::B || opName == JITOpcodeHashes::BC ||
      opName == JITOpcodeHashes::BCLR || opName == JITOpcodeHashes::BCCTR ||
      opName == JITOpcodeHashes::RFID);

    // Check if this instruction can cause sync exceptions
    bool canCauseException = InstrCanCauseSyncException(opName);

    // Branch instructions and sync exception-capable instructions need CIA/NIA updated
    // Safe instructions only need CI data written
    if (isBranchInstr || canCauseException) {
      InstrPrologueConst(jitBuilder.get(), thread.CIA, opcode);
    } else {
      InstrPrologueMinimal(jitBuilder.get(), opcode);
    }

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
        // Hybrid fallback needs CIA/NIA set for the interpreter
        // We must only set it once!
        if (!isBranchInstr && !canCauseException) {
          InstrPrologueConst(jitBuilder.get(), thread.CIA, opcode);
        }

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

    // Check if the executed instruction can produce Sync exceptions
    // Most instructions (System/ALU) don't, saving 3 x86 instructions per PPC instruction.
    // TODO: See if we can also include VXU/FPU arithmetic ops based on wheter MSR[SF,FP] is set at block build time.
    if (canCauseException) {
#if defined(ARCH_X86) || defined(ARCH_X86_64)
      // Test for present exceptions and return if any is found.
      Label skipRet = compiler.newLabel();
      x86::Gp exceptReg = compiler.newGpw();
      compiler.mov(exceptReg, jitBuilder->threadCtx->scalar(&sPPUThread::exceptReg));
      compiler.test(exceptReg, exceptReg);  // Check for a positive result.
      compiler.jz(skipRet);           // Skip return if no exceptions.
      compiler.ret();                 // Return if exceptions ocurred.
      compiler.bind(skipRet);         // Skip return Tag.
#endif
    }
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

  // If the block was not terminated by a branch instruction, NIA may not have been
  // materialized by the last instruction (if it used InstrPrologueMinimal). Emit a
  // final NIA write so that the execution resumes at the correct address
  if (!blockCanLink && instrCount > 0) {
    // Check if the last instruction was a branch, if so, it already set NIA
    bool lastWasBranch = false;
    if (!instrsTemp.empty()) {
      u32 lastOpcode = instrsTemp.back();
      u32 lastDecoded = PPCDecode(lastOpcode);
      u32 lastOpName = Base::JoaatStringHash(PPCInterpreter::ppcDecoder.getNameTable()[lastDecoded]);
      lastWasBranch = (lastOpName == JITOpcodeHashes::B || lastOpName == JITOpcodeHashes::BC ||
        lastOpName == JITOpcodeHashes::BCLR || lastOpName == JITOpcodeHashes::BCCTR ||
        lastOpName == JITOpcodeHashes::RFID);
    }
    if (!lastWasBranch) {
      // Block ended due to maxBlockSize, set final NIA
      u64 finalNIA = blockStartAddress + instrCount * 4;
      x86::Gp temp = compiler.newGpq();
      compiler.mov(temp, finalNIA);
      compiler.mov(jitBuilder->threadCtx->scalar(&sPPUThread::NIA), temp);
    }
  }

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
  block->linkedBlock.store(nullptr, std::memory_order_relaxed); // Will be linked later if target exists

  // Insert block into the block cache.
  {
    std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
    cache.jitBlocksCache.emplace(blockStartAddress, block);

    // Try to link this block to its target
    if (blockCanLink && blockLinkTarget != 0) {
      TryLinkBlock(block.get(), threadId);
    }

    // Check if any existing blocks want to link to this new block
    for (auto &[addr, existingBlock] : cache.jitBlocksCache) {
      if (existingBlock && existingBlock->canLink &&
        existingBlock->linkTargetAddr == blockStartAddress &&
        existingBlock->linkedBlock.load(std::memory_order_relaxed) == nullptr) {
        existingBlock->linkedBlock.store(block.get(), std::memory_order_release);
#ifdef JIT_DEBUG
        LOG_DEBUG(Xenon, "[JIT]: Linked existing block {:#x} -> {:#x} (thread {})", addr, blockStartAddress, static_cast<u8>(threadId));
#endif
      }
    }
  }

  // Register pages used by the block.
  RegisterBlockPages(blockStartAddress, block->size, threadId);

  return block;
}
#define GPR(x) curThread.GPR[x]

// Executes a given JIT block at a designated address.
u64 PPU_JIT::ExecuteJITBlock(u64 blockStartAddress, bool enableHalt, ePPUThreadID threadId) {
  auto &cache = threadCaches[static_cast<u8>(threadId)];
  std::shared_ptr<JITBlock> block;
  {
    std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
    auto it = cache.jitBlocksCache.find(blockStartAddress);
    if (it == cache.jitBlocksCache.end()) {
      return 0;
    }
    block = it->second;
  }
  block->codePtr(ppu, ppeState, enableHalt);
  return block->size / 4;
}

// Execute a given number of instructions using JIT.
void PPU_JIT::ExecuteJITInstrs(u64 numInstrs, bool active, ePPUThreadID threadId, bool enableHalt, bool singleBlock) {
  auto &cache = threadCaches[static_cast<u8>(threadId)];
  // Ensure the thread-local and legacy currentThread are set for this thread
  PPCInterpreter::SetCurrentThreadId(threadId);
  ppeState->currentThread = threadId;

  u32 instrsExecuted = 0;
  u32 instrCounter = 0;

  // Check for Async (System Reset) exceptions, this must be done here to avoid re-running a block that would 
  // ultimately suspend the thread until a system reset is issued.
  if (curThread.exceptReg & ppuSystemResetEx) { ppu->PPUProcessAsyncExceptions(ppeState); }

  while (instrsExecuted < numInstrs && active && (XeRunning && !XePaused)) {
    auto &thread = curThread;

    // When POST 0x2E HW_INIT fires, hwInitPosted is set and hwReturnAddress.
    // We make use of that to grab the LR and set it here, thus completly avoiding it.
    if (XeMain::GetCPU()->HasHWINITPosted()) {
      XeMain::GetCPU()->SetHWINITPosted(false);
      PPCInterpreter::ppuSetCR(ppeState, 0, false, false, true, false);
      thread.NIA = XeMain::GetCPU()->GetHWINITReturnAddress() & ~3ULL;
      LOG_INFO(Xenon, "[JIT] HwInit intercepted. Returning to {:#x}.", thread.NIA);
      continue;
    }

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
    case 0x80081764:
    case 0x817ac968:
      skipBlock = true;
      break;
    default:
      break;
    }

    // Skip to next block if needed.
    if (skipBlock) {
      instrsExecuted++;
      instrCounter++;
      thread.NIA += 4;
    }

    // Get next block start address.
    u64 blockStartAddress = thread.NIA;

    // Attempt to find the block in the cache under lock, taking a shared_ptr copy
    // to prevent the block from being destroyed by a concurrent invalidation.
    std::shared_ptr<JITBlock> blockRef;
    {
      std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
      auto it = cache.jitBlocksCache.find(blockStartAddress);
      if (it != cache.jitBlocksCache.end()) {
        blockRef = it->second;
      }
    }

    if (!blockRef) {
      // Block was not found. Attempt to create a new one.
      auto block = BuildJITBlock(blockStartAddress, numInstrs - instrsExecuted, threadId);
      if (!block) { continue; } // Block build attempt failed.

      // Execute our block and increse executed instructions.
      block->codePtr(ppu, ppeState, enableHalt);
      instrsExecuted += block->size / 4;
      instrCounter += block->size / 4;

      // For Testing and debugging purposes only.
      if (singleBlock)
        break;

      // If the thread was suspended due to CTRL being written, we must end execution on said thread.
      if (threadId == ePPUThread_Zero && !ppeState->SPR.CTRL.TE0) { break; }
      if (threadId == ePPUThread_One && !ppeState->SPR.CTRL.TE1) { break; }

      // Process pending synchronous exceptions.
      if (thread.exceptReg & SyncExceptionMask) { ppu->PPUProcessSyncExceptions(ppeState); }
    } else {
      bool realMode = false;
      realMode = !thread.SPR.MSR.DR || !thread.SPR.MSR.IR;
      if (realMode) { // When in real mode TLB is disabled. Fallback to old approach.
        // We have a match, check for the block hash to see if it hasn't been modified.
        u64 sum = 0;
        const u64 blockSize = blockRef->size;
        const u64 blockAddr = blockRef->ppuAddress;

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

        if (blockRef->hash != sum) {
#ifdef JIT_DEBUG
          LOG_DEBUG(Xenon, "[JIT]: Block hash mismatch for block at address {:#x}", blockStartAddress);
#endif // JIT_DEBUG
          // Blocks do not match. Erase it under lock and retry.
          blockRef.reset(); // Release our local reference first
          {
            std::lock_guard<std::mutex> lock(cache.jitCacheMutex);
            auto eraseIt = cache.jitBlocksCache.find(blockStartAddress);
            if (eraseIt != cache.jitBlocksCache.end()) {
              eraseIt->second.reset();
              cache.jitBlocksCache.erase(eraseIt);
            }
            UnregisterBlockLocked(blockStartAddress, cache);
          }
          continue;
        }
      }

      // Run block as usual. blockRef keeps the block alive during execution.
      JITBlock *currentBlock = blockRef.get();
      currentBlock->codePtr(ppu, ppeState, enableHalt);
      instrsExecuted += currentBlock->size / 4;
      instrCounter += currentBlock->size / 4;

      // Block linking optimization: follow linked blocks without returning to dispatcher
      // Only do this if we're not in single-block mode and have instructions remaining
      while (!singleBlock && instrsExecuted < numInstrs && (XeRunning && !XePaused)) {
        // Snapshot the linked block pointer to avoid TOCTOU race with InvalidateAllBlocks/UnlinkBlocksTo
        JITBlock *nextBlock = currentBlock->linkedBlock.load(std::memory_order_acquire);
        if (!nextBlock) {
          break;
        }

        // Verify that NIA matches the linked block's address
        // (exception handlers or interrupts may have changed NIA)
        if (thread.NIA != currentBlock->linkTargetAddr) {
          break;
        }

        // Check thread suspension
        if (threadId == ePPUThread_Zero && !ppeState->SPR.CTRL.TE0) {
          break;
        }

        if (threadId == ePPUThread_One && !ppeState->SPR.CTRL.TE1) {
          break;
        }

        // Execute linked block
        currentBlock = nextBlock;
        currentBlock->codePtr(ppu, ppeState, enableHalt);
        instrsExecuted += currentBlock->size / 4;
        instrCounter += currentBlock->size / 4;
      }

      // If the thread was suspended due to CTRL being written, we must end execution on said thread.
      if (threadId == ePPUThread_Zero && !ppeState->SPR.CTRL.TE0) { break; }
      if (threadId == ePPUThread_One && !ppeState->SPR.CTRL.TE1) { break; }

      // Process pending synchronous exceptions.
      if (thread.exceptReg & SyncExceptionMask) { ppu->PPUProcessSyncExceptions(ppeState); }
    }

    // Process asynchronous exceptions every x amount of instructions.
    // Avoids to be constantly checking for External exceptions and such.
    if (instrCounter >= 25) {
      if (thread.SPR.MSR.EE) {
        if (ppu->xenonContext->iic.hasPendingInterrupts(thread.SPR.PIR)) {
          thread.exceptReg |= ppuExternalEx;
        }
      }
      if (thread.exceptReg & AsyncExceptionMask) {
        ppu->PPUProcessAsyncExceptions(ppeState);
      }
      instrCounter = 0;
    }
  }
}