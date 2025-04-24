// Copyright 2025 Xenon Emulator Project

#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "Base/Types.h"

struct PPU_RES {
  u8 ppuID;
  volatile bool V;
  volatile u64 resAddr;
};

class XenonReservations {
public:
  XenonReservations();
  virtual bool Register(PPU_RES *Res);
  void Increment(void) {
    std::lock_guard lock(ReservationLock);
    nReservations++;
  }
  void Decrement(void) {
    std::lock_guard lock(ReservationLock);
    nReservations--;
  }
  void Check(u64 x) {
    if (nReservations)
      Scan(x);
  }
  virtual void Scan(u64 PhysAddress);
  void LockGuard(std::function<void()> callback) {
    std::lock_guard lock(ReservationLock);
    if (callback) {
      callback();
    }
  }
private:
  long nReservations;
  std::recursive_mutex ReservationLock;
  int nProcessors;
  struct PPU_RES *Reservations[6];
};
