/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include <immintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#elif defined(ARCH_AARCH64)
#include <arm_neon.h>
#endif

#define HIDW(data) (static_cast<u64>(data) >> 32)
#define LODW(data) (static_cast<u32>(data))

static constexpr u64 GetBits64(u64 data, s8 begin, s8 end) {
  const u64 mask = (0xFFFFFFFFFFFFFFFF << (63 - end));
  return data & mask;
}

static constexpr u32 ExtractBits(u32 input, u32 begin, u32 end) {
  return (input >> (32 - 1 - end)) & ((1 << (end - begin + 1)) - 1);
}

#define QMASK(b, e) ((0xFFFFFFFFFFFFFFFF << ((63 + (b)) - (e))) >> (b))
#define QGET(qw, b, e) ((static_cast<u64>(qw) & QMASK((b), (e))) >> (63 - (e)))
#define QSET(qw, b, e, qwSet)                                                  \
  (qw) &= ~QMASK(b, e);                                                        \
  (qw) |= (static_cast<u64>(qwSet) << (63 - (e))) & QMASK(b, e)

#define DMASK(b, e) (((0xFFFFFFFF << ((31 + (b)) - (e))) >> (b)))
#define DGET(dw, b, e) (((dw) & DMASK((b), (e))) >> (31 - (e)))
#define DSET(dw, b, e, dwSet)                                                  \
  (dw) &= ~DMASK(b, e);                                                        \
  (dw) |= ((dwSet) << (31 - (e))) & DMASK(b, e);

#define EXTS(qw, ib)                                                           \
  (((static_cast<u64>(qw)) & ((static_cast<u64>(1)) << ((ib) - 1)))            \
       ? ((static_cast<u64>(qw)) | QMASK(0, 63 - (ib)))                        \
       : (static_cast<u64>(qw)))

#define BMSK(w, i) ((static_cast<u64>(1)) << ((w) - (i) - (1)))

#define BGET(dw, w, i) (((dw) & BMSK(w, i)) ? 1 : 0)
#define BSET(dw, w, i) (dw) |= BMSK(w, i)
#define BCLR(dw, w, i) (dw) &= ~BMSK(w, i)

#define BO_GET(i) BGET(BO, 5, i)

#define CR_GET(i) BGET(curThread.CR.CR_Hex, 32, i)
#define CR_GETi(i) BGET(curThread.CR.CR_Hex, 32, _instr.cr##i)
#define CR_SET(i) BSET(curThread.CR.CR_Hex, 32, i)
#define CR_CLR(i) BCLR(curThread.CR.CR_Hex, 32, i)

#define CR_BIT_LT 0
#define CR_BIT_GT 1
#define CR_BIT_EQ 2
#define CR_BIT_SO 3

// VMX Bitfields.
// Sources:
// https://github.com/kakaroto/ps3ida/blob/master/plugins/PPCAltivec/src/main.cpp
// http://biallas.net/doc/vmx128/vmx128.txt

#define VMX128_VD128   (_instr.VMX128.VD128l | (_instr.VMX128.VD128h << 5))
#define VMX128_VA128   (_instr.VMX128.VA128l | (_instr.VMX128.VA128h << 5) | (_instr.VMX128.VA128H << 6))
#define VMX128_VB128   (_instr.VMX128.VB128l | (_instr.VMX128.VB128h << 5))

#define VMX128_1_VD128 (_instr.VMX128_1.VD128l | (_instr.VMX128_1.VD128h << 5))

#define VMX128_2_VD128 (_instr.VMX128_2.VD128l | (_instr.VMX128_2.VD128h << 5))
#define VMX128_2_VA128 (_instr.VMX128_2.VA128l | (_instr.VMX128_2.VA128h << 5) | (_instr.VMX128_2.VA128H << 6))
#define VMX128_2_VB128 (_instr.VMX128_2.VB128l | (_instr.VMX128_2.VB128h << 5))
#define VMX128_2_VC    (_instr.VMX128_2.VC)

#define VMX128_3_VD128 (_instr.VMX128_3.VD128l | (_instr.VMX128_3.VD128h << 5))
#define VMX128_3_VB128 (_instr.VMX128_3.VB128l | (_instr.VMX128_3.VB128h << 5))
#define VMX128_3_IMM   (_instr.VMX128_3.IMM)

#define VMX128_5_VD128 (_instr.VMX128_5.VD128l | (_instr.VMX128_5.VD128h << 5))
#define VMX128_5_VA128 (_instr.VMX128_5.VA128l | (_instr.VMX128_5.VA128h << 5)) | (_instr.VMX128_5.VA128H << 6)
#define VMX128_5_VB128 (_instr.VMX128_5.VB128l | (_instr.VMX128_5.VB128h << 5))
#define VMX128_5_SH    (_instr.VMX128_5.SH)

#define VMX128_R_VD128 (_instr.VMX128_R.VD128l | (_instr.VMX128_R.VD128h << 5))
#define VMX128_R_VA128 (_instr.VMX128_R.VA128l | (_instr.VMX128_R.VA128h << 5) | (_instr.VMX128_R.VA128H << 6))
#define VMX128_R_VB128 (_instr.VMX128_R.VB128l | (_instr.VMX128_R.VB128h << 5))
