/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "XenonMMU.h"

#include "Base/Global.h"
#include "Core/PCI/Devices/NAND/NAND.h"
#include "Core/XCPU/Context/PostBus/PostBus.h"
#include "Core/XCPU/PPU/PPCInternal.h"
#include "Core/XCPU/XenonCPU.h"

// #define MMU_DEBUG
#ifndef MMU_DEBUG
  #define DEBUGP(x, ...)
#else
  #define DEBUGP(x, ...) LOG_DEBUG(Xenon_MMU, x, ##__VA_ARGS__);
#endif

// Thread selection helper.
#define curThread ppeState->ppuThread[curThreadId]

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

// The processor generated address (EA) is subdivided, upper 32 bits are used
// as flags for the 'Security Engine'
//
// 0x00000X**_00000000 X = region, ** = key select
// X = 0 should be Physical
// X = 1 should be Hashed
// X = 2 should be SoC
// X = 3 should be Encrypted

// 0x8000020000060000 Seems to be the random number generator. Implement this?

// DSISR Flags.
#define DSISR_ISSTORE 0x02000000
#define DSISR_NOPTE   0x40000000

namespace Xe::XCPU::MMU {

  //
  // Constants and Structures
  //

  // TLB related constants
  static constexpr u32 TLB_IDX_BITS = 10; // log2(NUM_ENTRIES)
  static constexpr u32 TLB_WAYS_LOG2 = 2; // log2(NUM_WAYS)
  static constexpr u32 TLB_SEG_SIZE = 28; // 256 MB segment

  // MMU Page Sizes hanlded by the PPE. Defined by the architecture as 'p'.
  static constexpr u8 MMU_PAGE_SIZE_4KB = 12;
  static constexpr u8 MMU_PAGE_SIZE_64KB = 16;
  static constexpr u8 MMU_PAGE_SIZE_1MB = 20;
  static constexpr u8 MMU_PAGE_SIZE_16MB = 24;

  // Page valid.
  static constexpr u64 PPC_HPTE64_VALID = 0x0000000000000001ULL;
  // HPTEs per HPTEG.
  static constexpr u64 PPC_HPTES_PER_GROUP = 8;
  // Page Hash identifier.
  static constexpr u64 PPC_HPTE64_HASH = 0x0000000000000002ULL;
  // AVPN without Large bit.
  static constexpr u64 PPC_HPTE64_AVPN = 0xFFFFFFFFFFFFFF83Ull;
  // Page Large bit.
  static constexpr u64 PPC_HPTE64_LARGE = 0x0000000000000004ULL;
  // RPN when L = 0.
  static constexpr u64 PPC_HPTE64_RPN_NO_LP = 0x000003FFFFFFF000UL;
  // RPN when L = 1.
  static constexpr u64 PPC_HPTE64_RPN_LP = 0x000003FFFFFFE000UL;
  // Large Page Selector bit.
  static constexpr u64 PPC_HPTE64_LP = 0x0000000000001000ULL;
  // Changed bit.
  static constexpr u64 PPC_HPTE64_C = 0x0000000000000080ULL;
  // Referenced bit.
  static constexpr u64 PPC_HPTE64_R = 0x0000000000000100ULL;

  // Page table entry structure.
  struct HPTE64 {
    u64 pte0;
    u64 pte1;
  };

  //
  // Helpers
  //

  /* TLB Related */

  // Page-size-dependent runtime tag mask, used to compare TLB tags at a given p size.
  inline u64 GetTlbTagMask(u8 p) { return ~((4ULL * (1ULL << (TLB_IDX_BITS - TLB_WAYS_LOG2 + p - 3))) - 4ULL); }

  // Lookup search TLB tag, holds VSID, L bit and the VA encoded.
  inline u64 GetTlbSearchTag(u64 VA, u8 p, bool L) {
    const u64 VSID = (VA & ~0xFFFFFFFULL) >> 16;
    return (L ? 2ULL : 0ULL) | (VSID << 15) | ((VA & 0xFFFF000ULL) >> 1);
  }

  // Store tag built from the real/insert pte0 for the TLB entry. Encodes the L bit, EA page, and PTE0.
  inline u64 GetTlbStoreTag(u64 pte0, u64 ea) {
    return ((ea & 0x7FF000ULL) >> 1) | ((pte0 & ~0x7FULL) << 15) | (2ULL * ((pte0 >> 2) & 1)); // L bit -> tag[1]
  }

  // Congruence-class set-hash. Returns 0 - 255, the class index.
  inline u16 GetTlbClass(u64 VA, u8 p) {
    const u32 setMask = (1u << (TLB_IDX_BITS - TLB_WAYS_LOG2)) - 1; // 0xFF
    u64 set = setMask & (VA >> p);
    const u64 hi = ((0xFFFFFFFull >> p) & ~static_cast<u64>(setMask)) << p;
    if (hi) set ^= ((hi & VA) >> (TLB_WAYS_LOG2 + 28 - TLB_IDX_BITS)) & ~0xFull; // >> 20
    return static_cast<u16>(set & 0xFF);
  }

  // Precise, VSID-aware selective invalidation
  // Hash the VA to its single congruence class, build the page-size-masked search tag, and clear only the ways whose
  // stored tag matches.
  void XenonMMU::TlbInvalidateSelective(u64 VA, u8 p, bool L) {
    // Hold lock.
    std::unique_lock tlbLock(tlbMutex);

    const u16 classIdx = GetTlbClass(VA, p);
    const u64 tagMask = GetTlbTagMask(p);
    const u64 searchTag = GetTlbSearchTag(VA, p, L) & tagMask;

    for (u32 way = 0; way < TLB_Reg::NUM_WAYS; ++way) {
      TLBEntry& entry = tlb.entryAt(classIdx, way);
      if (entry.valid && (entry.tag & tagMask) == searchTag) {
        DEBUGP("[TLB]: Selective invalidate match: class:{:#x} way:{} tag:{:#x} (VA:{:#x} p:{} L:{})", classIdx, way,
               entry.tag, VA, p, static_cast<u32>(L));
        entry.valid = 0;
      }
    }
  }

  // Class-level (IS=3) selective invalidation. Clears every way of a single congruence class.
  void XenonMMU::TlbInvalidateClass(u32 classIdx) {
    std::unique_lock tlbLock(tlbMutex);
    tlb.invalidateClass(classIdx);
  }

  /* Hardware Based Page Table Walk */

  // Returns the HTAB Hash for the current VSID, EA and p.
  inline u64 GetHTABHash(u64 vsid, u64 pageEA, int pageShift) {
    u64 pi = (pageEA >> (pageShift - 12)) & (((1ull << (28 - pageShift)) - 1) << 12);
    return ((vsid ^ pi) >> 5) & 0x3FFFFFFFFF80ull;
  }

  // Returns the PTEG address based on the HTAB Hash and the SDR1 Reg contents.
  // For the primary PTEG, pass the hash directly, for the secondary, pass ~hash.
  inline u64 GetPTEGAddress(u64 sdr1, u64 hash) {
    u64 htabOrgLow = sdr1 & 0xFFFC0000ull;
    u64 htabSize = ((1ull << (sdr1 & 0x1F)) - 1) << 18;
    return htabOrgLow | (hash & (htabSize | 0x3FF80ull));
  }

  // Compares the fetched PTE against the generated MASK.
  inline bool CheckPTEMatch(u64 pte0, u64 token, u64 mask) { return (pte0 & mask) == token; }

  /* Page size and related utilities */

  // Helper function for getting Page Size (p bit).
  u8 XenonMMU::GetPageSize(bool L, u8 LP) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUGetPageSize", MP_AUTO);

    // Large page selection works the following way:
    // First check if pages are large (L)
    // if (L) the page size can be one of two defined pages. On the XBox 360, MS decided to use two of the three page
    // sizes, 64Kb and 16Mb. Selection between them is made using bits 16 - 19 of HID6 SPR.

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

  // Software-managed TLB insert
  // The flow:
  //   1. Take the flat entry index directly from PPE_TLB_Index. The register encodes (class_index << 4) |
  //      one_hot_way_bitmask; Convert that to (class * num_ways + (num_ways - 1 - bsf(way_mask))).
  //   2. Reconstruct the page EA from PPE_TLB_VPN.AVPN bits [37..47] and from pte0 bits [7..17]
  //   3. Compute the lpid_bit from VPN bit 12 when L=1, else 0.
  //   4. Build the store tag.
  void XenonMMU::AddTlbEntry() {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUAddTlbEntry", MP_AUTO);

    // Hold lock.
    std::unique_lock tlbLock(tlbMutex);

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
    const u8 p = GetPageSize(L, LP);

    // Reconstruct page EA for the store tag's EA[12:22] slice. The low VA bits (the LVPN, VA[12:22]) are carried
    // by PPE_TLB_Index[37..47], not by PPE_TLB_VPN. That register only holds the AVPN (high VA bits), which reach
    // the tag through the (pte0 & ~0x7F) term of GetTlbStoreTag.
    const u64 eaPage = ((((tlbIndexReg >> 37) & 0x7FF) | ((storedPte0 >> 7) << 11)) << 12);

    // Resolve flat entry index from PPE_TLB_Index.
    const u32 flatIdx = TLB_Reg::flatIndexFromTlbIndexReg(tlbIndexReg);
    if (flatIdx == ~0u) {
      DEBUGP("[TLB]: mmuAddTlbEntry: malformed PPE_TLB_Index {:#x}", tlbIndexReg);
      return;
    }
    const u32 classIdx = flatIdx / TLB_Reg::NUM_WAYS;
    const u32 wayIdx = flatIdx % TLB_Reg::NUM_WAYS;

    TLBEntry& entry = tlb.entry(flatIdx);

    // If we're replacing a live entry, invalidate it first.
    if (entry.valid) { entry.valid = 0; }

    // Populate everything BEFORE setting valid.
    // Store tag is the un-masked tag, the lookup masks it given that the p bit is controlled by the lookup.
    entry.tag = GetTlbStoreTag(storedPte0, eaPage);
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

    // Atomic publish: readers either see valid==0 (skip) or a fully written entry.
    entry.valid = 1;
    tlb.updateLRU(classIdx, static_cast<u8>(wayIdx));
  }

  // Hardware-managed TLB insert (HTAB walk path).
  void XenonMMU::AddTlbEntryHardware(u64 VA, u64 pte0, u64 pte1, u8 p, bool L, bool /*LP*/) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUAddTlbEntryHardware", MP_AUTO);

    // Hold lock.
    std::unique_lock tlbLock(tlbMutex);

    const u16 classIdx = GetTlbClass(VA, p);

    // pte0 stored with H cleared.
    const u64 storedPte0 = pte0 & ~PPC_HPTE64_HASH;

    // Victim selection: invalid ways first, then pseudo-LRU.
    const u32 wayIdx = tlb.pickVictimWay(classIdx);

    TLBEntry& entry = tlb.entryAt(classIdx, wayIdx);
    if (entry.valid) { entry.valid = 0; }

    // Un-masked store tag from the freshly-walked HPTE.
    entry.tag = GetTlbStoreTag(storedPte0, VA);
    entry.pte0 = storedPte0;
    entry.pte1 = pte1;
    entry.lpidr = ppeState->SPR.LPIDR.hexValue;

    DEBUGP("[TLB]: Hardware reload: class: {:#x} way: {} tag: {:#x} PTE0: {:#x} PTE1: {:#x} p: {} L: {}", classIdx,
           wayIdx, entry.tag, storedPte0, pte1, p, L);

    entry.valid = 1;
    tlb.updateLRU(classIdx, static_cast<u8>(wayIdx));
  }

  // TLB lookup
  bool XenonMMU::SearchTlbEntry(u64* RPN, u64 VA, u8 p, bool L, bool LP) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUSearchTlbEntry", MP_AUTO);

    // Shared lockdue to the fact that siblings may look up concurrently, but never while a writer is
    // inserting/invalidating (which would tear the multi-field entry read below).
    std::shared_lock tlbLock(tlbMutex);

    // Runtime tag + page-size mask + masked compare, mask is generated using the provide p size.
    const u16 classIdx = GetTlbClass(VA, p);
    const u64 tagMask = GetTlbTagMask(p);
    const u64 search = GetTlbSearchTag(VA, p, L) & tagMask;

    // Partition scoping, gates every hit on entry->lpidr == lpid, in addition to the masked tag compare.
    // Entries inserted under a different LPIDR never satisfy a lookup, so a partition switch
    // transparently shadows stale translations.
    const u32 lpid = static_cast<u32>(ppeState->SPR.LPIDR.hexValue);

    // Walk the four ways of the class over contiguous memory: one multiply to find
    // the class base, then linear indexing (vs recomputing class*4+way each step).
    TLBEntry* ways = &tlb.entryAt(classIdx, 0);
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
      tlb.updateLRU(classIdx, static_cast<u8>(way));
      return true;
    }

    // Miss. Update the PPE_TLB_Index_Hint with a replacement suggestion for software-managed mode so the kernel's
    // TLB-miss handler can MTSPR PPE_TLB_Index directly from the hint.
    const bool tlbSoftwareManaged = (ppeState->SPR.LPCR.hexValue & 0x400) >> 10;
    if (tlbSoftwareManaged) {
      const u32 replacementWay = tlb.pickVictimWay(classIdx);
      const u64 hint = (static_cast<u64>(classIdx) << 4) | static_cast<u64>(TLB_WAY_BITMASK[replacementWay]);
      curThread.SPR.PPE_TLB_Index_Hint.hexValue = hint;
    }

    return false;
  }

  // Hardware page-table (HTAB) walk.
  //
  // Mirrors the CELL-BE / Xenon PPE hardware reload sequence:
  //   1. Build the abbreviated-AVPN compare token and the page-size mask from (vsid, EA). The
  //      token packs the SLB VSID (already positioned at bits [12:48] by slbmte) with the top
  //      page-index bits EA[23:27] in token[7:11]; V is required set, H selects primary vs
  //      secondary, and L (large page) participates only for large pages.
  //   2. Compute the primary hash (VSID ^ page-index); the secondary hash is its complement.
  //   3. For each PTEG, read the 8 HPTEs (big-endian) from physical RAM at the SDR1-derived group
  //      address and compare pte0 against the token under the mask (CheckPTEMatch()).
  //   4. On a match: set Referenced (R), set Changed (C) on a store, write the PTE back to guest
  //      memory if it changed, reload the TLB from the walked entry, and return the RPN. On a
  //      full miss return false so the caller raises the page-fault interrupt.
  bool XenonMMU::WalkPageTable(u64* RPN, u64 EA, u64 VA, u64 vsid, u8 p, bool L, bool LP, bool memWrite) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUWalkPageTable", MP_AUTO);

    // Get SDR1
    const u64 sdr1 = ppeState->SPR.SDR1.hexValue;
    // Get the page-aligned EA, drives both the hash and the token, the low page bits are the byte offset.
    const u64 pageEA = EA & (~0ULL << p);

    // AVPN | V | H [| L] compare.
    u64 tokenMask = PPC_HPTE64_AVPN | PPC_HPTE64_VALID | PPC_HPTE64_HASH;
    u64 token = ((pageEA >> 16) & 0xF80ULL) | vsid | PPC_HPTE64_VALID;

    if (L) {
      tokenMask |= PPC_HPTE64_LARGE;
      token |= PPC_HPTE64_LARGE;
    }

    const u64 hash = GetHTABHash(vsid, pageEA, p);

    DEBUGP("[HTAB]: Search PTE for EA:{:#x} (VA:{:#x} vsid:{:#x} p:{} L:{} LP:{}) SDR1:{:#x} hash:{:#x} "
           "token:{:#x} mask:{:#x}",
           EA, VA, vsid, p, static_cast<u32>(L), static_cast<u32>(LP), sdr1, hash, token, tokenMask);

    // Two passes: primary PTEG (H=0), then secondary PTEG (H=1, hash complemented).
    for (u32 pass = 0; pass < 2; ++pass) {
      const bool secondary = (pass != 0);
      const u64 ptegRA = GetPTEGAddress(sdr1, secondary ? ~hash : hash);
      const u64 searchToken = secondary ? (token | PPC_HPTE64_HASH) : token;

      DEBUGP("[HTAB]: Search PTE Group starting at RA:{:#x} for EA:{:#x} ({}, search token:{:#x})", ptegRA, EA,
             secondary ? "secondary" : "primary", searchToken);

      // The PTEG lives at a real address in guest RAM; grab a host pointer to its 8 * 16 bytes.
      u8* ptegPtr = MMUGetPointerFromRAM(ptegRA);
      if (!ptegPtr) {
        DEBUGP("[HTAB]: PTEG RA:{:#x} outside guest RAM, skipping {} group", ptegRA,
               secondary ? "secondary" : "primary");
        continue;
      }

      for (u32 slot = 0; slot < PPC_HPTES_PER_GROUP; ++slot) {
        u8* ptePtr = ptegPtr + slot * sizeof(HPTE64);
        // HPTEs are stored big-endian in guest memory; byteswap into host order to compare.
        const u64 pte0 = byteswap_be<u64>(*reinterpret_cast<u64*>(ptePtr));
        u64 pte1 = byteswap_be<u64>(*reinterpret_cast<u64*>(ptePtr + 8));

        if (!CheckPTEMatch(pte0, searchToken, tokenMask)) continue;

        // Match
        const u64 pteRA = ptegRA + slot * sizeof(HPTE64);
        const u32 wimg = static_cast<u32>((pte1 >> 3) & 0xF);
        const u32 pp = static_cast<u32>(pte1 & 0x3);
        DEBUGP("[HTAB]: Found PTE (slot {}) for EA:{:#x} at RA:{:#x} pte0:{:#x} pte1:{:#x} WIMG:{:#x} pp:{:#x} "
               "R:{} C:{}",
               slot, EA, pteRA, pte0, pte1, wimg, pp, static_cast<u32>((pte1 & PPC_HPTE64_R) != 0),
               static_cast<u32>((pte1 & PPC_HPTE64_C) != 0));

        // Reference/Change bit update: R on any access, C on a store. Two cores
        // can walk the same PTEG at once, so update the guest PTE word with an
        // atomic compare-exchange (values are big-endian in memory) instead of a
        // plain store, so neither core loses the other's R/C update.
        {
          std::atomic_ref<u64> pte1Ref(*reinterpret_cast<u64*>(ptePtr + 8));
          u64 curBE = pte1Ref.load(std::memory_order_acquire);
          for (;;) {
            const u64 cur = byteswap_be<u64>(curBE);
            u64 want = cur | PPC_HPTE64_R;
            if (memWrite) want |= PPC_HPTE64_C;
            if (want == cur) {
              pte1 = cur; // Already set; nothing to write back.
              break;
            }
            const u64 wantBE = byteswap_be<u64>(want);
            if (pte1Ref.compare_exchange_weak(curBE, wantBE, std::memory_order_acq_rel, std::memory_order_acquire)) {
              pte1 = want;
              DEBUGP("[HTAB]: Set R/C bits at RA:{:#x}: {:#x} -> {:#x}", pteRA + 8, cur, want);
              break;
            }
            // curBE now holds the latest value; retry.
          }
        }

        // Reload the TLB from the walked entry so later accesses hit without another walk.
        AddTlbEntryHardware(VA, pte0, pte1, p, L, LP);

        // Derive the RPN exactly like the TLB-hit path (large pages use the wider RPN mask).
        *RPN = L ? (pte1 & PPC_HPTE64_RPN_LP) : (pte1 & PPC_HPTE64_RPN_NO_LP);

        DEBUGP("[HTAB]: Reload complete for EA:{:#x} -> RPN:{:#x} (p:{} L:{})", EA, *RPN, p, static_cast<u32>(L));
        return true;
      }
    }

    DEBUGP("[HTAB]: Walk miss for EA:{:#x} (no matching PTE in primary or secondary PTEG)", EA);
    return false;
  }

  // Routine to read a string from memory, using a PSTRNG given by the kernel.
  void XenonMMU::ReadString(u64 stringAddress, char* string, u32 maxLength) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUReadString", MP_AUTO);
    u32 stringBufferAddress = 0;
    const u16 strLength = MMURead16(stringAddress);

    if (strLength < maxLength) maxLength = strLength + 1;

    stringBufferAddress = MMURead32(stringAddress + 4);
    MMURead(stringBufferAddress, maxLength, reinterpret_cast<u8*>(string));
    string[maxLength - 1] = 0;
  }

  SECENG_ADDRESS_INFO XenonMMU::GetSecEngInfoFromAddress(u64 inputAddress) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUGetSecEngInfoFromAddress", MP_AUTO);
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

  u64 XenonMMU::ContructEndAddressFromSecEngAddr(u64 inputAddress, bool* socAccess) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUContructEndAddressFromSecEngAddr", MP_AUTO);
    SECENG_ADDRESS_INFO inputAddressInfo = GetSecEngInfoFromAddress(inputAddress);

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
  bool XenonMMU::MMUTranslateAddress(u64* EA, bool memWrite, ePPUThreadID thr) {
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

    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUTranslateAddress", MP_AUTO);

    // Current thread.
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
        p = GetPageSize(L, LP);

        // Get our Virtual Address.
        const u64 VA = (VSID << 16) | (*EA & 0xFFFFFFF);

        // Search the tlb for an entry.
        if (SearchTlbEntry(&RPN, VA, p, L, LP)) {
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
            // Page Table Lookup.
            // Walk the Page table to find a Page that translates our current VA.

            // Save MSR DR & IR Bits. When an exception occurs they must be reset to whatever they where.
            const bool msrDR = thread.SPR.MSR.DR;
            const bool msrIR = thread.SPR.MSR.IR;

            // Disable relocation
            thread.SPR.MSR.DR = 0;
            thread.SPR.MSR.IR = 0;

            // Hardware hash page table walk. On a hit this updates the PTE R/C bits, writes them back to guest memory,
            // reloads the TLB, and returns the RPN.
            const bool htabHit = WalkPageTable(&RPN, *EA, VA, VSID, p, L, LP, memWrite);

            // Set MSR to IR/DR mode before raising the interrupt to whatever they were.
            thread.SPR.MSR.DR = msrDR;
            thread.SPR.MSR.IR = msrIR;

            if (htabHit) {
              // PTE found and TLB reloaded, finish EA -> RA generation.
              goto end;
            }

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
  void XenonMMU::MMURead(u64 EA, u64 byteCount, u8* outData, ePPUThreadID thr) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMURead", MP_AUTO);
    sPPUThread& thread = ppeState->ppuThread[thr != ePPUThread_None ? thr : curThreadId];

    const u64 oldEA = EA;
    if (!MMUTranslateAddress(&EA, false, thr)) {
      memset(outData, 0, byteCount);
      return;
    }
    bool socRead = false;

    EA = ContructEndAddressFromSecEngAddr(EA, &socRead);

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
        memcpy(outData, &xenonContext->SROM[sromAddr], byteCount);
        return;
      }
      // Check if the read is from SRAM
      else if (EA >= XE_SRAM_ADDR && EA < XE_SRAM_ADDR + XE_SRAM_SIZE) {
        const u32 sramAddr = static_cast<u32>(EA - XE_SRAM_ADDR);
        memcpy(outData, &xenonContext->SRAM[sramAddr], byteCount);
        return;
      }
      // Integrated Interrupt Controller in real mode, used when the HV wants to
      // start a CPUs IC
      else if (EA >= XE_SOCINTS_BLOCK_START && EA <= XE_SOCINTS_BLOCK_START + XE_SOCINTS_BLOCK_SIZE) {
        // Pass it onto our context INT struct.
        xenonContext->HandleSOCRead(EA, outData, byteCount);
        return;
      }
      // Try to handle the SoC read, may belong to one of the CPU SoC blocks.
      else if (xenonContext->HandleSOCRead(EA, outData, byteCount)) {
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
  void XenonMMU::MMUWrite(const u8* data, u64 EA, u64 byteCount, ePPUThreadID thr) {
    MICROPROFILE_SCOPEI("[Xe::XenonMMU]", "MMUWrite", MP_AUTO);
    const u64 oldEA = EA;

    if (!MMUTranslateAddress(&EA, true, thr)) return;

    // Check if it's reserved
    xenonContext->xenonRes.Check(EA);

    bool socWrite = false;

    EA = ContructEndAddressFromSecEngAddr(EA, &socWrite);

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
        memcpy(&xenonContext->SRAM[sramAddr], data, byteCount);
        return;
      }
      // Integrated Interrupt Controller in real mode, used when the HV wants to
      // start a CPUs IC.
      else if (EA >= XE_SOCINTS_BLOCK_START && EA <= XE_SOCINTS_BLOCK_START + XE_SOCINTS_BLOCK_SIZE) {
        xenonContext->HandleSOCWrite(EA, data, byteCount);
        return;
      }
      // Try to handle the SoC write, may belong to one of the CPU SoC blocks.
      else if (xenonContext->HandleSOCWrite(EA, data, byteCount)) {
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

  void XenonMMU::MMUMemCpyFromHost(u64 EA, const void* source, u64 size, ePPUThreadID thr) {
    MMUWrite(reinterpret_cast<const u8*>(source), EA, size);
  }

  void XenonMMU::MMUMemCpy(u64 EA, u32 source, u64 size, ePPUThreadID thr) {
    std::unique_ptr<u8[]> data = std::make_unique<STRIP_UNIQUE_ARR(data)>(size);
    MMURead(source, size, data.get(), thr);
    MMUWrite(data.get(), EA, size, thr);
    data.reset();
  }

  void XenonMMU::MMUMemSet(u64 EA, s32 data, u64 size, ePPUThreadID thr) {
    const u64 oldEA = EA;

    if (MMUTranslateAddress(&EA, true, thr) == false) return;

    if (!xenonContext) return;

    // Check if it's reserved
    xenonContext->xenonRes.Check(EA);

    bool socWrite = false;

    EA = ContructEndAddressFromSecEngAddr(EA, &socWrite);
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

  u8* XenonMMU::MMUGetPointerFromRAM(u64 EA) { return xenonContext->GetRAM()->GetPointerToAddress(EA); }

  // Reads 1 byte of memory
  u8 XenonMMU::MMURead8(u64 EA, ePPUThreadID thr) {
    u8 data = 0;
    MMURead(EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
    return data;
  }
  // Reads 2 bytes of memory
  u16 XenonMMU::MMURead16(u64 EA, ePPUThreadID thr) {
    u16 data = 0;
    MMURead(EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
    return byteswap_be<u16>(data);
  }
  // Reads 4 bytes of memory
  u32 XenonMMU::MMURead32(u64 EA, ePPUThreadID thr) {
    u32 data = 0;
    MMURead(EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
    return byteswap_be<u32>(data);
  }
  // Reads 8 bytes of memory
  u64 XenonMMU::MMURead64(u64 EA, ePPUThreadID thr) {
    u64 data = 0;
    MMURead(EA, sizeof(data), reinterpret_cast<u8*>(&data), thr);
    return byteswap_be<u64>(data);
  }
  // Writes 1 byte to memory
  void XenonMMU::MMUWrite8(u64 EA, u8 data, ePPUThreadID thr) {
    MMUWrite(reinterpret_cast<const u8*>(&data), EA, sizeof(data), thr);
  }
  // Writes 2 bytes to memory
  void XenonMMU::MMUWrite16(u64 EA, u16 data, ePPUThreadID thr) {
    const u16 dataBS = byteswap_be<u16>(data);
    MMUWrite(reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
  }
  // Writes 4 bytes to memory
  void XenonMMU::MMUWrite32(u64 EA, u32 data, ePPUThreadID thr) {
    const u32 dataBS = byteswap_be<u32>(data);
    MMUWrite(reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
  }
  // Writes 8 bytes to memory
  void XenonMMU::MMUWrite64(u64 EA, u64 data, ePPUThreadID thr) {
    const u64 dataBS = byteswap_be<u64>(data);
    MMUWrite(reinterpret_cast<const u8*>(&dataBS), EA, sizeof(data), thr);
  }

} // namespace Xe::XCPU::MMU

#undef curThread
