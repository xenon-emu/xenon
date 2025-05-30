// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "PPCInterpreter.h"

// Data Stream Touch for Store
void PPCInterpreter::PPCInterpreter_dss(PPU_STATE *ppuState) {
  CHECK_VXU;

  // We don't really need to do anything here, as it's handling cache. We mostly ignore it
}

// Data Stream Touch for Store
void PPCInterpreter::PPCInterpreter_dst(PPU_STATE *ppuState) {
  CHECK_VXU;

  // We don't really need to do anything here, as it's handling cache. We mostly ignore it
}

// Data Stream Touch for Store
void PPCInterpreter::PPCInterpreter_dstst(PPU_STATE *ppuState) {
  CHECK_VXU;

  // We don't really need to do anything here, as it's handling cache. We mostly ignore it
}

// Move from Vector Status and Control Register (x'1000 0604')
void PPCInterpreter::PPCInterpreter_mfvscr(PPU_STATE *ppuState) {
  /*
  vD <- 0 || (VSCR)
  */

  CHECK_VXU;

  VRi(vd).dword[3] = curThread.VSCR.hexValue;
}

// Move to Vector Status and Control Register (x'1000 0C44')
void PPCInterpreter::PPCInterpreter_mtvscr(PPU_STATE *ppuState) {
  /*
  VSCR <- (vB)96:127
  */

  CHECK_VXU;

  curThread.VSCR.hexValue = VRi(vb).dword[3];
}

// Vector Add Floating Point (x'1000 000A')
void PPCInterpreter::PPCInterpreter_vaddfp(PPU_STATE *ppuState) {
/*
  do i = 0,127,32
    (vD)i:i+31 <- RndToNearFP32((vA)i:i+31 + fp (vB)i:i+31)
  end 
*/

  // TODO: Rounding and NJ mode check.

  CHECK_VXU;

  VRi(vd).flt[0] = VRi(va).flt[0] + VRi(vb).flt[0];
  VRi(vd).flt[1] = VRi(va).flt[1] + VRi(vb).flt[1];
  VRi(vd).flt[2] = VRi(va).flt[2] + VRi(vb).flt[2];
  VRi(vd).flt[3] = VRi(va).flt[3] + VRi(vb).flt[3];
}

// Vector Logical AND (x'1000 0404')
void PPCInterpreter::PPCInterpreter_vand(PPU_STATE *ppuState) {
  /*
  vD <- (vA) & (vB)
  */

  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] & VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] & VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] & VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] & VRi(vb).dword[3];
}

// Vector Logical AND with Complement (x'1000 0444')
void PPCInterpreter::PPCInterpreter_vandc(PPU_STATE *ppuState) {
  /*
  vD <- (vA) & ~(vB)
  */

  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] & ~VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] & ~VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] & ~VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] & ~VRi(vb).dword[3];
}

// Vector Convert from Unsigned Fixed-Point Word (x'1000 030A')
void PPCInterpreter::PPCInterpreter_vcfux(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 32
    vDi:i+31 <- CnvtUI32ToFP32((vB)i:i+31) / fp 2UIMM
  end
  */

  // TODO: Rounding

  const f32 divisor = 1 << _instr.vuimm;

  VRi(vd).flt[0] = VRi(va).dword[0] / divisor;
  VRi(vd).flt[1] = VRi(va).dword[1] / divisor;
  VRi(vd).flt[2] = VRi(va).dword[2] / divisor;
  VRi(vd).flt[3] = VRi(va).dword[3] / divisor;
}

// Vector Logical NOR (x'1000 0504')
void PPCInterpreter::PPCInterpreter_vnor(PPU_STATE *ppuState) {
  /*
  vD <- ~((vA) | (vB))
  */

  CHECK_VXU;

  VRi(vd).dword[0] = ~(VRi(va).dword[0] | VRi(vb).dword[0]);
  VRi(vd).dword[1] = ~(VRi(va).dword[1] | VRi(vb).dword[1]);
  VRi(vd).dword[2] = ~(VRi(va).dword[2] | VRi(vb).dword[2]);
  VRi(vd).dword[3] = ~(VRi(va).dword[3] | VRi(vb).dword[3]);
}

// Vector Logical OR (x'1000 0484')
void PPCInterpreter::PPCInterpreter_vor(PPU_STATE *ppuState) {
  /*
  vD <- (vA) | (vB)
  */

  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] | VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] | VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] | VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] | VRi(vb).dword[3];
}

// Vector Splat Word (x'1000 028C')
void PPCInterpreter::PPCInterpreter_vspltw(PPU_STATE *ppuState) {
  /*
   b <- UIMM*32
    do i=0 to 127 by 32
    vDi:i+31 <- (vB)b:b+31
    end
  */

  CHECK_VXU;

  const u8 b = (_instr.va & 0x3);

  VRi(vd).dword[0] = VRi(vb).dword[b];
  VRi(vd).dword[1] = VRi(vb).dword[b];
  VRi(vd).dword[2] = VRi(vb).dword[b];
  VRi(vd).dword[3] = VRi(vb).dword[b];
}

// Vector Maximum Unsigned Word (x'1000 0082')
void PPCInterpreter::PPCInterpreter_vmaxuw(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 32
  if (vA)i:i+31 >=ui (vB)i:i+31
   then vDi:i+31 <- (vA)i:i+31
   else vDi:i+31 <- (vB)i:i+31
  end
  */

  CHECK_VXU;

  VRi(vd).dword[0] = (VRi(va).dword[0] > VRi(vb).dword[0]) ? VRi(va).dword[0] : VRi(vb).dword[0];
  VRi(vd).dword[1] = (VRi(va).dword[1] > VRi(vb).dword[1]) ? VRi(va).dword[1] : VRi(vb).dword[1];
  VRi(vd).dword[2] = (VRi(va).dword[2] > VRi(vb).dword[2]) ? VRi(va).dword[2] : VRi(vb).dword[2];
  VRi(vd).dword[3] = (VRi(va).dword[3] > VRi(vb).dword[3]) ? VRi(va).dword[3] : VRi(vb).dword[3];
}

// Vector Minimum Unsigned Word (x'1000 0282')
void PPCInterpreter::PPCInterpreter_vminuw(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 32
  if (vA)i:i+31 < ui (vB)i:i+31
   then vDi:i+31 <- (vA)i:i+31
   else vDi:i+31 <- (vB)i:i+31
  end
  */

  CHECK_VXU;

  VRi(vd).dword[0] = (VRi(va).dword[0] < VRi(vb).dword[0]) ? VRi(va).dword[0] : VRi(vb).dword[0];
  VRi(vd).dword[1] = (VRi(va).dword[1] < VRi(vb).dword[1]) ? VRi(va).dword[1] : VRi(vb).dword[1];
  VRi(vd).dword[2] = (VRi(va).dword[2] < VRi(vb).dword[2]) ? VRi(va).dword[2] : VRi(vb).dword[2];
  VRi(vd).dword[3] = (VRi(va).dword[3] < VRi(vb).dword[3]) ? VRi(va).dword[3] : VRi(vb).dword[3];
}

// Vector 128 Multiply Floating Point
void PPCInterpreter::PPCInterpreter_vmulfp128(PPU_STATE *ppuState) {
  /*
  vD <- vA * vB (4 x FP)
  */

  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = VR(VMX128_VA128).flt[0] * VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = VR(VMX128_VA128).flt[1] * VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = VR(VMX128_VA128).flt[2] * VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = VR(VMX128_VA128).flt[3] * VR(VMX128_VB128).flt[3];
}

// Vector Merge High Word (x'1000 008C')
void PPCInterpreter::PPCInterpreter_vmrghw(PPU_STATE *ppuState) {
  /*
  do i=0 to 63 by 32
    vDi*2:(i*2)+63 <- (vA)i:i+31 || (vB)i:i+31
  end
  */

  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0];
  VRi(vd).dword[1] = VRi(vb).dword[0];
  VRi(vd).dword[2] = VRi(va).dword[1];
  VRi(vd).dword[3] = VRi(vb).dword[1];
}

// Vector Merge Low Word (x'1000 018C')
void PPCInterpreter::PPCInterpreter_vmrglw(PPU_STATE *ppuState) {
  /*
  do i=0 to 63 by 32
    vDi*2:(i*2)+63 <- (vA)i+64:i+95 || (vB)i+64:i+95
  end
  */

  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[2];
  VRi(vd).dword[1] = VRi(vb).dword[2];
  VRi(vd).dword[2] = VRi(va).dword[3];
  VRi(vd).dword[3] = VRi(vb).dword[3];
}

// Vector Merge High Word 128
void PPCInterpreter::PPCInterpreter_vmrghw128(PPU_STATE *ppuState) {

  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[1];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VB128).dword[1];
}

static const u8 reIndex[32] = { 3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12, 19,18,17,16, 23,22,21,20, 27,26,25,24, 31,30,29,28 };

static inline u8 vpermHelper(u8 idx, Base::Vector128 vra, Base::Vector128 vrb) {
  return (idx & 16) ? vrb.bytes[idx & 15] : vra.bytes[idx & 15];
}

// Vector Permute (x'1000 002B')
void PPCInterpreter::PPCInterpreter_vperm(PPU_STATE* ppuState) {
  /*
  temp0:255 <- (vA) || (vB)
  do i=0 to 127 by 8
    b <- (vC)i+3:i+7 || 0b000
    vDi:i+7 <- tempb:b+7
  end
  */

  CHECK_VXU;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vpermHelper(reIndex[VRi(vc).bytes[reIndex[idx]]], VRi(va), VRi(rb));
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);
}

// Vector Permute 128
void PPCInterpreter::PPCInterpreter_vperm128(PPU_STATE* ppuState) {

  CHECK_VXU;

  for (u8 idx = 0; idx < 16; idx++) {
    VR(VMX128_2_VD128).bytes[idx] = vpermHelper(reIndex[VR(VMX128_2_VC).bytes[reIndex[idx]]], VR(VMX128_2_VA128), VR(VMX128_2_VB128));
  }

  VR(VMX128_2_VD128).dword[0] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[0]);
  VR(VMX128_2_VD128).dword[1] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[1]);
  VR(VMX128_2_VD128).dword[2] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[2]);
  VR(VMX128_2_VD128).dword[3] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[3]);
}

// Vector Multiply Add Floating Point (x'1000 002E')
void PPCInterpreter::PPCInterpreter_vmaddfp(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 32
    vDi:i+31 <- RndToNearFP32(((vA)i:i+31 *fp (vC)i:i+31) +fp (vB)i:i+31)
  end
  */

  // TODO: Rounding.

  CHECK_VXU;

  VRi(vd).flt[0] = (VRi(va).flt[0] * VRi(vc).flt[0]) + VRi(vb).flt[0];
  VRi(vd).flt[1] = (VRi(va).flt[1] * VRi(vc).flt[1]) + VRi(vb).flt[1];
  VRi(vd).flt[2] = (VRi(va).flt[2] * VRi(vc).flt[2]) + VRi(vb).flt[2];
  VRi(vd).flt[3] = (VRi(va).flt[3] * VRi(vc).flt[3]) + VRi(vb).flt[3];
}

// Vector Shift Left Integer Byte (x'1000 0104')
void PPCInterpreter::PPCInterpreter_vslb(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 8
   sh <- (vB)i+5):i+7
   (vD)i:i+7 <- (vA)i:i+7 << ui sh
  end
  */

  CHECK_VXU;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = VRi(va).bytes[idx] << (VRi(vb).bytes[idx] & 0x7);
  }
}

// Vector Shift Left Integer Word (x'1000 0184')
void PPCInterpreter::PPCInterpreter_vslw(PPU_STATE* ppuState) {
  /*
  do i=0 to 127 by 32
    sh <- (vB)i+27:i+31
    vD[i:i+31] <- (vA)[i:i+31] << ui sh
  end
  */

  CHECK_VXU;

  VRi(rd).dword[0] = VRi(ra).dword[0] << (VRi(rb).dword[0] & 31);
  VRi(rd).dword[1] = VRi(ra).dword[1] << (VRi(rb).dword[1] & 31);
  VRi(rd).dword[2] = VRi(ra).dword[2] << (VRi(rb).dword[2] & 31);
  VRi(rd).dword[3] = VRi(ra).dword[3] << (VRi(rb).dword[3] & 31);
}

// Vector Shift Right (x'1000 02C4')
void PPCInterpreter::PPCInterpreter_vsr(PPU_STATE* ppuState) {
  /*
  sh <- (vB)125:127
  t <- 1
  do i = 0 to 127 by 8
    t <- t & ((vB)i+5:i+7 = sh)
    if t = 1 then vD <- (vA) >>ui sh
  else vD <- undefined
  end
  */

  // Let sh = vB[125-127]; sh is the shift count in bits (0<=sh<=7). The contents of vA are shifted right by sh bits. Bits
  // shifted out of bit 127 are lost. Zeros are supplied to the vacated bits on the left. The result is placed into vD.

  CHECK_VXU;

  const auto sh = VRi(rb).bytes[15] & 0x7;
  const auto rsh = 8 - sh;

  VRi(rd).bytes[15] = VRi(ra).bytes[15] >> sh | (VRi(rd).bytes[14] << rsh);
  VRi(rd).bytes[14] = VRi(ra).bytes[14] >> sh | (VRi(rd).bytes[13] << rsh);
  VRi(rd).bytes[13] = VRi(ra).bytes[13] >> sh | (VRi(rd).bytes[12] << rsh);
  VRi(rd).bytes[12] = VRi(ra).bytes[12] >> sh | (VRi(rd).bytes[11] << rsh);
  VRi(rd).bytes[11] = VRi(ra).bytes[11] >> sh | (VRi(rd).bytes[10] << rsh);
  VRi(rd).bytes[10] = VRi(ra).bytes[10] >> sh | (VRi(rd).bytes[9] << rsh);
  VRi(rd).bytes[9] = VRi(ra).bytes[9] >> sh | (VRi(rd).bytes[8] << rsh);
  VRi(rd).bytes[8] = VRi(ra).bytes[8] >> sh | (VRi(rd).bytes[7] << rsh);
  VRi(rd).bytes[7] = VRi(ra).bytes[7] >> sh | (VRi(rd).bytes[6] << rsh);
  VRi(rd).bytes[6] = VRi(ra).bytes[6] >> sh | (VRi(rd).bytes[5] << rsh);
  VRi(rd).bytes[5] = VRi(ra).bytes[5] >> sh | (VRi(rd).bytes[4] << rsh);
  VRi(rd).bytes[4] = VRi(ra).bytes[4] >> sh | (VRi(rd).bytes[3] << rsh);
  VRi(rd).bytes[3] = VRi(ra).bytes[3] >> sh | (VRi(rd).bytes[2] << rsh);
  VRi(rd).bytes[2] = VRi(ra).bytes[2] >> sh | (VRi(rd).bytes[1] << rsh);
  VRi(rd).bytes[1] = VRi(ra).bytes[1] >> sh | (VRi(rd).bytes[0] << rsh);
  VRi(rd).bytes[0] = VRi(ra).bytes[0] >> sh;
}

// Vector Shift Right Word (x'1000 0284')
void PPCInterpreter::PPCInterpreter_vsrw(PPU_STATE* ppuState) {
  /*
  do i=0 to 127 by 32
    sh <- (vB)i+(27):i+31
    vD[i:i+31] <- (vA)[i:i+31] >> ui sh
  end
  */

  CHECK_VXU;

  VRi(rd).dword[0] = VRi(ra).dword[0] >> (VRi(rb).dword[0] & 31);
  VRi(rd).dword[1] = VRi(ra).dword[1] >> (VRi(rb).dword[1] & 31);
  VRi(rd).dword[2] = VRi(ra).dword[2] >> (VRi(rb).dword[2] & 31);
  VRi(rd).dword[3] = VRi(ra).dword[3] >> (VRi(rb).dword[3] & 31);
}

static inline u8 vsldoiHelper(u8 sh, Base::Vector128 vra, Base::Vector128 vrb) {
  return (sh < 16) ? vra.bytes[sh] : vrb.bytes[sh & 0xF];
}

#if defined(ARCH_X86_64)
#ifdef __GNUC__
__attribute__((target("ssse3")))
#endif
__m128i vsldoi_sse(__m128i va, __m128i vb, u8 shb) {
  __m128i result = _mm_setzero_si128();
  switch (shb) {
#undef CASE
#define CASE(i) case i: result = _mm_or_si128(_mm_srli_si128(va, i), _mm_slli_si128(vb, 16 - i)); break
    CASE(0);
    CASE(1);
    CASE(2);
    CASE(3);
    CASE(4);
    CASE(5);
    CASE(6);
    CASE(7);
    CASE(8);
    CASE(9);
    CASE(10);
    CASE(11);
    CASE(12);
    CASE(13);
    CASE(14);
    CASE(15);
#undef CASE
  }
  return result;
}

#ifdef __GNUC__
__attribute__((target("ssse3")))
#endif
inline __m128i byteswap_be_u32x4_ssse3(__m128i x) {
  // Reverses bytes in each 32-bit word using SSSE3 shuffle_epi8
  const __m128i shuffle = _mm_set_epi8(
    12, 13, 14, 15,
    8, 9, 10, 11,
    4, 5, 6, 7,
    0, 1, 2, 3
  );
  return _mm_shuffle_epi8(x, shuffle);
}
#endif

// Vector Shift Left Double by Octet Immediate (x'1000 002C')
void PPCInterpreter::PPCInterpreter_vsldoi(PPU_STATE *ppuState) {
  /*
  vD <- ((vA) || (vB)) << ui (SHB || 0b000)
  */

  const u8 sh = _instr.vsh;
  // No shift
  if (sh == 0) {
    VRi(vd) = VRi(va);
    return;
  } else if (sh == 16) {
    // Don't touch VA
    VRi(vd) = VRi(vb);
    return;
  }

#if defined(ARCH_X86_64)
  __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(VRi(va).bytes.data()));
  __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(VRi(vb).bytes.data()));
  __m128i vd_sse = vsldoi_sse(va, vb, sh);

  // Store raw result before byte swap
  _mm_storeu_si128(reinterpret_cast<__m128i*>(VRi(vd).bytes.data()), vd_sse);
  // Swap
  __m128i result = _mm_loadu_si128(reinterpret_cast<__m128i*>(VRi(vd).bytes.data()));
  result = byteswap_be_u32x4_ssse3(vd_sse);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(VRi(vd).bytes.data()), result);
#else
  // TODO: Ugly, super slow, fix.

  VRi(va).dword[0] = byteswap_be<u32>(VRi(va).dword[0]);
  VRi(va).dword[1] = byteswap_be<u32>(VRi(va).dword[1]);
  VRi(va).dword[2] = byteswap_be<u32>(VRi(va).dword[2]);
  VRi(va).dword[3] = byteswap_be<u32>(VRi(va).dword[3]);

  VRi(vb).dword[0] = byteswap_be<u32>(VRi(vb).dword[0]);
  VRi(vb).dword[1] = byteswap_be<u32>(VRi(vb).dword[1]);
  VRi(vb).dword[2] = byteswap_be<u32>(VRi(vb).dword[2]);
  VRi(vb).dword[3] = byteswap_be<u32>(VRi(vb).dword[3]);

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vsldoiHelper(sh + idx, VRi(va), VRi(vb));
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);
#endif
}

// Vector Splat Byte (x'1000 020C')
void PPCInterpreter::PPCInterpreter_vspltb(PPU_STATE *ppuState) {
  /*
   b <- UIMM*8
  do i=0 to 127 by 8
  (vD)i:i+7 <- (vB)b:b+7
  end
  */

  CHECK_VXU;

  const u8 uimm = _instr.vuimm;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = VRi(vb).bytes[uimm];
  }
}

// Vector Splat Immediate Signed Halfword (x'1000 034C')
void PPCInterpreter::PPCInterpreter_vspltish(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 16
    (vD)i:i+15 <- SignExtend(SIMM,16)
  end
  */

  CHECK_VXU;

  s32 simm = 0;

  if (_instr.vsimm) { simm = ((_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm); }

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).sword[idx] = static_cast<s16>(simm);
  }
}

// Vector Splat Immediate Signed Word (x'1000 038C)
void PPCInterpreter::PPCInterpreter_vspltisw(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 32
    vDi:i+31 <- SignExtend(SIMM,32)
  end
  */
  
  CHECK_VXU;

  s32 simm = 0;

  if (_instr.vsimm) { simm = ((_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm); }

  for (u8 idx = 0; idx < 4; idx++) {
    VRi(vd).dsword[idx] = simm;
  }
}

// Vector Splat Immediate Signed Byte (x'1000 030C')
void PPCInterpreter::PPCInterpreter_vspltisb(PPU_STATE *ppuState) {
  /*
   do i = 0 to 127 by 8
   vDi:i+7 <- SignExtend(SIMM,8)
   end
  */

  CHECK_VXU;

  s32 simm = 0;

  if (_instr.vsimm) { simm = ((_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm); }

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = static_cast<s8>(simm);
  }
}

// Vector128 Splat Immediate Signed Word
void PPCInterpreter::PPCInterpreter_vspltisw128(PPU_STATE *ppuState) {
  /*
  (VRD.xyzw) <- sign_extend(uimm)
  */

  CHECK_VXU;

  s32 simm = 0;

  if (VMX128_3_IMM) {
    simm = ((VMX128_3_IMM & 0x10) ? (VMX128_3_IMM | 0xFFFFFFF0) : VMX128_3_IMM);
  }

  for (u8 idx = 0; idx < 4; idx++) {
    VR(VMX128_3_VD128).dsword[idx] = simm;
  }
}

// Vector Logical XOR (x'1000 04C4')
void PPCInterpreter::PPCInterpreter_vxor(PPU_STATE *ppuState) {
  /*
  vD <- (vA) ^ (vB)
  */

  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] ^ VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] ^ VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] ^ VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] ^ VRi(vb).dword[3];
}
