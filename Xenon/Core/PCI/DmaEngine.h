/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"
#include "Core/RAM/RAM.h"

#include <cstring>

// A bus master's access to guest RAM, shared by every device that moves data on its own (HDD, ODD, Flash, Ethernet,
// Audio, USB, XMA). Every access is range checked against the RAM that exists.
//
// The device changes guest memory behind the CPU's back, so accesses also have to play along with the GPU.
// RAM::FetchGpuPages goes before a read, and before a write for the pages it only partly covers: what the GPU
// wrote last is the newest in them, so it comes into RAM first. RAM::DeviceWrites goes before the bytes land, so pages
// the GPU held newer contents of stop being so. Without it the CPU's next access would fetch the GPU's older copy over
// the device's data. RAM::NotifyWrite goes after, so the renderer's shared memory learns the pages changed.
//
// BeginWrite and EndWrite bracket a write made through a host pointer, Write does both around a copy.

namespace Xe::PCI {

  // Little-endian dwords, the way device structures in guest memory (descriptor tables, PRDs) are laid out.
  inline u32 LoadLe32(const u8 *bytes) {
    u32 value;
    std::memcpy(&value, bytes, sizeof(value));
    return byteswap_le<u32>(value);
  }

  class DmaEngine {
  public:
    DmaEngine() = default;
    explicit DmaEngine(RAM *ram)
        : ram(ram) {}

    // True if [address, address + length) is a non-empty range inside RAM.
    bool Contains(u32 address, u64 length) const {
      return ram && length != 0 && static_cast<u64>(address) + length <= RAM_START_ADDR + ram->GetSize();
    }

    // Host pointer to read length bytes of guest memory through, nullptr when the range isn't in RAM.
    const u8 *ReadPointer(u32 address, u64 length) const {
      if (!Contains(address, length)) { return nullptr; }
      ram->FetchGpuPages(address, length);
      return ram->GetPointerToAddress(address);
    }

    // Host pointer to write length bytes of guest memory through, nullptr when the range isn't in RAM. Call EndWrite
    // with the bytes actually written once they are in.
    u8 *BeginWrite(u32 address, u64 length) const {
      if (!Contains(address, length)) { return nullptr; }
      // DeviceWrites hands over whole pages, so when the write leaves part of the first or last page alone, the GPU's
      // contents of that page come in first.
      if (address & 0xFFF) { ram->FetchGpuPages(address, 1); }
      if ((address + length) & 0xFFF) { ram->FetchGpuPages(static_cast<u32>(address + length - 1), 1); }
      ram->DeviceWrites(address, length);
      return ram->GetPointerToAddress(address);
    }
    void EndWrite(u32 address, u64 length) const {
      if (ram && length != 0) { ram->NotifyWrite(address, static_cast<u32>(length)); }
    }

    bool Read(u32 address, void *dst, u64 length) const {
      const u8 *src = ReadPointer(address, length);
      if (!src) { return false; }
      std::memcpy(dst, src, length);
      return true;
    }
    bool Write(u32 address, const void *src, u64 length) const {
      u8 *dst = BeginWrite(address, length);
      if (!dst) { return false; }
      std::memcpy(dst, src, length);
      EndWrite(address, length);
      return true;
    }

    u32 Read32(u32 address) const {
      u32 value = 0;
      Read(address, &value, sizeof(value));
      return byteswap_le<u32>(value);
    }
    void Write32(u32 address, u32 value) const {
      value = byteswap_le<u32>(value);
      Write(address, &value, sizeof(value));
    }
    // count consecutive little-endian dwords.
    bool ReadDwords(u32 address, u32 *dst, u32 count) const {
      if (!Read(address, dst, count * 4)) { return false; }
      for (u32 i = 0; i < count; ++i) { dst[i] = byteswap_le<u32>(dst[i]); }
      return true;
    }
    // At most MaxWriteDwords dwords.
    static constexpr u32 MaxWriteDwords = 16;
    void WriteDwords(u32 address, const u32 *src, u32 count) const {
      u32 swapped[MaxWriteDwords];
      if (count > MaxWriteDwords) { count = MaxWriteDwords; }
      for (u32 i = 0; i < count; ++i) { swapped[i] = byteswap_le<u32>(src[i]); }
      Write(address, swapped, count * 4);
    }

  private:
    RAM *ram = nullptr;
  };

} // namespace Xe::PCI
