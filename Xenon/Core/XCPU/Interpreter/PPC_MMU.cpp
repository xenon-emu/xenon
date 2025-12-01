/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Global.h"
#include "Core/PCI/Devices/NAND/NAND.h"
#include "Core/XCPU/Context/PostBus/PostBus.h"
#include "Core/XCPU/XenonCPU.h"

#include "PPCInterpreter.h"

//#define MMU_DEBUG

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

#define MMU_PAGE_SIZE_4KB 12
#define MMU_PAGE_SIZE_64KB 16
#define MMU_PAGE_SIZE_1MB 20
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

#define PPC_SPR_SDR_64_HTABORG 0x0FFFFFFFFFFC0000ULL
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
#define DSISR_NOPTE 0x40000000

// Page table entry structure.
struct PPC_HPTE64 {
  u64 pte0;
  u64 pte1;
};

inline bool mmuComparePTE(u64 VA, u64 VPN, u64 pte0, u64 pte1, u8 p, bool L, bool LP, u64 *RPN) {
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

  if (!pteV) {
    return false;
  }

  if (pteAVPN_0_51 != (VA & 0xFFFFFFFFF0000000)) {
    return false;
  }

  if (L != pteL) {
    LOG_DEBUG(Xenon_MMU, "L mismatch: L={}, PTE[L]={}", L, pteL);
    return false;
  }

  if (L && LP != pteLP) {
    LOG_DEBUG(Xenon_MMU, "LP mismatch: LP={}, PTE[LP]={}", LP, pteLP);
    return false;
  }

  bool match = false;

  // Behave differently for pre-calculated VPN's.
  u64 compareMask = 0;
  switch (p) {
  case MMU_PAGE_SIZE_4KB:   compareMask = 0xFFFFFFFFFFF00000; break; // VA[0:59]
  case MMU_PAGE_SIZE_64KB:  compareMask = 0xFFFFFFFFFF000000; break; // VA[0:55]
  case MMU_PAGE_SIZE_16MB:  compareMask = 0xFFFFFFFF00000000; break; // VA[0:47]
  }

  const u64 pteVPNAndMask = VPN & compareMask;
  const u64 vaAndMask = VA & compareMask;

  if (pteVPNAndMask == vaAndMask) { match = true; }

  // Match
  if (match) {
    // RPN = PTE[86:114] : PTE[86:115].
    *RPN = L ? pte1 & PPC_HPTE64_RPN_LP : pte1 & PPC_HPTE64_RPN_NO_LP;
  }

  return match;
}

// SLB Invalidate All
void PPCInterpreter::PPCInterpreter_slbia(sPPEState *ppeState) {
  for (auto &slbEntry : curThread.SLB) {
    slbEntry.V = 0;
  }
  // Invalidate both ERAT's
  curThread.iERAT.invalidateAll();
  curThread.dERAT.invalidateAll();
}

// TLB Invalidate Entry Local
void PPCInterpreter::PPCInterpreter_tlbiel(sPPEState *ppeState) {
  // The PPU adds two new fields to this instruction, them being LP and IS.

  const bool LP = (GPRi(rb) & 0x1000) >> 12;
  const bool invalSelector = (GPRi(rb) & 0x800) >> 11;
  const u8 p = mmuGetPageSize(ppeState, _instr.l10, LP);

  if (invalSelector) {
    // Index to one of the 256 rows of the tlb. Possible entire tlb
    // invalidation.
    u8 tlbCongruenceClass = 0;
    const u64 rb_44_51 = (GPRi(rb) & 0xFF000) >> 12;

    ppeState->TLB.tlbSet0[rb_44_51].V = false;
    ppeState->TLB.tlbSet0[rb_44_51].pte0 = 0;
    ppeState->TLB.tlbSet0[rb_44_51].pte1 = 0;

    ppeState->TLB.tlbSet1[rb_44_51].V = false;
    ppeState->TLB.tlbSet1[rb_44_51].pte0 = 0;
    ppeState->TLB.tlbSet1[rb_44_51].pte1 = 0;

    ppeState->TLB.tlbSet2[rb_44_51].V = false;
    ppeState->TLB.tlbSet2[rb_44_51].pte0 = 0;
    ppeState->TLB.tlbSet2[rb_44_51].pte1 = 0;

    ppeState->TLB.tlbSet3[rb_44_51].V = false;
    ppeState->TLB.tlbSet3[rb_44_51].pte0 = 0;
    ppeState->TLB.tlbSet3[rb_44_51].pte1 = 0;

    // Should only invalidate entries for a specific set of addresses.
    // Invalidate both ERAT's *** BUG *** !!!
    curThread.iERAT.invalidateAll();
    curThread.dERAT.invalidateAll();

    // Invalidate JIT blocks conservatively (full set invalidation).
    if (XeMain::GetCPU()) {
      PPU *ppu = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
      if (ppu && ppu->GetPPUJIT()) {
#ifdef MMU_DEBUG
        LOG_DEBUG(Xenon_MMU, "[TLBIEL]: Congruence-class invalidation (class {:#x}). Invalidating all JIT blocks.", rb_44_51);
#endif
        ppu->GetPPUJIT()->InvalidateAllBlocks();
      }
    }
  } else {
    // The TLB is as selective as possible when invalidating TLB entries.The
    // invalidation match criteria is VPN[38:79 - p], L, LP, and LPID.

    const u64 rb = GPRi(rb);
    u64 rpn = 0;

    u64 compareMask = 0;

#ifdef MMU_DEBUG
    LOG_DEBUG(Xenon_MMU, "[TLBIEL]: Attempting to find entry for RB {:#x}", rb);
#endif // MMU_DEBUG

    // TODO(bitsh1ft3r): Investigate this behavior. Why do 64kb and 16 mb behave the same?
    // and why doesn't is work as docs dictate.
    switch (p) {
    case MMU_PAGE_SIZE_4KB:   compareMask = 0xFFFFFFFFFFF00000; break;
    case MMU_PAGE_SIZE_64KB:  compareMask = 0xFFFFFFFFFF000000; break;
    case MMU_PAGE_SIZE_16MB:  compareMask = 0xFFFFFFFFFF000000; break;
    }

    for (auto &tlbEntry : ppeState->TLB.tlbSet0) {
      if (tlbEntry.V && ((tlbEntry.VPN & compareMask) == (rb & compareMask))) {
#ifdef DEBUG_BUILD
          LOG_TRACE(Xenon_MMU, "[TLB]: TLBIEL: Invalidating entry with VPN: {:#x}", tlbEntry.VPN);
#endif
        tlbEntry.V = false;
        tlbEntry.VPN = 0;
        tlbEntry.pte0 = 0;
        tlbEntry.pte1 = 0;
      }
    }
    for (auto &tlbEntry : ppeState->TLB.tlbSet1) {
      if (tlbEntry.V && ((tlbEntry.VPN & compareMask) == (rb & compareMask))) {
#ifdef DEBUG_BUILD
        LOG_TRACE(Xenon_MMU, "[TLB]: TLBIEL: Invalidating entry with VPN: {:#x}", tlbEntry.VPN);
#endif
        tlbEntry.V = false;
        tlbEntry.VPN = 0;
        tlbEntry.pte0 = 0;
        tlbEntry.pte1 = 0;
      }
    }
    for (auto &tlbEntry : ppeState->TLB.tlbSet2) {
      if (tlbEntry.V && ((tlbEntry.VPN & compareMask) == (rb & compareMask))) {
#ifdef DEBUG_BUILD
        LOG_TRACE(Xenon_MMU, "[TLB]: TLBIEL: Invalidating entry with VPN: {:#x}", tlbEntry.VPN);
#endif
        tlbEntry.V = false;
        tlbEntry.VPN = 0;
        tlbEntry.pte0 = 0;
        tlbEntry.pte1 = 0;
      }
    }
    for (auto &tlbEntry : ppeState->TLB.tlbSet3) {
      if (tlbEntry.V && ((tlbEntry.VPN & compareMask) == (rb & compareMask))) {
#ifdef DEBUG_BUILD
        LOG_TRACE(Xenon_MMU, "[TLB]: TLBIEL: Invalidating entry with VPN: {:#x}", tlbEntry.VPN);
#endif
        tlbEntry.V = false;
        tlbEntry.VPN = 0;
        tlbEntry.pte0 = 0;
        tlbEntry.pte1 = 0;
      }
    }
    // Should only invalidate entries for a specific set of addresses.
    // Invalidate both ERAT's *** BUG *** !!!
    curThread.iERAT.invalidateAll();
    curThread.dERAT.invalidateAll();

    // Invalidate JIT blocks that map to the page/rango afectado por RB/p
    if (XeMain::GetCPU()) {
      PPU *ppu = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
      if (ppu && ppu->GetPPUJIT()) {
        // Get page range based on 'p' (p = log2(pageSize))
        u64 pageSize = (p < 64) ? (1ULL << p) : 0ULL;
        if (pageSize == 0) {
          // Fallback: full cache invalidation. (should never happen but for safety).
#ifdef MMU_DEBUG
          LOG_DEBUG(Xenon_MMU, "[TLBIEL]: Unknown page size (p={}), invalidating all JIT blocks", p);
#endif
          ppu->GetPPUJIT()->InvalidateAllBlocks();
        } else {
          u64 start = rb & ~(pageSize - 1ULL);
          u64 end = start + pageSize;
#ifdef MMU_DEBUG
          LOG_DEBUG(Xenon_MMU, "[TLBIEL]: Invalidating JIT blocks for page {:#x} (size {:#x})", start, pageSize);
#endif
          ppu->GetPPUJIT()->InvalidateBlocksForRange(start, end);
        }
      }
    }
  }
}

/*
  The PowerPC instruction tlbie searches the Translation Look-Aside Buffer (TLB) for an entry corresponding to the effective address (EA).
  The search is done regardless of the setting of Machine State Register (MSR) Instruction Relocate bit or the MSR Data Relocate bit.
  The search uses a portion of the EA including the least significant bits, and ignores the content of the Segment Registers.
  Entries that satisfy the search criteria are made invalid so will not be used to translate subsequent storage accesses.
*/
// rB is the GPR containing the EA for the search
// L is the page size

// TLB Invalidate Entry
void PPCInterpreter::PPCInterpreter_tlbie(sPPEState *ppeState) {
  const u64 EA = GPRi(rb);
  bool LP = (GPRi(rb) & 0x1000) >> 12;
  const u8 p = mmuGetPageSize(ppeState, _instr.l10, LP);
  // Inverse of log2, as log2 is 2^? (finding ?)
  // Example: the Log2 of 4096 is 12, because 2^12 is 4096
  u64 fullPageSize = pow(2, p);
#ifdef DEBUG_BUILD
  if (Config::log.advanced)
    LOG_TRACE(Xenon, "tlbie, EA:0x{:X} | PageSize:{} | Full:0x{:X},{} | LP:{}", EA, p, fullPageSize, fullPageSize, LP ? "true" : "false");
#endif
  for (u64 i{}; i != fullPageSize; ++i) {
    curThread.iERAT.invalidateElement(EA+i);
    curThread.dERAT.invalidateElement(EA+i);
  }

  // Invalidate JIT blocks that map to the page/rango afectado por RB/p
  if (XeMain::GetCPU()) {
    PPU *ppu = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
    if (ppu && ppu->GetPPUJIT()) {
      // Get page range based on 'p' (p = log2(pageSize))
      u64 pageSize = (p < 64) ? (1ULL << p) : 0ULL;
      if (pageSize == 0) {
        // Fallback: full cache invalidation. (should never happen but for safety).
#ifdef MMU_DEBUG
        LOG_DEBUG(Xenon_MMU, "[TLBIE]: Unknown page size (p={}), invalidating all JIT blocks", p);
#endif
        ppu->GetPPUJIT()->InvalidateAllBlocks();
      }
      else {
        u64 start = EA & ~(pageSize - 1ULL);
        u64 end = start + pageSize;
#ifdef MMU_DEBUG
        LOG_DEBUG(Xenon_MMU, "[TLBIE]: Invalidating JIT blocks for page {:#x} (size {:#x})", start, pageSize);
#endif
        ppu->GetPPUJIT()->InvalidateBlocksForRange(start, end);
      }
    }
  }
}

// TLB Synchronize
void PPCInterpreter::PPCInterpreter_tlbsync(sPPEState *ppeState) {
  // Do nothing
#ifdef DEBUG_BUILD
  if (Config::log.advanced)
    LOG_TRACE(Xenon, "tlbsync");
#endif
}

// Helper function for getting Page Size (p bit).
u8 PPCInterpreter::mmuGetPageSize(sPPEState *ppeState, bool L, u8 LP) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUGetPageSize", MP_AUTO);

  // Large page selection works the following way:
  // First check if pages are large (L)
  // if (L) the page size can be one of two defined pages. On the XBox 360,
  // MS decided to use two of the three page sizes, 64Kb and 16Mb.
  // Selection between them is made using bits 16 - 19 of HID6 SPR.

  // HID6 16-17 bits select Large Page size 1.
  // HID6 18-19 bits select Large Page size 2.
  const u8 LB_16_17 = (ppeState->SPR.HID6.LB & 0b1100) >> 2;
  const u8 LB_18_19 = ppeState->SPR.HID6.LB & 0b11;

  // Final p size.
  u8 p = 0;
  // Page size in decimal.
  u32 pSize = 0;

  // Large page?
  if (L == 0) {
    // If L equals 0, the small page size is used, 4Kb in this case.
    pSize = 4096;
  } else {
    // Large Page Selector
    if (LP == 0) {
      switch (LB_16_17) {
      case 0b0000:
        pSize = 16777216; // 16 Mb page size
        break;
      case 0b0001:
        pSize = 1048576; // 1 Mb page size
        break;
      case 0b0010:
        pSize = 65536; // 64 Kb page size
        break;
      }
    } else if (LP == 1) {
      switch (LB_18_19) {
      case 0b0000:
        pSize = 16777216; // 16 Mb page size
        break;
      case 0b0001:
        pSize = 1048576; // 1 Mb page size
        break;
      case 0b0010:
        pSize = 65536; // 64 Kb page size
        break;
      }
    }
  }

  // p size is Log(2) of Page Size.
  p = static_cast<u8>(log2(pSize));
  return p;
}

// This is done when TLB Reload is in software-controlled mode.
void PPCInterpreter::mmuAddTlbEntry(sPPEState *ppeState) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUAddTlbEntry", MP_AUTO);
  // In said mode, software makes use of special registers of the CPU to directly reload the TLB
  // with PTE's, thus eliminating the need of a hardware page table and tablewalk.

#define MMU_GET_TLB_INDEX_TI(x)   (static_cast<u16>((x & 0xFF0) >> 4))
#define MMU_GET_TLB_INDEX_TS(x)   (static_cast<u16>(x & 0xF))
#define MMU_GET_TLB_INDEX_LVPN(x) (static_cast<u64>((x & 0xE00000000000) >> 25))

  const u64 tlbIndex = ppeState->SPR.PPE_TLB_Index.hexValue;
  const u64 tlbVpn = ppeState->SPR.PPE_TLB_VPN.hexValue;
  const u64 tlbRpn = ppeState->SPR.PPE_TLB_RPN.hexValue;

  // TLB Index (0 - 255) of current tlb set.
  const u16 TI = MMU_GET_TLB_INDEX_TI(tlbIndex);
  // TLB Set.
  const u16 TS = MMU_GET_TLB_INDEX_TS(tlbIndex);

  //  The abbreviated virtual page number (AVPN)[0:56] corresponds to VPN[0:56].
  const u64 AVPN = (tlbVpn & PPC_HPTE64_AVPN) << 16;
  // LVPN[0:2] corresponds to VPN[57:59].
  const u64 LVPN = MMU_GET_TLB_INDEX_LVPN(tlbIndex);

  // Our PTE VPN, pre calculated for ease of use.
  const u64 VPN = AVPN | LVPN;

#ifdef DEBUG_BUILD
  if (XeMain::GetCPU()) {
    PPU *ppu = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
    if (ppu && ppu->traceFile) {
      fprintf(ppu->traceFile, "TLB[%d:%d] map 0x%llx -> 0x%llx\n", TS, TI, tlbVpn, tlbRpn);
    }
  }
#endif

  LOG_TRACE(Xenon_MMU, "[TLB]: Adding entry: TLB Set: {:#d}, TLB Index: {:#x}, VPN: {:#x}, PTE VPN: {:#x}, PTE RPN: {:#x}",
    TS, TI, VPN, tlbVpn, tlbRpn);

  // TLB set to choose from
  // There are 4 sets of 256 entries each:
  switch (TS) {
  case 0b1000:
    ppeState->TLB.tlbSet0[TI].V = true;
    ppeState->TLB.tlbSet0[TI].VPN = VPN;
    ppeState->TLB.tlbSet0[TI].pte0 = tlbVpn;
    ppeState->TLB.tlbSet0[TI].pte1 = tlbRpn;
    break;
  case 0b0100:
    ppeState->TLB.tlbSet1[TI].V = true;
    ppeState->TLB.tlbSet1[TI].VPN = VPN;
    ppeState->TLB.tlbSet1[TI].pte0 = tlbVpn;
    ppeState->TLB.tlbSet1[TI].pte1 = tlbRpn;
    break;
  case 0b0010:
    ppeState->TLB.tlbSet2[TI].V = true;
    ppeState->TLB.tlbSet2[TI].VPN = VPN;
    ppeState->TLB.tlbSet2[TI].pte0 = tlbVpn;
    ppeState->TLB.tlbSet2[TI].pte1 = tlbRpn;
    break;
  case 0b0001:
    ppeState->TLB.tlbSet3[TI].V = true;
    ppeState->TLB.tlbSet3[TI].VPN = VPN;
    ppeState->TLB.tlbSet3[TI].pte0 = tlbVpn;
    ppeState->TLB.tlbSet3[TI].pte1 = tlbRpn;
    break;
  }
}

// Translation Lookaside Buffer Search
bool PPCInterpreter::mmuSearchTlbEntry(sPPEState *ppeState, u64 *RPN, u64 VA, u8 p, bool L, bool LP) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUSearchTlbEntry", MP_AUTO);
  // Index to choose from the 256 ways of the TLB
  u16 tlbIndex = 0;
  // Tlb Set that was least Recently used for replacement.
  u8 tlbSet = 0;

  // 4 Kb - (VA[52:55] xor VA[60:63]) || VA[64:67]
  // 64 Kb - (VA[52:55] xor VA[56:59]) || VA[60:63]
  // 16MB - VA[48:55]

  // 52-55 bits of 80 VA
  const u16 bits36_39 = static_cast<u16>(QGET(VA, 36, 39));
  // 56-59 bits of 80 VA
  const u16 bits40_43 = static_cast<u16>(QGET(VA, 40, 43));
  // 60-63 bits of 80 VA
  const u16 bits44_47 = static_cast<u16>(QGET(VA, 44, 47));
  // 64-67 bits of 80 VA
  const u16 bits48_51 = static_cast<u16>(QGET(VA, 48, 51));
  // 48-55 bits of 80 VA
  const u16 bits32_39 = static_cast<u16>(QGET(VA, 32, 39));

  switch (p) {
  case MMU_PAGE_SIZE_64KB:
    tlbIndex = ((bits36_39 ^ bits40_43) << 4 | bits44_47);
    break;
  case MMU_PAGE_SIZE_16MB:
    tlbIndex = (VA >> 24) & 0xFF;
    break;
  default:
    // p = 12 bits, 4KB
    tlbIndex = ((bits36_39 ^ bits44_47) << 4 | bits48_51);
    break;
  }

  //
  // Compare each valid entry at specified index in the TLB with the VA.
  //
  if (ppeState->TLB.tlbSet0[tlbIndex].V) {
    if (mmuComparePTE(VA, ppeState->TLB.tlbSet0[tlbIndex].VPN, ppeState->TLB.tlbSet0[tlbIndex].pte0, 
      ppeState->TLB.tlbSet0[tlbIndex].pte1, p, L, LP, RPN)) {
      return true;
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b1000;
  }
  if (ppeState->TLB.tlbSet1[tlbIndex].V) {
    if (mmuComparePTE(VA, ppeState->TLB.tlbSet1[tlbIndex].VPN, ppeState->TLB.tlbSet1[tlbIndex].pte0, 
      ppeState->TLB.tlbSet1[tlbIndex].pte1, p, L, LP, RPN)) {
      return true;
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b0100;
  }
  if (ppeState->TLB.tlbSet2[tlbIndex].V) {
    if (mmuComparePTE(VA, ppeState->TLB.tlbSet2[tlbIndex].VPN, ppeState->TLB.tlbSet2[tlbIndex].pte0, 
      ppeState->TLB.tlbSet2[tlbIndex].pte1, p, L, LP, RPN)) {
      return true;
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b0010;
  }
  if (ppeState->TLB.tlbSet3[tlbIndex].V) {
    if (mmuComparePTE(VA, ppeState->TLB.tlbSet3[tlbIndex].VPN, ppeState->TLB.tlbSet3[tlbIndex].pte0,
      ppeState->TLB.tlbSet3[tlbIndex].pte1, p, L, LP, RPN)) {
      return true;
    }
  } else {
    // Entry was invalid, make this set the a candidate for refill.
    tlbSet = 0b0001;
  }

  // If the PPE is running on TLB Software managed mode, then this SPR
  // is updated every time a Data or Instr Storage Exception occurs. This
  // ensures that the next time that the tlb software updates via an
  // interrupt, the index for replacement is not conflictive.
  // On normal conditions this is done for the LRU index of the TLB.

  // Software management of the TLB. 0 = Hardware, 1 = Software.
  bool tlbSoftwareManaged = ((ppeState->SPR.LPCR.hexValue & 0x400) >> 10);

  if (tlbSoftwareManaged) {
    u64 tlbIndexHint =
      curThread.SPR.PPE_TLB_Index_Hint.hexValue;
    u8 currentTlbSet = tlbIndexHint & 0xF;
    u8 currentTlbIndex = static_cast<u8>(tlbIndexHint & 0xFF0) >> 4;
    currentTlbSet = tlbSet;
    if (currentTlbIndex == 0xFF) {
      if (currentTlbSet == 8) {
        currentTlbSet = 1;
      }
      else {
        currentTlbSet = currentTlbSet << 1;
      }
    }

    if (currentTlbSet == 0)
      currentTlbSet = 1;

    tlbIndex = tlbIndex << 4;
    tlbIndexHint = tlbIndex |= currentTlbSet;
    curThread.SPR.PPE_TLB_Index_Hint.hexValue = tlbIndexHint;
  }
  return false;
}

// Routine to read a string from memory, using a PSTRNG given by the kernel.
void PPCInterpreter::mmuReadString(sPPEState *ppeState, u64 stringAddress,
                                   char *string, u32 maxLength) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUReadString", MP_AUTO);
  u32 strIndex;
  u32 stringBufferAddress = 0;
  const u16 strLength = MMURead16(ppeState, stringAddress);

  if (strLength < maxLength)
    maxLength = strLength + 1;

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
  case 0x0:
    addressInfo.regionType = SECENG_REGION_PHYS;
    break;
  case 0x100:
    addressInfo.regionType = SECENG_REGION_HASHED;
    break;
  case 0x200:
    addressInfo.regionType = SECENG_REGION_SOC;
    break;
  case 0x300:
    addressInfo.regionType = SECENG_REGION_ENCRYPTED;
    break;
  default:
    break;
  }
  return addressInfo;
}

u64 PPCInterpreter::mmuContructEndAddressFromSecEngAddr(u64 inputAddress,
                                                        bool *socAccess) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUContructEndAddressFromSecEngAddr", MP_AUTO);
  SECENG_ADDRESS_INFO inputAddressInfo =
      mmuGetSecEngInfoFromAddress(inputAddress);

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
  default:
    break;
  }
  return outputAddress;
}

// Main address translation mechanism used on the XCPU.
bool PPCInterpreter::MMUTranslateAddress(u64 *EA, sPPEState *ppeState,
                                         bool memWrite, ePPUThreadID thr) {
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
  sPPUThread &thread = ppeState->ppuThread[thr != ePPUThread_None ? thr : curThreadId];

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
  if (!_msr.SF)
    *EA = static_cast<u32>(*EA);

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
  }
  else {
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
  if (_msr.IR && thread.instrFetch)
    realMode = false;
  // Data fetch
  else if (_msr.DR)
    realMode = false;

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
        RA = (((*EA & 0x3FFFFF00000) | (HRMOR & 0x3FFFFF00000)) |
              (*EA & 0xFFFFF));
      }
    }
    // Real Offset Mode
    else {
      // RA = (EA[22:43] | RMOR[22:43]) || EA[44:63]
      if (lpcrLPESBit1) {
        RA = (((*EA & 0x3FFFFF00000) | (RMOR & 0x3FFFFF00000)) |
              (*EA & 0xFFFFF));
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
    for (auto &slbEntry : thread.SLB) {
      if (slbEntry.V) {
#ifdef DEBUG_BUILD
        if (Config::log.advanced)
          LOG_TRACE(Xenon_MMU, "Checking valid SLB (V:0x{:X},LP:0x{:X},C:0x{:X},L:0x{:X},N:0x{:X},Kp:0x{:X},Ks:0x{:X},VSID:0x{:X},ESID:0x{:X},vsidReg:0x{:X},esidReg:0x{:X})", static_cast<u32>(slbEntry.V), static_cast<u32>(slbEntry.LP), static_cast<u32>(slbEntry.C), static_cast<u32>(slbEntry.L),
                                static_cast<u32>(slbEntry.N), static_cast<u32>(slbEntry.Kp), static_cast<u32>(slbEntry.Ks),
                                slbEntry.VSID, slbEntry.ESID, slbEntry.vsidReg, slbEntry.esidReg);
#endif
        if (slbEntry.ESID == ESID) {
#ifdef DEBUG_BUILD
          if (Config::log.advanced)
            LOG_TRACE(Xenon_MMU, "SLB Match");
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

      // Get our Virtual Address - 65 bit
      // VSID + 28 bit adress data.
      const u64 VA = VSID | (*EA & 0xFFFFFFF);
      // Page Offset.
      const u64 PAGE = (QGET(*EA, 36, 63 - p) << p);

      // Search the tlb for an entry.
      if (mmuSearchTlbEntry(ppeState, &RPN, VA, p, L, LP)) {
        // TLB Hit, proceed.
        goto end;
      } else {
        // TLB miss, if we are in software managed mode, generate an
        // interrupt, else do page table search
        if (tlbSoftwareManaged) {
          if (thread.instrFetch) {
            _ex |= ppuInstrStorageEx;
          } else {
            _ex |= ppuDataStorageEx;
            thread.SPR.DAR = *EA;
            thread.SPR.DSISR = DSISR_NOPTE;
          }
          return false;
        } else {
          // Page Table Lookup:
          // Walk the Page table to find a Page that translates our current VA

          // TODO(bitsh1ft3r): Add TLB Reloading code

          // Save MSR DR & IR Bits. When an exception occurs they must be reset
          // to whatever they where
          const bool msrDR = thread.SPR.MSR.DR;
          const bool msrIR = thread.SPR.MSR.IR;

          // Disable relocation
          thread.SPR.MSR.DR = 0;
          thread.SPR.MSR.IR = 0;

          // Get the primary and secondary hashes
          u64 hash0 = (VSID >> 28) ^ (PAGE >> p);
          u64 hash1 = ~hash0;

          // Get hash table origin and hash table mask
          const u64 htabOrg = ppeState->SPR.SDR1.hexValue & PPC_SPR_SDR_64_HTABORG;
          const u64 htabSize = ppeState->SPR.SDR1.hexValue & PPC_SPR_SDR_64_HTABSIZE;

          // Create the mask
          u64 htabMask = QMASK(64 - (11 + htabSize), 63);

          // And both hashes with the created mask
          hash0 = hash0 & htabMask;
          hash1 = hash1 & htabMask;

          // Get both PTEG's addresses
          const u64 pteg0Addr = htabOrg | (hash0 << 7);
          const u64 pteg1Addr = htabOrg | (hash1 << 7);

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
            pteg0[i].pte0 =
                PPCInterpreter::MMURead64(ppeState, pteg0Addr + i * 16, thr);
            pteg0[i].pte1 =
                PPCInterpreter::MMURead64(ppeState, pteg0Addr + i * 16 + 8, thr);
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
            if (((pteg0[i].pte0 & PPC_HPTE64_HASH) >> 1) != 0) {
              continue;
            }

            // Get our VPN for comparisson.
            u64 VPN = (VA >> p) << p;

            // Perform the compare
            if (!mmuComparePTE(VA, VPN, pteg0[i].pte0, pteg0[i].pte1, p, L, LP, &RPN)) {
              continue;
            }

            // Match found. Set relocation back to whatever it was
            thread.SPR.MSR.DR = msrDR;
            thread.SPR.MSR.IR = msrIR;

            // Update Referenced and Change Bits if necessary
            if (!((pteg0[i].pte1 & PPC_HPTE64_R) >> 8)) {
              // Referenced
              MMUWrite64(ppeState, pteg0Addr + i * 16 + 8,
                         (pteg0[i].pte1 | 0x100), thr);
            }
            if (!((pteg0[i].pte1 & PPC_HPTE64_C) >> 7)) {
              // Access is a data write?
              if (memWrite) {
                // Change
                MMUWrite64(ppeState, pteg0Addr + i * 16 + 8,
                           (pteg0[i].pte1 | 0x80), thr);
              }
            }

            goto end;
          }

          // Second PTEG
          PPC_HPTE64 pteg1[PPC_HPTES_PER_GROUP];

          for (size_t i = 0; i < PPC_HPTES_PER_GROUP; i++) {
            pteg1[i].pte0 =
                PPCInterpreter::MMURead64(ppeState, pteg1Addr + i * 16, thr);
            pteg1[i].pte1 =
                PPCInterpreter::MMURead64(ppeState, pteg1Addr + i * 16 + 8, thr);
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
            if (((pteg1[i].pte0 & PPC_HPTE64_HASH) >> 1) != 1) {
              continue;
            }

            // Get our VPN for comparisson.
            u64 VPN = (VA >> p) << p;

            // Perform the compare
            if (!mmuComparePTE(VA, VPN, pteg1[i].pte0, pteg1[i].pte1, p, L, LP, &RPN)) {
              continue;
            }

            // Match found. Set relocation back to whatever it was
            thread.SPR.MSR.DR = msrDR;
            thread.SPR.MSR.IR = msrIR;

            // Update Referenced and Change Bits if necessary
            if (!((pteg1[i].pte1 & PPC_HPTE64_R) >> 8)) {
              // Referenced
              MMUWrite64(ppeState, pteg1Addr + i * 16 + 8,
                         (pteg1[i].pte1 | 0x100), thr);
            }
            if (!((pteg1[i].pte1 & PPC_HPTE64_C) >> 7)) {
              // Access is a data write?
              if (memWrite) {
                // Change
                MMUWrite64(ppeState, pteg1Addr + i * 16 + 8,
                           (pteg1[i].pte1 | 0x80), thr);
              }
            }

            if (L) {
              // RPN is PTE[86:114]
              RPN = pteg1[i].pte1 & PPC_HPTE64_RPN_LP;
            } else {
              // RPN is PTE[86:115]
              RPN = pteg1[i].pte1 & PPC_HPTE64_RPN_NO_LP;
            }

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
            _ex |= ppuInstrStorageEx;

          } else if (memWrite) {
            // Data write
            _ex |= ppuDataStorageEx;
            thread.SPR.DAR = *EA;
            thread.SPR.DSISR = DSISR_NOPTE | DSISR_ISSTORE;
          } else {
            // Data read
            _ex |= ppuDataStorageEx;
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
        _ex |= ppuInstrSegmentEx;
      } else {
        _ex |= ppuDataSegmentEx;
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
  }
  else {
    // dERAT
    thread.dERAT.putElement((*EA & ~0xFFF), (RA & ~0xFFF));
  }

  *EA = RA;
  return true;
}

// MMU Read Routine, used by the CPU
void PPCInterpreter::MMURead(Xe::XCPU::XenonContext *cpuContext, sPPEState *ppeState,
                             u64 EA, u64 byteCount, u8 *outData, ePPUThreadID thr) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMURead", MP_AUTO);
  sPPUThread &thread = ppeState->ppuThread[thr != ePPUThread_None ? thr : curThreadId];
  const u64 oldEA = EA;
  if (!MMUTranslateAddress(&EA, ppeState, false, thr)) {
    memset(outData, 0, byteCount);
    return;
  }
  bool socRead = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socRead);

  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write
  if (((oldEA & 0x000000007FFF0000ULL) >> 16) == 0x7FFF)
    socRead = true;

  // Debugger halt
  if (EA && EA == Config::debug.haltOnReadAddress && XeMain::GetCPU()) {
    XeMain::GetCPU()->Halt(); // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger after halting
  }

  // TODO: Investigate why FSB_CONFIG_RX_STATE needs these values to work
  switch (thread.CIA) {
  case 0x1003598ULL: {
    GPR(11) = 0x0E;
  } break;
  case 0x1003644ULL: {
    GPR(11) = 0x02;
  } break;
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
      cpuContext->xenonIIC.readInterrupt(EA, outData, byteCount);
      return;
    }
    // Try to handle the SoC read, may belong to one of the CPU SoC blocks.
    else if (cpuContext->HandleSOCRead(EA, outData, byteCount)) {
      return;
    }
  }

  // External read
  if (!xenonContext->GetRootBus()->Read(EA, outData, byteCount, socRead) && socRead) {
    if (Config::log.advanced)
      LOG_WARNING(Xenon_MMU, "Invalid SoC Read from 0x{:X}", EA);
  }
}

// MMU Write Routine, used by the CPU
void PPCInterpreter::MMUWrite(Xe::XCPU::XenonContext *cpuContext, sPPEState *ppeState,
                              const u8 *data, u64 EA, u64 byteCount, ePPUThreadID thr) {
  MICROPROFILE_SCOPEI("[Xe::PPCInterpreter]", "MMUWrite", MP_AUTO);
  const u64 oldEA = EA;

  if (!MMUTranslateAddress(&EA, ppeState, true, thr))
    return;

  // Check if it's reserved
  cpuContext->xenonRes.Check(EA);

  bool socWrite = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socWrite);

  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write
  if (((oldEA & 0x000000007FFFF0000ULL) >> 16) == 0x7FFF)
    socWrite = true;

  // Debugger halt
  if (EA && EA == Config::debug.haltOnWriteAddress && XeMain::GetCPU()) {
    XeMain::GetCPU()->Halt(); // Halt the CPU
    Config::imgui.debugWindow = true; // Open the debugger after halting
  }

  if (socWrite) {
#ifdef DEBUG_BUILD
    if (EA == 0x61010ULL) {
      u64 postCode = *reinterpret_cast<const u64 *>(data);
      std::string poseCodeStr = Xe::XCPU::POSTBUS::GET_POST(postCode);
      PPU *PPU = XeMain::GetCPU()->GetPPU(ppeState->ppuID);
      if (PPU->traceFile) {
        fprintf(PPU->traceFile, "POST,0x%llx,%s\n", postCode, poseCodeStr.c_str());
      }
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
      cpuContext->xenonIIC.writeInterrupt(EA, data, byteCount);
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
    if (Config::log.advanced)
      LOG_WARNING(Xenon_MMU, "Invalid SoC Write to 0x{:X}", EA);
  }
}

void PPCInterpreter::MMUMemCpyFromHost(sPPEState *ppeState,
                                       u64 EA, const void *source, u64 size, ePPUThreadID thr) {
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(source), EA, size);
}

void PPCInterpreter::MMUMemCpy(sPPEState *ppeState,
                               u64 EA, u32 source, u64 size, ePPUThreadID thr) {
  std::unique_ptr<u8[]> data = std::make_unique<STRIP_UNIQUE_ARR(data)>(size);
  MMURead(xenonContext, ppeState, source, size, data.get(), thr);
  MMUWrite(xenonContext, ppeState, data.get(), EA, size, thr);
  data.reset();
}

void PPCInterpreter::MMUMemSet(sPPEState *ppeState,
                               u64 EA, s32 data, u64 size, ePPUThreadID thr) {
  const u64 oldEA = EA;

  if (MMUTranslateAddress(&EA, ppeState, true, thr) == false)
    return;

  if (!xenonContext)
    return;

  // Check if it's reserved
  xenonContext->xenonRes.Check(EA);

  bool socWrite = false;

  EA = mmuContructEndAddressFromSecEngAddr(EA, &socWrite);
  // When the xboxkrnl writes to address 0x7FFFxxxx is writing to the IIC
  // so we use that address here to validate its an soc write
  if (((oldEA & 0x000000007FFFF0000ULL) >> 16) == 0x7FFF)
    socWrite = true;
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

u8* PPCInterpreter::MMUGetPointerFromRAM(u64 EA) {
  return xenonContext->GetRAM()->GetPointerToAddress(EA);
}

// Reads 1 byte of memory
u8 PPCInterpreter::MMURead8(sPPEState *ppeState, u64 EA, ePPUThreadID thr) {
  u8 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return data;
}
// Reads 2 bytes of memory
u16 PPCInterpreter::MMURead16(sPPEState *ppeState, u64 EA, ePPUThreadID thr) {
  u16 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return byteswap_be<u16>(data);
}
// Reads 4 bytes of memory
u32 PPCInterpreter::MMURead32(sPPEState *ppeState, u64 EA, ePPUThreadID thr) {
  u32 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return byteswap_be<u32>(data);
}
// Reads 8 bytes of memory
u64 PPCInterpreter::MMURead64(sPPEState *ppeState, u64 EA, ePPUThreadID thr) {
  u64 data = 0;
  MMURead(xenonContext, ppeState, EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
  return byteswap_be<u64>(data);
}
// Writes 1 byte to memory
void PPCInterpreter::MMUWrite8(sPPEState *ppeState, u64 EA, u8 data, ePPUThreadID thr) {
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&data), EA, sizeof(data), thr);
}
// Writes 2 bytes to memory
void PPCInterpreter::MMUWrite16(sPPEState *ppeState, u64 EA, u16 data, ePPUThreadID thr) {
  const u16 dataBS = byteswap_be<u16>(data);
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
}
// Writes 4 bytes to memory
void PPCInterpreter::MMUWrite32(sPPEState *ppeState, u64 EA, u32 data, ePPUThreadID thr) {
  const u32 dataBS = byteswap_be<u32>(data);
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
}
// Writes 8 bytes to memory
void PPCInterpreter::MMUWrite64(sPPEState *ppeState, u64 EA, u64 data, ePPUThreadID thr) {
  const u64 dataBS = byteswap_be<u64>(data);
  MMUWrite(xenonContext, ppeState, reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
}
