/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "NandImage.h"

#include "Base/Logging/Log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace Xe::NandImage {

  constexpr u32 kPageSize = 0x200;
  constexpr u32 kSparePageSize = 0x210;
  constexpr u32 kBigEccPageSize = 0x840;

  static bool IsAll(const u8 *data, size_t size, u8 value) {
    return std::all_of(data, data + size, [value](u8 byte) { return byte == value; });
  }

  static bool HasStandardSpare(const std::vector<u8> &raw) {
    return raw.size() >= 0x420 && (raw[0x205] == 0xFF || raw[0x415] == 0xFF || raw[0x200] == 0xFF);
  }

  static bool HasBigEccSpare(const std::vector<u8> &raw) {
    return raw.size() >= kBigEccPageSize && raw.size() % kBigEccPageSize == 0 && raw[0x800] == 0xFF
           && raw[0x810] == 0xFF && raw[0x820] == 0xFF;
  }

  // J-Runner applies raw-NAND remaps before stripping spare bytes. The replacement table is in the reserved tail:
  // 0x3E0 on small images, 0xF80 on 64 MiB small-block images, and 0x1E0 on big-block images.
  static void RemapBadBlocks(std::vector<u8> &raw) {
    if (raw.size() < 0x4200 || !HasStandardSpare(raw)) { return; }
    const bool bigBlock = raw.size() >= 0x4200000 && raw[0x205] != 0xFF;
    const u32 blockSize = bigBlock ? 0x21000 : 0x4200;
    if (raw.size() % blockSize != 0) { return; }

    const u32 blockCount = static_cast<u32>(raw.size() / blockSize);
    const u32 reservedOffset = bigBlock ? 0x1E0 : (raw.size() >= 0x4200000 ? 0xF80 : 0x3E0);
    const u32 markerOffset = bigBlock ? 0x200 : 0x205;
    const u32 idOffset = bigBlock ? 0x201 : 0x200;
    if (reservedOffset + 0x20 > blockCount) { return; }

    std::vector<u32> badBlocks;
    for (u32 block = 0; block < blockCount; ++block) {
      const u32 blockStart = block * blockSize;
      bool bad = false;
      bool firstPage = true;
      for (u32 page = 0; page + kSparePageSize <= blockSize; page += kSparePageSize) {
        const u8 *spare = raw.data() + blockStart + page + kPageSize;
        if (IsAll(spare, 0x10, 0x00) || ((!bigBlock || firstPage) && spare[markerOffset - kPageSize] != 0xFF)) {
          bad = true;
          break;
        }
        firstPage = false;
      }
      if (bad) { badBlocks.push_back(block); }
      if (badBlocks.size() >= 0x20) {
        LOG_WARNING(System, "NAND: at least 32 bad blocks found; skipping remapping.");
        return;
      }
    }
    if (badBlocks.empty()) { return; }

    auto hasBadBlockId = [&](u32 at, u32 block) {
      if (at + 4 > raw.size()) { return false; }
      // J-Runner reverses the four metadata bytes before comparing the hex block ID.
      return raw[at] == static_cast<u8>(block) && raw[at + 1] == static_cast<u8>(block >> 8)
             && raw[at + 2] == static_cast<u8>(block >> 16) && raw[at + 3] == static_cast<u8>(block >> 24);
    };
    const u32 reservedStart = reservedOffset * blockSize;
    std::vector<bool> replacementUsed(0x20, false);
    for (const u32 bad : badBlocks) {
      u32 replacement = blockCount;
      for (s32 slot = 0x1F; slot > 0; --slot) {
        const u32 slotStart = reservedStart + static_cast<u32>(slot) * blockSize;
        const u32 idA = slotStart + blockSize - (0x210 - idOffset);
        const u32 idB = slotStart + idOffset;
        if (replacementUsed[slot] || (!hasBadBlockId(idA, bad) && !hasBadBlockId(idB, bad))) { continue; }
        replacement = reservedOffset + static_cast<u32>(slot) + 1;
        replacementUsed[slot] = true;
        break;
      }
      if (replacement >= blockCount || replacement == bad) {
        LOG_WARNING(System, "NAND: no replacement found for bad block {:#x}.", bad);
        continue;
      }
      std::copy_n(raw.begin() + static_cast<size_t>(replacement) * blockSize, blockSize,
                  raw.begin() + static_cast<size_t>(bad) * blockSize);
      std::fill_n(raw.begin() + static_cast<size_t>(replacement) * blockSize, blockSize, 0xFF);
    }
  }

  static std::vector<u8> StripSpare(const std::vector<u8> &raw, u32 pageWithSpare, u32 pageData) {
    const size_t pages = raw.size() / pageWithSpare;
    std::vector<u8> logical(pages * pageData);
    for (size_t page = 0; page < pages; ++page) {
      std::copy_n(raw.begin() + page * pageWithSpare, pageData, logical.begin() + page * pageData);
    }
    return logical;
  }

  bool ReadLogicalImage(const std::string &path, Image &image) {
    image = {};
    std::ifstream file(path, std::ios::binary);
    std::error_code ec;
    const u64 fileSize = std::filesystem::file_size(path, ec);
    if (!file.is_open() || ec || fileSize > std::numeric_limits<size_t>::max()
        || fileSize > static_cast<u64>(std::numeric_limits<std::streamsize>::max())) {
      return false;
    }

    std::vector<u8> raw(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char *>(raw.data()), static_cast<std::streamsize>(raw.size()))) { return false; }

    // J-Runner determines this before ECC removal, then uses it to walk update modules.
    image.bigBlock = raw.size() >= 0x4200000 && raw.size() <= 0x21000000 && raw[0x205] != 0xFF;
    const bool standardEcc = raw.size() % kSparePageSize == 0 && HasStandardSpare(raw);
    const bool bigEcc = !standardEcc && HasBigEccSpare(raw);
    if (standardEcc) { RemapBadBlocks(raw); }
    image.hasSpare = standardEcc || bigEcc;
    if (standardEcc) {
      image.data = StripSpare(raw, kSparePageSize, kPageSize);
    } else if (bigEcc) {
      image.data = StripSpare(raw, kBigEccPageSize, 0x800);
    } else {
      image.data = std::move(raw);
    }
    return !image.data.empty();
  }

} // namespace Xe::NandImage
