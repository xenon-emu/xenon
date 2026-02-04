/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "JITEmitter_Helpers.h"

#if defined(ARCH_X86) || defined(ARCH_X86_64)

//
// Vector Register Pointer Helper
//

#define VPRPtr(x) b->threadCtx->array(&sPPUThread::VR).Ptr(x)

//
// Allocates a new XMM register for vector operations
//

#define newXMM() b->compiler->newXmm()

// Selects the right byte/word from a vector. 
// We need to flip logical indices (0,1,2,3,4,5,6,7,...) = (3,2,1,0,7,6,5,4,...)
#define VEC128_BYTE_VMX_TO_AVX(idx) ((idx) ^ 0x3)
#define VEC128_WORD_VMX_TO_AVX(idx) ((idx) ^ 0x1)

//
// VMX 128 Instruction bitfields
//

// Re declared here because of the way JIT uses CI

#define J_VMX128_VD128   (instr.VMX128.VD128l | (instr.VMX128.VD128h << 5))
#define J_VMX128_VA128   (instr.VMX128.VA128l | (instr.VMX128.VA128h << 5) | (instr.VMX128.VA128H << 6))
#define J_VMX128_VB128   (instr.VMX128.VB128l | (instr.VMX128.VB128h << 5))

#define J_VMX128_1_VD128 (instr.VMX128_1.VD128l | (instr.VMX128_1.VD128h << 5))

#define J_VMX128_2_VD128 (instr.VMX128_2.VD128l | (instr.VMX128_2.VD128h << 5))
#define J_VMX128_2_VA128 (instr.VMX128_2.VA128l | (instr.VMX128_2.VA128h << 5) | (instr.VMX128_2.VA128H << 6))
#define J_VMX128_2_VB128 (instr.VMX128_2.VB128l | (instr.VMX128_2.VB128h << 5))
#define J_VMX128_2_VC    (instr.VMX128_2.VC)

#define J_VMX128_3_VD128 (instr.VMX128_3.VD128l | (instr.VMX128_3.VD128h << 5))
#define J_VMX128_3_VB128 (instr.VMX128_3.VB128l | (instr.VMX128_3.VB128h << 5))
#define J_VMX128_3_IMM   (instr.VMX128_3.IMM)

#define J_VMX128_5_VD128 (instr.VMX128_5.VD128l | (instr.VMX128_5.VD128h << 5))
#define J_VMX128_5_VA128 (instr.VMX128_5.VA128l | (instr.VMX128_5.VA128h << 5)) | (instr.VMX128_5.VA128H << 6)
#define J_VMX128_5_VB128 (instr.VMX128_5.VB128l | (instr.VMX128_5.VB128h << 5))
#define J_VMX128_5_SH    (instr.VMX128_5.SH)

#define J_VMX128_R_VD128 (instr.VMX128_R.VD128l | (instr.VMX128_R.VD128h << 5))
#define J_VMX128_R_VA128 (instr.VMX128_R.VA128l | (instr.VMX128_R.VA128h << 5) | (instr.VMX128_R.VA128H << 6))
#define J_VMX128_R_VB128 (instr.VMX128_R.VB128l | (instr.VMX128_R.VB128h << 5))

//
// Constants
//

using namespace Base;

// We're using the same constants system used by Xenia, avoiding duplicate code and redundant movs/permutes.

static const Vector128 XMMFFFF = Vector128i(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
static const Vector128 XMMByteSwapMask = Vector128i(0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu);
static const Vector128 XMMSwapWordMask = Vector128i(0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u);
static const Vector128 XMMPermuteByteMask = Vector128b(0x1F);
static const Vector128 XMMPermuteControl15 = Vector128b(15);

// Table used for Load Vector Shift Left instruction
static const Vector128 loadVectorShiftLeftTable[16] = {
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
static const Vector128 loadVectorShiftRightTable[16] = {
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
static const Vector128 vsldoiTable[16] = {
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
static const Vector128 stvrxShuffleTable[16] = {
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
static const Vector128 stvlxBlendMasks[17] = {
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
static const Vector128 stvrxBlendMasks[16] = {
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
static const Vector128 vpkuwumShuffleMask = Vector128b(0x01,0x00,0x05,0x04,0x09,0x08,0x0D,0x0C,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80);

// Shuffle mask for vpkswss - applies byte swap
static const Vector128 vpkswssShuffleMask = Vector128b(0x01,0x00,0x03,0x02,0x05,0x04,0x07,0x06, 0x09, 0x08, 0x0B, 0x0A, 0x0D,0x0C,0x0F,0x0E);

//
// Helpers
// 

// Performs a denormals flush to zero as expected from VMX/AltiVec instructions.
#define VXU_FLUSH_DENORMALS_TO_ZERO

// Chcks for VX enabled bit of MSR and raises an exception if not set
inline void J_checkVXUEnabled(JITBlockBuilder *b) {
  x86::Gp msrReg = newGP64();
  x86::Gp exceptionReg = newGP16();

  Label vxEnabledLabel = b->compiler->newLabel();

  // Load MSR
  COMP->mov(msrReg, SPRPtr(MSR));
  // Check VX bit (bit 25 in LE)
  COMP->bt(msrReg, 25);
  COMP->jc(vxEnabledLabel);
  // VX not enabled, raise VXU exception
  COMP->mov(exceptionReg, EXPtr());
  COMP->or_(exceptionReg, ppuVXUnavailableEx);
  COMP->mov(EXPtr(), exceptionReg);
  COMP->ret();
  // VX enabled, proceed
  COMP->bind(vxEnabledLabel);
}

// Helper: Flush denormals to zero for a packed single-precision vector
// Denormals are values where exponent bits are all zero but fraction is non-zero
inline void J_FlushDenormalsToZero(JITBlockBuilder *b, x86::Xmm vec) {
#ifdef VXU_FLUSH_DENORMALS_TO_ZERO

  // Denormal floats have exponent = 0 and mantissa != 0
  // We detect them by checking if abs(value) < FLT_MIN (smallest normal float)
  // and value != 0, then set to zero.

  // Simpler approach: Use ANDPS with a mask that preserves only normalized values
  // A float is denormal if (bits & 0x7F800000) == 0 and (bits & 0x007FFFFF) != 0

  x86::Xmm absVal = newXMM();
  x86::Xmm expMask = newXMM();
  x86::Xmm cmpResult = newXMM();
  x86::Gp tempGp = newGP64();

  // Absolute value mask (clear sign bit): 0x7FFFFFFF
  COMP->mov(tempGp, 0x7FFFFFFF7FFFFFFF);
  COMP->vmovq(absVal, tempGp);
  COMP->vpbroadcastq(absVal, absVal);

  // Get absolute value
  COMP->vandps(absVal, vec, absVal);

  // Exponent mask: 0x7F800000 (exponent bits for single precision)
  COMP->mov(tempGp, 0x7F8000007F800000);
  COMP->vmovq(expMask, tempGp);
  COMP->vpbroadcastq(expMask, expMask);

  // Check if exponent is non-zero (normal or infinity/NaN)
  // If (value & expMask) != 0, it's not denormal (or it's zero)
  COMP->vandps(cmpResult, absVal, expMask);

  // Compare: if exponent bits are zero, the value is denormal or zero
  // We want to keep the value only if exponent bits are non-zero
  x86::Xmm zeroVec = newXMM();
  COMP->vxorps(zeroVec, zeroVec, zeroVec);

  // cmpResult = (cmpResult != 0) ? 0xFFFFFFFF : 0x00000000
  COMP->vcmpps(cmpResult, cmpResult, zeroVec, 4); // NEQ comparison (predicate 4)

  // Apply mask: keep value if exponent is non-zero, otherwise zero
  COMP->vandps(vec, vec, cmpResult);

#endif // VXU_FLUSH_DENORMALS_TO_ZERO
}

// Vector Add Floating Point (x'1000 000A')
void PPCInterpreter::PPCInterpreterJIT_vaddfp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(instr.va));
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Perform packed single-precision floating-point addition using AVX
  // vD = vA + vB (4 x float32)
  COMP->vaddps(vD, vA, vB);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Add Floating Point
void PPCInterpreter::PPCInterpreterJIT_vaddfp128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovaps(vB, VPRPtr(J_VMX128_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Perform packed single-precision floating-point addition using AVX
  // vD = vA + vB (4 x float32)
  COMP->vaddps(vD, vA, vB);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Add Carry Unsigned Word (x'1000 0180')
void PPCInterpreter::PPCInterpreterJIT_vaddcuw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vSum = newXMM();
  x86::Xmm vCmp = newXMM();
  x86::Xmm vOne = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // Compute sum: vSum = vA + vB (unsigned 32-bit addition, wrapping)
  COMP->vpaddd(vSum, vA, vB);

  // Detect carry: carry occurred if vSum < vA (unsigned comparison)
  // For unsigned comparison in SSE/AVX, we use the trick:
  // (a + b) < a (unsigned) is equivalent to carry out

  // AVX2 doesn't have unsigned comparison directly, so we use:
  // Carry = (vSum < vA) unsigned
  // We can detect this by: if high bit differs after XOR with 0x80000000, then compare signed

  // Alternative approach: Use saturating add and compare
  // If vA + vB would overflow, then (vA + vB) < vA (wrapping)

  // Simpler: Use vpmaxud to detect overflow
  // If max(vA, vSum) == vA and vB != 0, there was a carry
  // But easier: compare vSum < vA using unsigned trick

  x86::Xmm vSignBit = newXMM();
  x86::Gp tempGp = newGP64();

  // Create sign bit mask: 0x80000000 for each dword
  COMP->mov(tempGp, 0x8000000080000000ULL);
  COMP->vmovq(vSignBit, tempGp);
  COMP->vpbroadcastq(vSignBit, vSignBit);

  // XOR with sign bit to convert unsigned to signed comparison
  x86::Xmm vSumSigned = newXMM();
  x86::Xmm vASigned = newXMM();
  COMP->vpxor(vSumSigned, vSum, vSignBit);
  COMP->vpxor(vASigned, vA, vSignBit);

  // Signed compare: vSumSigned < vASigned (means unsigned vSum < vA, i.e., carry)
  COMP->vpcmpgtd(vCmp, vASigned, vSumSigned); // vCmp = (vASigned > vSumSigned) ? 0xFFFFFFFF : 0

  // Create vector of ones (0x00000001 per dword)
  COMP->mov(tempGp, 0x0000000100000001ULL);
  COMP->vmovq(vOne, tempGp);
  COMP->vpbroadcastq(vOne, vOne);

  // Convert mask to 1 or 0: vD = vCmp & 1
  COMP->vpand(vD, vCmp, vOne);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Logical AND (x'1000 0404')
void PPCInterpreter::PPCInterpreterJIT_vand(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vpand(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Logical AND
void PPCInterpreter::PPCInterpreterJIT_vand128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));
  COMP->vpand(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Logical AND with Complement (x'1000 0444')
void PPCInterpreter::PPCInterpreterJIT_vandc(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vpandn(vD, vB, vA);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Logical AND with Complement
void PPCInterpreter::PPCInterpreterJIT_vandc128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));
  COMP->vpandn(vD, vB, vA);
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Subtract Floating Point (x'1000 004A')
void PPCInterpreter::PPCInterpreterJIT_vsubfp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(instr.va));
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Perform packed single-precision floating-point subtraction
  // vD = vA - vB (4 x float32)
  COMP->vsubps(vD, vA, vB);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Subtract Floating Point
void PPCInterpreter::PPCInterpreterJIT_vsubfp128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovaps(vB, VPRPtr(J_VMX128_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Perform packed single-precision floating-point subtraction
  // vD = vA - vB (4 x float32)
  COMP->vsubps(vD, vA, vB);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Maximum Floating Point (x'1000 040A')
void PPCInterpreter::PPCInterpreterJIT_vmaxfp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(instr.va));
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormals to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Compare: mask = (vA >= vB)  (predicate 13 = GE used elsewhere)
  x86::Xmm vCmp = newXMM();
  COMP->vcmpps(vCmp, vA, vB, 13); // GE

  // Blend: vD = (vCmp ? vA : vB)  -> select the larger float per-lane
  COMP->vblendvps(vD, vB, vA, vCmp);

  // NaN handling: if either input is NaN, return QNaN (quiet bit set).
  // Build NaN masks for inputs and combine.
  x86::Xmm vNaNA = newXMM();
  x86::Xmm vNaNB = newXMM();
  x86::Xmm vAnyNaN = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vQNaNA = newXMM();
  x86::Xmm vQNaNB = newXMM();
  x86::Xmm vQNaN = newXMM();
  x86::Gp tmp = newGP32();

  // Detect NaNs: unordered with itself => NaN (predicate 3 = UNORD)
  COMP->vcmpps(vNaNA, vA, vA, 3);
  COMP->vcmpps(vNaNB, vB, vB, 3);

  // any NaN = NaNA | NaNB
  COMP->vorps(vAnyNaN, vNaNA, vNaNB);

  // Prepare quiet-bit mask (bit 22)
  COMP->mov(tmp, 0x00400000);
  COMP->vmovd(vQNaNBit, tmp);
  COMP->vbroadcastss(vQNaNBit, vQNaNBit);

  // Create QNaN candidates from inputs by OR'ing the quiet bit into each
  COMP->vorps(vQNaNA, vA, vQNaNBit);
  COMP->vorps(vQNaNB, vB, vQNaNBit);

  // Prefer QNaN from vA where vA is NaN, otherwise from vB where vB is NaN.
  // temp = (vNaNA ? vQNaNA : vQNaNB)
  COMP->vblendvps(vQNaN, vQNaNB, vQNaNA, vNaNA);

  // If any NaN lane exists, replace result lane with QNaN
  COMP->vblendvps(vD, vD, vQNaN, vAnyNaN);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Maximum Floating Point
void PPCInterpreter::PPCInterpreterJIT_vmaxfp128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovaps(vB, VPRPtr(J_VMX128_VB128));

  // Flush denormals to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Compare: mask = (vA >= vB)  (predicate 13 = GE used elsewhere)
  x86::Xmm vCmp = newXMM();
  COMP->vcmpps(vCmp, vA, vB, 13); // GE

  // Blend: vD = (vCmp ? vA : vB)  -> select the larger float per-lane
  COMP->vblendvps(vD, vB, vA, vCmp);

  // NaN handling: if either input is NaN, return QNaN (quiet bit set).
  // Build NaN masks for inputs and combine.
  x86::Xmm vNaNA = newXMM();
  x86::Xmm vNaNB = newXMM();
  x86::Xmm vAnyNaN = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vQNaNA = newXMM();
  x86::Xmm vQNaNB = newXMM();
  x86::Xmm vQNaN = newXMM();
  x86::Gp tmp = newGP32();

  // Detect NaNs: unordered with itself => NaN (predicate 3 = UNORD)
  COMP->vcmpps(vNaNA, vA, vA, 3);
  COMP->vcmpps(vNaNB, vB, vB, 3);

  // any NaN = NaNA | NaNB
  COMP->vorps(vAnyNaN, vNaNA, vNaNB);

  // Prepare quiet-bit mask (bit 22)
  COMP->mov(tmp, 0x00400000);
  COMP->vmovd(vQNaNBit, tmp);
  COMP->vbroadcastss(vQNaNBit, vQNaNBit);

  // Create QNaN candidates from inputs by OR'ing the quiet bit into each
  COMP->vorps(vQNaNA, vA, vQNaNBit);
  COMP->vorps(vQNaNB, vB, vQNaNBit);

  // Prefer QNaN from vA where vA is NaN, otherwise from vB where vB is NaN.
  // temp = (vNaNA ? vQNaNA : vQNaNB)
  COMP->vblendvps(vQNaN, vQNaNB, vQNaNA, vNaNA);

  // If any NaN lane exists, replace result lane with QNaN
  COMP->vblendvps(vD, vD, vQNaN, vAnyNaN);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Minimum Floating Point (x'1000 044A')
void PPCInterpreter::PPCInterpreterJIT_vminfp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(instr.va));
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Compare: mask = (vA <= vB) -> select smaller per-lane
  x86::Xmm vCmp = newXMM();
  COMP->vcmpps(vCmp, vA, vB, 2); // LE

  // Blend: vD = (vCmp ? vA : vB)
  COMP->vblendvps(vD, vB, vA, vCmp);

  // NaN handling: if either input is NaN, return QNaN (quiet bit set).
  x86::Xmm vNaNA = newXMM();
  x86::Xmm vNaNB = newXMM();
  x86::Xmm vAnyNaN = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vQNaNA = newXMM();
  x86::Xmm vQNaNB = newXMM();
  x86::Xmm vQNaN = newXMM();
  x86::Gp tmp = newGP32();

  // Detect NaNs: unordered with itself => NaN (predicate 3 = UNORD)
  COMP->vcmpps(vNaNA, vA, vA, 3);
  COMP->vcmpps(vNaNB, vB, vB, 3);

  // any NaN = NaNA | NaNB
  COMP->vorps(vAnyNaN, vNaNA, vNaNB);

  // Prepare quiet-bit mask (bit 22)
  COMP->mov(tmp, 0x00400000);
  COMP->vmovd(vQNaNBit, tmp);
  COMP->vbroadcastss(vQNaNBit, vQNaNBit);

  // Create QNaN candidates from inputs by OR'ing the quiet bit into each
  COMP->vorps(vQNaNA, vA, vQNaNBit);
  COMP->vorps(vQNaNB, vB, vQNaNBit);

  // Prefer QNaN from vA where vA is NaN, otherwise from vB where vB is NaN.
  COMP->vblendvps(vQNaN, vQNaNB, vQNaNA, vNaNA);

  // If any NaN lane exists, replace result lane with QNaN
  COMP->vblendvps(vD, vD, vQNaN, vAnyNaN);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Minimum Floating Point
void PPCInterpreter::PPCInterpreterJIT_vminfp128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovaps(vB, VPRPtr(J_VMX128_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);

  // Compare: mask = (vA <= vB) -> select smaller per-lane
  x86::Xmm vCmp = newXMM();
  COMP->vcmpps(vCmp, vA, vB, 2); // LE

  // Blend: vD = (vCmp ? vA : vB)
  COMP->vblendvps(vD, vB, vA, vCmp);

  // NaN handling: if either input is NaN, return QNaN (quiet bit set).
  x86::Xmm vNaNA = newXMM();
  x86::Xmm vNaNB = newXMM();
  x86::Xmm vAnyNaN = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vQNaNA = newXMM();
  x86::Xmm vQNaNB = newXMM();
  x86::Xmm vQNaN = newXMM();
  x86::Gp tmp = newGP32();

  // Detect NaNs: unordered with itself => NaN (predicate 3 = UNORD)
  COMP->vcmpps(vNaNA, vA, vA, 3);
  COMP->vcmpps(vNaNB, vB, vB, 3);

  // any NaN = NaNA | NaNB
  COMP->vorps(vAnyNaN, vNaNA, vNaNB);

  // Prepare quiet-bit mask (bit 22)
  COMP->mov(tmp, 0x00400000);
  COMP->vmovd(vQNaNBit, tmp);
  COMP->vbroadcastss(vQNaNBit, vQNaNBit);

  // Create QNaN candidates from inputs by OR'ing the quiet bit into each
  COMP->vorps(vQNaNA, vA, vQNaNBit);
  COMP->vorps(vQNaNB, vB, vQNaNBit);

  // Prefer QNaN from vA where vA is NaN, otherwise from vB where vB is NaN.
  COMP->vblendvps(vQNaN, vQNaNB, vQNaNA, vNaNA);

  // If any NaN lane exists, replace result lane with QNaN
  COMP->vblendvps(vD, vD, vQNaN, vAnyNaN);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  COMP->vmovaps(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Round to Floating-Point Integer Nearest (x'1000 020A')
void PPCInterpreter::PPCInterpreterJIT_vrfin(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round to nearest integer (roundps with mode 0 = round to nearest)
  COMP->vroundps(vD, vB, 0x00);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Round to Floating-Point Integer Nearest
void PPCInterpreter::PPCInterpreterJIT_vrfin128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(J_VMX128_3_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round to nearest integer (roundps with mode 0 = round to nearest)
  COMP->vroundps(vD, vB, 0x00);

  COMP->vmovaps(VPRPtr(J_VMX128_3_VD128), vD);
}

// Vector Round to Floating-Point Integer toward Zero (x'1000 024A')
void PPCInterpreter::PPCInterpreterJIT_vrfiz(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round toward zero (truncate) (roundps with mode 3 = truncate)
  COMP->vroundps(vD, vB, 0x03);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Round to Floating-Point Integer toward Zero
void PPCInterpreter::PPCInterpreterJIT_vrfiz128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(J_VMX128_3_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round toward zero (truncate) (roundps with mode 3 = truncate)
  COMP->vroundps(vD, vB, 0x03);

  COMP->vmovaps(VPRPtr(J_VMX128_3_VD128), vD);
}

// Vector Round to Floating-Point Integer toward +Infinity (x'1000 028A')
void PPCInterpreter::PPCInterpreterJIT_vrfip(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round toward +infinity (ceil) (roundps with mode 2 = ceil)
  COMP->vroundps(vD, vB, 0x02);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Round to Floating-Point Integer toward +Infinity
void PPCInterpreter::PPCInterpreterJIT_vrfip128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(J_VMX128_3_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round toward +infinity (ceil) (roundps with mode 2 = ceil)
  COMP->vroundps(vD, vB, 0x02);

  COMP->vmovaps(VPRPtr(J_VMX128_3_VD128), vD);
}

// Vector Round to Floating-Point Integer toward -Infinity (x'1000 02CA')
void PPCInterpreter::PPCInterpreterJIT_vrfim(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round toward -infinity (floor) (roundps with mode 1 = floor)
  COMP->vroundps(vD, vB, 0x01);

  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 128 Round to Floating-Point Integer toward -Infinity
void PPCInterpreter::PPCInterpreterJIT_vrfim128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovaps(vB, VPRPtr(J_VMX128_3_VB128));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Round toward -infinity (floor) (roundps with mode 1 = floor)
  COMP->vroundps(vD, vB, 0x01);

  COMP->vmovaps(VPRPtr(J_VMX128_3_VD128), vD);
}

// Vector Logical OR (x'1000 0484')
void PPCInterpreter::PPCInterpreterJIT_vor(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vorps(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Logical OR
void PPCInterpreter::PPCInterpreterJIT_vor128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));
  COMP->vorps(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Logical NOR (x'1000 0504')
void PPCInterpreter::PPCInterpreterJIT_vnor(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vAllOnes = newXMM();
  x86::Gp tmp = newGP32();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vorps(vD, vA, vB);
  COMP->mov(tmp, 0xFFFFFFFFu);
  COMP->vmovd(vAllOnes, tmp);
  COMP->vpbroadcastd(vAllOnes, vAllOnes);
  COMP->vpxor(vD, vD, vAllOnes);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Logical NOR
void PPCInterpreter::PPCInterpreterJIT_vnor128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vAllOnes = newXMM();
  x86::Gp tmp = newGP32();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));
  COMP->vorps(vD, vA, vB);
  COMP->mov(tmp, 0xFFFFFFFFu);
  COMP->vmovd(vAllOnes, tmp);
  COMP->vpbroadcastd(vAllOnes, vAllOnes);
  COMP->vpxor(vD, vD, vAllOnes);
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Logical XOR (x'1000 04C4')
void PPCInterpreter::PPCInterpreterJIT_vxor(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vpxor(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Logical XOR
void PPCInterpreter::PPCInterpreterJIT_vxor128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));
  COMP->vpxor(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Conditional Select (x'1000 002A')
void PPCInterpreter::PPCInterpreterJIT_vsel(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // vD = (vA & ~vC) | (vB & vC)
  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vC = newXMM();
  x86::Xmm tA = newXMM(); // holds ~vC & vA
  x86::Xmm tB = newXMM(); // holds vB & vC
  x86::Xmm vD = newXMM();

  // Load vectors
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vmovdqa(vC, VPRPtr(instr.vc));

  // tA = ~vC & vA  -> vpandn dest, src1, src2  => dest = ~src1 & src2
  COMP->vpandn(tA, vC, vA);

  // tB = vB & vC
  COMP->vpand(tB, vB, vC);

  // vD = tA | tB
  COMP->vorps(vD, tA, tB);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Conditional Select
void PPCInterpreter::PPCInterpreterJIT_vsel128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // vD = (vA & ~vC) | (vB & vC)
  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vC = newXMM();
  x86::Xmm tA = newXMM(); // holds ~vC & vA
  x86::Xmm tB = newXMM(); // holds vB & vC
  x86::Xmm vD = newXMM();

  // Load vectors
  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));
  COMP->vmovdqa(vC, VPRPtr(J_VMX128_VD128));

  // tA = ~vC & vA  -> vpandn dest, src1, src2  => dest = ~src1 & src2
  COMP->vpandn(tA, vC, vA);

  // tB = vB & vC
  COMP->vpand(tB, vB, vC);

  // vD = tA | tB
  COMP->vorps(vD, tA, tB);

  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Splat Immediate Signed Byte (x'1000 030C')
void PPCInterpreter::PPCInterpreterJIT_vspltisb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // Get the immediate value, sign-extend to 8 bits
  s8 simm = static_cast<s8>(instr.vsimm);

  // Broadcast the value to all 16 bytes of the XMM register
  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  // Move the sign-extended value into a Gp register
  // Replicate it to all bytes of a 32-bit value, then broadcast
  u32 byteVal = static_cast<u8>(simm);

  COMP->mov(tmp, byteVal);
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastb(vD, vD);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Splat Immediate Signed Halfword (x'1000 034C')
void PPCInterpreter::PPCInterpreterJIT_vspltish(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  s16 simm = static_cast<s16>(instr.vsimm);
  u32 halfVal = static_cast<u16>(simm);

  COMP->mov(tmp, halfVal);
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastw(vD, vD);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Splat Immediate Signed Word (x'1000 038C')
void PPCInterpreter::PPCInterpreterJIT_vspltisw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  s32 simm = static_cast<s32>(instr.vsimm);
  u32 wordVal = static_cast<u32>(simm);

  COMP->mov(tmp, wordVal);
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastd(vD, vD);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Splat Immediate Signed Word
void PPCInterpreter::PPCInterpreterJIT_vspltisw128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  s32 simm = static_cast<s32>(J_VMX128_3_IMM);
  u32 wordVal = static_cast<u32>(simm);

  COMP->mov(tmp, wordVal);
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastd(vD, vD);
  COMP->vmovdqa(VPRPtr(J_VMX128_3_VD128), vD);
}

// Vector Splat Byte (x'1000 020C')
void PPCInterpreter::PPCInterpreterJIT_vspltb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // Map VMX byte index to x86/XMM byte index
  u32 idx = static_cast<u32>(instr.vuimm) & 0x0F;

  // Extract the selected byte from vB into tmp (gp reg) and broadcast it to all bytes
  COMP->vpextrb(tmp, vB, imm<u32>(VEC128_BYTE_VMX_TO_AVX(idx)));
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastb(vD, vD);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Splat Halfword (x'1000 024C')
void PPCInterpreter::PPCInterpreterJIT_vsplth(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  u32 idx = static_cast<u32>(instr.vuimm) & 0x7;

  // Extract the selected 16-bit halfword into tmp and broadcast it to all halfwords.
  COMP->vpextrw(tmp, vB, imm<u32>(VEC128_WORD_VMX_TO_AVX(idx)));
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastw(vD, vD);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Splat Word (x'1000 028C')
void PPCInterpreter::PPCInterpreterJIT_vspltw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // UIMM selects a 32-bit word element [0..3] (VMX ordering).
  u32 idx = static_cast<u32>(instr.vuimm) & 0x3;

  // Convert VMX index to x86/XMM element ordering and extract the dword.
  COMP->vpextrd(tmp, vB, imm<u32>(idx));

  // Move extracted dword into an XMM and broadcast to all dword lanes.
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastd(vD, vD);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Splat Word
void PPCInterpreter::PPCInterpreterJIT_vspltw128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmp = newGP32();

  COMP->vmovdqa(vB, VPRPtr(J_VMX128_3_VB128));

  // UIMM selects a 32-bit word element [0..3] (VMX ordering).
  u32 idx = static_cast<u32>(J_VMX128_3_IMM) & 0x3;

  // Convert VMX index to x86/XMM element ordering and extract the dword.
  COMP->vpextrd(tmp, vB, imm<u32>(idx));

  // Move extracted dword into an XMM and broadcast to all dword lanes.
  COMP->vmovd(vD, tmp);
  COMP->vpbroadcastd(vD, vD);

  COMP->vmovdqa(VPRPtr(J_VMX128_3_VD128), vD);
}

// Vector Shift Left Integer Byte (x'1000 0104')
// NOTE: Theres a faster path for emulating this instruction, but requires AVX-512 for using vpsllvm.
void PPCInterpreter::PPCInterpreterJIT_vslb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // Registers
  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  // Temps for unpack/shift/pack
  x86::Xmm zero = newXMM();
  x86::Xmm aLoW = newXMM();
  x86::Xmm aHiW = newXMM();
  x86::Xmm bLoW = newXMM();
  x86::Xmm bHiW = newXMM();
  x86::Xmm rLoW = newXMM();
  x86::Xmm rHiW = newXMM();
  x86::Xmm maskLowByte = newXMM();
  x86::Xmm tmpX = newXMM(); // temp XMM for constants/comparisons
  x86::Gp tmp = newGP64();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  COMP->vxorps(zero, zero, zero);

  // Expand bytes -> words (interleave with zero high byte)
  // aLoW  contains words for bytes 0..7, aHiW for bytes 8..15
  COMP->vpunpcklbw(aLoW, vA, zero);
  COMP->vpunpckhbw(aHiW, vA, zero);

  // same for counts
  COMP->vpunpcklbw(bLoW, vB, zero);
  COMP->vpunpckhbw(bHiW, vB, zero);

  // Mask counts to low 3 bits (count & 7) - create 64-bit pattern 4x words of 0x0007
  COMP->mov(tmp, 0x0007000700070007ULL);
  COMP->vmovq(tmpX, tmp);
  COMP->vpbroadcastq(tmpX, tmpX);
  COMP->vpand(bLoW, bLoW, tmpX);
  COMP->vpand(bHiW, bHiW, tmpX);

  // Prepare mask to keep low byte later
  COMP->mov(tmp, 0x00FF00FF00FF00FFULL);
  COMP->vmovq(maskLowByte, tmp);
  COMP->vpbroadcastq(maskLowByte, maskLowByte);

  // Initialize accumulators to zero
  COMP->vxorps(rLoW, rLoW, rLoW);
  COMP->vxorps(rHiW, rHiW, rHiW);

  for (int k = 0; k <= 7; ++k) {
    // Build word vector filled with k for comparison (64-bit pattern of four words)
    u64 pat = (u64)k | ((u64)k << 16) | ((u64)k << 32) | ((u64)k << 48);
    COMP->mov(tmp, pat);
    COMP->vmovq(tmpX, tmp);
    COMP->vpbroadcastq(tmpX, tmpX); // tmpX = [k,k,k,k,...] as words

    // Compare counts == k -> mask_k (word-wise)
    x86::Xmm maskLo = newXMM();
    x86::Xmm maskHi = newXMM();
    COMP->vpcmpeqw(maskLo, bLoW, tmpX);
    COMP->vpcmpeqw(maskHi, bHiW, tmpX);

    // Shift aLoW / aHiW by immediate k (word lanes)
    x86::Xmm shiftedLo = newXMM();
    x86::Xmm shiftedHi = newXMM();
    COMP->vpsllw(shiftedLo, aLoW, k); // immediate shift per-word
    COMP->vpsllw(shiftedHi, aHiW, k);

    // Mask to keep only low byte of each word (since we shifted words)
    COMP->vpand(shiftedLo, shiftedLo, maskLowByte);
    COMP->vpand(shiftedHi, shiftedHi, maskLowByte);

    // Select lanes where count == k and OR into accumulator
    COMP->vpand(shiftedLo, shiftedLo, maskLo);
    COMP->vpand(shiftedHi, shiftedHi, maskHi);
    COMP->vpor(rLoW, rLoW, shiftedLo);
    COMP->vpor(rHiW, rHiW, shiftedHi);
  }

  // Pack words back to bytes. Since high bytes are zeroed/truncated, vpackuswb will
  // place the low bytes into the result.
  COMP->vpackuswb(vD, rLoW, rHiW);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Shift Left Half Word (x'1000 0104')
// NOTE: Theres a faster path for emulating this instruction, but requires AVX-512 for using vpsllvm.
void PPCInterpreter::PPCInterpreterJIT_vslh(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm maskCount = newXMM();
  x86::Xmm tmpX = newXMM();
  x86::Xmm acc = newXMM();
  x86::Gp tmp = newGP64();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // Mask counts to low 4 bits (count & 0x0F)
  COMP->mov(tmp, 0x000F000F000F000FULL);
  COMP->vmovq(maskCount, tmp);
  COMP->vpbroadcastq(maskCount, maskCount);
  COMP->vpand(vB, vB, maskCount);

  // Initialize accumulator to zero
  COMP->vxorps(acc, acc, acc);

  // For each possible shift 0..15 compute vA << k and select lanes where vB == k
  for (int k = 0; k <= 15; ++k) {
    // Build word vector filled with k for comparison
    u64 pat = (u64)(u16)k | ((u64)(u16)k << 16) | ((u64)(u16)k << 32) | ((u64)(u16)k << 48);
    COMP->mov(tmp, pat);
    COMP->vmovq(tmpX, tmp);
    COMP->vpbroadcastq(tmpX, tmpX); // tmpX = [k,k,k,k,...] in words

    // Compare counts == k -> mask_k (word-wise)
    x86::Xmm mask = newXMM();
    COMP->vpcmpeqw(mask, vB, tmpX);

    // Shift vA by immediate k (per-word)
    x86::Xmm shifted = newXMM();
    COMP->vpsllw(shifted, vA, k); // logical left shift of 16-bit lanes

    // Select lanes where count == k and OR into accumulator
    COMP->vpand(shifted, shifted, mask);
    COMP->vpor(acc, acc, shifted);
  }

  // Store result to vD (VPR[vd])
  COMP->vmovdqa(VPRPtr(instr.vd), acc);
}

// Vector Shift Left Integer Word (x'1000 0184')
void PPCInterpreter::PPCInterpreterJIT_vslw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm maskCount = newXMM();
  x86::Gp tmp = newGP32();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // Mask counts to low 5 bits (count & 0x1F) per dword lane.
  COMP->mov(tmp, imm<u32>(0x1F));
  COMP->vmovd(maskCount, tmp);
  COMP->vpbroadcastd(maskCount, maskCount);
  COMP->vpand(vB, vB, maskCount);

  // Perform variable per-dword left shifts: vD = vA << vB
  // Uses AVX2 VPSLLVD (variable dword shifts)
  COMP->vpsllvd(vD, vA, vB);

  // Store result to vD (VPR[vd])
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Shift Left Integer Word
void PPCInterpreter::PPCInterpreterJIT_vslw128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm maskCount = newXMM();
  x86::Gp tmp = newGP32();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));

  // Mask counts to low 5 bits (count & 0x1F) per dword lane.
  COMP->mov(tmp, imm<u32>(0x1F));
  COMP->vmovd(maskCount, tmp);
  COMP->vpbroadcastd(maskCount, maskCount);
  COMP->vpand(vB, vB, maskCount);

  // Perform variable per-dword left shifts: vD = vA << vB
  // Uses AVX2 VPSLLVD (variable dword shifts)
  COMP->vpsllvd(vD, vA, vB);

  // Store result to vD (VPR[vd])
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Shift Right Integer Word (x'1000 0284')
void PPCInterpreter::PPCInterpreterJIT_vsrw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm maskCount = newXMM();
  x86::Gp tmp = newGP32();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // Mask counts to low 5 bits (count & 0x1F) per dword lane.
  COMP->mov(tmp, imm<u32>(0x1F));
  COMP->vmovd(maskCount, tmp);
  COMP->vpbroadcastd(maskCount, maskCount);
  COMP->vpand(vB, vB, maskCount);

  // Perform variable per-dword left shifts: vD = vA >> vB
  // Uses AVX2 VPSRLVD (variable dword shifts)
  COMP->vpsrlvd(vD, vA, vB);

  // Store result to vD (VPR[vd])
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Shift Right Integer Word
void PPCInterpreter::PPCInterpreterJIT_vsrw128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm maskCount = newXMM();
  x86::Gp tmp = newGP32();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));

  // Mask counts to low 5 bits (count & 0x1F) per dword lane.
  COMP->mov(tmp, imm<u32>(0x1F));
  COMP->vmovd(maskCount, tmp);
  COMP->vpbroadcastd(maskCount, maskCount);
  COMP->vpand(vB, vB, maskCount);

  // Perform variable per-dword left shifts: vD = vA >> vB
  // Uses AVX2 VPSRLVD (variable dword shifts)
  COMP->vpsrlvd(vD, vA, vB);

  // Store result to vD (VPR[vd])
  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Permute (x'1000 002B')
void PPCInterpreter::PPCInterpreterJIT_vperm(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vC = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm tmp0 = newXMM();
  x86::Xmm vAShuffled = newXMM();
  x86::Xmm vBShuffled = newXMM();

  // Allocate temp GP for loading 64-bit addresses
  x86::Gp tmpAddr = newGP64();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  COMP->vmovdqa(vC, VPRPtr(instr.vc));

  // Load address of XMMSwapWordMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, vC, x86::ptr(tmpAddr));

  // Load address of XMMPermuteByteMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tmpAddr));

  // Shuffle both source operands
  COMP->vpshufb(vAShuffled, vA, tmp0);
  COMP->vpshufb(vBShuffled, vB, tmp0);

  // Load address of XMMPermuteControl15
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tmpAddr));
  // Blend and store result
  COMP->vpblendvb(vD, vAShuffled, vBShuffled, vD);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Permute
void PPCInterpreter::PPCInterpreterJIT_vperm128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vC = newXMM();
  x86::Xmm vD = newXMM();

  x86::Xmm tmp0 = newXMM();
  x86::Xmm vAShuffled = newXMM();
  x86::Xmm vBShuffled = newXMM();

  // Allocate temp GP for loading 64-bit addresses
  x86::Gp tmpAddr = newGP64();

  // Load operands
  COMP->vmovdqa(vA, VPRPtr(J_VMX128_2_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_2_VB128));
  COMP->vmovdqa(vC, VPRPtr(J_VMX128_2_VC));

  // Load address of XMMSwapWordMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, vC, x86::ptr(tmpAddr));

  // Load address of XMMPermuteByteMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tmpAddr));

  // Shuffle both source operands
  COMP->vpshufb(vAShuffled, vA, tmp0);
  COMP->vpshufb(vBShuffled, vB, tmp0);

  // Load address of XMMPermuteControl15
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tmpAddr));
  // Blend and store result
  COMP->vpblendvb(vD, vAShuffled, vBShuffled, vD);
  COMP->vmovdqa(VPRPtr(J_VMX128_2_VD128), vD);
}

// Vector Shift Left Double by Octet Immediate (x'1000 002C')
void PPCInterpreter::PPCInterpreterJIT_vsldoi(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm tmp0 = newXMM();
  x86::Xmm vAShuffled = newXMM();
  x86::Xmm vBShuffled = newXMM();
  x86::Gp tmpAddr = newGP64();
  x86::Xmm tableValue = newXMM();

  // Shift amount
  u8 sh = instr.vc & 0xF;

  // Fast paths
  if (!sh) {
    // No shift, just copy vA to vD
    COMP->vmovdqa(vD, VPRPtr(instr.va));
    COMP->vmovdqa(VPRPtr(instr.vd), vD);
    return;
  }
  else if (sh == 16) {
    // Full shift, copy vB to vD
    COMP->vmovdqa(vD, VPRPtr(instr.vb));
    COMP->vmovdqa(VPRPtr(instr.vd), vD);
    return;
  }

  // Get the shift value from the table
  COMP->mov(tmpAddr, (uintptr_t)&vsldoiTable[sh]);
  COMP->vmovdqa(tableValue, x86::ptr(tmpAddr));
  // Load operands
  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));
  // Load address of XMMSwapWordMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, tableValue, x86::ptr(tmpAddr));
  // Load address of XMMPermuteByteMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tmpAddr));
  // Shuffle both source operands
  COMP->vpshufb(vAShuffled, vA, tmp0);
  COMP->vpshufb(vBShuffled, vB, tmp0);
  // Load address of XMMPermuteControl15
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tmpAddr));
  // Blend and store result
  COMP->vpblendvb(vD, vAShuffled, vBShuffled, vD);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Shift Left Double by Octet Immediate
void PPCInterpreter::PPCInterpreterJIT_vsldoi128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm tmp0 = newXMM();
  x86::Xmm vAShuffled = newXMM();
  x86::Xmm vBShuffled = newXMM();
  x86::Gp tmpAddr = newGP64();
  x86::Xmm tableValue = newXMM();

  // Shift amount
  u8 sh = J_VMX128_5_SH;

  // Fast paths
  if (!sh) {
    // No shift, just copy vA to vD
    COMP->vmovdqa(vD, VPRPtr(J_VMX128_5_VA128));
    COMP->vmovdqa(VPRPtr(J_VMX128_5_VD128), vD);
    return;
  }
  else if (sh == 16) {
    // Full shift, copy vB to vD
    COMP->vmovdqa(vD, VPRPtr(J_VMX128_5_VB128));
    COMP->vmovdqa(VPRPtr(J_VMX128_5_VD128), vD);
    return;
  }

  // Get the shift value from the table
  COMP->mov(tmpAddr, (uintptr_t)&vsldoiTable[sh]);
  COMP->vmovdqa(tableValue, x86::ptr(tmpAddr));
  // Load operands
  COMP->vmovdqa(vA, VPRPtr(J_VMX128_5_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_5_VB128));
  // Load address of XMMSwapWordMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, tableValue, x86::ptr(tmpAddr));
  // Load address of XMMPermuteByteMask
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tmpAddr));
  // Shuffle both source operands
  COMP->vpshufb(vAShuffled, vA, tmp0);
  COMP->vpshufb(vBShuffled, vB, tmp0);
  // Load address of XMMPermuteControl15
  COMP->mov(tmpAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tmpAddr));
  // Blend and store result
  COMP->vpblendvb(vD, vAShuffled, vBShuffled, vD);
  COMP->vmovdqa(VPRPtr(J_VMX128_5_VD128), vD);
}

// Vector Pack Unsigned Word Unsigned Modulo (x'1000 004E')
void PPCInterpreter::PPCInterpreterJIT_vpkuwum(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmpGp = newGP64();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // Load address of shuffle mask that extracts low 16 bits of each dword
  COMP->mov(tmpGp, (uintptr_t)&vpkuwumShuffleMask);

  // Shuffle vA: extract low halfwords to low 64 bits
  COMP->vpshufb(vA, vA, x86::ptr(tmpGp));

  // Shuffle vB: extract low halfwords to low 64 bits  
  COMP->vpshufb(vB, vB, x86::ptr(tmpGp));

  // Combine: vA has packed halfwords in low 64 bits, vB has packed halfwords in low 64 bits
  // Use punpcklqdq to combine low 64 bits of both into one register
  COMP->vpunpcklqdq(vD, vA, vB);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Pack Unsigned Word Unsigned Modulo
void PPCInterpreter::PPCInterpreterJIT_vpkuwum128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmpGp = newGP64();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));

  // Load address of shuffle mask that extracts low 16 bits of each dword
  COMP->mov(tmpGp, (uintptr_t)&vpkuwumShuffleMask);

  // Shuffle vA: extract low halfwords to low 64 bits
  COMP->vpshufb(vA, vA, x86::ptr(tmpGp));

  // Shuffle vB: extract low halfwords to low 64 bits  
  COMP->vpshufb(vB, vB, x86::ptr(tmpGp));

  // Combine: Use punpcklqdq to combine low 64 bits of both
  COMP->vpunpcklqdq(vD, vA, vB);

  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Pack Signed Word Signed Saturate (x'1000 01CE')
void PPCInterpreter::PPCInterpreterJIT_vpkswss(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmpGp = newGP64();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpackssdw: Pack signed dwords to signed words with saturation
  COMP->vpackssdw(vD, vA, vB);

  // Shuffle to byteswap lanes
  COMP->mov(tmpGp, (uintptr_t)&vpkswssShuffleMask);
  COMP->vpshufb(vD, vD, x86::ptr(tmpGp));

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Pack Signed Word Signed Saturate
void PPCInterpreter::PPCInterpreterJIT_vpkswss128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp tmpGp = newGP64();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));

  // vpackssdw: Pack signed dwords to signed words with saturation
  COMP->vpackssdw(vD, vA, vB);

  // Shuffle to reorder lanes
  COMP->mov(tmpGp, (uintptr_t)&vpkswssShuffleMask);
  COMP->vpshufb(vD, vD, x86::ptr(tmpGp));

  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Subtract Unsigned Halfword Saturate (x'1000 0640')
void PPCInterpreter::PPCInterpreterJIT_vsubuhs(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpsubusw: packed subtract unsigned saturated words (16-bit)
  // If result would be negative, it saturates to 0
  COMP->vpsubusw(vD, vA, vB);

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

//*****************************************************************************
// Vector Merge
//*****************************************************************************

// Vector Merge High Word (x'1000 008C')
void PPCInterpreter::PPCInterpreterJIT_vmrghw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpunpckldq: Interleave low dwords from vA and vB
  COMP->vpunpckldq(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Merge High Word
void PPCInterpreter::PPCInterpreterJIT_vmrghw128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));

  COMP->vpunpckldq(vD, vA, vB);

  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Merge High Halfword (x'1000 004C')
void PPCInterpreter::PPCInterpreterJIT_vmrghh(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpunpcklwd with swapped operands for big-endian
  COMP->vpunpcklwd(vD, vB, vA);

  // Swap dword pairs: 0<->1 and 2<->3
  // vpshufd imm8 = 0b10110001 = 0xB1 swaps pairs
  COMP->vpshufd(vD, vD, imm(0xB1));

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Merge High Byte (x'1000 000C')
void PPCInterpreter::PPCInterpreterJIT_vmrghb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpunpcklbw with swapped operands for big-endian
  COMP->vpunpcklbw(vD, vB, vA);
  // Swap dword pairs: 0<->1 and 2<->3
  // vpshufd imm8 = 0b10110001 = 0xB1 swaps pairs
  COMP->vpshufd(vD, vD, imm(0xB1));
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Merge Low Word (x'1000 018C')
void PPCInterpreter::PPCInterpreterJIT_vmrglw(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpunpckhdq: Interleave high dwords from vA and vB
  COMP->vpunpckhdq(vD, vA, vB);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector 128 Merge Low Word
void PPCInterpreter::PPCInterpreterJIT_vmrglw128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(J_VMX128_VA128));
  COMP->vmovdqa(vB, VPRPtr(J_VMX128_VB128));

  COMP->vpunpckhdq(vD, vA, vB);

  COMP->vmovdqa(VPRPtr(J_VMX128_VD128), vD);
}

// Vector Merge Low Halfword (x'1000 014C')
void PPCInterpreter::PPCInterpreterJIT_vmrglh(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpunpckhwd with swapped operands for big-endian
  COMP->vpunpckhwd(vD, vB, vA);

  // Swap dword pairs: 0<->1 and 2<->3
  // vpshufd imm8 = 0b10110001 = 0xB1 swaps pairs
  COMP->vpshufd(vD, vD, imm(0xB1));

  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Vector Merge Low Byte (x'1000 010C')
void PPCInterpreter::PPCInterpreterJIT_vmrglb(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  J_checkVXUEnabled(b);

  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  COMP->vmovdqa(vA, VPRPtr(instr.va));
  COMP->vmovdqa(vB, VPRPtr(instr.vb));

  // vpunpckhbw with swapped operands for big-endian
  COMP->vpunpckhbw(vD, vB, vA);
  // Swap dword pairs: 0<->1 and 2<->3
  // vpshufd imm8 = 0b10110001 = 0xB1 swaps pairs
  COMP->vpshufd(vD, vD, imm(0xB1));
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

//*****************************************************************************
// Load Vector
//*****************************************************************************

// NOTE: lvexx instructions behave like lvx.

// Load Vector Indexed (x'7C00 00CE')
void PPCInterpreter::PPCInterpreterJIT_lvx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Xmm vD = newXMM();
  x86::Gp exceptReg = newGP16();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);

  // Got our translated address, pull the vector onto vD
  COMP->vmovdqa(vD, x86::ptr(EA));
  // Byteswap the loaded vector
  COMP->mov(EA, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vD, vD, x86::ptr(EA));
  // Move it onto destination VPR
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
  COMP->bind(endLabel);
}

// Load Vector 128 Indexed
void PPCInterpreter::PPCInterpreterJIT_lvx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Xmm vD = newXMM();
  x86::Gp exceptReg = newGP16();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);

  // Got our translated address, pull the vector onto vD
  COMP->vmovdqa(vD, x86::ptr(EA));
  // Byteswap the loaded vector
  COMP->mov(EA, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vD, vD, x86::ptr(EA));
  // Move it onto destination VPR
  COMP->vmovdqa(VPRPtr(J_VMX128_1_VD128), vD);
  COMP->bind(endLabel);
}

// Load Vector Left Indexed (x'7C00 040E')
void PPCInterpreter::PPCInterpreterJIT_lvlx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp eb = newGP64();
  x86::Gp tableAddr = newGP64();
  x86::Xmm value1 = newXMM();
  x86::Xmm value2 = newXMM();
  COMP->vxorps(value2, value2, value2);
  x86::Xmm control = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp exceptReg = newGP16();
  x86::Xmm tmp0 = newXMM();
  x86::Xmm vAShuffled = newXMM();
  x86::Xmm vBShuffled = newXMM();
  Label endLabel = COMP->newLabel();


  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(eb, EA);
  COMP->and_(eb, imm(0xF));
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);

  // Got our translated address, pull the vector onto value1
  COMP->vmovdqa(value1, x86::ptr(EA));
  // Byteswap the loaded vector
  COMP->mov(EA, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(value1, value1, x86::ptr(EA));

  // Load control from vector shift left using eb as source address

  // Each table entry is 16 bytes, so multiply index by 16
  COMP->shl(eb, imm(4));
  // Load address of the permutation table
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftLeftTable);
  // Load the permutation vector from the table: vD = loadVectorShiftLeftTable[EA & 0xF]
  // Using [tableAddr + EA]
  COMP->vmovdqa(control, x86::ptr(tableAddr, eb));

  // Permute
  // Load address of XMMSwapWordMask
  COMP->mov(tableAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, control, x86::ptr(tableAddr));

  // Load address of XMMPermuteByteMask
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tableAddr));

  // Shuffle both source operands
  COMP->vpshufb(vAShuffled, value1, tmp0);
  COMP->vpshufb(vBShuffled, value2, tmp0);

  // Load address of XMMPermuteControl15
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tableAddr));
  // Blend and store result
  COMP->vpblendvb(vD, vAShuffled, vBShuffled, vD);

  // Store result
  COMP->vmovdqa(VPRPtr(instr.vd), vD);

  COMP->bind(endLabel);
}

// Load Vector Left Indexed 128
void PPCInterpreter::PPCInterpreterJIT_lvlx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp eb = newGP64();
  x86::Gp tableAddr = newGP64();
  x86::Xmm value1 = newXMM();
  x86::Xmm value2 = newXMM();
  COMP->vxorps(value2, value2, value2);
  x86::Xmm control = newXMM();
  x86::Xmm vD = newXMM();
  x86::Gp exceptReg = newGP16();
  x86::Xmm tmp0 = newXMM();
  x86::Xmm vAShuffled = newXMM();
  x86::Xmm vBShuffled = newXMM();
  Label endLabel = COMP->newLabel();


  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  COMP->mov(eb, EA);
  COMP->and_(eb, imm(0xF));
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);

  // Got our translated address, pull the vector onto value1
  COMP->vmovdqa(value1, x86::ptr(EA));
  // Byteswap the loaded vector
  COMP->mov(EA, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(value1, value1, x86::ptr(EA));

  // Load control from vector shift left using eb as source address

  // Each table entry is 16 bytes, so multiply index by 16
  COMP->shl(eb, imm(4));
  // Load address of the permutation table
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftLeftTable);
  // Load the permutation vector from the table: vD = loadVectorShiftLeftTable[EA & 0xF]
  // Using [tableAddr + EA]
  COMP->vmovdqa(control, x86::ptr(tableAddr, eb));

  // Permute
  // Load address of XMMSwapWordMask
  COMP->mov(tableAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, control, x86::ptr(tableAddr));

  // Load address of XMMPermuteByteMask
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tableAddr));

  // Shuffle both source operands
  COMP->vpshufb(vAShuffled, value1, tmp0);
  COMP->vpshufb(vBShuffled, value2, tmp0);

  // Load address of XMMPermuteControl15
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tableAddr));
  // Blend and store result
  COMP->vpblendvb(vD, vAShuffled, vBShuffled, vD);

  // Store result
  COMP->vmovdqa(VPRPtr(J_VMX128_1_VD128), vD);

  COMP->bind(endLabel);
}

// Load Vector Right Indexed (x'7C00 044E')
void PPCInterpreter::PPCInterpreterJIT_lvrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // EA
  x86::Gp EA = newGP64();
  // EB
  x86::Gp eb = newGP64();
  // Temp reg for addresses to static tables
  x86::Gp tableAddr = newGP64();
  // Regs for Permute
  x86::Xmm value1 = newXMM();
  x86::Xmm value2 = newXMM();
  // Zero out the value2 vector used in the permute
  COMP->vxorps(value2, value2, value2);
  x86::Xmm control = newXMM();
  x86::Xmm operand0 = newXMM();
  x86::Xmm operand1 = newXMM();
  x86::Xmm tmp0 = newXMM();
  // Destination reg
  x86::Xmm vD = newXMM();
  // Exception reg
  x86::Gp exceptReg = newGP16();

  // Labels
  Label endLabel = COMP->newLabel();
  Label ebNotZero = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->mov(eb, EA);
  // Extract EB
  COMP->and_(eb, imm(0xF));
  // Check if EB = 0, we must clear the dest vector if so and exit.
  COMP->test(eb, eb);
  COMP->jnz(ebNotZero);
  // It's zero, clear it and exit.
  COMP->vxorps(vD, vD, vD);
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
  COMP->jmp(endLabel);
  COMP->bind(ebNotZero);
  // Align EA
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);

  // Get vector data from memory
  COMP->vmovdqa(value1, x86::ptr(EA));
  // Byteswap the data
  COMP->mov(EA, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(value1, value1, x86::ptr(EA));
  // Generate control vec using eb as indec to the LVSL table 
  COMP->shl(eb, imm(4));
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftLeftTable);
  COMP->vmovdqa(control, x86::ptr(tableAddr, eb));
  // Permute using control, zero fileld vec and byteswapped data.
  COMP->mov(tableAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, control, x86::ptr(tableAddr));
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tableAddr));
  COMP->vpshufb(operand0, value2, tmp0);
  COMP->vpshufb(operand1, value1, tmp0);
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tableAddr));
  COMP->vpblendvb(vD, operand0, operand1, vD);
  // Store result
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
  // Exit
  COMP->bind(endLabel);
}

// Load Vector 128 Right Indexed
void PPCInterpreter::PPCInterpreterJIT_lvrx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // EA
  x86::Gp EA = newGP64();
  // EB
  x86::Gp eb = newGP64();
  // Temp reg for addresses to static tables
  x86::Gp tableAddr = newGP64();
  // Regs for Permute
  x86::Xmm value1 = newXMM();
  x86::Xmm value2 = newXMM();
  // Zero out the value2 vector used in the permute
  COMP->vxorps(value2, value2, value2);
  x86::Xmm control = newXMM();
  x86::Xmm operand0 = newXMM();
  x86::Xmm operand1 = newXMM();
  x86::Xmm tmp0 = newXMM();
  // Destination reg
  x86::Xmm vD = newXMM();
  // Exception reg
  x86::Gp exceptReg = newGP16();

  // Labels
  Label endLabel = COMP->newLabel();
  Label ebNotZero = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  COMP->mov(eb, EA);
  // Extract EB
  COMP->and_(eb, imm(0xF));
  // Check if EB = 0, we must clear the dest vector if so and exit.
  COMP->test(eb, eb);
  COMP->jnz(ebNotZero);
  // It's zero, clear it and exit.
  COMP->vxorps(vD, vD, vD);
  COMP->vmovdqa(VPRPtr(J_VMX128_1_VD128), vD);
  COMP->jmp(endLabel);
  COMP->bind(ebNotZero);
  // Align EA
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for exceptions DStor/DSeg and return if found.
  COMP->mov(exceptReg, EXPtr());
  COMP->and_(exceptReg, imm<u16>(0xC));
  COMP->test(exceptReg, exceptReg);
  COMP->jnz(endLabel);

  // Get vector data from memory
  COMP->vmovdqa(value1, x86::ptr(EA));
  // Byteswap the data
  COMP->mov(EA, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(value1, value1, x86::ptr(EA));
  // Generate control vec using eb as indec to the LVSL table 
  COMP->shl(eb, imm(4));
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftLeftTable);
  COMP->vmovdqa(control, x86::ptr(tableAddr, eb));
  // Permute using control, zero fileld vec and byteswapped data.
  COMP->mov(tableAddr, (uintptr_t)&XMMSwapWordMask);
  COMP->vxorps(tmp0, control, x86::ptr(tableAddr));
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteByteMask);
  COMP->vpand(tmp0, tmp0, x86::ptr(tableAddr));
  COMP->vpshufb(operand0, value2, tmp0);
  COMP->vpshufb(operand1, value1, tmp0);
  COMP->mov(tableAddr, (uintptr_t)&XMMPermuteControl15);
  COMP->vpcmpgtb(vD, tmp0, x86::ptr(tableAddr));
  COMP->vpblendvb(vD, operand0, operand1, vD);
  // Store result
  COMP->vmovdqa(VPRPtr(J_VMX128_1_VD128), vD);
  // Exit
  COMP->bind(endLabel);
}

// Load Vector for Shift Left (x'1F80 0006')
void PPCInterpreter::PPCInterpreterJIT_lvsl(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Xmm vD = newXMM();
  x86::Gp tableAddr = newGP64();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Mask EA to low 4 bits to get the shift amount (0..15)
  COMP->and_(EA, imm(0xF));
  // Each table entry is 16 bytes, so multiply index by 16
  COMP->shl(EA, imm(4));
  // Load address of the permutation table
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftLeftTable);
  // Load the permutation vector from the table: vD = loadVectorShiftLeftTable[EA & 0xF]
  // Using [tableAddr + EA]
  COMP->vmovdqa(vD, x86::ptr(tableAddr, EA));
  // Store result to vD
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Load Vector 128 for Shift Left
void PPCInterpreter::PPCInterpreterJIT_lvsl128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Xmm vD = newXMM();
  x86::Gp tableAddr = newGP64();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  // Mask EA to low 4 bits to get the shift amount (0..15)
  COMP->and_(EA, imm(0xF));
  // Each table entry is 16 bytes, so multiply index by 16
  COMP->shl(EA, imm(4));
  // Load address of the permutation table
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftLeftTable);
  // Load the permutation vector from the table: vD = loadVectorShiftLeftTable[EA & 0xF]
  // Using [tableAddr + EA]
  COMP->vmovdqa(vD, x86::ptr(tableAddr, EA));
  // Store result to vD
  COMP->vmovdqa(VPRPtr(J_VMX128_1_VD128), vD);
}

// Load Vector for Shift Right (x'7C00 004C')
void PPCInterpreter::PPCInterpreterJIT_lvsr(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Xmm vD = newXMM();
  x86::Gp tableAddr = newGP64();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Mask EA to low 4 bits to get the shift amount (0..15)
  COMP->and_(EA, imm(0xF));
  // Each table entry is 16 bytes, so multiply index by 16
  COMP->shl(EA, imm(4));
  // Load address of the permutation table
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftRightTable);
  // Load the permutation vector from the table: vD = loadVectorShiftRightTable[EA & 0xF]
  // Using [tableAddr + EA]
  COMP->vmovdqa(vD, x86::ptr(tableAddr, EA));
  // Store result to vD
  COMP->vmovdqa(VPRPtr(instr.vd), vD);
}

// Load Vector 128 for Shift Right
void PPCInterpreter::PPCInterpreterJIT_lvsr128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Xmm vD = newXMM();
  x86::Gp tableAddr = newGP64();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  // Mask EA to low 4 bits to get the shift amount (0..15)
  COMP->and_(EA, imm(0xF));
  // Each table entry is 16 bytes, so multiply index by 16
  COMP->shl(EA, imm(4));
  // Load address of the permutation table
  COMP->mov(tableAddr, (uintptr_t)&loadVectorShiftRightTable);
  // Load the permutation vector from the table: vD = loadVectorShiftRightTable[EA & 0xF]
  // Using [tableAddr + EA]
  COMP->vmovdqa(vD, x86::ptr(tableAddr, EA));
  // Store result to vD
  COMP->vmovdqa(VPRPtr(J_VMX128_1_VD128), vD);
}

//*****************************************************************************
// Store Vector
//*****************************************************************************

// Store Vector Indexed (x'7C00 01CE')
void PPCInterpreter::PPCInterpreterJIT_stvx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp tmpAddress = newGP64();
  x86::Xmm vD = newXMM();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);
  // Move it onto destination VPR
  COMP->vmovdqa(vD, VPRPtr(instr.vd));
  // Byteswap the vector
  COMP->mov(tmpAddress, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vD, vD, x86::ptr(tmpAddress));
  // Got our translated address, pull the vector onto vD
  COMP->vmovdqa(x86::ptr(EA), vD);
  COMP->bind(endLabel);
}

// Store Vector 128 Indexed 
void PPCInterpreter::PPCInterpreterJIT_stvx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp tmpAddress = newGP64();
  x86::Xmm vD = newXMM();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  COMP->and_(EA, imm(~0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);
  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);
  // Move it onto destination VPR
  COMP->vmovdqa(vD, VPRPtr(J_VMX128_1_VD128));
  // Byteswap the vector
  COMP->mov(tmpAddress, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vD, vD, x86::ptr(tmpAddress));
  // Got our translated address, pull the vector onto vD
  COMP->vmovdqa(x86::ptr(EA), vD);
  COMP->bind(endLabel);
}

// Store Vector Element Byte Indexed (x'7C00 010E')
void PPCInterpreter::PPCInterpreterJIT_stvebx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp element = newGP64();
  x86::Xmm vS = newXMM();
  x86::Gp outByte = newGP32();
  Label endLabel = COMP->newLabel();
  Label case0 = COMP->newLabel();
  Label case1 = COMP->newLabel();
  Label case2 = COMP->newLabel();
  Label case3 = COMP->newLabel();
  Label case4 = COMP->newLabel();
  Label case5 = COMP->newLabel();
  Label case6 = COMP->newLabel();
  Label case7 = COMP->newLabel();
  Label case8 = COMP->newLabel();
  Label case9 = COMP->newLabel();
  Label case10 = COMP->newLabel();
  Label case11 = COMP->newLabel();
  Label case12 = COMP->newLabel();
  Label case13 = COMP->newLabel();
  Label case14 = COMP->newLabel();
  Label case15 = COMP->newLabel();
  Label storeLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Calculate element index: element = EA & 0xF
  COMP->mov(element, EA);
  COMP->and_(element, imm(0xF));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector register
  COMP->vmovdqa(vS, VPRPtr(instr.vd));
  // Use direct extraction based on element index (0-15)
  COMP->cmp(element, imm(0));
  COMP->je(case0);
  COMP->cmp(element, imm(1));
  COMP->je(case1);
  COMP->cmp(element, imm(2));
  COMP->je(case2);
  COMP->cmp(element, imm(3));
  COMP->je(case3);
  COMP->cmp(element, imm(4));
  COMP->je(case4);
  COMP->cmp(element, imm(5));
  COMP->je(case5);
  COMP->cmp(element, imm(6));
  COMP->je(case6);
  COMP->cmp(element, imm(7));
  COMP->je(case7);
  COMP->cmp(element, imm(8));
  COMP->je(case8);
  COMP->cmp(element, imm(9));
  COMP->je(case9);
  COMP->cmp(element, imm(10));
  COMP->je(case10);
  COMP->cmp(element, imm(11));
  COMP->je(case11);
  COMP->cmp(element, imm(12));
  COMP->je(case12);
  COMP->cmp(element, imm(13));
  COMP->je(case13);
  COMP->cmp(element, imm(14));
  COMP->je(case14);
  COMP->jmp(case15);
  // Element 0
  COMP->bind(case0);
  COMP->vpextrb(outByte, vS, imm(0));
  COMP->jmp(storeLabel);
  // Element 1
  COMP->bind(case1);
  COMP->vpextrb(outByte, vS, imm(1));
  COMP->jmp(storeLabel);
  // Element 2
  COMP->bind(case2);
  COMP->vpextrb(outByte, vS, imm(2));
  COMP->jmp(storeLabel);
  // Element 3
  COMP->bind(case3);
  COMP->vpextrb(outByte, vS, imm(3));
  COMP->jmp(storeLabel);
  // Element 4
  COMP->bind(case4);
  COMP->vpextrb(outByte, vS, imm(4));
  COMP->jmp(storeLabel);
  // Element 5
  COMP->bind(case5);
  COMP->vpextrb(outByte, vS, imm(5));
  COMP->jmp(storeLabel);
  // Element 6
  COMP->bind(case6);
  COMP->vpextrb(outByte, vS, imm(6));
  COMP->jmp(storeLabel);
  // Element 7
  COMP->bind(case7);
  COMP->vpextrb(outByte, vS, imm(7));
  COMP->jmp(storeLabel);
  // Element 8
  COMP->bind(case8);
  COMP->vpextrb(outByte, vS, imm(8));
  COMP->jmp(storeLabel);
  // Element 9
  COMP->bind(case9);
  COMP->vpextrb(outByte, vS, imm(9));
  COMP->jmp(storeLabel);
  // Element 10
  COMP->bind(case10);
  COMP->vpextrb(outByte, vS, imm(10));
  COMP->jmp(storeLabel);
  // Element 11
  COMP->bind(case11);
  COMP->vpextrb(outByte, vS, imm(11));
  COMP->jmp(storeLabel);
  // Element 12
  COMP->bind(case12);
  COMP->vpextrb(outByte, vS, imm(12));
  COMP->jmp(storeLabel);
  // Element 13
  COMP->bind(case13);
  COMP->vpextrb(outByte, vS, imm(13));
  COMP->jmp(storeLabel);
  // Element 14
  COMP->bind(case14);
  COMP->vpextrb(outByte, vS, imm(14));
  COMP->jmp(storeLabel);
  // Element 15
  COMP->bind(case15);
  COMP->vpextrb(outByte, vS, imm(15));
  // Store byte directly (no byte swap needed for single byte)
  COMP->bind(storeLabel);
  COMP->mov(x86::byte_ptr(EA), outByte.r8());

  COMP->bind(endLabel);
}

// Store Vector Element Halfword Indexed (x'7C00 014E')
void PPCInterpreter::PPCInterpreterJIT_stvehx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp element = newGP64();
  x86::Xmm vS = newXMM();
  x86::Gp outWord = newGP32();
  Label endLabel = COMP->newLabel();
  Label case0 = COMP->newLabel();
  Label case1 = COMP->newLabel();
  Label case2 = COMP->newLabel();
  Label case3 = COMP->newLabel();
  Label case4 = COMP->newLabel();
  Label case5 = COMP->newLabel();
  Label case6 = COMP->newLabel();
  Label case7 = COMP->newLabel();
  Label storeLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Calculate element index before aligning EA: element = (EA >> 1) & 0x7
  COMP->mov(element, EA);
  COMP->shr(element, imm(1));
  COMP->and_(element, imm(0x7));
  // Align EA to 2-byte boundary
  COMP->and_(EA, imm(~0x1));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector register
  COMP->vmovdqa(vS, VPRPtr(instr.vd));
  // Use direct extraction based on element index (0-7)
  COMP->cmp(element, imm(0));
  COMP->je(case0);
  COMP->cmp(element, imm(1));
  COMP->je(case1);
  COMP->cmp(element, imm(2));
  COMP->je(case2);
  COMP->cmp(element, imm(3));
  COMP->je(case3);
  COMP->cmp(element, imm(4));
  COMP->je(case4);
  COMP->cmp(element, imm(5));
  COMP->je(case5);
  COMP->cmp(element, imm(6));
  COMP->je(case6);
  COMP->jmp(case7);
  // Element 0
  COMP->bind(case0);
  COMP->vpextrw(outWord, vS, imm(0));
  COMP->jmp(storeLabel);
  // Element 1
  COMP->bind(case1);
  COMP->vpextrw(outWord, vS, imm(1));
  COMP->jmp(storeLabel);
  // Element 2
  COMP->bind(case2);
  COMP->vpextrw(outWord, vS, imm(2));
  COMP->jmp(storeLabel);
  // Element 3
  COMP->bind(case3);
  COMP->vpextrw(outWord, vS, imm(3));
  COMP->jmp(storeLabel);
  // Element 4
  COMP->bind(case4);
  COMP->vpextrw(outWord, vS, imm(4));
  COMP->jmp(storeLabel);
  // Element 5
  COMP->bind(case5);
  COMP->vpextrw(outWord, vS, imm(5));
  COMP->jmp(storeLabel);
  // Element 6
  COMP->bind(case6);
  COMP->vpextrw(outWord, vS, imm(6));
  COMP->jmp(storeLabel);
  // Element 7
  COMP->bind(case7);
  COMP->vpextrw(outWord, vS, imm(7));
  // Store with byteswap using movbe (16-bit)
  COMP->bind(storeLabel);
  COMP->movbe(x86::word_ptr(EA), outWord.r16());

  COMP->bind(endLabel);
}

// Store Vector Element Word Indexed (x'7C00 018E')
void PPCInterpreter::PPCInterpreterJIT_stvewx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp element = newGP64();
  x86::Xmm vS = newXMM();
  x86::Gp outDword = newGP32();
  Label endLabel = COMP->newLabel();
  Label case0 = COMP->newLabel();
  Label case1 = COMP->newLabel();
  Label case2 = COMP->newLabel();
  Label case3 = COMP->newLabel();
  Label storeLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));
  // Calculate element index before aligning EA: element = (EA >> 2) & 0x3
  COMP->mov(element, EA);
  COMP->shr(element, imm(2));
  COMP->and_(element, imm(0x3));
  // Align EA to 4-byte boundary
  COMP->and_(EA, imm(~0x3));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector register
  COMP->vmovdqa(vS, VPRPtr(instr.vd));
  // Use direct extraction based on element index (0-3)
  // NOTE: This is faster than using a lookup table, and modern CPU's should handle it very well.
  COMP->cmp(element, imm(0));
  COMP->je(case0);
  COMP->cmp(element, imm(1));
  COMP->je(case1);
  COMP->cmp(element, imm(2));
  COMP->je(case2);
  COMP->jmp(case3);
  // Element 0
  COMP->bind(case0);
  COMP->vpextrd(outDword, vS, imm(0));
  COMP->jmp(storeLabel);
  // Element 1
  COMP->bind(case1);
  COMP->vpextrd(outDword, vS, imm(1));
  COMP->jmp(storeLabel);
  // Element 2
  COMP->bind(case2);
  COMP->vpextrd(outDword, vS, imm(2));
  COMP->jmp(storeLabel);
  // Element 3
  COMP->bind(case3);
  COMP->vpextrd(outDword, vS, imm(3));
  // Store with byteswap using movbe
  COMP->bind(storeLabel);
  COMP->movbe(x86::dword_ptr(EA), outDword);

  COMP->bind(endLabel);
}

// Store Vector 128 Element Word Indexed
void PPCInterpreter::PPCInterpreterJIT_stvewx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp element = newGP64();
  x86::Xmm vS = newXMM();
  x86::Gp outDword = newGP32();
  Label endLabel = COMP->newLabel();
  Label case0 = COMP->newLabel();
  Label case1 = COMP->newLabel();
  Label case2 = COMP->newLabel();
  Label case3 = COMP->newLabel();
  Label storeLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));
  // Calculate element index before aligning EA: element = (EA >> 2) & 0x3
  COMP->mov(element, EA);
  COMP->shr(element, imm(2));
  COMP->and_(element, imm(0x3));
  // Align EA to 4-byte boundary
  COMP->and_(EA, imm(~0x3));

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector register
  COMP->vmovdqa(vS, VPRPtr(J_VMX128_1_VD128));
  // Use direct extraction based on element index (0-3)
  // NOTE: This is faster than using a lookup table, and modern CPU's should handle it very well.
  COMP->cmp(element, imm(0));
  COMP->je(case0);
  COMP->cmp(element, imm(1));
  COMP->je(case1);
  COMP->cmp(element, imm(2));
  COMP->je(case2);
  COMP->jmp(case3);
  // Element 0
  COMP->bind(case0);
  COMP->vpextrd(outDword, vS, imm(0));
  COMP->jmp(storeLabel);
  // Element 1
  COMP->bind(case1);
  COMP->vpextrd(outDword, vS, imm(1));
  COMP->jmp(storeLabel);
  // Element 2
  COMP->bind(case2);
  COMP->vpextrd(outDword, vS, imm(2));
  COMP->jmp(storeLabel);
  // Element 3
  COMP->bind(case3);
  COMP->vpextrd(outDword, vS, imm(3));
  // Store with byteswap using movbe
  COMP->bind(storeLabel);
  COMP->movbe(x86::dword_ptr(EA), outDword);

  COMP->bind(endLabel);
}

// Store Vector Left Indexed (x'7C00 050E')
// Stores the left portion of a vector to memory based on alignment.
// EA = (rA|0) + rB, eb = EA[60:63]
// Stores bytes 0..(15-eb) of the byteswapped vector to memory at EA
void PPCInterpreter::PPCInterpreterJIT_stvlx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp eb = newGP64();
  x86::Gp count = newGP64();
  x86::Gp tmpAddress = newGP64();
  x86::Xmm vS = newXMM();
  x86::Xmm vDst = newXMM();
  x86::Xmm vMask = newXMM();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));

  // Extract eb = EA & 0xF (alignment offset)
  COMP->mov(eb, EA);
  COMP->and_(eb, imm(0xF));

  // Calculate count = 16 - eb (number of bytes to store)
  COMP->mov(count, imm(16));
  COMP->sub(count, eb);

  // If count == 0, nothing to store
  COMP->test(count, count);
  COMP->jz(endLabel);

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector and byteswap it
  COMP->vmovdqa(vS, VPRPtr(instr.vd));
  COMP->mov(tmpAddress, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vS, vS, x86::ptr(tmpAddress));

  // Load current destination memory (unaligned)
  COMP->vmovdqu(vDst, x86::ptr(EA));

  // Load blend mask from stvlxBlendMasks[count]
  COMP->mov(tmpAddress, (uintptr_t)&stvlxBlendMasks);
  COMP->shl(count, imm(4));  // count * 16 (each Vector128 is 16 bytes)
  COMP->add(tmpAddress, count);
  COMP->vmovdqa(vMask, x86::ptr(tmpAddress));

  // Blend: result = (vS & mask) | (vDst & ~mask)
  // vpblendvb selects from vDst where mask bit is 0, from vS where mask bit is 1
  COMP->vpblendvb(vDst, vDst, vS, vMask);

  // Store result back to memory (unaligned)
  COMP->vmovdqu(x86::ptr(EA), vDst);

  COMP->bind(endLabel);
}

// Store Vector 128 Left Indexed
void PPCInterpreter::PPCInterpreterJIT_stvlx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp eb = newGP64();
  x86::Gp count = newGP64();
  x86::Gp tmpAddress = newGP64();
  x86::Xmm vS = newXMM();
  x86::Xmm vDst = newXMM();
  x86::Xmm vMask = newXMM();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));

  // Extract eb = EA & 0xF (alignment offset)
  COMP->mov(eb, EA);
  COMP->and_(eb, imm(0xF));

  // Calculate count = 16 - eb (number of bytes to store)
  COMP->mov(count, imm(16));
  COMP->sub(count, eb);

  // If count == 0, nothing to store
  COMP->test(count, count);
  COMP->jz(endLabel);

  // Get the translated address
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector and byteswap it
  COMP->vmovdqa(vS, VPRPtr(J_VMX128_1_VD128));
  COMP->mov(tmpAddress, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vS, vS, x86::ptr(tmpAddress));

  // Load current destination memory (unaligned)
  COMP->vmovdqu(vDst, x86::ptr(EA));

  // Load blend mask from stvlxBlendMasks[count]
  COMP->mov(tmpAddress, (uintptr_t)&stvlxBlendMasks);
  COMP->shl(count, imm(4));  // count * 16 (each Vector128 is 16 bytes)
  COMP->add(tmpAddress, count);
  COMP->vmovdqa(vMask, x86::ptr(tmpAddress));

  // Blend: result = (vS & mask) | (vDst & ~mask)
  // vpblendvb selects from vDst where mask bit is 0, from vS where mask bit is 1
  COMP->vpblendvb(vDst, vDst, vS, vMask);

  // Store result back to memory (unaligned)
  COMP->vmovdqu(x86::ptr(EA), vDst);

  COMP->bind(endLabel);
}

// Store Vector Right Indexed (x'7C00 054E')
// Stores the right portion of a vector to memory based on alignment.
// EA = (rA|0) + rB, eb = EA[60:63], EA &= ~0xF (align to 16-byte boundary)
// If eb == 0, nothing is stored.
// Otherwise, stores bytes (16-eb)..(15) of the byteswapped vector to aligned EA
void PPCInterpreter::PPCInterpreterJIT_stvrx(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp eb = newGP64();
  x86::Gp tmpAddress = newGP64();
  x86::Xmm vS = newXMM();
  x86::Xmm vDst = newXMM();
  x86::Xmm vMask = newXMM();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.ra != 0) { COMP->mov(EA, GPRPtr(instr.ra)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.rb));

  // Extract eb = EA & 0xF (alignment offset / count of bytes to store)
  COMP->mov(eb, EA);
  COMP->and_(eb, imm(0xF));

  // Align EA to 16-byte boundary
  COMP->and_(EA, imm(~0xF));

  // If eb == 0, nothing to store (return early)
  COMP->test(eb, eb);
  COMP->jz(endLabel);

  // Get the translated address (using aligned EA)
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector and byteswap it
  COMP->vmovdqa(vS, VPRPtr(instr.vd));
  COMP->mov(tmpAddress, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vS, vS, x86::ptr(tmpAddress));

  // Apply shuffle from stvrxShuffleTable[eb] to move bytes[16-eb..15] to bytes[0..eb-1]
  COMP->mov(tmpAddress, (uintptr_t)&stvrxShuffleTable);
  COMP->shl(eb, imm(4));  // eb * 16 (each Vector128 is 16 bytes)
  COMP->add(tmpAddress, eb);
  COMP->vpshufb(vS, vS, x86::ptr(tmpAddress));

  // Load current destination memory (use unaligned load for safety)
  COMP->vmovdqu(vDst, x86::ptr(EA));

  // Load blend mask from stvrxBlendMasks[eb] (eb is already multiplied by 16)
  COMP->mov(tmpAddress, (uintptr_t)&stvrxBlendMasks);
  COMP->add(tmpAddress, eb);
  COMP->vmovdqa(vMask, x86::ptr(tmpAddress));

  // Blend: result = (vS & mask) | (vDst & ~mask)
  // vpblendvb selects from vDst where mask bit is 0, from vS where mask bit is 1
  COMP->vpblendvb(vDst, vDst, vS, vMask);

  // Store result back to memory (use unaligned store for safety)
  COMP->vmovdqu(x86::ptr(EA), vDst);

  COMP->bind(endLabel);
}

// Store Vector 128 Right Indexed
void PPCInterpreter::PPCInterpreterJIT_stvrx128(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Gp EA = newGP64();
  x86::Gp eb = newGP64();
  x86::Gp tmpAddress = newGP64();
  x86::Xmm vS = newXMM();
  x86::Xmm vDst = newXMM();
  x86::Xmm vMask = newXMM();
  Label endLabel = COMP->newLabel();

  // Get effective address: EA = (rA|0) + rB
  if (instr.VMX128_1.RA != 0) { COMP->mov(EA, GPRPtr(instr.VMX128_1.RA)); }
  else { COMP->xor_(EA, EA); }
  COMP->add(EA, GPRPtr(instr.VMX128_1.RB));

  // Extract eb = EA & 0xF (alignment offset / count of bytes to store)
  COMP->mov(eb, EA);
  COMP->and_(eb, imm(0xF));

  // Align EA to 16-byte boundary
  COMP->and_(EA, imm(~0xF));

  // If eb == 0, nothing to store (return early)
  COMP->test(eb, eb);
  COMP->jz(endLabel);

  // Get the translated address (using aligned EA)
  InvokeNode *mmuTranslation = nullptr;
  COMP->invoke(&mmuTranslation, imm((void *)JITTranslateAndGetHostPtr), FuncSignature::build<u64, sPPEState *, u64, ePPUThreadID>());
  mmuTranslation->setArg(0, b->ppeState->Base());
  mmuTranslation->setArg(1, EA);
  mmuTranslation->setArg(2, ePPUThread_None);
  mmuTranslation->setRet(0, EA);

  // Check for valid address
  COMP->test(EA, EA);
  COMP->jz(endLabel);

  // Load source vector and byteswap it
  COMP->vmovdqa(vS, VPRPtr(J_VMX128_1_VD128));
  COMP->mov(tmpAddress, (uintptr_t)&XMMByteSwapMask);
  COMP->vpshufb(vS, vS, x86::ptr(tmpAddress));

  // Apply shuffle from stvrxShuffleTable[eb] to move bytes[16-eb..15] to bytes[0..eb-1]
  COMP->mov(tmpAddress, (uintptr_t)&stvrxShuffleTable);
  COMP->shl(eb, imm(4));  // eb * 16 (each Vector128 is 16 bytes)
  COMP->add(tmpAddress, eb);
  COMP->vpshufb(vS, vS, x86::ptr(tmpAddress));

  // Load current destination memory (use unaligned load for safety)
  COMP->vmovdqu(vDst, x86::ptr(EA));

  // Load blend mask from stvrxBlendMasks[eb] (eb is already multiplied by 16)
  COMP->mov(tmpAddress, (uintptr_t)&stvrxBlendMasks);
  COMP->add(tmpAddress, eb);
  COMP->vmovdqa(vMask, x86::ptr(tmpAddress));

  // Blend: result = (vS & mask) | (vDst & ~mask)
  // vpblendvb selects from vDst where mask bit is 0, from vS where mask bit is 1
  COMP->vpblendvb(vDst, vDst, vS, vMask);

  // Store result back to memory (use unaligned store for safety)
  COMP->vmovdqu(x86::ptr(EA), vDst);

  COMP->bind(endLabel);
}

//
// Bugged instructions, need to re check them. Disabled for now.
//

// Vector Reciprocal Estimate Floating Point (x'1000 010A')
void PPCInterpreter::PPCInterpreterJIT_vrefp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vTmp = newXMM();
  x86::Xmm vZero = newXMM();
  x86::Xmm vTwo = newXMM();
  x86::Xmm vOne = newXMM();
  x86::Xmm vPosInf = newXMM();
  x86::Xmm vNegInf = newXMM();
  x86::Xmm vIsNaN = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vQNaN = newXMM();
  x86::Gp tmp = newGP32();

  // Load vB
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Constants: 0.0, 1.0, 2.0, +inf, -inf, quiet-bit
  COMP->vxorps(vZero, vZero, vZero);
  COMP->mov(tmp, 0x40000000); COMP->vmovd(vTwo, tmp); COMP->vbroadcastss(vTwo, vTwo);     // 2.0f
  COMP->mov(tmp, 0x3F800000); COMP->vmovd(vOne, tmp); COMP->vbroadcastss(vOne, vOne);     // 1.0f
  COMP->mov(tmp, 0x7F800000); COMP->vmovd(vPosInf, tmp); COMP->vbroadcastss(vPosInf, vPosInf); // +inf
  COMP->mov(tmp, 0xFF800000); COMP->vmovd(vNegInf, tmp); COMP->vbroadcastss(vNegInf, vNegInf); // -inf
  COMP->mov(tmp, 0x00400000); COMP->vmovd(vQNaNBit, tmp); COMP->vbroadcastss(vQNaNBit, vQNaNBit); // QNaN quiet bit

  // Compute precise IEEE-754 reciprocal using hardware divide:
  // vD = 1.0f / vB
  COMP->vdivps(vD, vOne, vB);

  // Handle NaNs: ensure signaling NaNs are converted to quiet NaNs with payload preserved
  COMP->vcmpps(vIsNaN, vB, vB, 3);     // UNORD => NaN
  COMP->vorps(vQNaN, vB, vQNaNBit);    // set quiet bit on input NaN (SNaN -> QNaN)
  COMP->vblendvps(vD, vD, vQNaN, vIsNaN);

  // Flush denormal result to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vD);

  // Store result
  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector Reciprocal Square Root Estimate Floating Point (x'1000 014A')
void PPCInterpreter::PPCInterpreterJIT_vrsqrtefp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();

  // Load vB
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Flush denormal inputs to zero
  J_FlushDenormalsToZero(b, vB);

  // Perform packed single-precision reciprocal square root estimate
  // vD = 1.0 / sqrt(vB) (4 x float32) - approximate
  COMP->vrsqrtps(vD, vB);

  // Store result to vD
  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector Multiply-Add Floating Point (x'1000 002E')
void PPCInterpreter::PPCInterpreterJIT_vmaddfp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // vD = (vA * vC) + vB
  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vC = newXMM();
  x86::Xmm vD = newXMM();

  // Load vA, vB, and vC
  COMP->vmovaps(vA, VPRPtr(instr.va));
  COMP->vmovaps(vB, VPRPtr(instr.vb));
  COMP->vmovaps(vC, VPRPtr(instr.vc));

  // Flush denormal inputs to zero
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);
  J_FlushDenormalsToZero(b, vC);

  // Perform fused multiply-add: vD = (vA * vC) + vB
  // Using FMA instruction if available (vfmadd231ps), otherwise mul + add
  COMP->vmulps(vD, vA, vC);
  COMP->vaddps(vD, vD, vB);

  // Flush denormal result to zero
  J_FlushDenormalsToZero(b, vD);

  // Store result to vD
  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector Negative Multiply-Subtract Floating Point (x'1000 002F')
void PPCInterpreter::PPCInterpreterJIT_vnmsubfp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // vD = -((vA * vC) - vB) = vB - (vA * vC) then negate = -(vA*vC - vB)
  x86::Xmm vA = newXMM();
  x86::Xmm vB = newXMM();
  x86::Xmm vC = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vTemp = newXMM();

  // Load vA, vB, and vC
  COMP->vmovaps(vA, VPRPtr(instr.va));
  COMP->vmovaps(vB, VPRPtr(instr.vb));
  COMP->vmovaps(vC, VPRPtr(instr.vc));

  // Flush denormal inputs to zero
  J_FlushDenormalsToZero(b, vA);
  J_FlushDenormalsToZero(b, vB);
  J_FlushDenormalsToZero(b, vC);

  // vTemp = vA * vC
  COMP->vmulps(vTemp, vA, vC);

  // vD = vTemp - vB
  COMP->vsubps(vD, vTemp, vB);

  // Negate the result: vD = -vD (XOR with sign bit mask)
  x86::Xmm vSignMask = newXMM();
  x86::Gp tempGp = newGP64();
  COMP->mov(tempGp, 0x8000000080000000ULL);
  COMP->vmovq(vSignMask, tempGp);
  COMP->vpbroadcastq(vSignMask, vSignMask);
  COMP->vxorps(vD, vD, vSignMask);

  // Flush denormal result to zero
  J_FlushDenormalsToZero(b, vD);

  // Store result to vD
  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector 2 Raised to the Exponent Estimate Floating Point (x'1000 018A')
void PPCInterpreter::PPCInterpreterJIT_vexptefp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // vD = 2^vB for each element (estimate - limited precision)
  // Using the identity: 2^x = 2^floor(x) * 2^frac(x)
  // 2^floor(x) is computed by manipulating the IEEE 754 exponent bits
  // 2^frac(x) is approximated using a polynomial tuned to match VMX behavior

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vInput = newXMM(); // Save original input for NaN check

  // Load vB
  COMP->vmovaps(vB, VPRPtr(instr.vb));

  // Save original input before any modifications (for NaN detection)
  COMP->vmovaps(vInput, vB);

  // Flush denormal inputs to zero (VMX behavior)
  J_FlushDenormalsToZero(b, vB);

  // Constants
  x86::Gp tempGp = newGP32();
  x86::Xmm vOne = newXMM();
  x86::Xmm vClampMin = newXMM();
  x86::Xmm vClampMax = newXMM();
  x86::Xmm vExpBias = newXMM();
  x86::Xmm vC0 = newXMM();
  x86::Xmm vC1 = newXMM();
  x86::Xmm vC2 = newXMM();
  x86::Xmm vC3 = newXMM();
  x86::Xmm vC4 = newXMM();
  x86::Xmm vMantissaMask = newXMM();
  x86::Xmm vRoundBit = newXMM();

  // Load constant 1.0f and broadcast to all 4 lanes
  COMP->mov(tempGp, 0x3F800000); // 1.0f in IEEE 754
  COMP->vmovd(vOne, tempGp);
  COMP->vbroadcastss(vOne, vOne);

  // Clamp range: min = -126.0f (to avoid denormals in output)
  COMP->mov(tempGp, 0xC2FC0000); // -126.0f
  COMP->vmovd(vClampMin, tempGp);
  COMP->vbroadcastss(vClampMin, vClampMin);

  // Clamp range: max = 128.0f (to avoid overflow)
  COMP->mov(tempGp, 0x43000000); // 128.0f
  COMP->vmovd(vClampMax, tempGp);
  COMP->vbroadcastss(vClampMax, vClampMax);

  // Exponent bias: 127 << 23 = 0x3F800000
  COMP->mov(tempGp, 0x3F800000);
  COMP->vmovd(vExpBias, tempGp);
  COMP->vbroadcastss(vExpBias, vExpBias);

  // Optimized minimax polynomial coefficients for 2^x on [0, 1)
  // p(x) = C0 + x*(C1 + x*(C2 + x*(C3 + x*C4)))

  // C0 = 1.0
  COMP->mov(tempGp, 0x3F800000); // 1.0f
  COMP->vmovd(vC0, tempGp);
  COMP->vbroadcastss(vC0, vC0);

  // C1 = 0.693147180559945 (ln 2)
  COMP->mov(tempGp, 0x3F317218); // 0.6931472f
  COMP->vmovd(vC1, tempGp);
  COMP->vbroadcastss(vC1, vC1);

  // C2 = 0.240226506959101 (ln2^2 / 2)
  COMP->mov(tempGp, 0x3E75FDF0); // 0.2402265f
  COMP->vmovd(vC2, tempGp);
  COMP->vbroadcastss(vC2, vC2);

  // C3 = 0.0555041086648216 (ln2^3 / 6)
  COMP->mov(tempGp, 0x3D635847); // 0.0555041f
  COMP->vmovd(vC3, tempGp);
  COMP->vbroadcastss(vC3, vC3);

  // C4 = 0.00961812910762848 (ln2^4 / 24)
  COMP->mov(tempGp, 0x3C1D9539); // 0.0096181f
  COMP->vmovd(vC4, tempGp);
  COMP->vbroadcastss(vC4, vC4);

  // Mantissa mask to truncate lower bits for estimate precision
  COMP->mov(tempGp, 0xFFFF0000);
  COMP->vmovd(vMantissaMask, tempGp);
  COMP->vbroadcastss(vMantissaMask, vMantissaMask);

  // Rounding bit for round-to-nearest behavior
  COMP->mov(tempGp, 0x00008000);
  COMP->vmovd(vRoundBit, tempGp);
  COMP->vbroadcastss(vRoundBit, vRoundBit);

  // Clamp input to valid range
  x86::Xmm vClamped = newXMM();
  COMP->vmaxps(vClamped, vB, vClampMin);
  COMP->vminps(vClamped, vClamped, vClampMax);

  // Split x into integer and fractional parts: x = ipart + fpart
  // ipart = floor(x), fpart = x - floor(x), fpart in [0, 1)
  x86::Xmm vIPart = newXMM();
  COMP->vroundps(vIPart, vClamped, 0x01); // floor (mode 1)

  // Fractional part: fpart = x - ipart (will be in range [0, 1))
  x86::Xmm vFPart = newXMM();
  COMP->vsubps(vFPart, vClamped, vIPart);

  // Compute 2^ipart by manipulating IEEE 754 exponent bits
  // Convert ipart to integer
  x86::Xmm vIPartInt = newXMM();
  COMP->vcvtps2dq(vIPartInt, vIPart);

  // Shift left by 23 to move into exponent position
  COMP->vpslld(vIPartInt, vIPartInt, 23);

  // Add exponent bias (127 << 23)
  x86::Xmm vExp2IPart = newXMM();
  COMP->vpaddd(vExp2IPart, vIPartInt, vExpBias);

  // Compute 2^fpart using polynomial approximation (Horner's method)
  // p(x) = C0 + x*(C1 + x*(C2 + x*(C3 + x*C4)))
  x86::Xmm vPoly = newXMM();
  COMP->vmovaps(vPoly, vC4);
  COMP->vmulps(vPoly, vPoly, vFPart);
  COMP->vaddps(vPoly, vPoly, vC3);
  COMP->vmulps(vPoly, vPoly, vFPart);
  COMP->vaddps(vPoly, vPoly, vC2);
  COMP->vmulps(vPoly, vPoly, vFPart);
  COMP->vaddps(vPoly, vPoly, vC1);
  COMP->vmulps(vPoly, vPoly, vFPart);
  COMP->vaddps(vPoly, vPoly, vC0);

  // Final result: 2^x = 2^ipart * 2^fpart
  COMP->vmulps(vD, vExp2IPart, vPoly);

  // Round to nearest and truncate lower mantissa bits
  COMP->vpaddd(vD, vD, vRoundBit);
  COMP->vandps(vD, vD, vMantissaMask);

  // Handle special cases
  x86::Xmm vNegInf = newXMM();
  x86::Xmm vPosInf = newXMM();
  x86::Xmm vZero = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vCmpNegInf = newXMM();
  x86::Xmm vCmpPosInf = newXMM();
  x86::Xmm vCmpNaN = newXMM();
  x86::Xmm vCmpOverflow = newXMM();
  x86::Xmm vCmpUnderflow = newXMM();
  x86::Xmm vQNaN = newXMM();

  // -inf = 0xFF800000
  COMP->mov(tempGp, 0xFF800000);
  COMP->vmovd(vNegInf, tempGp);
  COMP->vbroadcastss(vNegInf, vNegInf);

  // +inf = 0x7F800000
  COMP->mov(tempGp, 0x7F800000);
  COMP->vmovd(vPosInf, tempGp);
  COMP->vbroadcastss(vPosInf, vPosInf);

  // Zero
  COMP->vxorps(vZero, vZero, vZero);

  // QNaN bit (bit 22) = 0x00400000 - used to convert SNaN to QNaN
  COMP->mov(tempGp, 0x00400000);
  COMP->vmovd(vQNaNBit, tempGp);
  COMP->vbroadcastss(vQNaNBit, vQNaNBit);

  // Check for NaN: a value is NaN if it's unordered with itself (predicate 3 = UNORD)
  COMP->vcmpps(vCmpNaN, vInput, vInput, 3);

  // Create QNaN from input by setting the quiet bit (convert SNaN to QNaN)
  COMP->vorps(vQNaN, vInput, vQNaNBit);

  // Check for -inf input (predicate 0 = EQ)
  COMP->vcmpps(vCmpNegInf, vInput, vNegInf, 0);
  // Check for +inf input (predicate 0 = EQ)
  COMP->vcmpps(vCmpPosInf, vInput, vPosInf, 0);

  // Check for overflow: input >= 128.0f (after denormal flush) -> result is +inf
  // predicate 13 = GE (a >= b)
  COMP->vcmpps(vCmpOverflow, vB, vClampMax, 13);

  // Check for underflow: input < -126.0f (after denormal flush) -> result is 0
  // predicate 1 = LT (a < b)
  COMP->vcmpps(vCmpUnderflow, vB, vClampMin, 1);

  // Apply special cases in order (NaN check must come last to override other cases)
  // Apply overflow: if input >= 128, set result to +inf
  COMP->vblendvps(vD, vD, vPosInf, vCmpOverflow);
  // Apply underflow: if input < -126, set result to 0
  COMP->vblendvps(vD, vD, vZero, vCmpUnderflow);
  // If input was -inf, set result to 0
  COMP->vblendvps(vD, vD, vZero, vCmpNegInf);
  // If input was +inf, set result to +inf
  COMP->vblendvps(vD, vD, vPosInf, vCmpPosInf);
  // If input was NaN, return QNaN (with quiet bit set)
  COMP->vblendvps(vD, vD, vQNaN, vCmpNaN);

  // Store result to vD
  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

// Vector Log Base2 Estimate Floating Point (x'1000 01CA')
void PPCInterpreter::PPCInterpreterJIT_vlogefp(sPPEState *ppeState, JITBlockBuilder *b, uPPCInstr instr) {
  // Ensure VXU is enabled
  J_checkVXUEnabled(b);

  // vD = log2(vB) for each element
 // Compute log2(x) = exponent + log2(mantissa) with mantissa in [1,2)

  x86::Xmm vB = newXMM();
  x86::Xmm vD = newXMM();
  x86::Xmm vInput = newXMM();

  // Load vB
  COMP->vmovaps(vB, VPRPtr(instr.vb));
  COMP->vmovaps(vInput, vB);

  // Instead of calling the generic flush helper, explicitly clear denormals using integer ops:
  // denormal if exponent ==0 and mantissa !=0 -> set lane to zero.

  x86::Gp tempGp = newGP32();
  x86::Xmm vBits = newXMM();
  x86::Xmm vExpMask = newXMM();
  x86::Xmm vMantMask = newXMM();
  x86::Xmm vExpShift = newXMM();
  x86::Xmm vMantI = newXMM();
  x86::Xmm vZeroInt = newXMM();
  x86::Xmm vAllOnes = newXMM();
  x86::Xmm vExpEqZero = newXMM();
  x86::Xmm vMantEqZero = newXMM();
  x86::Xmm vMantNotZero = newXMM();
  x86::Xmm vDenormMask = newXMM();
  x86::Xmm vKeepMask = newXMM();

  // Bitwise copy of input bits
  COMP->vmovaps(vBits, vB);

  // Prepare integer masks
  COMP->mov(tempGp, 0x7F800000);
  COMP->vmovd(vExpMask, tempGp);
  COMP->vpbroadcastd(vExpMask, vExpMask);

  COMP->mov(tempGp, 0x007FFFFF);
  COMP->vmovd(vMantMask, tempGp);
  COMP->vpbroadcastd(vMantMask, vMantMask);

  // Zero and all-ones integer vectors
  COMP->vxorps(vZeroInt, vZeroInt, vZeroInt); // zero
  COMP->mov(tempGp, 0xFFFFFFFFu);
  COMP->vmovd(vAllOnes, tempGp);
  COMP->vpbroadcastd(vAllOnes, vAllOnes);

  // Extract exponent bits (integer) and shift right by23
  COMP->vpand(vExpShift, vBits, vExpMask);
  COMP->vpsrld(vExpShift, vExpShift, 23);

  // Extract mantissa bits (integer)
  COMP->vpand(vMantI, vBits, vMantMask);

  // exp ==0 ?
  COMP->vpcmpeqd(vExpEqZero, vExpShift, vZeroInt);
  // mant ==0 ?
  COMP->vpcmpeqd(vMantEqZero, vMantI, vZeroInt);
  // mant !=0
  COMP->vpxor(vMantNotZero, vMantEqZero, vAllOnes);

  // denormal lanes = expEqZero & mantNotZero
  COMP->vpand(vDenormMask, vExpEqZero, vMantNotZero);

  // keep mask = ~denorm
  COMP->vpxor(vKeepMask, vDenormMask, vAllOnes);

  // Zero-out denormal lanes
  COMP->vpand(vB, vB, vKeepMask);

  // Save modified input for special-case detection
  COMP->vmovaps(vInput, vB);

  // Continue with rest of implementation...

  // Temps
  x86::Xmm vExpInt = newXMM();
  x86::Xmm vMantInt = newXMM();
  x86::Xmm vMant = newXMM();
  x86::Xmm vF = newXMM();
  x86::Xmm vPoly = newXMM();
  x86::Xmm vExp = newXMM();
  x86::Xmm vOneBits = newXMM(); //0x3F800000 (1.0f bits)
  x86::Xmm vExpBiasInt = newXMM(); //127 as int
  x86::Xmm vOne = newXMM();

  // Load ones/biases used later
  COMP->mov(tempGp, 0x3F800000);
  COMP->vmovd(vOneBits, tempGp);
  COMP->vpbroadcastd(vOneBits, vOneBits);

  COMP->mov(tempGp, 127);
  COMP->vmovd(vExpBiasInt, tempGp);
  COMP->vpbroadcastd(vExpBiasInt, vExpBiasInt);

  // Extract exponent -> integer value (use modified vB so denormals are treated as zero)
  COMP->vpand(vExpInt, vB, vExpMask);
  COMP->vpsrld(vExpInt, vExpInt, 23);
  COMP->vpsubd(vExpInt, vExpInt, vExpBiasInt); // exp -127
  COMP->vcvtdq2ps(vExp, vExpInt); // to float

  // Extract mantissa and set exponent bits to127 to get mantissa in [1,2) (use vB)
  COMP->vpand(vMantInt, vB, vMantMask);
  COMP->vpor(vMantInt, vMantInt, vOneBits);
  COMP->vmovaps(vMant, vMantInt);

  // f = mantissa -1.0
  COMP->mov(tempGp, 0x3F800000);
  COMP->vmovd(vOne, tempGp);
  COMP->vbroadcastss(vOne, vOne);
  COMP->vsubps(vF, vMant, vOne);

  // Polynomial approximation for log2(1+f) on f in [0,1)
  // Use4th-order Horner: (((c4*f + c3)*f + c2)*f + c1)*f)
  x86::Xmm vC1 = newXMM();
  x86::Xmm vC2 = newXMM();
  x86::Xmm vC3 = newXMM();
  x86::Xmm vC4 = newXMM();

  // Coefficients (approximate): these map to float bit patterns
  COMP->mov(tempGp, 0x3FB8AA3B); // ~1.442695 (1/ln2)
  COMP->vmovd(vC1, tempGp);
  COMP->vbroadcastss(vC1, vC1);

  COMP->mov(tempGp, 0xBE38D6AD); // ~-0.7213475 (-1/(2 ln2))
  COMP->vmovd(vC2, tempGp);
  COMP->vbroadcastss(vC2, vC2);

  COMP->mov(tempGp, 0x3EE3E6B0); // ~0.48089835 (1/(3 ln2))
  COMP->vmovd(vC3, tempGp);
  COMP->vbroadcastss(vC3, vC3);

  COMP->mov(tempGp, 0xBE2E1476); // ~-0.36067376 (-1/(4 ln2))
  COMP->vmovd(vC4, tempGp);
  COMP->vbroadcastss(vC4, vC4);

  // Horner evaluation
  COMP->vmovaps(vPoly, vC4);
  COMP->vmulps(vPoly, vPoly, vF);
  COMP->vaddps(vPoly, vPoly, vC3);
  COMP->vmulps(vPoly, vPoly, vF);
  COMP->vaddps(vPoly, vPoly, vC2);
  COMP->vmulps(vPoly, vPoly, vF);
  COMP->vaddps(vPoly, vPoly, vC1);
  COMP->vmulps(vPoly, vPoly, vF);
  // No constant term (log2(1+f) has zero constant)

  // result = exponent + poly
  COMP->vaddps(vD, vExp, vPoly);

  // Special cases
  x86::Xmm vZero = newXMM();
  x86::Xmm vPosInf = newXMM();
  x86::Xmm vNegInf = newXMM();
  x86::Xmm vQNaNBit = newXMM();
  x86::Xmm vQNaN = newXMM();
  x86::Xmm vCmpNaN = newXMM();
  x86::Xmm vCmpPosInf = newXMM();
  x86::Xmm vCmpNeg = newXMM();
  x86::Xmm vCmpZero = newXMM();

  COMP->vxorps(vZero, vZero, vZero);
  COMP->mov(tempGp, 0x7F800000); COMP->vmovd(vPosInf, tempGp); COMP->vbroadcastss(vPosInf, vPosInf);
  COMP->mov(tempGp, 0xFF800000); COMP->vmovd(vNegInf, tempGp); COMP->vbroadcastss(vNegInf, vNegInf);
  COMP->mov(tempGp, 0x00400000); COMP->vmovd(vQNaNBit, tempGp); COMP->vbroadcastss(vQNaNBit, vQNaNBit);

  // NaN test: unordered with itself
  COMP->vcmpps(vCmpNaN, vInput, vInput, 3);
  // +inf
  COMP->vcmpps(vCmpPosInf, vInput, vPosInf, 0);
  // negative values (less than zero)
  COMP->vcmpps(vCmpNeg, vInput, vZero, 1);
  // zero
  COMP->vcmpps(vCmpZero, vInput, vZero, 0);

  // Create QNaN by setting quiet bit
  COMP->vorps(vQNaN, vInput, vQNaNBit);

  // Apply special cases (order matters - NaN should override others)
  // negative -> QNaN
  COMP->vblendvps(vD, vD, vQNaN, vCmpNeg);
  // NaN -> QNaN
  COMP->vblendvps(vD, vD, vQNaN, vCmpNaN);
  // +inf -> +inf (log2(+inf) = +inf)
  COMP->vblendvps(vD, vD, vPosInf, vCmpPosInf);
  // zero -> -inf
  COMP->vblendvps(vD, vD, vNegInf, vCmpZero);

  // Store result
  COMP->vmovaps(VPRPtr(instr.vd), vD);
}

#endif