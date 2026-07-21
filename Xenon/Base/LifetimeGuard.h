/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>

#include "Types.h"

namespace Base {

// NEVER call back up the bus hierarchy (e.g. from a device's
// Read/Write back into its parent bridge/bus) while holding a Lease on a
// node below it. Doing so can deadlock RetireAndWait() on the ancestor.
class LifetimeGuard {
public:
  // RAII lease acquired via LifetimeGuard::TryAcquire(). Non-copyable,
  // movable. An empty (falsy) Lease means the guard is retired/retiring;
  // treat it exactly like a failed weak_ptr::lock().
  class Lease {
  public:
    Lease() = default;
    Lease(Lease &&other) noexcept : guard(std::exchange(other.guard, nullptr)) {}
    Lease &operator=(Lease &&other) noexcept {
      if (this != &other) {
        Release();
        guard = std::exchange(other.guard, nullptr);
      }
      return *this;
    }
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;
    ~Lease() { Release(); }

    explicit operator bool() const { return guard != nullptr; }

  private:
    friend class LifetimeGuard;
    explicit Lease(LifetimeGuard *g) : guard(g) {}
    void Release() {
      if (guard) {
        LifetimeGuard *g = std::exchange(guard, nullptr);
        if (g->activeCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          std::lock_guard lck(g->drainMutex);
          g->drainCv.notify_all();
        }
      }
    }
    LifetimeGuard *guard = nullptr;
  };

  // Hot path. Call at the top of every Read/Write/MemSet/ConfigRead/
  // ConfigWrite entry point. Returns an empty Lease if the object is
  // retired/retiring.
  [[nodiscard]] Lease TryAcquire() {
    if (retired.load(std::memory_order_acquire)) {
      return Lease{};
    }
    activeCount.fetch_add(1, std::memory_order_acq_rel);
    // Re-check to close the window where Retire() ran between our first
    // check and the increment.
    if (retired.load(std::memory_order_acquire)) {
      if (activeCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        std::lock_guard lck(drainMutex);
        drainCv.notify_all();
      }
      return Lease{};
    }
    return Lease{this};
  }

  // Teardown path only. Marks the object as no-longer-acquirable, then
  // blocks until every outstanding Lease has released or timeoutMs elapses
  // (0 = wait forever). Returns false on timeout; caller should LOG_CRITICAL
  // and decide whether to proceed anyway or abort.
  bool RetireAndWait(u32 timeoutMs = 250) {
    retired.store(true, std::memory_order_release);
    std::unique_lock lck(drainMutex);
    if (timeoutMs == 0) {
      drainCv.wait(lck, [this] { return activeCount.load(std::memory_order_acquire) == 0; });
      return true;
    }
    return drainCv.wait_for(lck, std::chrono::milliseconds(timeoutMs),
      [this] { return activeCount.load(std::memory_order_acquire) == 0; });
  }

  // Allows an object to accept new leases again after a successful
  // RetireAndWait() (e.g. in-place device replacement).
  void Reopen() {
    retired.store(false, std::memory_order_release);
  }

  bool IsRetired() const { return retired.load(std::memory_order_acquire); }
  s32 ActiveLeaseCount() const { return activeCount.load(std::memory_order_relaxed); }

private:
  std::atomic<s32> activeCount{ 0 };
  std::atomic<bool> retired{ false };
  std::mutex drainMutex{};
  std::condition_variable drainCv{};
};

} // namespace Base
