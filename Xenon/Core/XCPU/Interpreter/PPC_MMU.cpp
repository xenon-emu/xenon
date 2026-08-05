/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Global.h"
#include "PPCInterpreter.h"

// #define MMU_DEBUG
#ifndef MMU_DEBUG
  #define DEBUGP(x, ...)
#else
  #define DEBUGP(x, ...) LOG_DEBUG(Xenon_MMU, x, ##__VA_ARGS__);
#endif

// TLB IS (Invalidation Selector) field, RB[52:53]. Chooses how the matching
// congruence class is invalidated:
//   IS = 0  selective : clear entries whose (tag & mask) == search_tag
//   IS = 1  reserved  : NO entry is invalidated
//   IS = 2  partition : clear entries whose LPIDR == the issuing LPIDR
//   IS = 3  class     : clear EVERY way of the congruence class, no tag test
enum MMU_TLB_IS : u8 {
  MMU_TLB_IS_SELECTIVE = 0,
  MMU_TLB_IS_RESERVED = 1,
  MMU_TLB_IS_LPID = 2,
  MMU_TLB_IS_CLASS = 3,
};

// SLB Invalidate All
void PPCInterpreter::PPCInterpreter_slbia(sPPEState* ppeState) {
  // Entry 0 is the bolted segment and must be preserved across SLBIA. Confirmed thru reverse-engineering efforts.
  for (size_t i = 1; i < std::size(curThread.SLB); ++i) { curThread.SLB[i].V = 0; }

  // Invalidate both ERAT's
  curThread.iERAT.invalidateAll();
  curThread.dERAT.invalidateAll();
}

// TLB Invalidate Entry Local
void PPCInterpreter::PPCInterpreter_tlbiel(sPPEState* ppeState) {
  const u64 rb = GPRi(rb);
  const u8 IS = static_cast<u8>((rb >> 10) & 0x3); // RB[52:53]
  const bool LP = (rb & 0x1000) >> 12;
  const u8 p = ppeState->mmu->GetPageSize(_instr.l10, LP);

  DEBUGP("[TLBIEL]: RB:{:#x} IS:{} p:{} L:{} LP:{}", rb, static_cast<u32>(IS), p, static_cast<u32>(_instr.l10),
         static_cast<u32>(LP));

  switch (IS) {
    case MMU_TLB_IS_RESERVED:
      // IS=1: Invalidate nothing. No ERAT/JIT flush either.
      LOG_ERROR(Xenon_MMU, "[TLBIEL]: IS=1 (reserved), UNIMPLEMENTED! Please report to Xenon devs.");
      return;

    case MMU_TLB_IS_SELECTIVE: {
      // IS=0: Be as selective as possible when invalidating.
      ppeState->mmu->TlbInvalidateSelective(rb, p, _instr.l10);

      // Invalidate other caches
      curThread.iERAT.invalidateAll();
      curThread.dERAT.invalidateAll();
      break;
    }

    case MMU_TLB_IS_LPID: {
      // IS=2: partition-scoped. Unused on Xenon apparently.
      LOG_ERROR(Xenon_MMU, "[TLBIEL]: IS=2: Partition flush based on Logical Partition ID (LPID), UNIMPLEMENTED!"
                           "Please report to Xenon devs.");

      // Perform invalidation and notify the user. This isn't implemented.
      curThread.iERAT.invalidateAll();
      curThread.dERAT.invalidateAll();
      break;
    }

    case MMU_TLB_IS_CLASS: {
      // IS=3: class level. Invalidate every way of the congruence class. The class index is carried in RB[12:19].
      const u16 classIndex = static_cast<u16>((rb & 0xFF000) >> 12);
      ppeState->TLB.invalidateClass(classIndex);

      curThread.iERAT.invalidateAll();
      curThread.dERAT.invalidateAll();
      break;
    }
  }
}

// rB is the GPR containing the EA for the search
// L is the page size

// TLB Invalidate Entry
void PPCInterpreter::PPCInterpreter_tlbie(sPPEState* ppeState) {
  const u64 EA = GPRi(rb);
  const bool LP = (GPRi(rb) & 0x1000) >> 12;
  const u8 p = ppeState->mmu->GetPageSize(_instr.l10, LP);
  const u64 pageSize = 1ULL << p;
  const u64 pageMask = pageSize - 1;
  const u64 pageBase = EA & ~pageMask;

#ifdef DEBUGP
  if (Config::log.advanced)
    DEBUGP("[TLBIE]: EA: {:#x} | PageSize: {} | Full: {:#x} | LP:{}", EA, p, pageSize, LP ? "true" : "false");
#endif

  // IS is forced to 0 (selective).
  ppeState->mmu->TlbInvalidateSelective(EA, p, _instr.l10);

  // Broadcast: flush the per-thread ERAT page on every PPE thread.
  for (auto& thread : ppeState->ppuThread) {
    thread.iERAT.invalidateElement(pageBase);
    thread.dERAT.invalidateElement(pageBase);
  }
}

// TLB Synchronize
void PPCInterpreter::PPCInterpreter_tlbsync(sPPEState* ppeState) {
  // Do nothing
#ifdef DEBUGP
  if (Config::log.advanced) DEBUGP("tlbsync");
#endif
}
