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

void XenonReservations::Scan(u64 PhysAddress) {
  std::lock_guard lock(reservationLock);
  // CBE processor's reservation granule is 128 bytes (PPE cache line size).
  // Reservations are invalidated if any store hits the same 128-byte block.
  constexpr u64 RESERVATION_GRANULE_MASK = ~u64(127);
  PhysAddress &= RESERVATION_GRANULE_MASK;

  for (int i = 0; i < processors; i++) {
    // NB: order of checks matters!
    if (reservations[i]->valid && PhysAddress == (reservations[i]->reservedAddr & RESERVATION_GRANULE_MASK)) {
      reservations[i]->valid = false;
      numReservations--;
    }
  }
}
