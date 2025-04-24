// Copyright 2025 Xenon Emulator Project

#include "XenonReservations.h"

XenonReservations::XenonReservations() {
  std::lock_guard lock(ReservationLock);
  nReservations = 0;
  nProcessors = 0;
  Reservations[0] = nullptr;
}

bool XenonReservations::Register(PPU_RES *Res) {
  std::lock_guard lock(ReservationLock);
  Reservations[nProcessors] = Res;
  nProcessors++;
  return true;
}

void XenonReservations::Scan(u64 PhysAddress) {
  std::lock_guard lock(ReservationLock);
  // Address must be aligned.
  PhysAddress &= ~7;

  for (int i = 0; i < nProcessors; i++) {
    // NB: order of checks matters!
    if ((Reservations[i]->V) && PhysAddress >= Reservations[i]->resAddr) {
      Reservations[i]->V = false;
      nReservations--;
    }
  }
}
