/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <mutex>
#endif

#include "Types.h"

namespace Base {

class FutexMutex {
public:
#ifdef _WIN32
  FutexMutex() :
    state(0)
  {}
#else
  FutexMutex()
  {}
#endif

  void lock() {
#ifdef _WIN32
    s32 expected = 0;
    while (!state.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
      // Failed to acquire lock, wait on address
      expected = 0;
      WaitOnAddress(&state, &expected, sizeof(expected), INFINITE);
    }
#else
    mtx.lock();
#endif
  }

  void unlock() {
#ifdef _WIN32
    state.store(0, std::memory_order_release);
    WakeByAddressSingle(&state);
#else
    mtx.unlock();
#endif
  }

  bool try_lock() {
#ifdef _WIN32
    s32 expected = 0;
    return state.compare_exchange_strong(expected, 1, std::memory_order_acquire);
#else
    return mtx.try_lock();
#endif
  }

private:
#ifdef _WIN32
  alignas(4) std::atomic<s32> state{};
#else
  std::mutex mtx{};
#endif
};

class FutexRecursiveMutex {
public:
  void lock() {
    std::thread::id this_id = std::this_thread::get_id();
    if (owner == this_id) {
      recursion++;
      return;
    }

    baseLock.lock();

    owner = this_id;
    recursion = 1;
  }

  bool try_lock() {
    std::thread::id this_id = std::this_thread::get_id();
    if (owner == this_id) {
      recursion++;
      return true;
    }

    if (!baseLock.try_lock())
      return false;

    owner = this_id;
    recursion = 1;

    return true;
  }

  void unlock() {
    if (--recursion == 0) {
      owner = std::thread::id();
      baseLock.unlock();
    }
  }

private:
  FutexMutex baseLock = {};
  std::thread::id owner = {};
  s32 recursion = 0;
};

} // namespace Base
