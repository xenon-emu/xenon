/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

struct PPU_RES {
  u8 ppuID;
  std::atomic<bool> valid{false};
  std::atomic<u64> reservedAddr{0};
};

class XenonReservations {
public:
  XenonReservations();
  virtual bool Register(PPU_RES *Res);
  void Increment(void) {
    numReservations.fetch_add(1, std::memory_order_relaxed);
  }
  void Decrement(void) {
    numReservations.fetch_sub(1, std::memory_order_relaxed);
  }
  void Check(u64 x) {
    if (numReservations.load(std::memory_order_relaxed))
      Scan(x);
  }
  virtual void Scan(u64 PhysAddress);
  void LockGuard(std::function<void()> callback) {
    std::lock_guard lock(reservationLock);
    if (callback) {
      callback();
    }
  }
private:
  std::atomic<s32> numReservations{0};
  std::mutex reservationLock;
  s32 processors;
  struct PPU_RES *reservations[6];
};
