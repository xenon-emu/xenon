/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/JIT/x86VectorConstants.h"

#include <climits>

namespace Xe::XCPU::JIT {

using namespace Base;

// Shared vector constants and lookup tables used by both the legacy JIT and the HIR x86 backend emitters.
// We're using the same constants system used by Xenia, avoiding duplicate code and redundant movs/permutes.
XE_XMM_CONST_ALIGN const Vector128 XMMZero = Vector128f(0.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMOne = Vector128f(1.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMOnePD = Vector128d(1.0);
XE_XMM_CONST_ALIGN const Vector128 XMMNegativeOne = Vector128f(-1.0f, -1.0f, -1.0f, -1.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMFFFF = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMMaskX16Y16 = Vector128i(0x0000FFFFu, 0xFFFF0000u, 0x00000000u, 0x00000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMFlipX16Y16 = Vector128i(0x00008000u, 0x00000000u, 0x00000000u, 0x00000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMFixX16Y16 = Vector128f(-32768.0f, 0.0f, 0.0f, 0.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMNormalizeX16Y16 = Vector128f(1.0f / 32767.0f, 1.0f / (32767.0f * 65536.0f), 0.0f, 0.0f);
XE_XMM_CONST_ALIGN const Vector128 XMM0001 = Vector128f(0.0f, 0.0f, 0.0f, 1.0f);
XE_XMM_CONST_ALIGN const Vector128 XMM3301 = Vector128f(3.0f, 3.0f, 0.0f, 1.0f);
XE_XMM_CONST_ALIGN const Vector128 XMM3331 = Vector128f(3.0f, 3.0f, 3.0f, 1.0f);
XE_XMM_CONST_ALIGN const Vector128 XMM3333 = Vector128f(3.0f, 3.0f, 3.0f, 3.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMSignMaskPS = Vector128i(0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMSignMaskPD = Vector128i(0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMAbsMaskPS = Vector128i(0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMAbsMaskPD = Vector128i(0xFFFFFFFFu, 0x7FFFFFFFu, 0xFFFFFFFFu, 0x7FFFFFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMByteSwapMask = Vector128i(0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu);
XE_XMM_CONST_ALIGN const Vector128 XMMByteOrderMask = Vector128i(0x01000302u, 0x05040706u, 0x09080B0Au, 0x0D0C0F0Eu);
XE_XMM_CONST_ALIGN const Vector128 XMMPermuteControl15 = Vector128b(15);
XE_XMM_CONST_ALIGN const Vector128 XMMPermuteByteMask = Vector128b(0x1F);
XE_XMM_CONST_ALIGN const Vector128 XMMPackD3DCOLORSat = Vector128i(0x404000FFu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackD3DCOLOR = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x0C000408u);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackD3DCOLOR = Vector128i(0xFFFFFF0Eu, 0xFFFFFF0Du, 0xFFFFFF0Cu, 0xFFFFFF0Fu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackFLOAT16_2 = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x01000302u);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackFLOAT16_2 = Vector128i(0x0D0C0F0Eu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackFLOAT16_4 = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0x01000302u, 0x05040706u);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackFLOAT16_4 = Vector128i(0x09080B0Au, 0x0D0C0F0Eu, 0xFFFFFFFFu, 0xFFFFFFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackSHORT_Min = Vector128i(0x403F8001u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackSHORT_Max = Vector128i(0x40407FFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackSHORT_2 = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x01000504u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackSHORT_4 = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0x01000504u, 0x09080D0Cu);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackSHORT_2 = Vector128i(0xFFFF0F0Eu, 0xFFFF0D0Cu, 0xFFFFFFFFu, 0xFFFFFFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackSHORT_4 = Vector128i(0xFFFF0B0Au, 0xFFFF0908u, 0xFFFF0F0Eu, 0xFFFF0D0Cu);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackSHORT_Overflow = Vector128i(0x403F8000u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackUINT_2101010_MinUnpacked = Vector128i(0x403FFE01u, 0x403FFE01u, 0x403FFE01u, 0x40400000u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackUINT_2101010_MaxUnpacked = Vector128i(0x404001FFu, 0x404001FFu, 0x404001FFu, 0x40400003u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackUINT_2101010_MaskUnpacked = Vector128i(0x3FFu, 0x3FFu, 0x3FFu, 0x3u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackUINT_2101010_MaskPacked = Vector128i(0x3FFu, 0x3FFu << 10, 0x3FFu << 20, 0x3u << 30);
XE_XMM_CONST_ALIGN const Vector128 XMMPackUINT_2101010_Shift = Vector128i(0, 10, 20, 30);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackUINT_2101010_Overflow = Vector128i(0x403FFE00u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackULONG_4202020_MinUnpacked = Vector128i(0x40380001u, 0x40380001u, 0x40380001u, 0x40400000u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackULONG_4202020_MaxUnpacked = Vector128i(0x4047FFFFu, 0x4047FFFFu, 0x4047FFFFu, 0x4040000Fu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackULONG_4202020_MaskUnpacked = Vector128i(0xFFFFFu, 0xFFFFFu, 0xFFFFFu, 0xFu);
XE_XMM_CONST_ALIGN const Vector128 XMMPackULONG_4202020_PermuteXZ = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0x0A0908FFu, 0xFF020100u);
XE_XMM_CONST_ALIGN const Vector128 XMMPackULONG_4202020_PermuteYW = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0x0CFFFF06u, 0x0504FFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackULONG_4202020_Permute = Vector128i(0xFF0E0D0Cu, 0xFF0B0A09u, 0xFF080F0Eu, 0xFFFFFF0Bu);
XE_XMM_CONST_ALIGN const Vector128 XMMUnpackULONG_4202020_Overflow = Vector128i(0x40380000u);
XE_XMM_CONST_ALIGN const Vector128 XMMOneOver255 = Vector128f(1.0f / 255.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMMaskEvenPI16 = Vector128i(0x0000FFFFu, 0x0000FFFFu, 0x0000FFFFu, 0x0000FFFFu);
XE_XMM_CONST_ALIGN const Vector128 XMMShiftMaskEvenPI16 = Vector128i(0x0000000Fu, 0x0000000Fu, 0x0000000Fu, 0x0000000Fu);
XE_XMM_CONST_ALIGN const Vector128 XMMShiftMaskPS = Vector128i(0x0000001Fu, 0x0000001Fu, 0x0000001Fu, 0x0000001Fu);
XE_XMM_CONST_ALIGN const Vector128 XMMShiftByteMask = Vector128i(0x000000FFu, 0x000000FFu, 0x000000FFu, 0x000000FFu);
XE_XMM_CONST_ALIGN const Vector128 XMMSwapWordMask = Vector128i(0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u);
XE_XMM_CONST_ALIGN const Vector128 XMMUnsignedDwordMax = Vector128i(0xFFFFFFFFu, 0x00000000u, 0xFFFFFFFFu, 0x00000000u);
XE_XMM_CONST_ALIGN const Vector128 XMM255 = Vector128f(255.0f);
XE_XMM_CONST_ALIGN const Vector128 XMMPI32 = Vector128i(32);
XE_XMM_CONST_ALIGN const Vector128 XMMSignMaskI8 = Vector128i(0x80808080u, 0x80808080u, 0x80808080u, 0x80808080u);
XE_XMM_CONST_ALIGN const Vector128 XMMSignMaskI16 = Vector128i(0x80008000u, 0x80008000u, 0x80008000u, 0x80008000u);
XE_XMM_CONST_ALIGN const Vector128 XMMSignMaskI32 = Vector128i(0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMSignMaskF32 = Vector128i(0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMShortMinPS = Vector128f(SHRT_MIN);
XE_XMM_CONST_ALIGN const Vector128 XMMShortMaxPS = Vector128f(SHRT_MAX);
XE_XMM_CONST_ALIGN const Vector128 XMMIntMin = Vector128i(INT_MIN);
XE_XMM_CONST_ALIGN const Vector128 XMMIntMax = Vector128i(INT_MAX);
XE_XMM_CONST_ALIGN const Vector128 XMMIntMaxPD = Vector128d(INT_MAX);
XE_XMM_CONST_ALIGN const Vector128 XMMPosIntMinPS = Vector128f((f32)0x80000000u);
XE_XMM_CONST_ALIGN const Vector128 XMMQNaN = Vector128i(0x7FC00000u);
XE_XMM_CONST_ALIGN const Vector128 XMMInt127 = Vector128i(0x7Fu);
XE_XMM_CONST_ALIGN const Vector128 XMM2To32 = Vector128f(0x1.0p32f);

// Table used for Load Vector Shift Left instruction
XE_XMM_CONST_ALIGN const Vector128 loadVectorShiftLeftTable[16] = {
    Vector128b(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
    Vector128b(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
    Vector128b(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17),
    Vector128b(3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18),
    Vector128b(4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19),
    Vector128b(5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20),
    Vector128b(6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21),
    Vector128b(7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22),
    Vector128b(8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23),
    Vector128b(9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24),
    Vector128b(10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25),
    Vector128b(11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    Vector128b(12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27),
    Vector128b(13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28),
    Vector128b(14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29),
    Vector128b(15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30),
};

// Table used for Load Vector Shift Right instruction
XE_XMM_CONST_ALIGN const Vector128 loadVectorShiftRightTable[16] = {
    Vector128b(16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31),
    Vector128b(15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30),
    Vector128b(14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29),
    Vector128b(13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28),
    Vector128b(12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27),
    Vector128b(11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    Vector128b(10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25),
    Vector128b(9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24),
    Vector128b(8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23),
    Vector128b(7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22),
    Vector128b(6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21),
    Vector128b(5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20),
    Vector128b(4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19),
    Vector128b(3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18),
    Vector128b(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17),
    Vector128b(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
};

// Table used for Vector Shift Double Octet Immediate instruction
XE_XMM_CONST_ALIGN const Vector128 vsldoiTable[16] = {
    Vector128b(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
    Vector128b(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
    Vector128b(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17),
    Vector128b(3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18),
    Vector128b(4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19),
    Vector128b(5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20),
    Vector128b(6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21),
    Vector128b(7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22),
    Vector128b(8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23),
    Vector128b(9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24),
    Vector128b(10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25),
    Vector128b(11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26),
    Vector128b(12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27),
    Vector128b(13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28),
    Vector128b(14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29),
    Vector128b(15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30),
};

// Table used for Store Vector Right Indexed instruction
// Maps bytes[16-eb..15] to bytes[0..eb-1] for each eb value (0-15)
// NOTE: These indices account for the fact that XMMByteSwapMask reverses bytes within dwords
// After byteswap: positions 0-3 have orig[3,2,1,0], 4-7 have orig[7,6,5,4], etc.
// So "byte[15]" from interpreter (after byteswap) is at x86 position 12
XE_XMM_CONST_ALIGN const Vector128 stvrxShuffleTable[16] = {
    Vector128b(0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80), // eb=0 (no store)
    Vector128b(12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),   // eb=1: byte[15]->pos[0]
    Vector128b(13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),     // eb=2: byte[14,15]->pos[0,1]
    Vector128b(14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),       // eb=3: byte[13,14,15]->pos[0,1,2]
    Vector128b(15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),         // eb=4: byte[12..15]->pos[0..3]
    Vector128b(8,15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),            // eb=5: byte[11..15]
    Vector128b(9,8,15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),               // eb=6
    Vector128b(10,9,8,15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),                 // eb=7
    Vector128b(11,10,9,8,15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80),                   // eb=8: byte[8..15]->pos[0..7]
    Vector128b(4,11,10,9,8,15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80,0x80),                      // eb=9
    Vector128b(5,4,11,10,9,8,15,14,13,12,0x80,0x80,0x80,0x80,0x80,0x80),                         // eb=10
    Vector128b(6,5,4,11,10,9,8,15,14,13,12,0x80,0x80,0x80,0x80,0x80),                            // eb=11
    Vector128b(7,6,5,4,11,10,9,8,15,14,13,12,0x80,0x80,0x80,0x80),                               // eb=12: byte[4..15]->pos[0..11]
    Vector128b(0,7,6,5,4,11,10,9,8,15,14,13,12,0x80,0x80,0x80),                                  // eb=13
    Vector128b(1,0,7,6,5,4,11,10,9,8,15,14,13,12,0x80,0x80),                                     // eb=14
    Vector128b(2,1,0,7,6,5,4,11,10,9,8,15,14,13,12,0x80),                                        // eb=15: byte[1..15]->pos[0..14]
};

// Blend masks for Store Vector Left Indexed (stvlx)
// For count bytes to store (1-16), sets first 'count' bytes to 0xFF, rest to 0x00
// Used with vpblendvb: selects from src where mask is 0xFF
XE_XMM_CONST_ALIGN const Vector128 stvlxBlendMasks[17] = {
    Vector128b(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=0
    Vector128b(0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=1
    Vector128b(0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=2
    Vector128b(0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=3
    Vector128b(0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=4
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=5
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=6
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=7
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=8
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // count=9
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00), // count=10
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00), // count=11
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00), // count=12
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00), // count=13
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00), // count=14
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00), // count=15
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF), // count=16
};

// Blend masks for Store Vector Right Indexed (stvrx)
// For eb bytes to store (1-15), sets first 'eb' bytes to 0xFF, rest to 0x00
// Used after shuffle has positioned bytes at start of vector
XE_XMM_CONST_ALIGN const Vector128 stvrxBlendMasks[16] = {
    Vector128b(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=0 (no store)
    Vector128b(0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=1
    Vector128b(0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=2
    Vector128b(0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=3
    Vector128b(0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=4
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=5
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=6
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=7
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=8
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // eb=9
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00), // eb=10
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00), // eb=11
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00), // eb=12
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00), // eb=13
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00), // eb=14
    Vector128b(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00), // eb=15
};

// Shuffle mask for vpkuwum - extracts low 16 bits of each dword into low 64 bits
// Bytes 1,0 from dword0, 5,4 from dword1, 9,8 from dword2, 13,12 from dword3 -> bytes 0-7
// Note: bytes are swapped within each pair for big-endian halfword ordering
// High bytes are zeroed (0x80)
XE_XMM_CONST_ALIGN const Vector128 vpkuwumShuffleMask = Vector128b(0x01, 0x00, 0x05, 0x04, 0x09, 0x08, 0x0D, 0x0C, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);

// Shuffle mask for vpkswss - applies byte swap
XE_XMM_CONST_ALIGN const Vector128 vpkswssShuffleMask = Vector128b(0x01, 0x00, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06, 0x09, 0x08, 0x0B, 0x0A, 0x0D, 0x0C, 0x0F, 0x0E);

__m128i EmulateShlV128(void *, __m128i src1, u8 src2) {
  // Almost all instances are shamt = 1, but non-constant.
  // shamt is [0,7]
  u8 shamt = src2 & 0x7;
  alignas(16) Base::Vector128 value;
  _mm_store_si128(reinterpret_cast<__m128i *>(&value), src1);
  for (u8 i = 0; i < 15; ++i) {
    value.bytes[i ^ 0x3] = (value.bytes[i ^ 0x3] << shamt) | (value.bytes[(i + 1) ^ 0x3] >> (8 - shamt));
  }
  value.bytes[15 ^ 0x3] = value.bytes[15 ^ 0x3] << shamt;
  return _mm_load_si128(reinterpret_cast<__m128i *>(&value));
}

__m128i EmulateShrV128(void *, __m128i src1, u8 src2) {
  // Almost all instances are shamt = 1, but non-constant.
  // shamt is [0,7]
  u8 shamt = src2 & 0x7;
  alignas(16) Base::Vector128 value;
  _mm_store_si128(reinterpret_cast<__m128i *>(&value), src1);
  for (u8 i = 15; i > 0; --i) {
    value.bytes[i ^ 0x3] = (value.bytes[i ^ 0x3] >> shamt) |
      (value.bytes[(i - 1) ^ 0x3] << (8 - shamt));
  }
  value.bytes[0 ^ 0x3] = value.bytes[0 ^ 0x3] >> shamt;
  return _mm_load_si128(reinterpret_cast<__m128i *>(&value));
}

template <typename T>
  requires std::is_integral_v<T>
__m128i EmulateVectorShl(void *, __m128i src1, __m128i src2) {
  alignas(16) T value[16 / sizeof(T)];
  alignas(16) T shamt[16 / sizeof(T)];

  // Load SSE registers into a C array.
  _mm_store_si128(reinterpret_cast<__m128i *>(value), src1);
  _mm_store_si128(reinterpret_cast<__m128i *>(shamt), src2);

  for (u8 i = 0; i < (16 / sizeof(T)); ++i) {
    value[i] = value[i] << (shamt[i] & ((sizeof(T) * 8) - 1));
  }

  // Store result and return it.
  return _mm_load_si128(reinterpret_cast<__m128i *>(value));
}

template <typename T>
  requires std::is_integral_v<T>
__m128i EmulateVectorShr(void *, __m128i src1, __m128i src2) {
  alignas(16) T value[16 / sizeof(T)];
  alignas(16) T shamt[16 / sizeof(T)];

  // Load SSE registers into a C array.
  _mm_store_si128(reinterpret_cast<__m128i *>(value), src1);
  _mm_store_si128(reinterpret_cast<__m128i *>(shamt), src2);

  for (u8 i = 0; i < (16 / sizeof(T)); ++i) {
    value[i] = value[i] >> (shamt[i] & ((sizeof(T) * 8) - 1));
  }

  // Store result and return it.
  return _mm_load_si128(reinterpret_cast<__m128i *>(value));
}

template <typename T>
  requires std::is_integral_v<T>
__m128i EmulateVectorRotateLeft(void*, __m128i src1, __m128i src2) {
  alignas(16) T value[16 / sizeof(T)];
  alignas(16) T shamt[16 / sizeof(T)];

  _mm_store_si128(reinterpret_cast<__m128i*>(value), src1);
  _mm_store_si128(reinterpret_cast<__m128i*>(shamt), src2);

  for (u8 i = 0; i < 16 / sizeof(T); ++i) {
    value[i] = RotateLeftValue(value[i], static_cast<u32>(shamt[i]));
  }

  return _mm_load_si128(reinterpret_cast<const __m128i*>(value));
}

template <typename T>
  requires std::is_integral_v<T>
__m128i EmulateVectorAverage(void *, __m128i src1, __m128i src2) {
  alignas(16) T src1v[16 / sizeof(T)];
  alignas(16) T src2v[16 / sizeof(T)];
  alignas(16) T value[16 / sizeof(T)];

  // Load SSE registers into a C array.
  _mm_store_si128(reinterpret_cast<__m128i *>(src1v), src1);
  _mm_store_si128(reinterpret_cast<__m128i *>(src2v), src2);

  for (u8 i = 0; i < (16 / sizeof(T)); ++i) {
    auto t = (u64(src1v[i]) + u64(src2v[i]) + 1) / 2;
    value[i] = T(t);
  }

  // Store result and return it.
  return _mm_load_si128(reinterpret_cast<__m128i *>(value));
}

__m128 EmulatePow2Float(void *, __m128 src) {
  f32 srcValue;
  _mm_store_ss(&srcValue, src);
  f32 result = std::exp2(srcValue);
  return _mm_load_ss(&result);
}

__m128d EmulatePow2Double(void *, __m128d src) {
  f64 srcValue;
  _mm_store_sd(&srcValue, src);
  f64 result = std::exp2(srcValue);
  return _mm_load_sd(&result);
}

__m128 EmulatePow2Vec(void *, __m128 src) {
  alignas(16) f32 values[4];
  _mm_store_ps(values, src);
  for (u8 i = 0; i < 4; ++i) {
    values[i] = std::exp2(values[i]);
  }
  return _mm_load_ps(values);
}

__m128 EmulateLog2Float(void *, __m128 src) {
  f32 srcValue;
  _mm_store_ss(&srcValue, src);
  float result = std::log2(srcValue);
  return _mm_load_ss(&result);
}

__m128d EmulateLog2Double(void *, __m128d src) {
  f64 srcValue;
  _mm_store_sd(&srcValue, src);
  f64 result = std::log2(srcValue);
  return _mm_load_sd(&result);
}

__m128 EmulateLog2Vec(void *, __m128 src) {
  alignas(16) f32 values[4];
  _mm_store_ps(values, src);
  for (u8 i = 0; i < 4; ++i) {
    values[i] = std::log2(values[i]);
  }
  return _mm_load_ps(values);
}

__m128i EmulatePack8_IN_16_UN_UN_SAT(void *, __m128i src1, __m128i src2) {
  alignas(16) u16 a[8];
  alignas(16) u16 b[8];
  alignas(16) u8 c[16];
  _mm_store_si128(reinterpret_cast<__m128i *>(a), src1);
  _mm_store_si128(reinterpret_cast<__m128i *>(b), src2);
  for (u8 i = 0; i < 8; ++i) {
    c[i] = u8(std::max(u16(0), std::min(u16(255), a[i])));
    c[i + 8] = u8(std::max(u16(0), std::min(u16(255), b[i])));
  }
  return _mm_load_si128(reinterpret_cast<__m128i *>(c));
}

__m128i EmulatePack8_IN_16_UN_UN(void *, __m128i src1, __m128i src2) {
  alignas(16) u8 a[16];
  alignas(16) u8 b[16];
  alignas(16) u8 c[16];
  _mm_store_si128(reinterpret_cast<__m128i *>(a), src1);
  _mm_store_si128(reinterpret_cast<__m128i *>(b), src2);
  for (u8 i = 0; i < 8; ++i) {
    c[i] = a[i * 2];
    c[i + 8] = b[i * 2];
  }
  return _mm_load_si128(reinterpret_cast<__m128i *>(c));
}

// Explicit instantiations for the concrete element types used by the code
// generation backends. Definitions above live in this translation unit only,
// so any type used from another .cpp must be instantiated here.
template __m128i EmulateVectorShl<uint8_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorShl<uint16_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorShr<uint8_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorShr<uint16_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorShr<int8_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorShr<int16_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorRotateLeft<uint8_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorRotateLeft<uint16_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorAverage<uint32_t>(void *, __m128i, __m128i);
template __m128i EmulateVectorAverage<int32_t>(void *, __m128i, __m128i);

}  // namespace Xe::XCPU::JIT