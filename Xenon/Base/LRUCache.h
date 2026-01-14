/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <cstring>

// Optimized ERAT cache implementation
// The Xbox 360 ERAT is a 64-entry, 2-way set-associative cache
// This implementation uses direct array indexing for O(1) lookups

class LRUCache {
private:
  // Cache entry structure - packed for better cache line utilization
  struct CacheEntry {
    u64 key;    // EA page (4KB aligned)
    u64 value;  // RA page (4KB aligned)
    bool valid;
    u8 padding[7]; // Align to 24 bytes
  };

  // 2-way set-associative cache with 256 sets = 512 total entries
  // NOTE: On real hardware, the ERAT uses 64 entry sets, but we use 512 here for perf increases.
  static constexpr size_t NUM_SETS = 256;
  static constexpr size_t NUM_WAYS = 2;
  static constexpr u64 INVALID_KEY = ~0ULL;

  // Cache storage - each set has 2 ways
  CacheEntry entries[NUM_SETS][NUM_WAYS];

  // LRU bit per set: 0 = way0 is LRU, 1 = way1 is LRU
  u8 lruBits[NUM_SETS];

  // Hash function to compute set index from EA page
  // Uses bits that vary most in typical address patterns
  static constexpr size_t getSetIndex(u64 key) {
    // Use bits 12-16 XOR'd with bits 17-21 for better distribution
    // (bits 0-11 are always 0 since key is 4KB aligned)
    return ((key >> 12) ^ (key >> 17)) & (NUM_SETS - 1);
  }

public:
  LRUCache() {
    invalidateAll();
  }

  ~LRUCache() = default;

  // Fast lookup - returns value or -1 if not found
  u64 getElement(u64 key) {
    const size_t setIdx = getSetIndex(key);

    // Check way 0
    if (entries[setIdx][0].valid && entries[setIdx][0].key == key) {
      lruBits[setIdx] = 1; // Way 0 was just used, way 1 is now LRU
      return entries[setIdx][0].value;
    }

    // Check way 1
    if (entries[setIdx][1].valid && entries[setIdx][1].key == key) {
      lruBits[setIdx] = 0; // Way 1 was just used, way 0 is now LRU
      return entries[setIdx][1].value;
    }

    return static_cast<u64>(-1); // Cache miss
  }

  // Insert or update entry
  void putElement(u64 key, u64 value) {
    const size_t setIdx = getSetIndex(key);

    // Check if key already exists in either way
    if (entries[setIdx][0].valid && entries[setIdx][0].key == key) {
      entries[setIdx][0].value = value;
      lruBits[setIdx] = 1; // Way 0 just used
      return;
    }

    if (entries[setIdx][1].valid && entries[setIdx][1].key == key) {
      entries[setIdx][1].value = value;
      lruBits[setIdx] = 0; // Way 1 just used
      return;
    }

    // Key not found - insert into LRU way
    const u8 lruWay = lruBits[setIdx];
    entries[setIdx][lruWay].key = key;
    entries[setIdx][lruWay].value = value;
    entries[setIdx][lruWay].valid = true;

    // Update LRU: the way we just wrote to is now MRU
    lruBits[setIdx] = lruWay ^ 1;
  }

  // Invalidate a specific entry
  void invalidateElement(u64 key) {
    const size_t setIdx = getSetIndex(key);

    if (entries[setIdx][0].valid && entries[setIdx][0].key == key) {
      entries[setIdx][0].valid = false;
      entries[setIdx][0].key = INVALID_KEY;
      return;
    }

    if (entries[setIdx][1].valid && entries[setIdx][1].key == key) {
      entries[setIdx][1].valid = false;
      entries[setIdx][1].key = INVALID_KEY;
    }
  }

  // Invalidate all entries - fast bulk clear
  void invalidateAll() {
    for (size_t i = 0; i < NUM_SETS; ++i) {
      entries[i][0].valid = false;
      entries[i][0].key = INVALID_KEY;
      entries[i][1].valid = false;
      entries[i][1].key = INVALID_KEY;
      lruBits[i] = 0;
    }
  }
};
