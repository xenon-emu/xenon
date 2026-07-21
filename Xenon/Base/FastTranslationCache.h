/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <cstring>

#include "Base/Types.h"

// Fast EA-to-host-pointer translation cache:
// Direct-mapped, 8192-entry cache that maps guest EA pages directly to host
// memory pointers, completely bypassing SecEng decode and RootBus dispatch
// on cache hits.

class FastTranslationCache {
public:
  static constexpr u64 PAGE_SHIFT = 12;
  static constexpr u64 PAGE_SIZE = 1ULL << PAGE_SHIFT;
  static constexpr u64 PAGE_MASK = PAGE_SIZE - 1;
  static constexpr size_t NUM_ENTRIES = 8192;
  static constexpr size_t INDEX_MASK = NUM_ENTRIES - 1;
  static constexpr u64 INVALID_TAG = ~0ULL;

  FastTranslationCache() { invalidateAll(); }
  ~FastTranslationCache() = default;

  // Fast lookup - returns host pointer for the full EA, or nullptr on miss
  u8* lookup(u64 EA) const {
    const u64 page = EA & ~PAGE_MASK;
    const size_t idx = computeIndex(page);
    const Entry &e = entries[idx];
    if (e.tag == page) [[likely]] {
      return e.hostPage + (EA & PAGE_MASK);
    }
    return nullptr;
  }

  // Insert a mapping: EA page -> host page base pointer.
  // Only call this for RAM-backed translations (physical addr < PHYS_MEMORY_END).
  void insert(u64 EA, u8 *hostPageBase) {
    const u64 page = EA & ~PAGE_MASK;
    const size_t idx = computeIndex(page);
    entries[idx].tag = page;
    entries[idx].hostPage = hostPageBase;
  }

  // Invalidate all entries. Used on SLB invalidate all (slbia),
  // and TLB invalidation instructions.
  // NOTE: Due to how the tlb behaves, we cant do a 1:1 page mapping and insted we must invalidate the entire cache.
  void invalidateAll() {
    for (size_t i = 0; i < NUM_ENTRIES; ++i) {
      entries[i].tag = INVALID_TAG;
      entries[i].hostPage = nullptr;
    }
  }

private:
  struct Entry {
    u64 tag;       // EA page (EA & ~0xFFF), INVALID_TAG if empty
    u8 *hostPage;  // Host pointer to page base
  };

  // Hash function: XOR-fold upper bits into lower for better distribution.
  // bits 12..24 XOR'd with bits 25..37 gives the 13-bit index.
  static constexpr size_t computeIndex(u64 page) {
    const u64 shifted = page >> PAGE_SHIFT;
    return static_cast<size_t>((shifted ^ (shifted >> 13)) & INDEX_MASK);
  }

  Entry entries[NUM_ENTRIES];
};
