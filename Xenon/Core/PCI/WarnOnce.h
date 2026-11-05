/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <mutex>
#include <unordered_set>

// Remembers what was already reported so a warning gets logged once, not on every access.

namespace Xe::PCI {

  template <typename T>
  class WarnOnce {
  public:
    // True the first time key is passed in.
    bool First(const T &key) {
      std::lock_guard lock(mutex);
      return seen.insert(key).second;
    }

  private:
    std::mutex mutex;
    std::unordered_set<T> seen;
  };

} // namespace Xe::PCI
