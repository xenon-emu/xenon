/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

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

void XenonReservations::Scan(u64 PhysAddress, bool word) {
  std::lock_guard lock(reservationLock);
  // Address must be aligned.
  PhysAddress &= (word ? ~3 : ~7);

  for (int i = 0; i < processors; i++) {
    // NB: order of checks matters!
    if (reservations[i]->valid && PhysAddress == reservations[i]->reservedAddr) {
      reservations[i]->valid = false;
      numReservations--;
    }
  }
}
