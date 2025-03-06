#pragma once
/*
* Copyright 2025 Xenon Emulator Project

* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include <array>

#include "Base/Types.h"
#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"

constexpr u64 PPCRotateMask(u32 mb, u32 me) {
  const u64 mask = ~0ull << (~(me - mb) & 63);
  return (mask >> (mb & 63)) | (mask << ((64 - mb) & 63));
}

constexpr u32 PPCDecode(u32 inst) {
  return ((inst >> 26) | (inst << 6)) & 0x1FFFF; // Rotate + mask
}

namespace PPCInterpreter {
  // Define a type alias for function pointers
  using instructionHandler = void(*)(PPU_STATE&);
  extern void PPCInterpreter_nop(PPU_STATE &ppuState);
  extern void PPCInterpreter_invalid(PPU_STATE &ppuState);
  extern void PPCInterpreter_known_unimplemented(const char *name, PPU_STATE &ppuState);
  class PPCDecoder {
    class InstrInfo {
    public:
      constexpr InstrInfo(u32 v, instructionHandler p, instructionHandler pRc, u32 m = 0) :
        value(v), ptr0(p), ptrRc(pRc), magn(m)
      {}

      constexpr InstrInfo(u32 v, const instructionHandler* p, const instructionHandler* pRc, u32 m = 0) :
        value(v), ptr0(*p), ptrRc(*pRc), magn(m)
      {}

      u32 value;
      instructionHandler ptr0;
      instructionHandler ptrRc;
      u32 magn; // Non-zero for "columns" (effectively, number of most significant bits "eaten")
    };
  public:
    PPCDecoder();
    ~PPCDecoder() = default;
    const std::array<instructionHandler, 0x20000>& getTable() const noexcept {
      return table;
    }
    instructionHandler decode(u32 inst) const noexcept {
      if (inst == 0x60000000) {
        return &PPCInterpreter_nop;
      }
      return table[PPCDecode(inst)];
    }
  private:
    // Fast lookup table
    std::array<instructionHandler, 0x20000> table;

    void fillTable(u32 mainOp, u32 count, u32 sh, std::initializer_list<InstrInfo> entries) noexcept {
      if (sh < 11) {
        for (const auto& v : entries) {
          for (u32 i = 0; i < 1u << (v.magn + (11 - sh - count)); i++) {
            for (u32 j = 0; j < 1u << sh; j++) {
              const u32 k = (((i << (count - v.magn)) | v.value) << sh) | j;
              c_at(table, (k << 6) | mainOp) = k & 1 ? v.ptrRc : v.ptr0;
            }
          }
        }
      }
      else {
        // Main table (special case)
        for (const auto& v : entries) {
          for (u32 i = 0; i < 1u << 11; i++) {
            c_at(table, i << 6 | v.value) = i & 1 ? v.ptrRc : v.ptr0;
          }
        }
      }
    }
  };
  std::string legacy_GetOpcodeName(u32 instrData);
} // namespace PPCInterpreter
