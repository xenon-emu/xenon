/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Thread.h"
#include "Base/Types.h"

#include <string>
#include <string_view>
#include <thread>
#include <utility>

// A device's worker thread, named the same way everywhere ("[Xe] name"). Each device keeps its own stop flag and
// condition variable since they wait on the lock guarding its state: the device tells the thread to stop and wakes
// it before calling Join.

namespace Xe::PCI {

  class Worker {
  public:
    Worker() = default;
    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;
    ~Worker() { Join(); }

    // Runs body on a new thread.
    template <typename F>
    void Start(std::string_view name, F &&body) {
      thread = std::thread([threadName = "[Xe] " + std::string(name), body = std::forward<F>(body)]() mutable {
        Base::SetCurrentThreadName(threadName);
        body();
      });
    }

    void Join() {
      if (thread.joinable()) { thread.join(); }
    }

  private:
    std::thread thread;
  };

} // namespace Xe::PCI
