/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <cstring>

// How a NAND lays out its first bytes, for everything that reads them: the SFCX (which logs them), the 1BL simulation
// (which checks CB_A's header) and NandInfo (which reads the whole image). Both structs are big-endian on flash.

namespace Xe::NandLayout {

  // The flash header, the first 0x80 bytes.
  struct Header {
    u16 nandMagic;
    u16 build;
    u16 qfe;
    u16 flags;
    u32 entry; // CB_A offset
    u32 size;  // end of the bootloader area
    char copyright[64];
    u8 reserved[16];
    u32 keyvaultSize;
    u32 sysUpdateAddr; // patch slot offset
    u16 sysUpdateCount;
    u16 keyvaultVer;
    u32 keyvaultAddr;
    u32 sysUpdateSize; // patch slot size, 0 = 64 KiB
    u32 smcConfigAddr;
    u32 smcBootSize;
    u32 smcBootAddr;
  };
  static_assert(sizeof(Header) == 0x80, "the flash header is 0x80 bytes");

  // The header every bootloader starts with. The low nibble of the second name byte is the stage (2-7).
  struct BootloaderHeader {
    u8 name[2];
    u16 buildNumber;
    u16 pairingData;
    u16 flags;
    u32 entryPoint;
    u32 length;
  };
  static_assert(sizeof(BootloaderHeader) == 0x10, "a bootloader header is 0x10 bytes");

  // The flash header out of the first 0x80 bytes of an image, with the integers in host order.
  inline Header ReadHeader(const u8 *bytes) {
    Header h;
    std::memcpy(&h, bytes, sizeof(h));
    h.nandMagic = byteswap_be(h.nandMagic);
    h.build = byteswap_be(h.build);
    h.qfe = byteswap_be(h.qfe);
    h.flags = byteswap_be(h.flags);
    h.entry = byteswap_be(h.entry);
    h.size = byteswap_be(h.size);
    h.keyvaultSize = byteswap_be(h.keyvaultSize);
    h.sysUpdateAddr = byteswap_be(h.sysUpdateAddr);
    h.sysUpdateCount = byteswap_be(h.sysUpdateCount);
    h.keyvaultVer = byteswap_be(h.keyvaultVer);
    h.keyvaultAddr = byteswap_be(h.keyvaultAddr);
    h.sysUpdateSize = byteswap_be(h.sysUpdateSize);
    h.smcConfigAddr = byteswap_be(h.smcConfigAddr);
    h.smcBootSize = byteswap_be(h.smcBootSize);
    h.smcBootAddr = byteswap_be(h.smcBootAddr);
    return h;
  }

  // A bootloader header out of the 0x10 bytes at 'bytes', with the integers in host order.
  inline BootloaderHeader ReadBootloaderHeader(const u8 *bytes) {
    BootloaderHeader h;
    std::memcpy(&h, bytes, sizeof(h));
    h.buildNumber = byteswap_be(h.buildNumber);
    h.pairingData = byteswap_be(h.pairingData);
    h.flags = byteswap_be(h.flags);
    h.entryPoint = byteswap_be(h.entryPoint);
    h.length = byteswap_be(h.length);
    return h;
  }

} // namespace Xe::NandLayout
