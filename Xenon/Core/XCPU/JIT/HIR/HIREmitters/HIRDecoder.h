/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

/*
* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#pragma once

#include <array>

#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/PPU/PPCInternal.h"
#include "Core/XCPU/JIT/HIR/HIREmitters/HIREmitters.h"

namespace Xe::XCPU::HIR {

  constexpr u64 PPCRotateMask(u32 mb, u32 me) {
    const u64 mask = ~0ULL << (~(me - mb) & 63);
    return (mask >> (mb & 63)) | (mask << ((64 - mb) & 63));
  }
  constexpr u32 PPCDecode(u32 instr) {
    return ((instr >> 26) | (instr << 6)) & 0x1FFFF; // Rotate + mask
  }

  // Define a type alias for function pointers
  using instructionHandlerHIR = fptr<int(HIRBuilder &f, const uPPCInstr &instr)>;

  class HIRDecoder {
    template <typename T>
    class InstrInfo {
    public:
      constexpr InstrInfo(u32 v, T p, T pRc, u32 m = 0) :
        value(v), ptr0(p), ptrRc(pRc), magn(m) {
      }

      constexpr InstrInfo(u32 v, const T *p, const T *pRc, u32 m = 0) :
        value(v), ptr0(*p), ptrRc(*pRc), magn(m) {
      }

      T ptr0;
      T ptrRc;
      u32 value;
      u32 magn; // Non-zero for "columns" (effectively, number of most significant bits "eaten")
      u32 mask;
    };
  public:
    void fillTables();
    HIRDecoder();
    ~HIRDecoder() = default;
    const std::array<instructionHandlerHIR, 0x20000> &getTable() const noexcept {
      return table;
    }

    // Get HIR emmiter based on the decoded instruction
    instructionHandlerHIR decode(u32 instrData) const noexcept {
      instructionHandlerHIR handler = getTable()[PPCDecode(instrData)];
      if (handler != HIRInstrEmit_invalid) {
        return handler;
      } else {

#define GET_HANDLER(name) &HIRInstrEmit_##name

        // VMX128 Lookup.
        switch (ExtractBits(instrData, 0, 5)) {
        case 4:
          switch ((ExtractBits(instrData, 21, 27) << 4) | (ExtractBits(instrData, 30, 31) << 0)) {
          case 0b00000000011: handler = GET_HANDLER(lvsl128); break;
          case 0b00001000011: handler = GET_HANDLER(lvsr128); break;
          case 0b00010000011: handler = GET_HANDLER(lvewx128); break;
          case 0b00011000011: handler = GET_HANDLER(lvx128); break;
          case 0b00110000011: handler = GET_HANDLER(stvewx128); break;
          case 0b00111000011: handler = GET_HANDLER(stvx128); break;
          case 0b01011000011: handler = GET_HANDLER(lvxl128); break;
          case 0b01111000011: handler = GET_HANDLER(stvxl128); break;
          case 0b10000000011: handler = GET_HANDLER(lvlx128); break;
          case 0b10001000011: handler = GET_HANDLER(lvrx128); break;
          case 0b10100000011: handler = GET_HANDLER(stvlx128); break;
          case 0b10101000011: handler = GET_HANDLER(stvrx128); break;
          case 0b11000000011: handler = GET_HANDLER(lvlxl128); break;
          case 0b11001000011: handler = GET_HANDLER(lvrxl128); break;
          case 0b11100000011: handler = GET_HANDLER(stvlxl128); break;
          case 0b11101000011: handler = GET_HANDLER(stvrxl128); break;
          }
          switch ((ExtractBits(instrData, 21, 31) << 0)) {
          case 0b00000000000: handler = GET_HANDLER(vaddubm); break;
          case 0b00000000010: handler = GET_HANDLER(vmaxub); break;
          case 0b00000000100: handler = GET_HANDLER(vrlb); break;
          //case 0b00000001000: handler = GET_HANDLER(vmuloub); break;
          case 0b00000001010: handler = GET_HANDLER(vaddfp); break;
          case 0b00000001100: handler = GET_HANDLER(vmrghb); break;
          case 0b00000001110: handler = GET_HANDLER(vpkuhum); break;
          case 0b00001000000: handler = GET_HANDLER(vadduhm); break;
          case 0b00001000010: handler = GET_HANDLER(vmaxuh); break;
          case 0b00001000100: handler = GET_HANDLER(vrlh); break;
          //case 0b00001001000: handler = GET_HANDLER(vmulouh); break;
          case 0b00001001010: handler = GET_HANDLER(vsubfp); break;
          case 0b00001001100: handler = GET_HANDLER(vmrghh); break;
          case 0b00001001110: handler = GET_HANDLER(vpkuwum); break;
          case 0b00010000000: handler = GET_HANDLER(vadduwm); break;
          case 0b00010000010: handler = GET_HANDLER(vmaxuw); break;
          case 0b00010000100: handler = GET_HANDLER(vrlw); break;
          case 0b00010001100: handler = GET_HANDLER(vmrghw); break;
          case 0b00010001110: handler = GET_HANDLER(vpkuhus); break;
          case 0b00011001110: handler = GET_HANDLER(vpkuwus); break;
          case 0b00100000010: handler = GET_HANDLER(vmaxsb); break;
          case 0b00100000100: handler = GET_HANDLER(vslb); break;
          //case 0b00100001000: handler = GET_HANDLER(vmulosb); break;
          case 0b00100001010: handler = GET_HANDLER(vrefp); break;
          case 0b00100001100: handler = GET_HANDLER(vmrglb); break;
          case 0b00100001110: handler = GET_HANDLER(vpkshus); break;
          case 0b00101000010: handler = GET_HANDLER(vmaxsh); break;
          case 0b00101000100: handler = GET_HANDLER(vslh); break;
          //case 0b00101001000: handler = GET_HANDLER(vmulosh); break;
          case 0b00101001010: handler = GET_HANDLER(vrsqrtefp); break;
          case 0b00101001100: handler = GET_HANDLER(vmrglh); break;
          case 0b00101001110: handler = GET_HANDLER(vpkswus); break;
          case 0b00110000000: handler = GET_HANDLER(vaddcuw); break;
          case 0b00110000010: handler = GET_HANDLER(vmaxsw); break;
          case 0b00110000100: handler = GET_HANDLER(vslw); break;
          case 0b00110001010: handler = GET_HANDLER(vexptefp); break;
          case 0b00110001100: handler = GET_HANDLER(vmrglw); break;
          case 0b00110001110: handler = GET_HANDLER(vpkshss); break;
          case 0b00111000100: handler = GET_HANDLER(vsl); break;
          case 0b00111001010: handler = GET_HANDLER(vlogefp); break;
          case 0b00111001110: handler = GET_HANDLER(vpkswss); break;
          case 0b01000000000: handler = GET_HANDLER(vaddubs); break;
          case 0b01000000010: handler = GET_HANDLER(vminub); break;
          case 0b01000000100: handler = GET_HANDLER(vsrb); break;
          //case 0b01000001000: handler = GET_HANDLER(vmuleub); break;
          case 0b01000001010: handler = GET_HANDLER(vrfin); break;
          case 0b01000001100: handler = GET_HANDLER(vspltb); break;
          case 0b01000001110: handler = GET_HANDLER(vupkhsb); break;
          case 0b01001000000: handler = GET_HANDLER(vadduhs); break;
          case 0b01001000010: handler = GET_HANDLER(vminuh); break;
          case 0b01001000100: handler = GET_HANDLER(vsrh); break;
          //case 0b01001001000: handler = GET_HANDLER(vmuleuh); break;
          case 0b01001001010: handler = GET_HANDLER(vrfiz); break;
          case 0b01001001100: handler = GET_HANDLER(vsplth); break;
          case 0b01001001110: handler = GET_HANDLER(vupkhsh); break;
          case 0b01010000000: handler = GET_HANDLER(vadduws); break;
          case 0b01010000010: handler = GET_HANDLER(vminuw); break;
          case 0b01010000100: handler = GET_HANDLER(vsrw); break;
          case 0b01010001010: handler = GET_HANDLER(vrfip); break;
          case 0b01010001100: handler = GET_HANDLER(vspltw); break;
          case 0b01010001110: handler = GET_HANDLER(vupklsb); break;
          case 0b01011000100: handler = GET_HANDLER(vsr); break;
          case 0b01011001010: handler = GET_HANDLER(vrfim); break;
          case 0b01011001110: handler = GET_HANDLER(vupklsh); break;
          case 0b01100000000: handler = GET_HANDLER(vaddsbs); break;
          case 0b01100000010: handler = GET_HANDLER(vminsb); break;
          case 0b01100000100: handler = GET_HANDLER(vsrab); break;
          //case 0b01100001000: handler = GET_HANDLER(vmulesb); break;
          case 0b01100001010: handler = GET_HANDLER(vcfux); break;
          case 0b01100001100: handler = GET_HANDLER(vspltisb); break;
          case 0b01100001110: handler = GET_HANDLER(vpkpx); break;
          case 0b01101000000: handler = GET_HANDLER(vaddshs); break;
          case 0b01101000010: handler = GET_HANDLER(vminsh); break;
          case 0b01101000100: handler = GET_HANDLER(vsrah); break;
          //case 0b01101001000: handler = GET_HANDLER(vmulesh); break;
          case 0b01101001010: handler = GET_HANDLER(vcfsx); break;
          case 0b01101001100: handler = GET_HANDLER(vspltish); break;
          case 0b01101001110: handler = GET_HANDLER(vupkhpx); break;
          case 0b01110000000: handler = GET_HANDLER(vaddsws); break;
          case 0b01110000010: handler = GET_HANDLER(vminsw); break;
          case 0b01110000100: handler = GET_HANDLER(vsraw); break;
          case 0b01110001010: handler = GET_HANDLER(vctuxs); break;
          case 0b01110001100: handler = GET_HANDLER(vspltisw); break;
          case 0b01111001010: handler = GET_HANDLER(vctsxs); break;
          case 0b01111001110: handler = GET_HANDLER(vupklpx); break;
          case 0b10000000000: handler = GET_HANDLER(vsububm); break;
          case 0b10000000010: handler = GET_HANDLER(vavgub); break;
          case 0b10000000100: handler = GET_HANDLER(vand); break;
          case 0b10000001010: handler = GET_HANDLER(vmaxfp); break;
          case 0b10000001100: handler = GET_HANDLER(vslo); break;
          case 0b10001000000: handler = GET_HANDLER(vsubuhm); break;
          case 0b10001000010: handler = GET_HANDLER(vavguh); break;
          case 0b10001000100: handler = GET_HANDLER(vandc); break;
          case 0b10001001010: handler = GET_HANDLER(vminfp); break;
          case 0b10001001100: handler = GET_HANDLER(vsro); break;
          case 0b10010000000: handler = GET_HANDLER(vsubuwm); break;
          case 0b10010000010: handler = GET_HANDLER(vavguw); break;
          case 0b10010000100: handler = GET_HANDLER(vor); break;
          case 0b10011000100: handler = GET_HANDLER(vxor); break;
          case 0b10100000010: handler = GET_HANDLER(vavgsb); break;
          case 0b10100000100: handler = GET_HANDLER(vnor); break;
          case 0b10101000010: handler = GET_HANDLER(vavgsh); break;
          case 0b10110000000: handler = GET_HANDLER(vsubcuw); break;
          case 0b10110000010: handler = GET_HANDLER(vavgsw); break;
          case 0b11000000000: handler = GET_HANDLER(vsububs); break;
          //case 0b11000000100: handler = GET_HANDLER(mfvscr); break;
          case 0b11000001000: handler = GET_HANDLER(vsum4ubs); break;
          case 0b11001000000: handler = GET_HANDLER(vsubuhs); break;
          //case 0b11001000100: handler = GET_HANDLER(mtvscr); break;
          case 0b11001001000: handler = GET_HANDLER(vsum4shs); break;
          case 0b11010000000: handler = GET_HANDLER(vsubuws); break;
          case 0b11010001000: handler = GET_HANDLER(vsum2sws); break;
          case 0b11100000000: handler = GET_HANDLER(vsubsbs); break;
          case 0b11100001000: handler = GET_HANDLER(vsum4sbs); break;
          case 0b11101000000: handler = GET_HANDLER(vsubshs); break;
          case 0b11110000000: handler = GET_HANDLER(vsubsws); break;
          case 0b11110001000: handler = GET_HANDLER(vsumsws); break;
          }
          switch ((ExtractBits(instrData, 22, 31) << 0)) {
          case 0b0000000110: handler = GET_HANDLER(vcmpequb); break;
          case 0b0001000110: handler = GET_HANDLER(vcmpequh); break;
          case 0b0010000110: handler = GET_HANDLER(vcmpequw); break;
          case 0b0011000110: handler = GET_HANDLER(vcmpeqfp); break;
          case 0b0111000110: handler = GET_HANDLER(vcmpgefp); break;
          case 0b1000000110: handler = GET_HANDLER(vcmpgtub); break;
          case 0b1001000110: handler = GET_HANDLER(vcmpgtuh); break;
          case 0b1010000110: handler = GET_HANDLER(vcmpgtuw); break;
          case 0b1011000110: handler = GET_HANDLER(vcmpgtfp); break;
          case 0b1100000110: handler = GET_HANDLER(vcmpgtsb); break;
          case 0b1101000110: handler = GET_HANDLER(vcmpgtsh); break;
          case 0b1110000110: handler = GET_HANDLER(vcmpgtsw); break;
          case 0b1111000110: handler = GET_HANDLER(vcmpbfp); break;
          }
          switch ((ExtractBits(instrData, 26, 31) << 0)) {
          case 0b100000: handler = GET_HANDLER(vmhaddshs); break;
          case 0b100001: handler = GET_HANDLER(vmhraddshs); break;
          case 0b100010: handler = GET_HANDLER(vmladduhm); break;
          //case 0b100100: handler = GET_HANDLER(vmsumubm); break;
          //case 0b100101: handler = GET_HANDLER(vmsummbm); break;
          //case 0b100110: handler = GET_HANDLER(vmsumuhm); break;
          //case 0b100111: handler = GET_HANDLER(vmsumuhs); break;
          //case 0b101000: handler = GET_HANDLER(vmsumshm); break;
          //case 0b101001: handler = GET_HANDLER(vmsumshs); break;
          case 0b101010: handler = GET_HANDLER(vsel); break;
          case 0b101011: handler = GET_HANDLER(vperm); break;
          case 0b101100: handler = GET_HANDLER(vsldoi); break;
          case 0b101110: handler = GET_HANDLER(vmaddfp); break;
          case 0b101111: handler = GET_HANDLER(vnmsubfp); break;
          }
          switch ((ExtractBits(instrData, 27, 27) << 0)) {
          case 0b1: handler = GET_HANDLER(vsldoi128); break;
          }
          break;
        case 5:
          switch ((ExtractBits(instrData, 22, 22) << 5) | (ExtractBits(instrData, 27, 27) << 0)) {
          case 0b000000: handler = GET_HANDLER(vperm128); break;
          }
          switch ((ExtractBits(instrData, 22, 25) << 2) | (ExtractBits(instrData, 27, 27) << 0)) {
          case 0b000001: handler = GET_HANDLER(vaddfp128); break;
          case 0b000101: handler = GET_HANDLER(vsubfp128); break;
          case 0b001001: handler = GET_HANDLER(vmulfp128); break;
          case 0b001101: handler = GET_HANDLER(vmaddfp128); break;
          case 0b010001: handler = GET_HANDLER(vmaddcfp128); break;
          case 0b010101: handler = GET_HANDLER(vnmsubfp128); break;
          case 0b011001: handler = GET_HANDLER(vmsum3fp128); break;
          case 0b011101: handler = GET_HANDLER(vmsum4fp128); break;
          case 0b100000: handler = GET_HANDLER(vpkshss128); break;
          case 0b100001: handler = GET_HANDLER(vand128); break;
          case 0b100100: handler = GET_HANDLER(vpkshus128); break;
          case 0b100101: handler = GET_HANDLER(vandc128); break;
          case 0b101000: handler = GET_HANDLER(vpkswss128); break;
          case 0b101001: handler = GET_HANDLER(vnor128); break;
          case 0b101100: handler = GET_HANDLER(vpkswus128); break;
          case 0b101101: handler = GET_HANDLER(vor128); break;
          case 0b110000: handler = GET_HANDLER(vpkuhum128); break;
          case 0b110001: handler = GET_HANDLER(vxor128); break;
          case 0b110100: handler = GET_HANDLER(vpkuhus128); break;
          case 0b110101: handler = GET_HANDLER(vsel128); break;
          case 0b111000: handler = GET_HANDLER(vpkuwum128); break;
          case 0b111001: handler = GET_HANDLER(vslo128); break;
          case 0b111100: handler = GET_HANDLER(vpkuwus128); break;
          case 0b111101: handler = GET_HANDLER(vsro128); break;
          }
          break;
        case 6:
          switch ((ExtractBits(instrData, 21, 22) << 5) | (ExtractBits(instrData, 26, 27) << 0)) {
          case 0b0100001: handler = GET_HANDLER(vpermwi128); break;
          }
          switch ((ExtractBits(instrData, 21, 23) << 4) | (ExtractBits(instrData, 26, 27) << 0)) {
          case 0b1100001: handler = GET_HANDLER(vpkd3d128); break;
          case 0b1110001: handler = GET_HANDLER(vrlimi128); break;
          }
          switch ((ExtractBits(instrData, 21, 27) << 0)) {
          case 0b0100011: handler = GET_HANDLER(vcfpsxws128); break;
          case 0b0100111: handler = GET_HANDLER(vcfpuxws128); break;
          case 0b0101011: handler = GET_HANDLER(vcsxwfp128); break;
          case 0b0101111: handler = GET_HANDLER(vcuxwfp128); break;
          case 0b0110011: handler = GET_HANDLER(vrfim128); break;
          case 0b0110111: handler = GET_HANDLER(vrfin128); break;
          case 0b0111011: handler = GET_HANDLER(vrfip128); break;
          case 0b0111111: handler = GET_HANDLER(vrfiz128); break;
          case 0b1100011: handler = GET_HANDLER(vrefp128); break;
          case 0b1100111: handler = GET_HANDLER(vrsqrtefp128); break;
          case 0b1101011: handler = GET_HANDLER(vexptefp128); break;
          case 0b1101111: handler = GET_HANDLER(vlogefp128); break;
          case 0b1110011: handler = GET_HANDLER(vspltw128); break;
          case 0b1110111: handler = GET_HANDLER(vspltisw128); break;
          case 0b1111111: handler = GET_HANDLER(vupkd3d128); break;
          }
          switch ((ExtractBits(instrData, 22, 24) << 3) | (ExtractBits(instrData, 27, 27) << 0)) {
          case 0b000000: handler = GET_HANDLER(vcmpeqfp128); break;
          case 0b001000: handler = GET_HANDLER(vcmpgefp128); break;
          case 0b010000: handler = GET_HANDLER(vcmpgtfp128); break;
          case 0b011000: handler = GET_HANDLER(vcmpbfp128); break;
          case 0b100000: handler = GET_HANDLER(vcmpequw128); break;
          }
          switch ((ExtractBits(instrData, 22, 25) << 2) | (ExtractBits(instrData, 27, 27) << 0)) {
          case 0b000101: handler = GET_HANDLER(vrlw128); break;
          case 0b001101: handler = GET_HANDLER(vslw128); break;
          case 0b010101: handler = GET_HANDLER(vsraw128); break;
          case 0b011101: handler = GET_HANDLER(vsrw128); break;
          case 0b101000: handler = GET_HANDLER(vmaxfp128); break;
          case 0b101100: handler = GET_HANDLER(vminfp128); break;
          case 0b110000: handler = GET_HANDLER(vmrghw128); break;
          case 0b110100: handler = GET_HANDLER(vmrglw128); break;
          case 0b111000: handler = GET_HANDLER(vupkhsb128); break;
          case 0b111100: handler = GET_HANDLER(vupklsb128); break;
          }
          break;
        case 31:
          switch ((ExtractBits(instrData, 6, 10) << 20) | (ExtractBits(instrData, 21, 30) << 0)) {
          case 0b0000000000000001111110110: handler = GET_HANDLER(dcbz); break;
          case 0b0000100000000001111110110: handler = GET_HANDLER(dcbz128); break;
          }
        }
#undef GET_HANDLER
        return handler;
      }
    }
  private:
    // Fast lookup table
    std::array<instructionHandlerHIR, 0x20000> table;

    template <typename T>
    void fillTable(std::array<T, 0x20000> &t, u32 mainOp, u32 count, u32 sh,
      std::initializer_list<InstrInfo<T>> entries) noexcept {
      const bool isVxuMode = (count == static_cast<u32>(-1)) &&
        std::any_of(entries.begin(), entries.end(), [](const InstrInfo<T> &v) {
        return v.mask != 0;
          });

      for (const auto &v : entries) {
        if (isVxuMode) {
          // VXU-style masked mode
          for (u32 i = 0; i < 1u << 11; i++) {
            const u32 instr = i << 21;
            if ((instr & v.mask) == v.value) {
              const u32 key = (i << 6) | mainOp;
              c_at(t, key) = (i & 1) ? v.ptrRc : v.ptr0;
            }
          }
        } else if (sh < 11) {
          // Old-style table expansion
          for (u32 i = 0; i < 1u << (v.magn + (11 - sh - count)); i++) {
            for (u32 j = 0; j < 1u << sh; j++) {
              const u32 k = (((i << (count - v.magn)) | v.value) << sh) | j;
              c_at(t, (k << 6) | mainOp) = (k & 1) ? v.ptrRc : v.ptr0;
            }
          }
        } else {
          // Special fallback
          for (u32 i = 0; i < 1u << 11; i++) {
            c_at(t, (i << 6) | v.value) = (i & 1) ? v.ptrRc : v.ptr0;
          }
        }
      }
    }
  };
} // namespace HIRDecoder
