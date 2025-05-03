// Copyright 2025 Xenon Emulator Project

#include "XenonReservations.h"

XenonReservations::XenonReservations() {
  std::lock_guard lock(reservationLock);
  numReservations = 0;
  processors = 0;
  reservations[0] = nullptr;
}

bool XenonReservations::Register(PPU_RES *Res) {
  std::lock_guard lock(reservationLock);
  reservations[processors] = Res;
  processors++;
  return true;
}

void XenonReservations::Scan(u64 PhysAddress) {
  std::lock_guard lock(reservationLock);
  // Address must be aligned.
  PhysAddress &= ~7;

  for (int i = 0; i < processors; i++) {
    // NB: order of checks matters!
    if (reservations[i]->valid && PhysAddress >= reservations[i]->reservedAddr) {
      reservations[i]->valid = false;
      numReservations--;
    }
  }
}
