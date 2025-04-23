// Copyright 2025 Xenon Emulator Project

#include "XenonReservations.h"

XenonReservations::XenonReservations() {
  nReservations = 0;
  nProcessors = 0;
  Reservations[0] = nullptr;
}

bool XenonReservations::Register(PPU_RES *Res) {
  AcquireLock();
  Reservations[nProcessors] = Res;
  nProcessors++;
  ReleaseLock();
  return true;
}

void XenonReservations::Scan(u64 PhysAddress) {
  // Address must be aligned.
  PhysAddress &= ~7;

  AcquireLock();
  for (int i = 0; i < nProcessors; i++) {
    // NB: order of checks matters!
    if ((Reservations[i]->V) && PhysAddress >= Reservations[i]->resAddr) {
      Reservations[i]->V = false;
      Decrement();
    }
  }
  ReleaseLock();
}
