/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <ctime>
#include <vector>
#include <utility>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>

#include <Base/EndianReader.h>

// Namespace xboxkrnl: Guest kernel related utilities for dumping, hooking and patching of specific exported functions.
// TODO: move into systemhooks ns?

namespace xboxkrnl {
  
  // xboxkrnl image base
  constexpr u64 XBOXKRNL_IMAGE_BASE = 0x40000;
  // xboxkrnl image header size
  constexpr u64 XBOXKRNL_HEADER_SIZE = 256 * 1024;

  // PE constants
  namespace PE {
    // MZ
    constexpr size_t MZ_LFANEW = 0x3C;
    // COFF  (base = PE sig + 4)
    constexpr size_t COFF_MACHINE = 0x00;
    constexpr size_t COFF_NUMSEC = 0x02;
    constexpr size_t COFF_TIMESTAMP = 0x04;
    constexpr size_t COFF_OPTHDR = 0x10;
    constexpr size_t COFF_SIZE = 0x14;
    // Optional PE32  (base = COFF base + COFF_SIZE)
    constexpr size_t OPT32_MAGIC = 0x00;
    constexpr size_t OPT32_ENTRY = 0x10;
    constexpr size_t OPT32_BASE = 0x1C;
    constexpr size_t OPT32_IMGSIZE = 0x38;
    constexpr size_t OPT32_SUBSYS = 0x44;
    constexpr size_t OPT32_NUMDIRS = 0x5C;
    constexpr size_t OPT32_XRVA = 0x60;  // DataDir[0] export RVA
    constexpr size_t OPT32_XSZ = 0x64;  // DataDir[0] export size
    // Optional PE32+
    constexpr size_t OPT64_ENTRY = 0x10;
    constexpr size_t OPT64_BASE = 0x18;
    constexpr size_t OPT64_IMGSIZE = 0x38;
    constexpr size_t OPT64_SUBSYS = 0x44;
    constexpr size_t OPT64_NUMDIRS = 0x6C;
    constexpr size_t OPT64_XRVA = 0x70;
    constexpr size_t OPT64_XSZ = 0x74;
    // Section header  (0x28 bytes each)
    constexpr size_t SEC_NAME = 0x00;
    constexpr size_t SEC_VIRTSIZE = 0x08;
    constexpr size_t SEC_VIRTADDR = 0x0C;
    constexpr size_t SEC_RAWSIZE = 0x10;
    constexpr size_t SEC_RAWOFF = 0x14;
    constexpr size_t SEC_CHARS = 0x24;
    constexpr size_t SEC_HDRSZ = 0x28;
    // Export directory (relative to export dir base)
    constexpr size_t EXP_BASE = 0x10;  // ordinal base
    constexpr size_t EXP_NUMFUNCS = 0x14;
    constexpr size_t EXP_NUMNAMES = 0x18;  // asserted 0 for xboxkrnl
    constexpr size_t EXP_EAT_RVA = 0x1C;
    // PE magic
    constexpr u16 OPT_PE32 = 0x010Bu;
    constexpr u16 OPT_PE32P = 0x020Bu;
    // Machine IDs
    constexpr u16 MACHINE_PPC = 0x01F0u;
    constexpr u16 MACHINE_PPCBE = 0x01F2u;  // Xbox 360
    constexpr u16 MACHINE_PPC_FP = 0x01F1u;
    constexpr u16 MACHINE_X86 = 0x014Cu;
    constexpr u16 MACHINE_X64 = 0x8664u;
    constexpr u16 MACHINE_IA64 = 0x0200u;
    constexpr u16 MACHINE_ARM = 0x01C4u;
    constexpr u16 MACHINE_ARM64 = 0xAA64u;
  } // namespace pe

  // Section Info Header
  struct SectionInfo {
    char name[9]{};
    u32 virtualAddress{};
    u32 virtualSize{};
    u32 rawOffset{};
    u32 rawSize{};
    u32 characteristics{};
  };

  // Program Executable Parser used for analyzing xboxkrnl PE image data inside the guest.
  // Extracts and stores the export symbols as ordinals -> relative memory address for PE base
  // NOTE: xboxkrnl image base (physical) is always 0x40000 offseting from RAM base
  class PEImageParser {
  public:
    bool isBigEndian{ false };
    bool is64bit{ false };
    u16 machine{};
    u16 subsystem{};
    u32 ordinalBase{};
    u32 timeDateStamp{};
    u32 entryPoint{};
    u32 sizeOfImage{};
    uint64_t imageBase{};

    std::vector<SectionInfo> sections;

    // Unordered map conatining ordinal -> relative offset map.
    std::unordered_map<u32, u32> ordinalMap;

    // Class constructor
    PEImageParser() {}

    void ParseImageData(const u8 *bufPtr, size_t size) {
      if (!bufPtr || size < 0x80)
        throw std::runtime_error("PEMemoryParser: null or undersized buffer");
      parseImage(bufPtr, size);
    }

    //
    // Ordinal lookup
    //

    // Returns true if ordinal has a non-zero EAT entry.
    bool hasOrdinal(u32 ordinal) const noexcept {
      return ordinalMap.count(ordinal) > 0;
    }

    // Performs a lookup of the Relative VA for the specified ordinal
    // Throws std::out_of_range when the ordinal is absent/not found
    u32 getRelVAByOrdinal(u32 ordinal) const {
      auto it = ordinalMap.find(ordinal);
      if (it == ordinalMap.end()) {
        std::ostringstream o;
        o << "getRVAByOrdinal: ordinal " << ordinal << " not present in export table";
        throw std::out_of_range(o.str());
      }
      return it->second;
    }

    // Performs a lookup of the absolute virtual address (imageBase + RVA) for the specified ordinal.
    // Throws std::out_of_range when the ordinal is absent/not found
    uint64_t getAbsVAByOrdinal(u32 ordinal) const {
      return imageBase + getRelVAByOrdinal(ordinal);
    }

  private:
    //  RVA -> buffer file offset via section table
    size_t rva2off(u32 rva) const {
      for (const auto &s : sections) {
        if (rva >= s.virtualAddress &&
          rva < s.virtualAddress + s.virtualSize)
          return s.rawOffset + (rva - s.virtualAddress);
      }
      std::ostringstream o;
      o << "RVA 0x" << std::hex << rva << " not in any section";
      throw std::runtime_error(o.str());
    }

    // 
    //  Endianness probe:
    //  'MZ' bytes (0x4D, 0x5A) are ASCII — identical in both endiannesses.
    //  We read e_lfanew at 0x3C both as LE and BE, then check which
    //  interpretation resolves to a raw 'P','E',0,0 byte sequence.
    //
    static bool detectBigEndian(const u8 *buf, size_t size) {
      if (buf[0] != 0x4D || buf[1] != 0x5A)
        throw std::runtime_error("Not an MZ image — bad magic bytes");

      u32 raw32;
      std::memcpy(&raw32, buf + 0x3C, 4);
      u32 leOff = raw32;
      u32 beOff = utils::bswap32(raw32);

      auto isPE = [&](u32 off) -> bool {
        return off + 4 <= size &&
          buf[off + 0] == 0x50 && buf[off + 1] == 0x45 &&
          buf[off + 2] == 0x00 && buf[off + 3] == 0x00;
        };

      if (isPE(leOff)) return false;
      if (isPE(beOff)) return true;
      throw std::runtime_error(
        "Cannot detect endianness: no PE signature found at "
        "either LE or BE interpretation of e_lfanew");
    }

    //
    //  Main parse of the input data
    //
    void parseImage(const u8 *buf, size_t size) {
      isBigEndian = detectBigEndian(buf, size);
      utils::EndianReader reader(buf, size, isBigEndian);

      // MZ -> PE signature offset
      size_t peBase = static_cast<size_t>(reader.i32(PE::MZ_LFANEW));
      // PE sig is raw ASCII — compare bytes without endian swap
      if (reader.u8(peBase + 0) != 0x50 || reader.u8(peBase + 1) != 0x45 ||
        reader.u8(peBase + 2) != 0x00 || reader.u8(peBase + 3) != 0x00)
        throw std::runtime_error("Bad PE signature at e_lfanew offset");

      // COFF header
      size_t   coffBase = peBase + 4;
      machine = reader.u16(coffBase + PE::COFF_MACHINE);
      u16 numSec = reader.u16(coffBase + PE::COFF_NUMSEC);
      timeDateStamp = reader.u32(coffBase + PE::COFF_TIMESTAMP);
      u16 optHdrSz = reader.u16(coffBase + PE::COFF_OPTHDR);

      // Optional header
      size_t   optBase = coffBase + PE::COFF_SIZE;
      u16 optMagic = reader.u16(optBase + PE::OPT32_MAGIC);
      is64bit = (optMagic == PE::OPT_PE32P);

      u32 exportRVA = 0, exportSz = 0;
      if (!is64bit) {
        imageBase = reader.u32(optBase + PE::OPT32_BASE);
        entryPoint = reader.u32(optBase + PE::OPT32_ENTRY);
        sizeOfImage = reader.u32(optBase + PE::OPT32_IMGSIZE);
        subsystem = reader.u16(optBase + PE::OPT32_SUBSYS);
        if (reader.u32(optBase + PE::OPT32_NUMDIRS) > 0) {
          exportRVA = reader.u32(optBase + PE::OPT32_XRVA);
          exportSz = reader.u32(optBase + PE::OPT32_XSZ);
        }
      } else {
        imageBase = reader.u64(optBase + PE::OPT64_BASE);
        entryPoint = reader.u32(optBase + PE::OPT64_ENTRY);
        sizeOfImage = reader.u32(optBase + PE::OPT64_IMGSIZE);
        subsystem = reader.u16(optBase + PE::OPT64_SUBSYS);
        if (reader.u32(optBase + PE::OPT64_NUMDIRS) > 0) {
          exportRVA = reader.u32(optBase + PE::OPT64_XRVA);
          exportSz = reader.u32(optBase + PE::OPT64_XSZ);
        }
      }

      // Section table
      size_t secBase = optBase + optHdrSz;
      sections.reserve(numSec);
      for (u16 i = 0; i < numSec; ++i) {
        size_t sh = secBase + i * PE::SEC_HDRSZ;
        SectionInfo si;
        std::memcpy(si.name, buf + sh + PE::SEC_NAME, 8);
        si.name[8] = '\0';
        si.virtualSize = reader.u32(sh + PE::SEC_VIRTSIZE);
        si.virtualAddress = reader.u32(sh + PE::SEC_VIRTADDR);
        si.rawSize = reader.u32(sh + PE::SEC_RAWSIZE);
        si.rawOffset = reader.u32(sh + PE::SEC_RAWOFF);
        si.characteristics = reader.u32(sh + PE::SEC_CHARS);
        sections.push_back(si);
      }

      // Export directory
      if (exportRVA != 0 && exportSz != 0)
        parseExports(reader, exportRVA);
    }

    //  Parse Export table
    //  In xboxkrnl.exe, name and related tables are stripped, we walk only the EAT:
    void parseExports(const utils::EndianReader &reader, u32 exportRVA) {
      size_t expBase = rva2off(exportRVA);

      ordinalBase = reader.u32(expBase + PE::EXP_BASE);
      u32 numFuncs = reader.u32(expBase + PE::EXP_NUMFUNCS);
      // NumberOfNames is expected to be 0 — we do not use the NPT
      // regardless, so we read but intentionally ignore it.
      u32 numNames = reader.u32(expBase + PE::EXP_NUMNAMES);
      (void)numNames;

      size_t eatOff = rva2off(reader.u32(expBase + PE::EXP_EAT_RVA));

      ordinalMap.reserve(numFuncs);

      for (u32 slot = 0; slot < numFuncs; ++slot) {
        u32 funcRVA = reader.u32(eatOff + slot * 4);
        if (funcRVA == 0) continue;  // unused / gap slot

        u32 ordinal = ordinalBase + slot;
        ordinalMap.emplace(ordinal, funcRVA);
      }
    }
  };

}