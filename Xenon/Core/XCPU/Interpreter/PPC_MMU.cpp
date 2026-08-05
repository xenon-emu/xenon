/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Global.h"
#include "Core/PCI/Devices/NAND/NAND.h"
#include "Core/XCPU/Context/PostBus/PostBus.h"
#include "Core/XCPU/XenonCPU.h"
#include "PPCInterpreter.h"

// #define MMU_DEBUG
#ifndef MMU_DEBUG
  #define DEBUGP(x, ...)
#else
  #define DEBUGP(x, ...) LOG_DEBUG(Xenon_MMU, x, ##__VA_ARGS__);
#endif

//
// Xbox 360 Memory map, info taken from various sources.
//

// Everything can fit on 32 bits on the 360, so MS uses upper bits of the 64 bit
// EA to manage L2 cache, further research required on this.

// 0x200 00000000 - 0x200 00008000                  32K SROM - 1BL Location.
// 0x200 00010000 - 0x200 00020000                  64K SRAM.
// 0x200 00050000 - 0x200 00056000                  Interrupt controller.
// 0x200 C8000000 - 0x200 C9000000                  NAND Flash 1:1
// 0x200 C9000000 - 0x200 CA000000                  Currently unknown, I
// suspect that maybe it is additional space for 512 MB NAND Flash images.
// 0x200 EA000000 - 0x200 EA010000                  PCI Bridge
// 0x200 EC800000 - 0x200 EC810000                  GPU

#define MMU_PAGE_SIZE_4KB  12
#define MMU_PAGE_SIZE_64KB 16
#define MMU_PAGE_SIZE_1MB  20
#define MMU_PAGE_SIZE_16MB 24

// The processor generated address (EA) is subdivided, upper 32 bits are used
// as flags for the 'Security Engine'
//
// 0x00000X**_00000000 X = region, ** = key select
// X = 0 should be Physical
// X = 1 should be Hashed
// X = 2 should be SoC
// X = 3 should be Encrypted

// 0x8000020000060000 Seems to be the random number generator. Implement this?

/*
 * Hash page table definitions
 */

#define PPC_SPR_SDR_64_HTABORG  0x0FFFFFFFFFFC0000ULL
#define PPC_SPR_SDR_64_HTABSIZE 0x000000000000001FULL

#define PPC_HPTES_PER_GROUP 8

//
// PTE 0.
//

// Page valid.
#define PPC_HPTE64_VALID 0x0000000000000001ULL
// Page Hash identifier.
#define PPC_HPTE64_HASH 0x0000000000000002ULL
// Page Large bit.
#define PPC_HPTE64_LARGE 0x0000000000000004ULL
// Page AVPN.
#define PPC_HPTE64_AVPN 0x0001FFFFFFFFFF80ULL
// Page AVPN [0:51]
#define PPC_HPTE64_AVPN_0_51 0x0001FFFFFFFFF000ULL

//
// PTE 1.
//

// RPN when L = 0.
#define PPC_HPTE64_RPN_NO_LP 0x000003FFFFFFF000UL
// RPN when L = 1.
#define PPC_HPTE64_RPN_LP 0x000003FFFFFFE000UL
// Large Page Selector bit.
#define PPC_HPTE64_LP 0x0000000000001000ULL
// Bolted PTE.
#define HPTE64_V_BOLTED 0x0000000000000010ULL
// Changed bit.
#define PPC_HPTE64_C 0x0000000000000080ULL
// Referenced bit.
#define PPC_HPTE64_R 0x0000000000000100ULL

// DSISR Flags.
#define DSISR_ISSTORE 0x02000000
#define DSISR_NOPTE   0x40000000

// Page table entry structure.
struct PPC_HPTE64 {
  u64 pte0;
  u64 pte1;
};

static constexpr u32 TLB_IDX_BITS = 10; // log2(NUM_ENTRIES)
static constexpr u32 TLB_WAYS_LOG2 = 2; // log2(NUM_WAYS)
static constexpr u32 TLB_SEG_SIZE = 28; // 256 MB segment

// Page-size-dependent runtime tag mask, used to compare TLB tags at a given p size.
inline u64 mmuTlbTagMask(u8 p) { return ~((4ULL * (1ULL << (TLB_IDX_BITS - TLB_WAYS_LOG2 + p - 3))) - 4ULL); }

// Lookup search TLB tag, holds VSID, L bit and the VA encoded.
inline u64 mmuTlbSearchTag(u64 VA, u8 p, bool L) {
  const u64 VSID = (VA & ~0xFFFFFFFULL) >> 16;
  return (L ? 2ULL : 0ULL) | (VSID << 15) | ((VA & 0xFFFF000ULL) >> 1);
}

// Store tag built from the real/insert pte0 for the TLB entry. Encodes the L bit, EA page, and PTE0.
inline u64 mmuTlbStoreTag(u64 pte0, u64 ea) {
  return ((ea & 0x7FF000ULL) >> 1) | ((pte0 & ~0x7FULL) << 15) | (2ULL * ((pte0 >> 2) & 1)); // L bit -> tag[1]
}

// Congruence-class set-hash:
// Returns 0..255: the class index.
inline u16 mmuTlbClass(u64 VA, u8 p) {
  const u32 setMask = (1u << (TLB_IDX_BITS - TLB_WAYS_LOG2)) - 1; // 0xFF
  u64 set = setMask & (VA >> p);
  const u64 hi = ((0xFFFFFFFull >> p) & ~static_cast<u64>(setMask)) << p;
  if (hi) set ^= ((hi & VA) >> (TLB_WAYS_LOG2 + 28 - TLB_IDX_BITS)) & ~0xFull; // >> 20
  return static_cast<u16>(set & 0xFF);
}

// Note for tlbiel/tlbie:
// The IS (Invalidation Selector) field is RB[52:53], it chooses how the matching congruence class is invalidated:
//   IS = 0  selective : clear entries whose (tag & mask) == search_tag  ("as selective as possible", mask widens
//                       with page size)
//   IS = 1  reserved  : NO entry is invalidated
//   IS = 2  partition : clear entries whose LPIDR == the issuing LPIDR
//   IS = 3  class     : clear EVERY way of the congruence class, no tag test

// TLB IS Fields
enum MMU_TLB_IS : u8 {
  MMU_TLB_IS_SELECTIVE = 0,
  MMU_TLB_IS_RESERVED = 1,
  MMU_TLB_IS_LPID = 2,
  MMU_TLB_IS_CLASS = 3,
};

// Precise, VSID-aware selective invalidation
// Hash the VA to its single congruence class, build the page-size-masked search tag, and clear only the ways whose
// stored tag matches.
inline void mmuTlbInvalidateSelective(sPPEState* ppeState, u64 VA, u8 p, bool L) {
  const u16 classIdx = mmuTlbClass(VA, p);
  const u64 tagMask = mmuTlbTagMask(p);
  const u64 searchTag = mmuTlbSearchTag(VA, p, L) & tagMask;

  for (u32 way = 0; way < TLB_Reg::NUM_WAYS; ++way) {
    TLBEntry& entry = ppeState->TLB.entryAt(classIdx, way);
    if (entry.valid && (entry.tag & tagMask) == searchTag) {
      DEBUGP("[TLB]: Selective invalidate match: class:{:#x} way:{} tag:{:#x} (VA:{:#x} p:{} L:{})", classIdx, way,
             entry.tag, VA, p, static_cast<u32>(L));
      entry.valid = 0;
    }
  }
}

// Page-size masks used by the HTAB PTE compare (mmuComparePTE) � unrelated to the TLB tag; left as-is.
static constexpr u64 TLB_COMPARE_MASK_4KB = 0xFFFFFFFFFFF00000ULL;  // VA[0:59]
static constexpr u64 TLB_COMPARE_MASK_64KB = 0xFFFFFFFFFF000000ULL; // VA[0:55]
static constexpr u64 TLB_COMPARE_MASK_16MB = 0xFFFFFFFF00000000ULL; // VA[0:47]

inline u64 mmuGetCompareMask(u8 p) {
  switch (p) {
    case MMU_PAGE_SIZE_4KB: return TLB_COMPARE_MASK_4KB;
    case MMU_PAGE_SIZE_64KB: return TLB_COMPARE_MASK_64KB;
    case MMU_PAGE_SIZE_16MB: return TLB_COMPARE_MASK_16MB;
    default: return TLB_COMPARE_MASK_4KB;
  }
}

inline bool mmuComparePTE(u64 VA, u64 VPN, u64 pte0, u64 pte1, u8 p, bool L, bool LP, u64* RPN) {
  // Requirements:
  // PTE[H] = 0 for the primary PTEG, 1 for the secondary PTEG
  // PTE[V] = 1
  // PTE[AVPN][0:51] = VA[0:51]
  // if p < 28, PTEAVPN[52:51 + q] = VA[52 : 51 + q]
  // PTE[LP] = SLBE[LP] whenever PTE[L] = 1

  // Valid
  bool pteV = (pte0 & PPC_HPTE64_VALID);
  // L
  bool pteL = (pte0 & PPC_HPTE64_LARGE) >> 2;
  // LP
  bool pteLP = (pte1 & PPC_HPTE64_LP) >> 12;
  // AVPN 0:51
  const u64 pteAVPN_0_51 = (pte0 & PPC_HPTE64_AVPN_0_51) << 16;
  // q = minimum(5, 28-p).
  const u8 q = std::min(5, 28 - p);

  if (!pteV) { return false; }

  if (pteAVPN_0_51 != (VA & 0xFFFFFFFFF0000000)) { return false; }

  if (L != pteL) {
    DEBUGP("L mismatch: L={}, PTE[L]={}", L, pteL);
    return false;
  }

  if (L && LP != pteLP) {
    DEBUGP("LP mismatch: LP={}, PTE[LP]={}", LP, pteLP);
    return false;
  }

  // Compare using mask
  const u64 compareMask = mmuGetCompareMask(p);
  if ((VPN & compareMask) != (VA & compareMask)) { return false; }

  // Extract RPN
  *RPN = L ? (pte1 & PPC_HPTE64_RPN_LP) : (pte1 & PPC_HPTE64_RPN_NO_LP);
  return true;
}

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
  const u8 p = mmuGetPageSize(ppeState, _instr.l10, LP);

  DEBUGP("[TLBIEL]: RB:{:#x} IS:{} p:{} L:{} LP:{}", rb, static_cast<u32>(IS), p, static_cast<u32>(_instr.l10),
         static_cast<u32>(LP));

  switch (IS) {
    case MMU_TLB_IS_RESERVED:
      // IS=1: Invalidate nothing. No ERAT/JIT flush either.
      LOG_ERROR(Xenon_MMU, "[TLBIEL]: IS=1 (reserved), UNIMPLEMENTED! Please report to Xenon devs.");
      return;

    case MMU_TLB_IS_SELECTIVE: {
      // IS=0: Be as selective as possible when invalidating.
      mmuTlbInvalidateSelective(ppeState, rb, p, _instr.l10);

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

/*
  The PowerPC instruction tlbie searches the Translation Look-Aside Buffer (TLB) for an entry corresponding to the
  effective address (EA). The search is done regardless of the setting of Machine State Register (MSR) Instruction
  Relocate bit or the MSR Data Relocate bit. The search uses a portion of the EA including the least significant bits,
  and ignores the content of the Segment Registers. Entries that satisfy the search criteria are made invalid so will
  not be used to translate subsequent storage accesses.
*/

// rB is the GPR containing the EA for the search
// L is the page size

// TLB Invalidate Entry
void PPCInterpreter::PPCInterpreter_tlbie(sPPEState* ppeState) {
  const u64 EA = GPRi(rb);
  const bool LP = (GPRi(rb) & 0x1000) >> 12;
  const u8 p = mmuGetPageSize(ppeState, _instr.l10, LP);
  const u64 pageSize = 1ULL << p;
  const u64 pageMask = pageSize - 1;
  const u64 pageBase = EA & ~pageMask;

#ifdef DEBUGP
  if (Config::log.advanced)
    DEBUGP("[TLBIE]: EA: {:#x} | PageSize: {} | Full: {:#x} | LP:{}", EA, p, pageSize, LP ? "true" : "false");
#endif

  // IS is forced to 0 (selective).
  mmuTlbInvalidateSelective(ppeState, EA, p, _instr.l10);

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

// Helper function for getting Page Size (p bit).
u8 PPCInterpreter::mmuGetPageSize(sPPEState* ppeState, bool L, u8 LP) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUGetPageSize", MP_AUTO);

  // Large page selection works the following way:
  // First check if pages are large (L)
  // if (L) the page size can be one of two defined pages. On the XBox 360,
  // MS decided to use two of the three page sizes, 64Kb and 16Mb.
  // Selection between them is made using bits 16 - 19 of HID6 SPR.

  // HID6 16-17 bits select Large Page size 1.
  // HID6 18-19 bits select Large Page size 2.
  if (!L) { return MMU_PAGE_SIZE_4KB; }

  // Large page size lookup tables indexed by HID6 LB bits
  static constexpr u8 lpSize0[4] = {MMU_PAGE_SIZE_16MB, MMU_PAGE_SIZE_1MB, MMU_PAGE_SIZE_64KB, MMU_PAGE_SIZE_4KB};
  static constexpr u8 lpSize1[4] = {MMU_PAGE_SIZE_16MB, MMU_PAGE_SIZE_1MB, MMU_PAGE_SIZE_64KB, MMU_PAGE_SIZE_4KB};

  const u8 LB = ppeState->SPR.HID6.LB;

  if (LP == 0) {
    const u8 LB_16_17 = (LB & 0b1100) >> 2;
    return lpSize0[LB_16_17];
  } else {
    const u8 LB_18_19 = LB & 0b11;
    return lpSize1[LB_18_19];
  }
}

//
// Software-managed TLB insert
//
// The flow:
//   1. Take the flat entry index directly from PPE_TLB_Index. The register encodes (class_index << 4) |
//      one_hot_way_bitmask; Convert that to (class * num_ways + (num_ways - 1 - bsf(way_mask))).
//   2. Reconstruct the page EA from PPE_TLB_VPN.AVPN bits [37..47] and from pte0 bits [7..17]
//   3. Compute the lpid_bit from VPN bit 12 when L=1, else 0.
//   4. Build the store tag.
void PPCInterpreter::mmuAddTlbEntry(sPPEState* ppeState) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUAddTlbEntry", MP_AUTO);

  const u64 tlbIndexReg = ppeState->SPR.PPE_TLB_Index.hexValue;
  const u64 tlbVpnReg = ppeState->SPR.PPE_TLB_VPN.hexValue;
  const u64 tlbRpnReg = ppeState->SPR.PPE_TLB_RPN.hexValue;

  // The PPE_TLB_VPN register is the pte0 image (V|H|L|AVPN bits). The PPE_TLB_RPN register is the pte1
  // image (RPN|attrs|R|C|LP). Stored pte0 has the H bit cleared.
  const u64 storedPte0 = tlbVpnReg & ~PPC_HPTE64_HASH;
  const u64 storedPte1 = tlbRpnReg;

  // Page attributes for diagnostics / EA reconstruction.
  const bool L = (tlbVpnReg & PPC_HPTE64_LARGE) >> 2;
  const bool LP = (tlbRpnReg & PPC_HPTE64_LP) >> 12;
  const u8 p = mmuGetPageSize(ppeState, L, LP);

  // Reconstruct page EA for the store tag's EA[12:22] slice. The low VA bits (the LVPN, VA[12:22]) are carried
  // by PPE_TLB_Index[37..47], not by PPE_TLB_VPN. That register only holds the AVPN (high VA bits), which reach
  // the tag through the (pte0 & ~0x7F) term of mmuTlbStoreTag.
  const u64 eaPage = ((((tlbIndexReg >> 37) & 0x7FF) | ((storedPte0 >> 7) << 11)) << 12);

  // Resolve flat entry index from PPE_TLB_Index.
  const u32 flatIdx = TLB_Reg::flatIndexFromTlbIndexReg(tlbIndexReg);
  if (flatIdx == ~0u) {
    DEBUGP("[TLB]: mmuAddTlbEntry: malformed PPE_TLB_Index {:#x}", tlbIndexReg);
    return;
  }
  const u32 classIdx = flatIdx / TLB_Reg::NUM_WAYS;
  const u32 wayIdx = flatIdx % TLB_Reg::NUM_WAYS;

  TLBEntry& entry = ppeState->TLB.entry(flatIdx);

  // If we're replacing a live entry, invalidate it first.
  if (entry.valid) { entry.valid = 0; }

  // Populate everything BEFORE setting valid.
  // Store tag is the un-masked tag, the lookup masks it given that the p bit is controlled by the lookup.
  entry.tag = mmuTlbStoreTag(storedPte0, eaPage);
  entry.pte0 = storedPte0;
  entry.pte1 = storedPte1;
  entry.lpidr = ppeState->SPR.LPIDR.hexValue;

#ifdef DEBUGP
  if (XeMain::GetCPU()) {
    PPU* ppu = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
    if (ppu && ppu->traceFile) {
      fprintf(ppu->traceFile, "TLB[%d:%d] map 0x%llx -> 0x%llx\n", static_cast<int>(tlbIndexReg & 0xF),
              static_cast<int>((tlbIndexReg >> 4) & 0xFF), storedPte0, storedPte1);
    }
  }
#endif
  DEBUGP("[TLB]: Software insert: class: {:#x} way: {} tag: {:#x} PTE0: {:#x} PTE1: {:#x} p: {}", classIdx, wayIdx,
         entry.tag, storedPte0, storedPte1, p);

  // Atomic publish � readers either see valid==0 (skip) or a fully written entry.
  entry.valid = 1;
  ppeState->TLB.updateLRU(classIdx, static_cast<u8>(wayIdx));
}

// Hardware-managed TLB insert (HTAB walk path).
void PPCInterpreter::mmuAddTlbEntryHardware(sPPEState* ppeState, u64 VA, u64 pte0, u64 pte1, u8 p, bool L,
                                            bool /*LP*/) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUAddTlbEntryHardware", MP_AUTO);

  const u16 classIdx = mmuTlbClass(VA, p);

  // pte0 stored with H cleared.
  const u64 storedPte0 = pte0 & ~PPC_HPTE64_HASH;

  // Victim selection � invalid ways first, then pseudo-LRU.
  const u32 wayIdx = ppeState->TLB.pickVictimWay(classIdx);

  TLBEntry& entry = ppeState->TLB.entryAt(classIdx, wayIdx);
  if (entry.valid) { entry.valid = 0; }

  // Un-masked store tag from the freshly-walked HPTE.
  entry.tag = mmuTlbStoreTag(storedPte0, VA);
  entry.pte0 = storedPte0;
  entry.pte1 = pte1;
  entry.lpidr = ppeState->SPR.LPIDR.hexValue;

  DEBUGP("[TLB]: Hardware reload: class: {:#x} way: {} tag: {:#x} PTE0: {:#x} PTE1: {:#x} p: {} L: {}", classIdx,
         wayIdx, entry.tag, storedPte0, pte1, p, L);

  entry.valid = 1;
  ppeState->TLB.updateLRU(classIdx, static_cast<u8>(wayIdx));
}

// TLB lookup
bool PPCInterpreter::mmuSearchTlbEntry(sPPEState* ppeState, u64* RPN, u64 VA, u8 p, bool L, bool LP) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUSearchTlbEntry", MP_AUTO);

  // Runtime tag + page-size mask + masked compare, mask is generated using the provide p size.
  const u16 classIdx = mmuTlbClass(VA, p);
  const u64 tagMask = mmuTlbTagMask(p);
  const u64 search = mmuTlbSearchTag(VA, p, L) & tagMask;

  // Partition scoping, gates every hit on entry->lpidr == lpid, in addition to the masked tag compare.
  // Entries inserted under a different LPIDR never satisfy a lookup, so a partition switch
  // transparently shadows stale translations.
  const u32 lpid = static_cast<u32>(ppeState->SPR.LPIDR.hexValue);

  // Walk the four ways of the class over contiguous memory: one multiply to find
  // the class base, then linear indexing (vs recomputing class*4+way each step).
  TLBEntry* ways = &ppeState->TLB.entryAt(classIdx, 0);
  for (u32 way = 0; way < TLB_Reg::NUM_WAYS; ++way) {
    TLBEntry& entry = ways[way];

    // valid==0 entries can be safely skipped, given that valid is only set after an insert has fully finished.
    if (!entry.valid) continue;
    if (entry.lpidr != lpid) continue;
    if ((entry.tag & tagMask) != search) continue;

    // Tag hit. Derive RPN from the stored pte1
    const bool entryL = (entry.pte0 & PPC_HPTE64_LARGE) >> 2;
    const bool entryLP = (entry.pte1 & PPC_HPTE64_LP) >> 12;
    if (entryL != L) continue;
    if (L && entryLP != LP) continue;

    *RPN = entryL ? (entry.pte1 & PPC_HPTE64_RPN_LP) : (entry.pte1 & PPC_HPTE64_RPN_NO_LP);
    ppeState->TLB.updateLRU(classIdx, static_cast<u8>(way));
    return true;
  }

  // Miss. Update the PPE_TLB_Index_Hint with a replacement suggestion for software-managed mode so the kernel's
  // TLB-miss handler can MTSPR PPE_TLB_Index directly from the hint.
  const bool tlbSoftwareManaged = (ppeState->SPR.LPCR.hexValue & 0x400) >> 10;
  if (tlbSoftwareManaged) {
    const u32 replacementWay = ppeState->TLB.pickVictimWay(classIdx);
    const u64 hint = (static_cast<u64>(classIdx) << 4) | static_cast<u64>(TLB_WAY_BITMASK[replacementWay]);
    curThread.SPR.PPE_TLB_Index_Hint.hexValue = hint;
  }

  return false;
}

// Routine to read a string from memory, using a PSTRNG given by the kernel.
void PPCInterpreter::mmuReadString(sPPEState* ppeState, u64 stringAddress, char* string, u32 maxLength) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUReadString", MP_AUTO);
  u32 strIndex;
  u32 stringBufferAddress = 0;
  const u16 strLength = MMURead16(ppeState, stringAddress);

  if (strLength < maxLength) maxLength = strLength + 1;

  stringBufferAddress = MMURead32(ppeState, stringAddress + 4);
  MMURead(xenonContext, ppeState, stringBufferAddress, maxLength, reinterpret_cast<u8*>(string));
  string[maxLength - 1] = 0;
}

SECENG_ADDRESS_INFO
PPCInterpreter::mmuGetSecEngInfoFromAddress(u64 inputAddress) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUGetSecEngInfoFromAddress", MP_AUTO);
  // 0x00000X**_00000000 X = region, ** = key select
  // X = 0 should be Physical
  // X = 1 should be Hashed
  // X = 2 should be SoC
  // X = 3 should be Encrypted

  SECENG_ADDRESS_INFO addressInfo;

  constexpr u64 regionMask = 0xF0000000000;
  constexpr u64 keyMask = 0xFF00000000;
  const u32 region = (inputAddress & regionMask) >> 32;

  addressInfo.keySelected = static_cast<u8>((inputAddress & keyMask) >> 32);
  addressInfo.accessedAddr = static_cast<u32>(inputAddress);

  switch (region) {
    case 0x0: addressInfo.regionType = SECENG_REGION_PHYS; break;
    case 0x100: addressInfo.regionType = SECENG_REGION_HASHED; break;
    case 0x200: addressInfo.regionType = SECENG_REGION_SOC; break;
    case 0x300: addressInfo.regionType = SECENG_REGION_ENCRYPTED; break;
    default: break;
  }
  return addressInfo;
}

u64 PPCInterpreter::mmuContructEndAddressFromSecEngAddr(u64 inputAddress, bool* socAccess) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUContructEndAddressFromSecEngAddr", MP_AUTO);
  SECENG_ADDRESS_INFO inputAddressInfo = mmuGetSecEngInfoFromAddress(inputAddress);

  u64 outputAddress = 0;

  switch (inputAddressInfo.regionType) {
    case SECENG_REGION_PHYS:
      // Low order 32 bits of te address map directly to the physical address.
      outputAddress = inputAddressInfo.accessedAddr;
      break;
    case SECENG_REGION_HASHED:
      // Only 30 bits of this address map to physical address.
      outputAddress = (inputAddressInfo.accessedAddr & 0x3FFFFFFF);
      break;
    case SECENG_REGION_SOC:
      *socAccess = true;
      outputAddress = inputAddressInfo.accessedAddr;
      break;
    case SECENG_REGION_ENCRYPTED:
      // Only 30 bits of this address map to physical address.
      outputAddress = (inputAddressInfo.accessedAddr & 0x3FFFFFFF);
      break;
    default: break;
  }
  return outputAddress;
}

// Main address translation mechanism used on the XCPU.
bool PPCInterpreter::MMUTranslateAddress(u64* EA, sPPEState* ppeState, bool memWrite, ePPUThreadID thr) {
  // Every time the CPU does a load or store, it goes trough the MMU.
  // The MMU decides based on MSR, and some other regs if address translation
  // for Instr/Data is in Real Mode (EA = RA) or in Virtual Mode (Page
  // Address Translation).

  // Xbox 360 MMU contains a very similar to the CELL-BE MMU.
  // Has two ERAT's (64 entry, 2 way), one for Instructions (I-ERAT) and Data
  // (D-ERAT), this  Effective to Physical adress translations done
  // recently.
  // It also contains a 1024 entry 4 * 256 columns TLB array, wich caches
  // recent Page tables. TLB on the Xbox 360 can be Software/Hardware managed.
  // This is controlled via TL bit of the LPCR SPR.

  /* TODO */
  // Implement L1 per-core data/inst cache and cache handling code.

  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUTranslateAddress", MP_AUTO);

  //
  // Current thread SPR's used in MMU..
  //
  sPPUThread& thread = ppeState->ppuThread[thr != ePPUThread_None ? thr : curThreadId];

  // Machine State Register.
  const uMSR _msr = thread.SPR.MSR;
  // Logical Partition Control Register.
  const u64 LPCR = ppeState->SPR.LPCR.hexValue;
  // Hypervisor Real Mode Offset Register.
  const u64 HRMOR = ppeState->SPR.HRMOR.hexValue;
  // Real Mode Offset Register.
  const u64 RMOR = ppeState->SPR.RMOR.hexValue;
  // Upper 32 bits of EA, used when getting the VPN.
  const u64 upperEA = (*EA & 0xFFFFFFFF00000000);

  // On 32-Bit mode of opertaion MSR[SF] = 0, high order 32 bits of the EA
  // are truncated, effectively clearing them.
  if (!_msr.SF) *EA = static_cast<u32>(*EA);

  // Real Adress, this is what we want.
  u64 RA = 0;

  //
  // ERAT's
  //

  // Each ERAT entry holds the EA-to-RA translation for an aligned 4 KB area of memory. When
  // using a 4 KB page size, each ERAT entry holds the information for exactly one page.When
  // using large pages, each ERAT entry contains a 4 KB section of the page, meaning that large
  // pages can occupy several ERAT entries.All EA - to - RA mappings are kept in the ERAT including
  // both real - mode and virtual - mode addresses(that is, addresses accessed with MSR[IR] equal to
  // 0 or 1).
  // TODO:
  // The ERATs identify each translation entry with some combination of the MSR[SF, IR, DR,
  // PR, and HV] bits, depending on whether the entry is in the I - ERAT or D - ERAT.This allows the
  // ERATs to distinguish between translations that are valid for the various modes of operation.
  // See IBM_CBE_Handbook_v1.1 Page 82.

  // Search ERAT's
  if (thread.instrFetch) {
    // iERAT
    RA = thread.iERAT.getElement((*EA & ~0xFFF));
    if (RA != -1) {
      RA |= (*EA & 0xFFF);
      *EA = RA;
      return true;
    }
  } else {
    // dERAT
    RA = thread.dERAT.getElement((*EA & ~0xFFF));
    if (RA != -1) {
      RA |= (*EA & 0xFFF);
      *EA = RA;
      return true;
    }
  }

  // Holds whether the cpu thread issuing the fetch is running in Real or
  // Virtual mode. It defaults to Real Mode, as this is how the XCPU starts
  // its threads
  bool realMode = true;
  // If this EA bit is set, then address generated in Real Mode isn't OR'ed
  // with the contents of HRMOR register
  const bool eaZeroBit = ((*EA & 0x8000000000000000) >> 63);
  // LPCR(LPES) bit 1
  const bool lpcrLPESBit1 = ((LPCR & 0x8) >> 3);
  // Software management of the TLB
  // 0 = Hardware, 1 = Software
  const bool tlbSoftwareManaged = ((LPCR & 0x400) >> 10);

  // Instruction relocate and instruction fetch
  if (_msr.IR && thread.instrFetch) realMode = false;
  // Data fetch
  else if (_msr.DR) realMode = false;

  // Real Addressing Mode
  if (realMode) {
    // If running in Hypervisor Offset mode
    if (_msr.HV) {
      if (eaZeroBit) {
        // Real address is bits 22-63 of Effective Address
        // RA = EA[22:63]
        RA = (*EA & 0x3FFFFFFFFFF);
      } else {
        // RA = (EA[22:43] | HRMOR[22:43]) || EA[44:63]
        RA = (((*EA & 0x3FFFFF00000) | (HRMOR & 0x3FFFFF00000)) | (*EA & 0xFFFFF));
      }
    }
    // Real Offset Mode
    else {
      // RA = (EA[22:43] | RMOR[22:43]) || EA[44:63]
      if (lpcrLPESBit1) {
        RA = (((*EA & 0x3FFFFF00000) | (RMOR & 0x3FFFFF00000)) | (*EA & 0xFFFFF));
      } else {
        // Mode Fault. LPAR Interrupt
        LOG_CRITICAL(Xenon_MMU, "LPAR Interrupt unimplemented.");
      }
    }
  } else {
    //
    // Virtual Mode
    //
    // Page size bits
    u8 p = 0;
    // Large pages
    bool L = 0;
    // Large Page Selector (LP)
    u8 LP = 0;
    // Efective Segment ID
    u64 ESID = QGET(*EA, 0, 35);
    // ESID = ESID << 28;
    //  Virtual Segment ID
    u64 VSID = 0;

    /*** Segmentation ***/
    // 64 bit EA -> 65 bit VA
    // ESID -> VSID

    sSLBEntry currslbEntry;

    bool slbHit = false;
    // Search the SLB to get the VSID
    for (auto& slbEntry : thread.SLB) {
      if (slbEntry.V) {
#ifdef DEBUGP
        if (Config::log.advanced)
          DEBUGP("Checking valid SLB "
                 "(V:0x{:X},LP:0x{:X},C:0x{:X},L:0x{:X},N:0x{:X},Kp:0x{:X},Ks:0x{:X},VSID:0x{:X},ESID:0x{:X},vsidReg:"
                 "0x{:X},esidReg:0x{:X})",
                 static_cast<u32>(slbEntry.V), static_cast<u32>(slbEntry.LP), static_cast<u32>(slbEntry.C),
                 static_cast<u32>(slbEntry.L), static_cast<u32>(slbEntry.N), static_cast<u32>(slbEntry.Kp),
                 static_cast<u32>(slbEntry.Ks), slbEntry.VSID, slbEntry.ESID, slbEntry.vsidReg, slbEntry.esidReg);
#endif
        if (slbEntry.ESID == ESID) {
#ifdef DEBUGP
          if (Config::log.advanced) DEBUGP("SLB Match");
#endif
          // Entry valid & SLB->ESID = EA->VSID
          currslbEntry = slbEntry;
          VSID = slbEntry.VSID;
          L = slbEntry.L;
          LP = slbEntry.LP;
          slbHit = true;
          break;
        }
      }
    }

    // Real Page Number
    u64 RPN = 0;
    // Page
    u32 Page = 0;
    // Byte offset
    u32 Byte = 0;

    // We hit the SLB, get the VA
    if (slbHit) {
      //
      // Virtual Addresss Generation
      //

      // 1. Get the p Size
      p = mmuGetPageSize(ppeState, L, LP);

      // Get our Virtual Address.
      const u64 VA = (VSID << 16) | (*EA & 0xFFFFFFF);

      // Search the tlb for an entry.
      if (mmuSearchTlbEntry(ppeState, &RPN, VA, p, L, LP)) {
        // TLB Hit, proceed.
        goto end;
      } else {
        // TLB miss, if we are in software managed mode, generate an
        // interrupt, else do page table search
        if (tlbSoftwareManaged) {
          if (thread.instrFetch) {
            thread.RaiseExc(ppuInstrStorageEx);
          } else {
            thread.RaiseExc(ppuDataStorageEx);
            thread.SPR.DAR = *EA;
            thread.SPR.DSISR = DSISR_NOPTE;
          }
          return false;
        } else {
          // Page Table Lookup:
          // Walk the Page table to find a Page that translates our current VA

          // Save MSR DR & IR Bits. When an exception occurs they must be reset
          // to whatever they where
          const bool msrDR = thread.SPR.MSR.DR;
          const bool msrIR = thread.SPR.MSR.IR;

          // Disable relocation
          thread.SPR.MSR.DR = 0;
          thread.SPR.MSR.IR = 0;

          // Hash page table lookup
          //
          // SDR1 layout: HTABORG = bits[18:63], HTABSIZE = bits[59:63] (5-bit exponent).
          // HTAB byte size = 2^(HTABSIZE+11) * 128.
          //
          // page_idx = EA page-aligned, masked to segment page index bits [p:27].
          // primary_hash = ((vsid_<<12 XOR page_idx_<<p) >> 5) & PTEG-align-mask.
          // pteg_addr = HTABORG | (hash & (htab_size_mask | 0x3FF80)).

          const u64 htabOrg = ppeState->SPR.SDR1.hexValue & PPC_SPR_SDR_64_HTABORG;
          const u64 htabSize = ppeState->SPR.SDR1.hexValue & PPC_SPR_SDR_64_HTABSIZE;
          const u64 htab_mask = ((1ULL << htabSize) - 1) << 18;

          // Page-aligned EA, bits [p:27] only (the segment page index field).
          const u64 ea_page = *EA & ~((1ULL << p) - 1);
          const u64 page_idx = ea_page & (((1ULL << (28 - p)) - 1) << p);

          const u64 primary_hash = ((VSID ^ page_idx) >> 5) & 0x3FFFFFFFFF80ULL;
          const u64 pteg0Addr = htabOrg | (primary_hash & (htab_mask | 0x3FF80ULL));
          const u64 pteg1Addr = htabOrg | (~primary_hash & (htab_mask | 0x3FF80ULL));

          /*
          The 16-byte PTEs are organized in memory as groups of eight entries,
          called PTE groups (PTEGs), each one a full 128-byte cache line. A
          hardware table lookup consists of searching a primary PTEG and then,
          if necessary, searching a secondary PTEG to find the correct PTE to be
          reloaded into the TLB
          */

          // Hardware searches PTEGs in the following order:
          // 1. Request the even primary PTEG entries
          // 2. Search PTE[0], PTE[2], PTE[4], and PTE[6]
          // 3. Request the odd primary PTEG entries
          // 4. Search PTE[1], PTE[3], PTE[5], and PTE[7]
          // 5. Repeat steps 1 through 4 with the secondary PTE
          // 6. If no match occurs, raise a data storage exception

          // First PTEG
          PPC_HPTE64 pteg0[PPC_HPTES_PER_GROUP];

          // Get the pteg data from memory while relocation is off
          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            pteg0[i].pte0 = PPCInterpreter::MMURead64(ppeState, pteg0Addr + i * 16, thr);
            pteg0[i].pte1 = PPCInterpreter::MMURead64(ppeState, pteg0Addr + i * 16 + 8, thr);
          }

          // We compare all pte's in order for simplicity
          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            /*
                Conditions for a match to occur:

                * PTE: H = 0 for the primary PTEG, 1 for the secondary PTEG
                * PTE: V = 1
                * PTE: AVPN[0:51] = VA0:51
                * if p < 28, PTE: AVPN[52:51+q] = VA[52:51+q]
            */

            // H = 0?
            if (((pteg0[i].pte0 & PPC_HPTE64_HASH) >> 1) != 0) { continue; }

            // Get our VPN for comparisson.
            u64 VPN = (VA >> p) << p;

            // Perform the compare
            if (!mmuComparePTE(VA, VPN, pteg0[i].pte0, pteg0[i].pte1, p, L, LP, &RPN)) { continue; }

            // Match found. Set relocation back to whatever it was
            thread.SPR.MSR.DR = msrDR;
            thread.SPR.MSR.IR = msrIR;

            // Update R (Referenced) and C (Changed) bits in pte1 with a single write.
            // R is set on every access; C is set only on stores.
            // Both bits live in pte1 (PTEL): R=0x100, C=0x80.
            // Do this BEFORE caching into the TLB so the reloaded entry carries
            // the up-to-date R/C state, which is fed the freshly-updated HPTE by the HTAB-walk caller).
            {
              u64 newPte1 = pteg0[i].pte1;
              if (!(newPte1 & PPC_HPTE64_R)) newPte1 |= PPC_HPTE64_R;
              if (memWrite && !(newPte1 & PPC_HPTE64_C)) newPte1 |= PPC_HPTE64_C;
              if (newPte1 != pteg0[i].pte1) {
                MMUWrite64(ppeState, pteg0Addr + i * 16 + 8, newPte1, thr);
                pteg0[i].pte1 = newPte1;
              }
            }

            // Reload the on-chip TLB with this walked HPTE so subsequent
            // accesses to the page hit the TLB instead of re-walking the HTAB
            mmuAddTlbEntryHardware(ppeState, VA, pteg0[i].pte0, pteg0[i].pte1, p, L, LP);

            goto end;
          }

          // Second PTEG
          PPC_HPTE64 pteg1[PPC_HPTES_PER_GROUP];

          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            pteg1[i].pte0 = PPCInterpreter::MMURead64(ppeState, pteg1Addr + i * 16, thr);
            pteg1[i].pte1 = PPCInterpreter::MMURead64(ppeState, pteg1Addr + i * 16 + 8, thr);
          }

          // We compare all pte's in order for simplicity
          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            /*
                Conditions for a match to occur:

                * PTE: H = 0 for the primary PTEG, 1 for the secondary PTEG
                * PTE: V = 1
                * PTE: AVPN[0:51] = VA0:51
                * if p < 28, PTE: AVPN[52:51+q] = VA[52:51+q]
            */

            // H = 0?
            if (((pteg1[i].pte0 & PPC_HPTE64_HASH) >> 1) != 1) { continue; }

            // Get our VPN for comparisson.
            u64 VPN = (VA >> p) << p;

            // Perform the compare
            if (!mmuComparePTE(VA, VPN, pteg1[i].pte0, pteg1[i].pte1, p, L, LP, &RPN)) { continue; }

            // Match found. Set relocation back to whatever it was
            thread.SPR.MSR.DR = msrDR;
            thread.SPR.MSR.IR = msrIR;

            // Update R/C bits in pte1 with a single write (same logic as primary PTEG).
            {
              u64 newPte1 = pteg1[i].pte1;
              if (!(newPte1 & PPC_HPTE64_R)) newPte1 |= PPC_HPTE64_R;
              if (memWrite && !(newPte1 & PPC_HPTE64_C)) newPte1 |= PPC_HPTE64_C;
              if (newPte1 != pteg1[i].pte1) {
                MMUWrite64(ppeState, pteg1Addr + i * 16 + 8, newPte1, thr);
                pteg1[i].pte1 = newPte1;
              }
            }

            // Reload the on-chip TLB (secondary-PTEG hit). pte0 here has H=1;
            // mmuAddTlbEntryHardware stores it with H cleared.
            mmuAddTlbEntryHardware(ppeState, VA, pteg1[i].pte0, pteg1[i].pte1, p, L, LP);

            goto end;
          }

          // Set MSR to IR/DR mode before raising the interrupt to whatever they
          // were
          thread.SPR.MSR.DR = msrDR;
          thread.SPR.MSR.IR = msrIR;

          // Page Table Lookup Fault
          // Issue Data/Instr Storage interrupt

          // Instruction read
          if (thread.instrFetch) {
            thread.RaiseExc(ppuInstrStorageEx);

          } else if (memWrite) {
            // Data write
            thread.RaiseExc(ppuDataStorageEx);
            thread.SPR.DAR = *EA;
            thread.SPR.DSISR = DSISR_NOPTE | DSISR_ISSTORE;
          } else {
            // Data read
            thread.RaiseExc(ppuDataStorageEx);
            thread.SPR.DAR = *EA;
            thread.SPR.DSISR = DSISR_NOPTE;
          }
          return false;
        }
      }
    } else {
      // SLB Miss
      // Data or Inst Segment Exception
      if (thread.instrFetch) {
        thread.RaiseExc(ppuInstrSegmentEx);
      } else {
        thread.RaiseExc(ppuDataSegmentEx);
        thread.SPR.DAR = *EA;
      }
      return false;
    }

  end:
    RA = (RPN | QGET(*EA, 64 - p, 63));
    // Real Address 0 - 21 bits are not implemented
    QSET(RA, 0, 21, 0);
  }

  // Save in ERAT's
  if (thread.instrFetch) {
    // iERAT
    thread.iERAT.putElement((*EA & ~0xFFF), (RA & ~0xFFF));
  } else {
    // dERAT
    thread.dERAT.putElement((*EA & ~0xFFF), (RA & ~0xFFF));
  }

  *EA = RA;
  return true;
}

// MMU Read Routine, used by the CPU
void PPCInterpreter::MMURead(Xe::XCPU::XenonContext* cpuContext, sPPEState* ppeState, u64 EA, u64 byteCount,
                             u8* outData, ePPUThreadID thr) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMURead", MP_AUTO);
  sPPUThread& thread = ppeState->ppuThread[thr != ePPUThread_None ? thr : curThreadId];

  const u64 oldEA = EA;
  if (!MMUTranslateAddress(&EA, ppeState, false, thr)) {
    memset(outData, 0, byteCount);
    return;
  }
  bool socRead = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socRead);

  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write
  if (((oldEA & 0x000000007FFF0000ULL) >> 16) == 0x7FFF) socRead = true;

  // Debugger halt
  if (EA && EA == Config::debug.haltOnReadAddress && XeMain::GetCPU()) {
    XeMain::GetCPU()->Halt();         // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger after halting
  }
  // Handle SoC reads
  if (socRead) {
    // Check if the read is from the SROM
    if (EA >= XE_SROM_ADDR && EA < XE_SROM_ADDR + XE_SROM_SIZE) {
      const u32 sromAddr = static_cast<u32>(EA - XE_SROM_ADDR);
      memcpy(outData, &cpuContext->SROM[sromAddr], byteCount);
      return;
    }
    // Check if the read is from SRAM
    else if (EA >= XE_SRAM_ADDR && EA < XE_SRAM_ADDR + XE_SRAM_SIZE) {
      const u32 sramAddr = static_cast<u32>(EA - XE_SRAM_ADDR);
      memcpy(outData, &cpuContext->SRAM[sramAddr], byteCount);
      return;
    }
    // Integrated Interrupt Controller in real mode, used when the HV wants to
    // start a CPUs IC
    else if (EA >= XE_SOCINTS_BLOCK_START && EA <= XE_SOCINTS_BLOCK_START + XE_SOCINTS_BLOCK_SIZE) {
      // Pass it onto our context INT struct.
      cpuContext->HandleSOCRead(EA, outData, byteCount);
      return;
    }
    // Try to handle the SoC read, may belong to one of the CPU SoC blocks.
    else if (cpuContext->HandleSOCRead(EA, outData, byteCount)) {
      return;
    }
  }

  // External read
  if (!xenonContext->GetRootBus()->Read(EA, outData, byteCount, socRead) && socRead) {
    if (Config::log.advanced) LOG_WARNING(Xenon_MMU, "Invalid SoC Read from 0x{:X}", EA);
  }

  // TODO: Investigate why FSB_CONFIG_RX_STATE needs these values to work
  switch (thread.CIA) {
    case 0x1003590ULL: {
      u64 patchData = 0xEEEEEEEEEEEEEEEE;
      memcpy(outData, &patchData, byteCount);
    } break;
    case 0x100363CULL: {
      u64 patchData = 0x2222222222222222;
      memcpy(outData, &patchData, byteCount);
    } break;
  }
}

// MMU Write Routine, used by the CPU
void PPCInterpreter::MMUWrite(Xe::XCPU::XenonContext* cpuContext, sPPEState* ppeState, const u8* data, u64 EA,
                              u64 byteCount, ePPUThreadID thr) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUWrite", MP_AUTO);
  const u64 oldEA = EA;

  if (!MMUTranslateAddress(&EA, ppeState, true, thr)) return;

  // Check if it's reserved
  cpuContext->xenonRes.Check(EA);

  bool socWrite = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socWrite);

  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write
  if (((oldEA & 0x000000007FFFF0000ULL) >> 16) == 0x7FFF) socWrite = true;

  // Debugger halt
  if (EA && EA == Config::debug.haltOnWriteAddress && XeMain::GetCPU()) {
    XeMain::GetCPU()->Halt();         // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger after halting
  }

  if (socWrite) {
#ifdef DEBUGP
    if (EA == 0x61010ULL) {
      u64 postCode = *reinterpret_cast<const u64*>(data);
      std::string poseCodeStr = Xe::XCPU::POSTBUS::GET_POST(postCode);
      PPU* PPU = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
      if (PPU->traceFile) { fprintf(PPU->traceFile, "POST,0x%llx,%s\n", postCode, poseCodeStr.c_str()); }
    }
#endif
    // Check if writing to SROM region.
    if (EA >= XE_SROM_ADDR && EA < XE_SROM_ADDR + XE_SROM_SIZE) {
      LOG_ERROR(Xenon_MMU, "Tried to write to XCPU SROM!");
      return;
    }
    // Check if writing to internal SRAM.
    else if (EA >= XE_SRAM_ADDR && EA < XE_SRAM_ADDR + XE_SRAM_SIZE) {
      u32 sramAddr = static_cast<u32>(EA - XE_SRAM_ADDR);
      memcpy(&cpuContext->SRAM[sramAddr], data, byteCount);
      return;
    }
    // Integrated Interrupt Controller in real mode, used when the HV wants to
    // start a CPUs IC.
    else if (EA >= XE_SOCINTS_BLOCK_START && EA <= XE_SOCINTS_BLOCK_START + XE_SOCINTS_BLOCK_SIZE) {
      cpuContext->HandleSOCWrite(EA, data, byteCount);
      return;
    }
    // Try to handle the SoC write, may belong to one of the CPU SoC blocks.
    else if (cpuContext->HandleSOCWrite(EA, data, byteCount)) {
      return;
    }
  }

  // External write
  if (!xenonContext->GetRootBus()->Write(EA, data, byteCount, socWrite) && socWrite) {
    u64 tmp = 0;
    memcpy(&tmp, data, byteCount);
    if (Config::log.advanced) LOG_WARNING(Xenon_MMU, "Invalid SoC Write to 0x{:X}", EA);
  }
}

void PPCInterpreter::MMUMemCpyFromHost(sPPEState* ppeState, u64 EA, const void* source, u64 size, ePPUThreadID thr) {
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(source), EA, size);
}

void PPCInterpreter::MMUMemCpy(sPPEState* ppeState, u64 EA, u32 source, u64 size, ePPUThreadID thr) {
  std::unique_ptr<u8[]> data = std::make_unique<STRIP_UNIQUE_ARR(data)>(size);
  MMURead(xenonContext, ppeState, source, size, data.get(), thr);
  MMUWrite(xenonContext, ppeState, data.get(), EA, size, thr);
  data.reset();
}

void PPCInterpreter::MMUMemSet(sPPEState* ppeState, u64 EA, s32 data, u64 size, ePPUThreadID thr) {
  const u64 oldEA = EA;

  if (MMUTranslateAddress(&EA, ppeState, true, thr) == false) return;

  if (!xenonContext) return;

  // Check if it's reserved
  xenonContext->xenonRes.Check(EA);

  bool socWrite = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socWrite);
  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write
  if (((oldEA & 0x000000007FFFF0000ULL) >> 16) == 0x7FFF) socWrite = true;
  if (socWrite) {
    switch (EA) {
      default: {
        // Check if writing to bootloader section
        if (EA >= XE_SROM_ADDR && EA < XE_SROM_ADDR + XE_SROM_SIZE) {
          LOG_ERROR(Xenon_MMU, "Tried to write to XCPU SROM!");
          return;
        }
        // Check if writing to internal SRAM
        else if (EA >= XE_SRAM_ADDR && EA < XE_SRAM_ADDR + XE_SRAM_SIZE) {
          const u32 sramAddr = static_cast<u32>(EA - XE_SRAM_ADDR);
          memset(&xenonContext->SRAM[sramAddr], data, size);
          return;
        }
        // Check if writing to Security Engine Config Block
        else if (EA >= XE_SOCSECENG_BLOCK_START && EA < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
          const u32 secEngOffset = static_cast<u32>(EA - XE_SOCSECENG_BLOCK_START);
          memset(reinterpret_cast<u8*>(xenonContext->socSecEngBlock.get()) + secEngOffset, 0, size);
          return;
        }
      } break;
    }
  }

  // External MemSet
  xenonContext->GetRootBus()->MemSet(EA, data, size);
}

u8* PPCInterpreter::MMUGetPointerFromRAM(u64 EA) { return xenonContext->GetRAM()->GetPointerToAddress(EA); }

// Reads 1 byte of memory
u8 PPCInterpreter::MMURead8(sPPEState* ppeState, u64 EA, ePPUThreadID thr) {
  u8 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return data;
}
// Reads 2 bytes of memory
u16 PPCInterpreter::MMURead16(sPPEState* ppeState, u64 EA, ePPUThreadID thr) {
  u16 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return byteswap_be<u16>(data);
}
// Reads 4 bytes of memory
u32 PPCInterpreter::MMURead32(sPPEState* ppeState, u64 EA, ePPUThreadID thr) {
  u32 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return byteswap_be<u32>(data);
}
// Reads 8 bytes of memory
u64 PPCInterpreter::MMURead64(sPPEState* ppeState, u64 EA, ePPUThreadID thr) {
  u64 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return byteswap_be<u64>(data);
}
// Writes 1 byte to memory
void PPCInterpreter::MMUWrite8(sPPEState* ppeState, u64 EA, u8 data, ePPUThreadID thr) {
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&data), EA, sizeof(data), thr);
}
// Writes 2 bytes to memory
void PPCInterpreter::MMUWrite16(sPPEState* ppeState, u64 EA, u16 data, ePPUThreadID thr) {
  const u16 dataBS = byteswap_be<u16>(data);
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
}
// Writes 4 bytes to memory
void PPCInterpreter::MMUWrite32(sPPEState* ppeState, u64 EA, u32 data, ePPUThreadID thr) {
  const u32 dataBS = byteswap_be<u32>(data);
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
}
// Writes 8 bytes to memory
void PPCInterpreter::MMUWrite64(sPPEState* ppeState, u64 EA, u64 data, ePPUThreadID thr) {
  const u64 dataBS = byteswap_be<u64>(data);
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
}
