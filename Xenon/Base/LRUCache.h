#pragma once

#include <iostream>
#include <unordered_map>
#include <list>

class LRUCache {
private:
  int capacity = 0;
  std::list<uint64_t> keys; // Lista doblemente enlazada
  std::unordered_map<uint64_t, std::pair<uint64_t, std::list<uint64_t>::iterator>> cache;

public:
  void resizeCache(uint32_t size) { capacity = size; }

  uint64_t getElement(uint64_t key) {
    if (cache.find(key) == cache.end()) {
      return static_cast<uint64_t>(-1);
    }

    keys.erase(cache[key].second);
    keys.push_front(key);
    cache[key].second = keys.begin();

    return cache[key].first;
  }

  void putElement(uint64_t key, uint64_t value) {
    if (cache.find(key) != cache.end()) {
      keys.erase(cache[key].second);
    }
    else if (keys.size() >= capacity && !keys.empty()) {
      uint64_t lru = keys.back();
      keys.pop_back();
      cache.erase(lru);
    }

    keys.push_front(key);
    cache[key] = { value, keys.begin() };
  }
  void invalidateElement(uint64_t key) {
    if (cache.find(key) != cache.end()) {
      keys.erase(cache[key].second);
      cache.erase(key);
    }
  }

  void invalidateAll() {
    keys.clear();
    cache.clear();
  }
};
