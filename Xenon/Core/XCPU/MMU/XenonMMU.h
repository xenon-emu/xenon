/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/XCPU/Context/XenonContext.h"
#include "Core/XCPU/PPU/PowerPC.h"

#include <atomic>
#include <bit>
#include <shared_mutex>

namespace Xe::XCPU::MMU {
  // The CELL B/E manual states that any CELL arch based PPE's (like the ones inside the Xenon CPU) can have the
  // 'small' 4KB Page size and any two out of the three 'large' page sizes described therein. The PPU's inside the
  // Xenon use the 'large' 64Kb and 16Mb page sizes.
  enum ePageSize : u8 {
    pSize4Kb = 12,       // 4Kb 'small' page size. p = 12 bits.
    pSize64Kb = 16,      // 64Kb 'large' page size. p = 16 bits.
    pSize16Mb = 24,      // 16Mb 'large' page size. p = 24 bits.
    pSizeUnsupported = 0 // Unknown or unsupported (1Mb) page size.
  };

  // Translation Lookaside Buffer entry:
  //   tag    : compressed tlb tag.
  //   pte0   : HPTE word 0 (V|H|L|AVPN|...). Stored with H bit cleared.
  //   pte1   : HPTE word 1 (RPN|attributes|R|C|LP).
  //   lpidr  : Logical partition ID that owned this insertion. Snapshot of the
  //            owning thread's LPIDR - gates lookups and partition-scoped flushes.
  //   valid  : non-zero when the entry holds a live translation. WRITTEN LAST when
  //            inserting so a concurrent lookup walking the same class can never
  //            observe a half-initialized entry.
  struct TLBEntry {
    u64 tag;   // TLB match tag
    u64 pte0;  // HPTE word 0, H bit cleared (& ~PPC_HPTE64_HASH)
    u64 pte1;  // HPTE word 1 raw
    u32 lpidr; // logical partition ID at insert
    u8 valid;  // non-zero when live; written last on insert (publish barrier)
  };
  static_assert(sizeof(TLBEntry) == 32, "TLBEntry should pack to 32 bytes (2 ways/line halved)");

  // PPE_TLB_Index one-hot way encoding (see CBE Public Registers spec, PPE_TLB_Index).
  // Low 4 bits of the register select which of the 4 ways within a congruence
  // class to read/write; the upper bits index the class.
  //
  //   0b1000 -> way 0
  //   0b0100 -> way 1
  //   0b0010 -> way 2
  //   0b0001 -> way 3
  constexpr u8 TLB_WAY_BITMASK[4] = {0b1000, 0b0100, 0b0010, 0b0001};

  // PPE Translation Lookaside Buffer.
  //
  // 1024 entries organized as 256 congruence classes of 4 ways each.
  // Shared by both PPE threads (PPE TLB is per-PPE, not per-thread). Owned by the
  // XenonMMU (which is itself per-PPE); the per-thread ERATs sit in front of it.
  struct TLB_Reg {
    static constexpr u32 NUM_CLASSES = 256;
    static constexpr u32 NUM_WAYS = 4;
    static constexpr u32 NUM_ENTRIES = NUM_CLASSES * NUM_WAYS; // 1024

    // Flat 1024-entry array. entries[class*4 + way] addresses ways linearly.
    TLBEntry entries[NUM_ENTRIES]{};

    // Pseudo-LRU bits, one byte per congruence class.
    //   bit 0..1 : MRU within pair (way0, way1)
    //   bit 2..3 : MRU within pair (way2, way3)
    //   bit 4..5 : MRU between the two pairs
    std::atomic<u8> lruBits[NUM_CLASSES]{};

    // Entry access helpers.

    inline TLBEntry& entry(u32 flatIdx) { return entries[flatIdx]; }
    inline const TLBEntry& entry(u32 flatIdx) const { return entries[flatIdx]; }
    inline TLBEntry& entryAt(u32 classIdx, u32 way) { return entries[classIdx * NUM_WAYS + way]; }
    inline const TLBEntry& entryAt(u32 classIdx, u32 way) const { return entries[classIdx * NUM_WAYS + way]; }

    // Map a PPE_TLB_Index register value to a flat entry index.
    // The low 4 bits are a one-hot way selector, the next 8 bits index the class.
    // Returns ~0u if the way selector is zero or not one-hot (malformed).
    static inline u32 flatIndexFromTlbIndexReg(u64 idxReg) {
      const u32 wayMask = static_cast<u32>(idxReg) & 0xF;
      // CBE PPE_TLB_Index way field is strictly one-hot, anything else is malformed.
      if (!std::has_single_bit(wayMask)) return ~0u;
      // One-hot -> way index via trailing-zero count: 0b1000 -> 0 ... 0b0001 -> 3.
      const u32 way = (NUM_WAYS - 1) - static_cast<u32>(std::countr_zero(wayMask));
      const u32 classIdx = static_cast<u32>((idxReg >> 4) & (NUM_CLASSES - 1));
      return classIdx * NUM_WAYS + way;
    }

    // Pseudo-LRU

    inline u8 getLRUWay(u32 classIdx) const {
      const u8 lru = lruBits[classIdx].load(std::memory_order_relaxed);
      if (lru & 0x30) {              // pair (2,3) is the MRU pair
        return (lru & 0x03) ? 1 : 0; // -> LRU of pair (0,1)
      } else {                       // pair (0,1) is the MRU pair
        return (lru & 0x0C) ? 3 : 2; // -> LRU of pair (2,3)
      }
    }

    inline void updateLRU(u32 classIdx, u8 accessedWay) {
      std::atomic<u8>& lru = lruBits[classIdx];
      switch (accessedWay) {
        case 0:
          lru.fetch_or(u8(0x01), std::memory_order_relaxed);
          lru.fetch_and(u8(~0x30), std::memory_order_relaxed);
          break;
        case 1:
          lru.fetch_and(u8(~0x01), std::memory_order_relaxed);
          lru.fetch_and(u8(~0x30), std::memory_order_relaxed);
          break;
        case 2:
          lru.fetch_or(u8(0x04), std::memory_order_relaxed);
          lru.fetch_or(u8(0x30), std::memory_order_relaxed);
          break;
        case 3:
          lru.fetch_and(u8(~0x04), std::memory_order_relaxed);
          lru.fetch_or(u8(0x30), std::memory_order_relaxed);
          break;
      }
    }

    // Pick the way to (re)fill in a congruence class: first invalid way if any,
    // otherwise the pseudo-LRU victim. Shared by the HW reload and the SW-managed
    // miss-hint paths so both follow identical "invalid-first, else LRU" policy.
    inline u32 pickVictimWay(u32 classIdx) const {
      for (u32 way = 0; way < NUM_WAYS; ++way) {
        if (!entryAt(classIdx, way).valid) return way;
      }
      return getLRUWay(classIdx);
    }

    // Invalidation

    // Invalidate one way of a congruence class. Only the valid byte needs to be
    // cleared - the tag/pte fields are dead state until valid is set again.
    inline void invalidateWay(u32 classIdx, u32 way) { entries[classIdx * NUM_WAYS + way].valid = 0; }

    inline void invalidateClass(u32 classIdx) {
      for (u32 w = 0; w < NUM_WAYS; ++w) { entries[classIdx * NUM_WAYS + w].valid = 0; }
      lruBits[classIdx].store(0, std::memory_order_relaxed);
    }

    inline void invalidateAll() {
      for (u32 i = 0; i < NUM_ENTRIES; ++i) { entries[i].valid = 0; }
      for (u32 c = 0; c < NUM_CLASSES; ++c) { lruBits[c].store(0, std::memory_order_relaxed); }
    }
  };

  // Xenon Memory Management Unit.
  // Performs address translation and the physical routing of guest PPU Thread's accesses to System Devices and Busses.
  class XenonMMU {
  public:
    XenonMMU(XenonContext* inXenonContext, sPPEState* inPPEState)
        : xenonContext(inXenonContext)
        , ppeState(inPPEState) { }

    //
    // Address translation
    //

    // Main effective->real address translation. Returns false and raises the
    // appropriate storage/segment exception on the addressed thread on failure.
    bool MMUTranslateAddress(u64* EA, bool memWrite, ePPUThreadID thr = ePPUThread_None);

    //
    // Guest memory access (translate + physical routing)
    //

    void MMURead(u64 EA, u64 byteCount, u8* outData, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite(const u8* data, u64 EA, u64 byteCount, ePPUThreadID thr = ePPUThread_None);

    void MMUMemCpyFromHost(u64 EA, const void* source, u64 size, ePPUThreadID thr = ePPUThread_None);
    void MMUMemCpy(u64 EA, u32 source, u64 size, ePPUThreadID thr = ePPUThread_None);
    void MMUMemSet(u64 EA, s32 data, u64 size, ePPUThreadID thr = ePPUThread_None);

    // Raw host pointer into guest RAM (no translation, no bus, no SoC handling).
    u8* MMUGetPointerFromRAM(u64 EA);

    // Sized helpers (big-endian guest <-> host byteswap included).
    u8 MMURead8(u64 EA, ePPUThreadID thr = ePPUThread_None);
    u16 MMURead16(u64 EA, ePPUThreadID thr = ePPUThread_None);
    u32 MMURead32(u64 EA, ePPUThreadID thr = ePPUThread_None);
    u64 MMURead64(u64 EA, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite8(u64 EA, u8 data, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite16(u64 EA, u16 data, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite32(u64 EA, u32 data, ePPUThreadID thr = ePPUThread_None);
    void MMUWrite64(u64 EA, u64 data, ePPUThreadID thr = ePPUThread_None);

    // Reads a kernel PSTRING from guest memory into a host buffer.
    void ReadString(u64 stringAddress, char* string, u32 maxLength);

    //
    // TLB / page-size services used by the interpreter's SLB/TLB opcode handlers
    //

    // Page size (p, log2 bytes) for the current HID6 large-page config.
    u8 GetPageSize(bool L, u8 LP);
    // Software-managed TLB insert from the PPE_TLB_* SPR image.
    void AddTlbEntry();
    // Selective (IS=0) TLB invalidation: clear ways whose tag matches VA at page size p.
    void TlbInvalidateSelective(u64 VA, u8 p, bool L);
    // Class-level (IS=3) TLB invalidation: clear every way of a congruence class.
    void TlbInvalidateClass(u32 classIdx);

  private:
    // Xenon CPU Context.
    XenonContext* xenonContext = nullptr;
    // The PPE state this MMU translates for.
    sPPEState* ppeState = nullptr;

    // PPE Translation Lookaside Buffer.
    TLB_Reg tlb{};

    // Guards TLB lookups (shared lock) vs inserts/invalidations (unique lock),
    // since the two SMT threads drive this MMU concurrently.
    std::shared_mutex tlbMutex{};

    // Hardware-managed TLB insert (HTAB walk path).
    void AddTlbEntryHardware(u64 VA, u64 pte0, u64 pte1, u8 p, bool L, bool LP);
    // TLB lookup. Returns true and sets *RPN on hit.
    bool SearchTlbEntry(u64* RPN, u64 VA, u8 p, bool L, bool LP);

    // Hardware page-table (HTAB) walk. Searches the primary then secondary PTEG for a PTE
    // matching (vsid, EA) at page size p. On a hit it updates the PTE Reference/Change bits,
    // writes them back to guest memory, reloads the TLB, and returns the RPN via *RPN.
    bool WalkPageTable(u64* RPN, u64 EA, u64 VA, u64 vsid, u8 p, bool L, bool LP, bool memWrite);

    // Security-engine address decoding (Xbox 360 specific).
    SECENG_ADDRESS_INFO GetSecEngInfoFromAddress(u64 inputAddress);
    u64 ContructEndAddressFromSecEngAddr(u64 inputAddress, bool* socAccess);
  };
} // namespace Xe::XCPU::MMU
