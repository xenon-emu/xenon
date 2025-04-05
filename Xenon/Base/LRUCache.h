// Copyright 2025 Xenon Emulator Project

#pragma once

#include <unordered_map>
#include <list>

#include "Base/Types.h"

class LRUCache {
private:
  size_t capacity;
  std::list<u64> keys;
  // Doubly linked list storing keys for LRU tracking
  std::unordered_map<u64, std::pair<u64, std::list<u64>::iterator>> cache;

public:
  explicit LRUCache(size_t size) : capacity(size) {}

  void resizeCache(size_t size) {
    capacity = size;
    while (keys.size() > capacity) {
      u64 lru = keys.back();
      keys.pop_back();
      cache.erase(lru);
    }
  }

  const u64 getElement(u64 key) {
    auto it = cache.find(key);
    if (it == cache.end()) {
      return static_cast<u64>(-1);
    }

    // Move accessed key to front (most recently used)
    keys.splice(keys.begin(), keys, it->second.second);

    return it->second.first;
  }

  void putElement(u64 key, u64 value) {
    if (capacity == 0)
      return;
    
    auto it = cache.find(key);
    if (it != cache.end()) {
      // Key exists, move it to front and update value
      keys.splice(keys.begin(), keys, it->second.second);
      it->second.first = value;
      return;
    }

    if (keys.size() >= capacity) {
      // Evict LRU element
      u64 lru = keys.back();
      keys.pop_back();
      cache.erase(lru);
    }

    keys.push_front(key);
    cache[key] = {value, keys.begin()};
  }

  void invalidateElement(u64 key) {
    auto it = cache.find(key);
    if (it != cache.end()) {
      keys.erase(it->second.second);
      cache.erase(it);
    }
  }

  void invalidateAll() {
    keys.clear();
    cache.clear();
  }
};
