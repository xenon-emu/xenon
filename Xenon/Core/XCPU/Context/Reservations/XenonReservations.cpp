/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "XenonReservations.h"

XenonReservations::XenonReservations() {
  numReservations.store(0, std::memory_order_relaxed);
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
  // CBE processor's reservation granule is 128 bytes (PPE cache line size).
  // Reservations are invalidated if any store hits the same 128-byte block.
  
  // Lock-free scan: PPU_RES fields are now atomic, so we can check and
  // invalidate reservations without acquiring the mutex
  constexpr u64 RESERVATION_GRANULE_MASK = ~u64(127);
  PhysAddress &= RESERVATION_GRANULE_MASK;

  for (int i = 0; i < processors; i++) {
    // NB: order of checks matters!
    if (reservations[i]->valid.load(std::memory_order_acquire) &&
        PhysAddress == (reservations[i]->reservedAddr.load(std::memory_order_relaxed) & RESERVATION_GRANULE_MASK)) {
      reservations[i]->valid.store(false, std::memory_order_release);
      numReservations.fetch_sub(1, std::memory_order_relaxed);
    }
  }
}
