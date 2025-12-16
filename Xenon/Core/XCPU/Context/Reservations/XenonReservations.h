/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <functional>
#include <mutex>
#include <vector>

struct PPU_RES {
  u8 ppuID;
  volatile bool valid;
  volatile u64 reservedAddr;
};

class XenonReservations {
public:
  XenonReservations();
  virtual bool Register(PPU_RES *Res);
  void Increment(void) {
    std::lock_guard lock(reservationLock);
    numReservations++;
  }
  void Decrement(void) {
    std::lock_guard lock(reservationLock);
    numReservations--;
  }
  void Check(u64 x, bool word) {
    if (numReservations)
      Scan(x, word);
  }
  virtual void Scan(u64 PhysAddress, bool word);
  void LockGuard(std::function<void()> callback) {
    std::lock_guard lock(reservationLock);
    if (callback) {
      callback();
    }
  }
private:
  s32 numReservations;
  Base::FutexRecursiveMutex reservationLock;
  s32 processors;
  struct PPU_RES *reservations[6];
};
