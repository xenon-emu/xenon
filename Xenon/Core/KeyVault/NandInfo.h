/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/KeyVault/FuseDerive.h"
#include "Core/KeyVault/KeyVault.h"

#include <array>
#include <string>
#include <vector>

// What a NAND dump holds. Load() reads the flash header, the bootloader chain, the SMC firmware, the encrypted keyvault
// and the fuse policy once. Unlock() decrypts the keyvault with the CPU key and derives the fuses. The console being
// booted has one of these (Console()), and the setup wizard makes its own to preview a NAND.

namespace Xe::NandInfo {

  struct Bootloader {
    char name[3] = {}; // "CB", "CD", "CE", ...
    u32 offset = 0;    // logical address in the flash
    u16 build = 0;
    u32 entry = 0;
    u32 length = 0;
  };

  struct Result {
    bool opened = false;
    std::string error; // why the file could not be read
    u64 fileSize = 0;
    bool hasSpare = false; // raw dump with recognized page spare/ECC data

    // Flash header.
    u16 magic = 0;
    u16 build = 0;
    u16 qfe = 0;
    u16 flags = 0;
    u32 entry = 0; // CB_A offset
    std::string copyright;
    u32 keyvaultAddr = 0;
    u32 keyvaultSize = 0;
    u16 keyvaultVer = 0;
    u32 sysUpdateAddr = 0;
    u16 sysUpdateCount = 0;
    u32 smcConfigAddr = 0;
    u32 smcBootAddr = 0;
    u32 smcBootSize = 0;

    // SMC firmware, decrypted.
    bool smcOk = false;
    std::string smcVersion;
    std::array<u8, 4> smcVersionBytes{}; // the 4 bytes the version is read from, for checking it by eye

    std::vector<Bootloader> bootloaders;

    // The keyvault as the NAND stores it (empty when the header names none that fits).
    std::vector<u8> encryptedKeyVault;

    // CB_A's fuse policy and the CF patch slots (what the NAND needs the fuses to be).
    bool policyOk = false;
    FuseDerive::NandPolicy policy;

    // A split CB's second stage, as stored, with CB_A's header. From CB 13121 on only CB_B carries the policy (CB_A has
    // zeros at 0x3B0), and CB_B's key needs the CPU key, so Unlock() reads it from here over cbAPolicy.
    std::vector<u8> cbBRaw;
    std::array<u8, 0x20> cbAHeader{};
    FuseDerive::NandPolicy cbAPolicy; // the policy CB_A alone gives, plus the CF slots

    // Set by Unlock().
    bool cpuKeyGiven = false;
    KeyVault keyvault; // decrypted with the key. Its state says whether the key is the right one

    // The console's fuses, when the NAND and the key are enough to build them.
    bool fusesOk = false;
    FuseDerive::FuseLines fuses{};

    // Reads nandPath and extracts everything the NAND holds without a key. True if it opened, `error` says why not.
    bool Load(const std::string &nandPath);

    // Decrypts the keyvault with cpuKey and derives the fuses (update LDV from CF slot cfPatchSlot, -1 = highest).
    // Returns whether the keyvault decrypted (the key belongs to this NAND).
    bool Unlock(const std::array<u8, KeyVault::CpuKeySize> &cpuKey, s32 cfPatchSlot = -1);

    // Reads the CPU key from the key file, unlocks with it and logs the outcome. For the console being booted.
    bool UnlockFromKeyFile(const std::string &cpuKeyPath, s32 cfPatchSlot = -1);

    // The fuses for cfPatchSlot with cpuKey, without changing this object (the wizard previews slot choices).
    FuseDerive::FuseLines FusesFor(const std::array<u8, KeyVault::CpuKeySize> &cpuKey, s32 cfPatchSlot) const {
      return FuseDerive::BuildFuses(policy, cpuKey, cfPatchSlot);
    }

    // eFuses: a guest reboot does not change them, only the hypervisor blowing bits does. This takes the
    // update LDV lines (7-11) from 'sensed' (the fuse lines as the previous boot left them) instead of from the NAND,
    // which a system update has just rewritten: its new CF (header flag 0x4000) expects the fuses one LDV below it on
    // the reboot, so CD reports "burn pending" and the update's second phase blows them. Returns whether that changed
    // the lines derived from the NAND.
    bool KeepSensedFuses(const FuseDerive::FuseLines &sensed);

    // The 12 fuse lines for the CPU, null when they could not be built.
    const FuseDerive::FuseLines *ConsoleFuses() const { return fusesOk ? &fuses : nullptr; }
  };

  // Load() plus, with a key, Unlock().
  Result Analyze(const std::string &nandPath, const std::array<u8, KeyVault::CpuKeySize> *cpuKey);

  // The NAND of the console being booted. The devices that need keys read its `keyvault`.
  Result &Console();

} // namespace Xe::NandInfo
