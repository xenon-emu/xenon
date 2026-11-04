/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <array>
#include <string>

// Builds a console's eFuse lines from what its NAND says (extracted by NandInfo) and its CPU key. Nothing here reads
// the NAND or a file: NandInfo reads everything out of the image once and owns the result, this is only the arithmetic.
//
// CB_A carries the policy its fuse check enforces. Byte 0x3B0 is the console-type policy: where bit i is set, byte i
// (LSB first) of fuse line 1 is 0xF0, where it is clear the byte is 0x0F. Byte 0x3B1 is the CB LDV: fuse line 2 holds
// that many 0xF nibbles from the MSB (the hypervisor panics on a smaller value). Line 0 uses the standard factory
// JTAG-disable value, and retail or devkit target identity is in line 1. The CPU key supplies lines 3-6, and lines 7-11
// are the update LDV of a CF patch slot in the NAND.

namespace Xe::FuseDerive {

  constexpr u32 FuseLineCount = 12;

  enum class NandTarget : u8 {
    Unknown,
    RetailPhat,
    RetailSlim,
    Devkit,
    TestKit,
  };

  // What NandInfo extracted from the bootloaders for the fuse derivation.
  struct NandPolicy {
    u16 cbBuild = 0;
    u8 consoleTypePolicy = 0;         // CB_A[0x3B0]
    u8 cbLdv = 0;                     // CB_A[0x3B1]
    bool layoutKnown = false;         // the bootloader decrypted and its 0x3B0 policy/0x3B1 LDV passed validation
    bool developerBootloader = false; // J-Runner: a bootloader tag whose first byte is not 'C'
    NandTarget nandTarget = NandTarget::Unknown; // target encoded by CB_A's console-type fuse policy

    // J-Runner decrypts CF patch slots 0/1 and reads their update LDV at plaintext offset 0x21F. A differing pair is
    // reported. 'updateLdv' is the highest NAND-declared value, an inferred minimum, not a hardware fuse readout.
    std::array<bool, 2> cfFound{};
    std::array<u8, 2> cfLdv{};
    std::array<u16, 2> cfBuild{};
    bool updateLdvKnown = false;
    u8 updateLdv = 0;
  };

  using FuseLines = std::array<u64, FuseLineCount>;

  // Fuse line 1 for a CB_A console-type policy byte.
  u64 Line1FromPolicy(u8 policy);
  // A fuse line whose top 'nibbles' nibbles are 0xF.
  u64 LdvLine(u32 nibbles);
  // The console type CB_A's policy byte stands for.
  NandTarget NandTargetFromPolicy(u8 policy);

  // The update LDV BuildFuses takes from policy for cfPatchSlot. 'slotFound' says whether that slot exists (else the
  // highest LDV is used).
  u32 UpdateLdvFor(const NandPolicy &policy, s32 cfPatchSlot, bool *slotFound = nullptr);

  // Builds all fuse lines from the CB policy, selected CF slot (or highest LDV when cfPatchSlot is -1), and cpuKey.
  FuseLines BuildFuses(const NandPolicy &policy, const std::array<u8, 16> &cpuKey, s32 cfPatchSlot = -1);

} // namespace Xe::FuseDerive
