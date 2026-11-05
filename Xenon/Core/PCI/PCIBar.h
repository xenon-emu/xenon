/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <algorithm>
#include <bit>
#include <cstring>

// PCI memory BARs, config space layout and access splitting, shared by every device on the bus so they all decode
// the same way.
//
// Bits 0-3 of a memory BAR are type flags, the rest is the base, aligned to the BAR's size. A BAR
// software hasn't assigned (0, or all ones as a size probe leaves it) decodes nothing, and neither does one the
// device doesn't implement (size 0).

namespace Xe::PCI {

  constexpr u32 BarFlagMask = 0xF;

  // The decode size of a BAR: its size rounded up to a power of two, and at least 16 bytes for the flag bits.
  constexpr u32 BarDecodeSize(u32 size) { return size ? std::bit_ceil(size < 16 ? 16u : size) : 0; }

  // True if the BAR register value bar, of a BAR that is size bytes, decodes anything at all.
  constexpr bool BarAssigned(u32 bar, u32 size) { return size != 0 && bar != 0xFFFFFFFF && (bar & ~BarFlagMask) != 0; }

  // True if address falls in the memory BAR bar of a device whose BAR is size bytes.
  constexpr bool BarDecodes(u32 bar, u32 size, u32 address) {
    if (!BarAssigned(bar, size)) { return false; }
    return address - (bar & ~BarFlagMask) < BarDecodeSize(size); // unsigned: also rejects addresses below the base
  }

  // Offset of address inside BAR bar.
  constexpr u32 BarOffset(u32 bar, u32 address) { return address - (bar & ~BarFlagMask); }

  // What a BAR register latches when software writes value: the base bits its size implements, with the type flags
  // kept from current. Writing all ones reads back ~(size - 1) | flags, which is how software sizes it. A BAR the
  // device doesn't implement stays 0.
  constexpr u32 BarLatch(u32 current, u32 value, u32 size) {
    if (size == 0) { return 0; }
    const u32 decode = BarDecodeSize(size);
    return (value & ~(decode - 1) & ~BarFlagMask) | (current & BarFlagMask);
  }

  // Type 0 / type 1 header offsets.
  constexpr u32 ConfigCommand = 0x04;
  constexpr u32 ConfigRevisionId = 0x08;
  constexpr u32 ConfigBar0 = 0x10;
  constexpr u32 ConfigBarCount = 6;
  constexpr u32 ConfigSecondaryBus = 0x19; // type 1
  constexpr u32 ConfigSpaceSize = 0x100;
  constexpr bool IsBarOffset(u32 offset) { return offset >= ConfigBar0 && offset < ConfigBar0 + 4 * ConfigBarCount; }

  // COMMAND bits.
  constexpr u16 CommandMemorySpace = 0x0002;

  // Config space window: 0xD0000000 | bus << 20 | device << 15 | function << 12 | register. The kernel and the
  // hypervisor both address it this way, and software only uses registers 0x00-0xFF of each function.
  constexpr u32 ConfigWindowBase = 0xD0000000;
  constexpr u32 ConfigWindowSize = 0x01000000;
  constexpr u32 ConfigBus(u32 address) { return (address >> 20) & 0xFF; }
  constexpr u32 ConfigDevice(u32 address) { return (address >> 15) & 0x1F; }
  constexpr u32 ConfigFunction(u32 address) { return (address >> 12) & 0x7; }
  constexpr u32 ConfigRegister(u32 address) { return address & 0xFFF; }

  // Calls f(at, done, count) for each piece of an access of size bytes at offset that stays inside one dword. at is
  // the piece's offset, done the bytes before it and count its length.
  template <typename F>
  void ForEachDwordLane(u32 offset, u64 size, F &&f) {
    for (u64 done = 0; done < size;) {
      const u32 at = offset + static_cast<u32>(done);
      const u64 count = std::min<u64>(size - done, 4 - (at & 3));
      f(at, done, count);
      done += count;
    }
  }

  // A MemSet built from ordinary writes: calls write(done, bytes, count) with pieces of up to 16 bytes of value that
  // together cover size bytes (done is the bytes before the piece).
  template <typename F>
  void FillInChunks(s32 value, u64 size, F &&write) {
    u8 bytes[16];
    memset(bytes, value, sizeof(bytes));
    for (u64 done = 0; done < size; done += sizeof(bytes)) {
      write(done, bytes, std::min<u64>(sizeof(bytes), size - done));
    }
  }

} // namespace Xe::PCI
