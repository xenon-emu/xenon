/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "NandInfo.h"

#include "Base/Crypto/Rc4.h"
#include "Base/Crypto/Sha1.h"
#include "Base/Logging/Log.h"
#include "Core/KeyVault/BootCrypto.h"
#include "Core/KeyVault/NandImage.h"
#include "Core/KeyVault/NandLayout.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <limits>
#include <span>

// Note: Most of this file is a derivation of what JRunner does, all and any credit of the code here and the research
// of the NAND layouts and xebuild behavior goes to it. Octal450 is a very talented individual and we at the Xenon
// project are very grateful for his cocntributions to the Xbox 360 community.

namespace Xe::NandInfo {

  constexpr u32 kMaxBootloaders = 30;
  constexpr u32 kSmcVersionOffset = 0x100;

  constexpr u32 kCbHeaderSize = 0x20;
  constexpr u32 kCbPolicyOffset = 0x3B0;
  constexpr u32 kCbMaxSize = 0x10000;
  constexpr u32 kCfNonceOffset = 0x20;
  constexpr u32 kCfLdvOffset = 0x21F;
  constexpr u32 kCfMaxSize = 0x200000;
  constexpr u32 kUpdateFuseLineCount = FuseDerive::FuseLineCount - 7;

  // What a decrypted CB_B holds at its entry point: lis r3, 0x100.
  constexpr u32 kCbBEntryStub = 0x3C600100;

  static bool HasBytes(const std::vector<u8> &image, u64 at, u64 count) {
    return at <= image.size() && count <= image.size() - static_cast<size_t>(at);
  }

  static bool ReadAt(const std::vector<u8> &image, u64 at, void *dst, u64 count) {
    if (!HasBytes(image, at, count)) { return false; }
    std::memcpy(dst, image.data() + static_cast<size_t>(at), static_cast<size_t>(count));
    return true;
  }

  // The SMC firmware is stored with a rolling XOR: each plaintext byte feeds the key bytes used for later ones.
  static void DecryptSmc(std::vector<u8> &data) {
    u32 key[4] = {0x42, 0x75, 0x4E, 0x79};
    for (size_t i = 0; i < data.size(); ++i) {
      const u8 plain = static_cast<u8>(data[i] ^ (key[i & 3] & 0xFF));
      const u32 mod = static_cast<u32>(plain) * 0xFB;
      data[i] = plain;
      key[(i + 1) & 3] += mod;
      key[(i + 2) & 3] += mod >> 8;
    }
  }

  static bool IsBootloaderName(const u8 *name) {
    // The stage number is the low nibble of the second byte: B/C/D/E/F/G encode stages 2-7,
    // while early DD1 developer NANDs use the literal '2'. J-Runner accepts non-C first bytes.
    const u8 stage = name[1] & 0x0F;
    return stage >= 2 && stage <= 7;
  }

  static bool IsInvalidBootloaderNameByte(u8 byte) {
    return byte < 0x20 || byte > 0x7E || byte == '<' || byte == '>' || byte == ':' || byte == '"' || byte == '/'
           || byte == '\\' || byte == '|' || byte == '?' || byte == '*';
  }

  static bool IsEarlyS2Tag(u8 first, u8 second) {
    // J-Runner replaces an invalid first filename character with 'S' before testing for the DD1 "S2" bootloader.
    return second == '2' && (first == 'S' || IsInvalidBootloaderNameByte(first));
  }

  // The console type policy and the CB LDV a decrypted CB carries at 0x3B0 and 0x3B1, and the console they stand for.
  static void TakePolicyBytes(std::span<const u8> cb, FuseDerive::NandPolicy &policy) {
    policy.cbBuild = NandLayout::ReadBootloaderHeader(cb.data()).buildNumber;
    policy.consoleTypePolicy = cb[kCbPolicyOffset];
    policy.cbLdv = cb[kCbPolicyOffset + 1];
    policy.nandTarget = FuseDerive::NandTargetFromPolicy(policy.consoleTypePolicy);
  }

  // CB_A: decrypted with the 1BL key, it carries the console-type policy and the CB LDV the fuse check enforces.
  static bool DecodeFusePolicy(const std::vector<u8> &image, u32 cbOffset, FuseDerive::NandPolicy &policy) {
    if (!HasBytes(image, cbOffset, kCbHeaderSize)) { return false; }
    const NandLayout::BootloaderHeader cbHeader = NandLayout::ReadBootloaderHeader(image.data() + cbOffset);
    // J-Runner identifies 2BLs from the low nibble of the second tag byte. The first byte
    // distinguishes retail ('C') from developer bootloaders and may be a legacy garbage byte.
    if ((cbHeader.name[1] & 0x0F) != 2) { return false; }
    const bool earlyDevkit = IsEarlyS2Tag(cbHeader.name[0], cbHeader.name[1]);

    const u32 size = cbHeader.length;
    if (size <= kCbPolicyOffset + 4 || size > kCbMaxSize || !HasBytes(image, cbOffset, size)) { return false; }

    std::vector<u8> cb(image.begin() + cbOffset, image.begin() + cbOffset + size);
    // J-Runner uses the fixed 1BL key for CB/SB. Early S2 developer bootloaders use an all-zero 1BL key.
    BootCrypto::DecryptBootloader(earlyDevkit ? BootCrypto::kZeroKey : BootCrypto::k1blKey, cb, 0x10);

    FuseDerive::NandPolicy decoded;
    TakePolicyBytes(cb, decoded);
    if (decoded.cbBuild == 0 || decoded.cbLdv > 16) { return false; }
    decoded.layoutKnown = true;
    decoded.developerBootloader = cbHeader.name[0] != 'C';
    policy = decoded;
    return true;
  }

  // A decrypted CB_B has the stub at its entry point and an LDV that can be real.
  static bool IsCbB(std::span<const u8> cb) {
    if (cb.size() <= kCbPolicyOffset + 4) { return false; }
    const u32 entry = NandLayout::ReadBootloaderHeader(cb.data()).entryPoint;
    return entry <= cb.size() - 4 && LoadBe<u32>(cb.data() + entry) == kCbBEntryStub && cb[kCbPolicyOffset + 1] <= 16;
  }

  // CB_B of a split CB: key = HMAC(CB_A's key, CB_B nonce + CPU key [+ CB_A's first 16 header bytes with the flags
  // cleared]). CB_A header flag 0x1000 says whether the header is part of it (6754: flags 0x1800, yes, 13121: 0x0800,
  // no). A result counts when CB_B's entry point holds the stub every CB_B starts with. On success the policy bytes at
  // 0x3B0/0x3B1 replace the ones CB_A gave.
  static bool DecodeCbBPolicy(const std::vector<u8> &cbBRaw, const std::array<u8, 0x20> &cbAHeader,
                              const std::array<u8, KeyVault::CpuKeySize> &cpuKey, FuseDerive::NandPolicy &policy) {
    // RGH3 images chain CB_A -> a tiny stub CB (e.g. 15432, 0x400 bytes) -> the CB that really runs (e.g.
    // 13121). CB_A decrypts only the stub (key = HMAC(CB_A key, stub nonce + 16 zero bytes), no CPU key). The stub
    // copies the next stage raw from flash and jumps to it, so the real CB is stored in plaintext and no key derivation
    // can decrypt it. Its policy bytes (console type, CB LDV at 0x3B0) are what the HV checks the fuse lines against,
    // so read them as is. An encrypted CB_B never has the stub at its entry point, so this cannot match a normal split
    // CB.
    if (IsCbB(cbBRaw)) {
      TakePolicyBytes(cbBRaw, policy);
      LOG_INFO(System, "NAND: fuse policy read from the plaintext CB (console type policy {:#04x}, CB LDV {}).",
               policy.consoleTypePolicy, policy.cbLdv);
      return true;
    }
    const Base::Crypto::Sha1::Digest keyA = BootCrypto::DeriveKey(BootCrypto::k1blKey, {cbAHeader.data() + 0x10, 0x10});
    std::array<u8, 16> headerPart;
    std::copy_n(cbAHeader.begin(), 16, headerPart.begin());
    headerPart[6] = headerPart[7] = 0;
    const bool headerFirst = (NandLayout::ReadBootloaderHeader(cbAHeader.data()).flags & 0x1000) != 0;
    for (u32 attempt = 0; attempt < 2; ++attempt) {
      const bool withHeader = headerFirst == (attempt == 0);
      const std::span<const u8> nonce(cbBRaw.data() + 0x10, 0x10);
      const std::span<const u8> key(cpuKey.data(), cpuKey.size());
      const Base::Crypto::Sha1::Digest keyB = withHeader
                                                ? Base::Crypto::HmacSha1({keyA.data(), 0x10}, {nonce, key, headerPart})
                                                : Base::Crypto::HmacSha1({keyA.data(), 0x10}, {nonce, key});
      std::vector<u8> cb = cbBRaw;
      Base::Crypto::Rc4 rc4({keyB.data(), 0x10});
      rc4.Process({cb.data() + kCbHeaderSize, cb.size() - kCbHeaderSize});
      if (!IsCbB(cb)) { continue; }
      TakePolicyBytes(cb, policy);
      LOG_INFO(System, "NAND: fuse policy read from CB_B (console type policy {:#04x}, CB LDV {}).",
               policy.consoleTypePolicy, policy.cbLdv);
      return true;
    }
    LOG_WARNING(System, "NAND: CB_B did not decrypt with this CPU key; using CB_A's fuse policy.");
    return false;
  }

  // The CF patch slots: each CF decrypts with the 1BL key and states its update LDV at plaintext 0x21F.
  static void ReadCfUpdateLdv(const std::vector<u8> &image, bool bigBlock, FuseDerive::NandPolicy &policy) {
    // Match J-Runner's unpack_update walk. Slot 1 is reached through the update's CG/CF
    // transitions. Scanning every 16 bytes mistakes unrelated module data for a patch.
    const u32 nandBlockSize = bigBlock ? 0x20000 : 0x4000;
    u64 offset = 0;
    u64 patchOffset = 0;
    u32 patch = 0;

    auto ByteAt = [&image](u64 at) { return image[static_cast<size_t>(at)]; };

    for (u32 step = 0; step < 10; ++step) {
      if (!HasBytes(image, offset, 0x10)) { break; }
      const NandLayout::BootloaderHeader header =
        NandLayout::ReadBootloaderHeader(image.data() + static_cast<size_t>(offset));
      const u64 alignedSize64 = (static_cast<u64>(header.length) + 0xF) & ~u64(0xF);
      if (alignedSize64 > std::numeric_limits<u32>::max()) { break; }
      u32 blockSize = static_cast<u32>(alignedSize64);
      const u8 id = header.name[1] & 0x0F;
      if (blockSize == 0 || blockSize > image.size()) { break; }

      if (id == 6) { // CF
        patchOffset = offset;
        const u32 slot = std::min<u32>(patch, static_cast<u32>(policy.cfFound.size()));
        if (slot < policy.cfFound.size() && blockSize >= 0x220 && blockSize <= kCfMaxSize
            && HasBytes(image, offset, blockSize)) {
          const auto cfStart = image.begin() + static_cast<std::ptrdiff_t>(offset);
          std::vector<u8> cf(cfStart, cfStart + blockSize);
          BootCrypto::DecryptBootloader(BootCrypto::k1blKey, cf, kCfNonceOffset);
          const u8 ldv = cf[kCfLdvOffset];
          if (ldv <= kUpdateFuseLineCount * 16) {
            policy.cfFound[slot] = true;
            policy.cfLdv[slot] = ldv;
            policy.cfBuild[slot] = header.buildNumber;
            policy.updateLdv = std::max(policy.updateLdv, ldv);
            policy.updateLdvKnown = true;
            LOG_INFO(System, "NAND: found CF patch slot {} (build {}, update LDV {}).", slot, policy.cfBuild[slot],
                     ldv);
          }
        }
      }
      if (id == 7) { // CG marks the transition to the second update patch.
        if (patch == 0) {
          patch = 1;
        } else {
          break;
        }
      }

      // J-Runner's special jumps account for patch modules which are not adjacent in the
      // logical NAND. It checks all look-ahead ranges before reading any of them.
      if (!HasBytes(image, patchOffset + nandBlockSize, 2) || !HasBytes(image, patchOffset + 0x10000, 2)
          || !HasBytes(image, offset + 0x10000, 2) || !HasBytes(image, offset + blockSize, 2)) {
        break;
      }
      const u8 patchBlockFirst = ByteAt(patchOffset + nandBlockSize);
      const u8 patchBlockSecond = ByteAt(patchOffset + nandBlockSize + 1);
      const u8 patchWindowFirst = ByteAt(patchOffset + 0x10000);
      const u8 patchWindowSecond = ByteAt(patchOffset + 0x10001);
      const u8 currentWindowFirst = ByteAt(offset + 0x10000);
      const u8 currentWindowSecond = ByteAt(offset + 0x10001);
      const u8 currentBlockFirst = ByteAt(offset + blockSize);
      const u8 currentBlockSecond = ByteAt(offset + blockSize + 1);

      if (patch == 1 && offset < 0x80000 && patchWindowSecond == 0x46 && patchWindowFirst == 0x43) {
        offset = patchOffset + 0x10000;
        continue;
      }
      if (patch == 1 && patchBlockSecond == 0x46 && patchBlockFirst == 0x43) {
        offset = patchOffset + nandBlockSize;
        continue;
      }
      if (patch == 0 && currentWindowFirst == 0x43 && currentWindowSecond == 0x46 && currentBlockFirst != 0x43
          && currentBlockSecond != 0x47) {
        offset += 0x10000;
        patch = 1;
        continue;
      }
      if (patch == 0 && offset > 0x80000 && patchOffset < 0x80000) {
        patch = 1;
        offset = 0x80000;
        continue;
      }

      if (blockSize == 0x10) {
        blockSize = 0x20000;
        patch = 1;
      }
      // J-Runner advances by the aligned module size, except for its 0x10 placeholder.
      if (blockSize == 0 || !HasBytes(image, offset, blockSize)) { break; }
      offset += blockSize;
      if (offset > image.size()) { break; }
    }
    if (policy.cfFound[0] && policy.cfFound[1] && policy.cfLdv[0] != policy.cfLdv[1]) {
      LOG_WARNING(System, "NAND: CF slots have different update LDVs ({} and {}); using the larger value.",
                  policy.cfLdv[0], policy.cfLdv[1]);
    }
  }

  bool Result::Load(const std::string &nandPath) {
    *this = Result{};

    std::error_code ec;
    if (nandPath.empty() || !std::filesystem::is_regular_file(nandPath, ec)) {
      error = nandPath.empty() ? "No file selected" : "The file does not exist";
      return false;
    }
    fileSize = std::filesystem::file_size(nandPath, ec);
    if (ec || fileSize < sizeof(NandLayout::Header)) {
      error = "The file is too small to be a NAND dump";
      return false;
    }
    NandImage::Image nand;
    if (!NandImage::ReadLogicalImage(nandPath, nand)) {
      error = "The file could not be opened or normalized as a NAND image";
      return false;
    }
    hasSpare = nand.hasSpare;

    if (nand.data.size() < sizeof(NandLayout::Header)) {
      error = "The file is too small to be a NAND dump";
      return false;
    }
    const NandLayout::Header h = NandLayout::ReadHeader(nand.data.data());
    opened = true;

    magic = h.nandMagic;
    build = h.build;
    qfe = h.qfe;
    flags = h.flags;
    entry = h.entry;
    for (const char c : h.copyright) {
      if (!c) { break; }
      copyright += std::isprint(static_cast<unsigned char>(c)) ? c : '?';
    }
    keyvaultSize = h.keyvaultSize;
    sysUpdateAddr = h.sysUpdateAddr;
    sysUpdateCount = h.sysUpdateCount;
    keyvaultVer = h.keyvaultVer;
    keyvaultAddr = h.keyvaultAddr;
    smcConfigAddr = h.smcConfigAddr;
    smcBootSize = h.smcBootSize;
    smcBootAddr = h.smcBootAddr;

    // Bootloader chain: each header holds its stage's length, and the next stage follows it.
    u32 at = entry;
    for (u32 i = 0; i < kMaxBootloaders && at != 0; ++i) {
      if (!HasBytes(nand.data, at, sizeof(NandLayout::BootloaderHeader))) { break; }
      const NandLayout::BootloaderHeader bl = NandLayout::ReadBootloaderHeader(nand.data.data() + at);
      if (!IsBootloaderName(bl.name)) { break; }
      Bootloader b;
      // Match J-Runner's old-devkit display normalization for invalid filename characters.
      b.name[0] = IsInvalidBootloaderNameByte(bl.name[0]) ? 'S' : static_cast<char>(bl.name[0]);
      b.name[1] = static_cast<char>(bl.name[1]);
      b.offset = at;
      b.build = bl.buildNumber;
      b.entry = bl.entryPoint;
      b.length = bl.length;
      bootloaders.push_back(b);
      if (b.length < sizeof(bl) || b.length > 0x1000000) { break; }
      at += (b.length + 0xF) & ~0xFu;
    }

    // SMC firmware.
    if (smcBootAddr != 0 && smcBootSize > kSmcVersionOffset + 4 && smcBootSize <= 0x10000) {
      std::vector<u8> smc(smcBootSize);
      if (ReadAt(nand.data, smcBootAddr, smc.data(), smcBootSize)) {
        DecryptSmc(smc);
        std::copy_n(smc.begin() + kSmcVersionOffset, 4, smcVersionBytes.begin());
        // Major and minor are the two bytes after a pad byte.
        const u8 major = smc[kSmcVersionOffset + 1];
        const u8 minor = smc[kSmcVersionOffset + 2];
        if (major >= 1 && major <= 9) {
          smcOk = true;
          smcVersion = fmt::format("{}.{}", major, minor);
        }
      }
    }

    // The keyvault, still encrypted.
    {
      const u32 kvSize = keyvaultSize == 0 ? KeyVault::Size : keyvaultSize;
      if (keyvaultAddr != 0 && kvSize <= KeyVault::Size && kvSize >= KeyVault::MinSize
          && HasBytes(nand.data, keyvaultAddr, kvSize)) {
        encryptedKeyVault.assign(nand.data.begin() + keyvaultAddr, nand.data.begin() + keyvaultAddr + kvSize);
      } else {
        LOG_ERROR(System, "NAND: the header has no usable keyvault (address {:#x}, size {:#x}).", keyvaultAddr, kvSize);
      }
    }

    // The fuse policy: CB_A, then the CF patch slots.
    policyOk = DecodeFusePolicy(nand.data, entry, policy);
    if (policyOk) {
      ReadCfUpdateLdv(nand.data, nand.bigBlock, policy);
      if (!policy.updateLdvKnown) {
        // A NAND without a CF patch runs its base kernel (early devkit NANDs ship SB/SC/SD/SE only): no system update
        // has blown an update fuse, so lines 7-11 are blank.
        policy.updateLdvKnown = true;
        policy.updateLdv = 0;
        LOG_WARNING(System,
                    "NAND: no CF patch was found; assuming the base kernel (update LDV 0, fuse lines 7-11 blank).");
      }
      cbAPolicy = policy;
      // More 'CB's right behind CB_A are a split CB. The last of the run is the one that runs: a normal split CB has
      // just CB_B, but a glitch image has a stub CB in between, and taking the stub's policy bytes (all zero) derives
      // the wrong fuse lines 1/2, which makes the HV panic right after INIT_SOC_MMIO.
      size_t lastCb = 0;
      while (lastCb + 1 < bootloaders.size() && (bootloaders[lastCb + 1].name[1] & 0x0F) == 2) { ++lastCb; }
      if (lastCb >= 1 && (bootloaders[0].name[1] & 0x0F) == 2) {
        const Bootloader &cbA = bootloaders[0];
        const Bootloader &cbB = bootloaders[lastCb];
        if (cbB.length > kCbPolicyOffset + 4 && cbB.length <= kCbMaxSize
            && ReadAt(nand.data, cbA.offset, cbAHeader.data(), cbAHeader.size())) {
          cbBRaw.resize(cbB.length);
          if (!ReadAt(nand.data, cbB.offset, cbBRaw.data(), cbB.length)) { cbBRaw.clear(); }
        }
      }
    } else {
      LOG_ERROR(System, "NAND: no supported, decryptable CB_A was found; the fuses cannot be derived.");
    }
    return true;
  }

  bool Result::Unlock(const std::array<u8, KeyVault::CpuKeySize> &cpuKey, s32 cfPatchSlot) {
    cpuKeyGiven = true;
    keyvault.Decrypt(encryptedKeyVault, cpuKey);
    fusesOk = false;
    if (policyOk && !cbBRaw.empty()) {
      policy = cbAPolicy;
      DecodeCbBPolicy(cbBRaw, cbAHeader, cpuKey, policy);
    }
    if (policyOk && policy.layoutKnown && policy.updateLdvKnown && keyvault.IsUsable()) {
      fuses = FusesFor(cpuKey, cfPatchSlot);
      fusesOk = true;
    }
    return keyvault.IsUsable();
  }

  bool Result::UnlockFromKeyFile(const std::string &cpuKeyPath, s32 cfPatchSlot) {
    std::array<u8, KeyVault::CpuKeySize> cpuKey{};
    if (!KeyVault::ReadCpuKeyFile(cpuKeyPath, cpuKey)) {
      LOG_WARNING(System, "NAND: no CPU key is available from '{}'.", cpuKeyPath);
      return false;
    }

    if (!Unlock(cpuKey, cfPatchSlot)) {
      LOG_WARNING(System, "NAND: the CPU key does not decode this NAND's keyvault; no fuses were built.");
      return false;
    }
    if (keyvault.GetState() == KeyVault::State::Unverified) {
      LOG_WARNING(System, "KeyVault: digest mismatch, but the keyvault decodes. Using it anyway.");
    }
    keyvault.LogSummary();

    if (!fusesOk) {
      LOG_WARNING(System, "NAND: the fuses could not be built (CB_A policy or CF update LDV unavailable).");
      return false;
    }
    bool slotFound = false;
    const u32 usedLdv = FuseDerive::UpdateLdvFor(policy, cfPatchSlot, &slotFound);
    if (policy.updateLdvKnown && usedLdv < policy.updateLdv) {
      // CD boots a CF one LDV above the fuses only when the CF has header flag 0x4000: that is a system update whose
      // fuse burn is still pending (the console rebooted between flashing it and blowing the fuses, the update finishes
      // it).
      LOG_WARNING(
        System,
        "Fuses: update LDV {} is below the NAND's highest CF LDV {}: boots only as a system update with its fuse "
        "burn pending. Otherwise set the CF slot to automatic (the setup wizard's NAND page).",
        usedLdv, policy.updateLdv);
    }
    LOG_INFO(System,
             "Fuses: built from the NAND (CB {}, console type policy {:#04x}, CB LDV {}, update LDV {} from {}).",
             policy.cbBuild, policy.consoleTypePolicy, policy.cbLdv, usedLdv,
             slotFound ? fmt::format("CF slot {}", cfPatchSlot) : std::string("the highest CF slot"));
    return true;
  }

  bool Result::KeepSensedFuses(const FuseDerive::FuseLines &sensed) {
    if (!fusesOk) { return false; }
    bool changed = false;
    for (u32 line = 7; line < FuseDerive::FuseLineCount; ++line) {
      changed |= sensed[line] != fuses[line];
      fuses[line] = sensed[line];
    }
    if (changed) {
      LOG_INFO(System, "Fuses: kept the console's update fuses across the reboot (line 07 {:016X}, line 08 {:016X}).",
               fuses[7], fuses[8]);
    }
    return changed;
  }

  Result Analyze(const std::string &nandPath, const std::array<u8, KeyVault::CpuKeySize> *cpuKey) {
    Result r;
    if (r.Load(nandPath) && cpuKey) { r.Unlock(*cpuKey); }
    return r;
  }

  Result &Console() {
    static Result console;
    return console;
  }

} // namespace Xe::NandInfo
