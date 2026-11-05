/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/PCI/DmaEngine.h"
#include "Core/PCI/SATA.h"

// Walking a Physical Region Descriptor table, the way the southbridge's SATA bus master does for a DMA transfer.
// Entries are {u32 address, u32 count | PrdEndOfTable on the last entry}, little endian, back to back. A count of
// 0 means 64 KiB as per the ATA spec.

namespace Xe::PCI {

  // Last entry flag of a PRD.
  constexpr u32 PrdEndOfTable = 0x80000000;

  enum class PrdFault {
    None,
    Unreadable, // an entry isn't in guest memory
    TooLong     // no end-of-table entry within XE_ATA_PRD_MAX_ENTRIES entries
  };

  struct PrdWalk {
    PrdFault fault = PrdFault::None;
    bool last = false; // the last entry visited was the end-of-table entry
  };

  // Calls entry(address, length) for each entry of the table until it returns false, the end-of-table entry has been
  // visited or the table is cut off (fault).
  template <typename F>
  PrdWalk WalkPrdTable(const DmaEngine &dma, u32 table, F &&entry) {
    PrdWalk walk;
    for (u32 index = 0;; ++index) {
      if (index == XE_ATA_PRD_MAX_ENTRIES) {
        walk.fault = PrdFault::TooLong;
        return walk;
      }
      const u8 *prd = dma.ReadPointer(table, 8);
      if (!prd) {
        walk.fault = PrdFault::Unreadable;
        return walk;
      }
      table += 8;
      const u32 control = LoadLe32(prd + 4);
      walk.last = (control & PrdEndOfTable) != 0;
      const u32 count = control & 0xFFFF;
      if (!entry(LoadLe32(prd), count ? count : 0x10000) || walk.last) { return walk; }
    }
  }

} // namespace Xe::PCI
