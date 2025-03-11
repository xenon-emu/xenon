#pragma once

#include <iostream>
#include <unordered_map>
#include <list>


class LRUCache {
private:
  size_t capacity;
  std::list<uint64_t> keys;
  // Doubly linked list storing keys for LRU tracking
  std::unordered_map<uint64_t, std::pair<uint64_t, std::list<uint64_t>::iterator>> cache;

public:
  explicit LRUCache(size_t size) : capacity(size) {}

  void resizeCache(size_t size) {
    capacity = size;
    while (keys.size() > capacity) {
      uint64_t lru = keys.back();
      keys.pop_back();
      cache.erase(lru);
    }
  }

  uint64_t getElement(uint64_t key) {
    auto it = cache.find(key);
    if (it == cache.end()) {
      return static_cast<uint64_t>(-1);
    }

    // Move accessed key to front (most recently used)
    keys.splice(keys.begin(), keys, it->second.second);

    return it->second.first;
  }

  void putElement(uint64_t key, uint64_t value) {
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
      uint64_t lru = keys.back();
      keys.pop_back();
      cache.erase(lru);
    }

    keys.push_front(key);
    cache[key] = {value, keys.begin()};
  }

  void invalidateElement(uint64_t key) {
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
