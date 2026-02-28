/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

namespace utils {

  // Does a quick runtime check to know if the host is little-endian.
  // This is used to determine whether to swap bytes when reading big-endian PE fields,
  // and yes, there's a compiler macro that can be usd but this check I feel is a better approach.
  inline bool hostIsLittleEndian() noexcept {
    static const u16 k = 0x0001u;
    u8 lo; std::memcpy(&lo, &k, 1);
    return lo == 0x01u;
  }

  // Regular byteswapping utilities
  // TODO: Use std::byteswap

  inline u16 bswap16(u16 v) noexcept {
    return static_cast<u16>((v << 8u) | (v >> 8u));
  }
  inline u32 bswap32(u32 v) noexcept {
    return ((v & 0xFF000000u) >> 24u) | ((v & 0x00FF0000u) >> 8u) |
      ((v & 0x0000FF00u) << 8u) | ((v & 0x000000FFu) << 24u);
  }
  inline u64 bswap64(u64 v) noexcept {
    return ((v & 0xFF00000000000000ULL) >> 56ULL) |
      ((v & 0x00FF000000000000ULL) >> 40ULL) |
      ((v & 0x0000FF0000000000ULL) >> 24ULL) |
      ((v & 0x000000FF00000000ULL) >> 8ULL) |
      ((v & 0x00000000FF000000ULL) << 8ULL) |
      ((v & 0x0000000000FF0000ULL) << 24ULL) |
      ((v & 0x000000000000FF00ULL) << 40ULL) |
      ((v & 0x00000000000000FFULL) << 56ULL);
  }

  // Endian reader, provides a quick way of performing reads using endianness abstraction.
  // Performs endianness detection using the utils namespace.
  // Inputs: Ptr to data, size of data, and endiannes of data.
  class EndianReader {
  public:
    EndianReader(const uint8_t *bufPtr, size_t size, bool dataBigEndian)
      : dataBufferPtr(bufPtr), dataSize(size), isDataBigEndian(dataBigEndian)
      , swapNeeded(isDataBigEndian == utils::hostIsLittleEndian()) {
    }

    uint8_t  u8(size_t off) const { return raw<uint8_t>(off); }
    uint16_t u16(size_t off) const { return maySw(raw<uint16_t>(off)); }
    uint32_t u32(size_t off) const { return maySw(raw<uint32_t>(off)); }
    uint64_t u64(size_t off) const { return maySw(raw<uint64_t>(off)); }
    int32_t  i32(size_t off) const {
      uint32_t v = u32(off); int32_t r;
      std::memcpy(&r, &v, 4); return r;
    }

    // Returns the image size
    size_t size()    const noexcept { return dataSize; }
    // Returns wheter the data is BE
    bool isDataBE() const noexcept { return isDataBigEndian; }
    // Returns the data pointer
    const uint8_t *data()    const noexcept { return dataBufferPtr; }

  private:
    const uint8_t *dataBufferPtr;
    size_t dataSize;
    bool isDataBigEndian;
    bool swapNeeded;

    template<typename T>
    T raw(size_t off) const {
      if (off + sizeof(T) > dataSize) {
        std::ostringstream o;
        o << "EndianReader OOB @ 0x" << std::hex << off
          << " (need " << sizeof(T) << " bytes, buf=" << std::dec << dataSize << ")";
        throw std::out_of_range(o.str());
      }
      T v; std::memcpy(&v, dataBufferPtr + off, sizeof(T)); return v;
    }

    // Byteswap methods using the swap flag
    uint16_t maySw(uint16_t v) const noexcept { return swapNeeded ? utils::bswap16(v) : v; }
    uint32_t maySw(uint32_t v) const noexcept { return swapNeeded ? utils::bswap32(v) : v; }
    uint64_t maySw(uint64_t v) const noexcept { return swapNeeded ? utils::bswap64(v) : v; }
  };
}