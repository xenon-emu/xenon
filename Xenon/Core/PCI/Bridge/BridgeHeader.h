/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/PCI/PCIBar.h"

#include <array>
#include <cstring>

// The config header a bridge on bus 0 keeps for itself: the host bridge's (type 0) and the PCI-PCI bridge's
// (type 1). Software can change command, cache line size and latency, BAR0 and, on type 1, the bus numbers and
// windows. IDs, class, revision, header type and status are fixed.

namespace Xe::PCI {

  class BridgeHeader {
  public:
    std::array<u8, ConfigSpaceSize> bytes{};

    u32 Dword(u32 offset) const {
      u32 value;
      memcpy(&value, &bytes[offset], sizeof(value));
      return value;
    }
    void SetDword(u32 offset, u32 value) { memcpy(&bytes[offset], &value, sizeof(value)); }

    void Read(u32 reg, u8 *data, u64 size) const {
      for (u64 i = 0; i != size; ++i) { data[i] = bytes[(reg + i) & 0xFF]; }
    }

    // Software writes size bytes at reg. BAR0 latches to bar0Size (a size probe reads back ~(bar0Size - 1)). The BARs
    // after it, up to barCount, aren't implemented and read as zero.
    void Write(u32 reg, const u8 *data, u64 size, u32 bar0Size, u32 barCount) {
      ForEachDwordLane(reg, size, [&](u32 at, u64 done, u64 count) {
        const u32 dword = at & 0xFC;
        const u32 current = Dword(dword);
        u32 value = current;
        memcpy(reinterpret_cast<u8 *>(&value) + (at & 3), data + done, count);
        switch (dword) {
          case 0x00: // vendor/device ID
          case 0x08: // revision/class
            return;
          case 0x04:
            // Status (upper half) has nothing to clear.
            value = (value & 0xFFFF) | (current & 0xFFFF0000);
            break;
          case 0x0C:
            // Header type is read-only.
            value = (value & 0xFF00FFFF) | (current & 0x00FF0000);
            break;
          default:
            if (IsBarOffset(dword) && dword < ConfigBar0 + 4 * barCount) {
              value = BarLatch(current, value, dword == ConfigBar0 ? bar0Size : 0);
            }
            break;
        }
        SetDword(dword, value);
      });
    }
  };

} // namespace Xe::PCI
