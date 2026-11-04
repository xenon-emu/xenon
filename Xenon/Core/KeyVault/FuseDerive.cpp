/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "FuseDerive.h"

#include <algorithm>

namespace Xe::FuseDerive {

  constexpr u64 kStandardLine0 = 0xC0FFFFFFFFFFFFFFull;

  // Fuse lines are LSB-first bytes: byte i is 0xF0 when bit i of policy is set, else 0x0F.
  u64 Line1FromPolicy(u8 policy) {
    u64 line = 0;
    for (u32 i = 0; i < 8; ++i) { line |= static_cast<u64>((policy >> i) & 1 ? 0xF0 : 0x0F) << (8 * i); }
    return line;
  }

  // The top 'nibbles' nibbles are 0xF.
  u64 LdvLine(u32 nibbles) {
    if (nibbles == 0) { return 0; }
    if (nibbles >= 16) { return ~0ull; }
    return ~0ull << (64 - 4 * nibbles);
  }

  NandTarget NandTargetFromPolicy(u8 policy) {
    // CB_A[0x3B0] maps to the documented fuse-set-01 patterns: devkit=0, retail phat=1,
    // test kit=2, and retail slim=3. Leave other policy values unclassified.
    switch (policy) {
      case 0: return NandTarget::Devkit;
      case 1: return NandTarget::RetailPhat;
      case 2: return NandTarget::TestKit;
      case 3: return NandTarget::RetailSlim;
      default: return NandTarget::Unknown;
    }
  }

  u32 UpdateLdvFor(const NandPolicy &policy, s32 cfPatchSlot, bool *slotFound) {
    const bool found = cfPatchSlot >= 0 && static_cast<size_t>(cfPatchSlot) < policy.cfFound.size()
                       && policy.cfFound[static_cast<size_t>(cfPatchSlot)];
    if (slotFound) { *slotFound = found; }
    if (found) { return policy.cfLdv[static_cast<size_t>(cfPatchSlot)]; }
    return policy.updateLdvKnown ? policy.updateLdv : 0;
  }

  FuseLines BuildFuses(const NandPolicy &policy, const std::array<u8, 16> &cpuKey, s32 cfPatchSlot) {
    FuseLines lines{};
    // Devkit identity is encoded by line 1. Line 0 keeps the standard factory value.
    lines[0] = kStandardLine0;
    lines[1] = Line1FromPolicy(policy.consoleTypePolicy);
    lines[2] = LdvLine(policy.cbLdv);
    u64 hi = 0;
    u64 lo = 0;
    for (u32 i = 0; i < 8; ++i) {
      hi = (hi << 8) | cpuKey[i];
      lo = (lo << 8) | cpuKey[8 + i];
    }
    lines[3] = lines[4] = hi;
    lines[5] = lines[6] = lo;
    const u32 updateLdv = UpdateLdvFor(policy, cfPatchSlot);
    for (u32 line = 0; line < 5; ++line) {
      const u32 firstNibble = line * 16;
      const u32 nibbles = updateLdv > firstNibble ? std::min(16u, updateLdv - firstNibble) : 0;
      lines[7 + line] = LdvLine(nibbles);
    }
    return lines;
  }

} // namespace Xe::FuseDerive
