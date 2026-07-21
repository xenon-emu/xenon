/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Vector128.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <immintrin.h>

#define XE_XMM_CONST_ALIGN alignas(16)

namespace Xe::XCPU::JIT {

XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMZero;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMOne;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMOnePD;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMNegativeOne;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMFFFF;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMMaskX16Y16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMFlipX16Y16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMFixX16Y16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMNormalizeX16Y16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMM0001;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMM3301;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMM3331;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMM3333;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSignMaskPS;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSignMaskPD;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMAbsMaskPS;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMAbsMaskPD;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMByteSwapMask;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMByteOrderMask;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPermuteControl15;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPermuteByteMask;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackD3DCOLORSat;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackD3DCOLOR;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackD3DCOLOR;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackFLOAT16_2;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackFLOAT16_2;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackFLOAT16_4;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackFLOAT16_4;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackSHORT_Min;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackSHORT_Max;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackSHORT_2;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackSHORT_4;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackSHORT_2;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackSHORT_4;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackSHORT_Overflow;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackUINT_2101010_MinUnpacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackUINT_2101010_MaxUnpacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackUINT_2101010_MaskUnpacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackUINT_2101010_MaskPacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackUINT_2101010_Shift;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackUINT_2101010_Overflow;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackULONG_4202020_MinUnpacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackULONG_4202020_MaxUnpacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackULONG_4202020_MaskUnpacked;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackULONG_4202020_PermuteXZ;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPackULONG_4202020_PermuteYW;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackULONG_4202020_Permute;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnpackULONG_4202020_Overflow;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMOneOver255;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMMaskEvenPI16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMShiftMaskEvenPI16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMShiftMaskPS;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMShiftByteMask;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSwapWordMask;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMUnsignedDwordMax;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMM255;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPI32;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSignMaskI8;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSignMaskI16;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSignMaskI32;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMSignMaskF32;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMShortMinPS;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMShortMaxPS;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMIntMin;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMIntMax;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMIntMaxPD;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMPosIntMinPS;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMQNaN;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMMInt127;
XE_XMM_CONST_ALIGN extern const Base::Vector128 XMM2To32;
XE_XMM_CONST_ALIGN extern const Base::Vector128 loadVectorShiftLeftTable[16];
XE_XMM_CONST_ALIGN extern const Base::Vector128 loadVectorShiftRightTable[16];
XE_XMM_CONST_ALIGN extern const Base::Vector128 vsldoiTable[16];
XE_XMM_CONST_ALIGN extern const Base::Vector128 stvrxShuffleTable[16];
XE_XMM_CONST_ALIGN extern const Base::Vector128 stvlxBlendMasks[17];
XE_XMM_CONST_ALIGN extern const Base::Vector128 stvrxBlendMasks[16];
XE_XMM_CONST_ALIGN extern const Base::Vector128 vpkuwumShuffleMask;
XE_XMM_CONST_ALIGN extern const Base::Vector128 vpkswssShuffleMask;

using namespace Base;

template <typename T>
static T RotateLeftValue(T value, u32 shift) {
  using U = std::make_unsigned_t<T>;
  constexpr u32 bits = sizeof(T) * 8;

  shift &= bits - 1;
  return static_cast<T>(std::rotl(static_cast<U>(value), static_cast<s32>(shift)));
}

//
// Emulated instructions, meant to be used when no easy replacements exist for tricky HIR/PPC opcodes
//

extern __m128i EmulateShlV128(void *, __m128i src1, u8 src2);

extern __m128i EmulateShrV128(void *, __m128i src1, u8 src2);

template <typename T>
  requires std::is_integral_v<T>
extern __m128i EmulateVectorShl(void *, __m128i src1, __m128i src2);

template <typename T>
  requires std::is_integral_v<T>
extern __m128i EmulateVectorShr(void *, __m128i src1, __m128i src2);

template <typename T>
  requires std::is_integral_v<T>
extern __m128i EmulateVectorRotateLeft(void*, __m128i src1, __m128i src2);

template <typename T>
  requires std::is_integral_v<T>
extern __m128i EmulateVectorAverage(void *, __m128i src1, __m128i src2);

extern __m128 EmulatePow2Float(void *, __m128 src);

extern __m128d EmulatePow2Double(void *, __m128d src);

extern __m128 EmulatePow2Vec(void *, __m128 src);

extern __m128 EmulateLog2Float(void *, __m128 src);

extern __m128d EmulateLog2Double(void *, __m128d src);

extern __m128 EmulateLog2Vec(void *, __m128 src);

extern __m128i EmulatePack8_IN_16_UN_UN_SAT(void *, __m128i src1, __m128i src2);

extern __m128i EmulatePack8_IN_16_UN_UN(void *, __m128i src1, __m128i src2);

}  // namespace Xe::XCPU::JIT