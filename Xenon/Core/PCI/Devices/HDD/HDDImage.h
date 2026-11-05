/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

// Creation of a blank retail hard drive image around a genuine security sector.

#pragma once

#include "Base/Types.h"

#include <array>
#include <string>

namespace Xe::HDDImage {

  // Where the kernel reads the security sector, and how much it reads (the 0x200 sector, then the 0xC00 byte logo).
  constexpr u64 kSecuritySectorOffset = 0x2000;
  constexpr u32 kSecuritySectorSize = 0x200;
  constexpr u32 kSecurityRegionSize = 0xE00;

  // A security sector as the kernel reads it to authenticate the drive.
  struct SecuritySector {
    std::array<u8, kSecurityRegionSize> data{}; // The sector and the logo behind it
    std::string serial, firmware, model;        // The drive's identity, trimmed
    u32 sectors = 0;                            // The sector count the signature covers
    bool hasLogo = false;                       // True if the logo bytes were given
    bool logoOk = false;                        // True if the logo hashes to the digest in the sector
    // Why a drive built from this sector would be rejected by a retail kernel, empty if nothing is wrong.
    std::string problem;
  };

  // Reads a security sector from path: a dump of the sector (0x200 bytes, or 0xE00 with the logo), or an image of a
  // drive, which holds it at 0x2000. Returns false with error set if there is none.
  bool ReadSecuritySector(const std::string &path, SecuritySector &out, std::string &error);

  // Creates a sparse image of the sector count signed into the security sector: the sector at 0x2000 and every retail
  // partition formatted as an empty FATX volume. Refuses to overwrite a file.
  bool CreateRetailImage(const SecuritySector &security, const std::string &path, std::string &error);

} // namespace Xe::HDDImage
