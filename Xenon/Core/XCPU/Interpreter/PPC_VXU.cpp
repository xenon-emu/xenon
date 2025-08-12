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

static inline u32 vecSaturate32(PPU_STATE* ppuState, u64 inValue) {
  if (inValue > UINT_MAX) {
    // Set SAT bit in VSCR and truncate to 2^32 - 1.
    curThread.VSCR.SAT = 1;
    inValue = UINT_MAX;
  }
  return static_cast<u32>(inValue);
}

// Vector Add Unsigned Word Saturate (x'1000 0280')
void PPCInterpreter::PPCInterpreter_vadduws(PPU_STATE* ppuState) {
  // TODO: Check behavior, logic seems okay.

  CHECK_VXU;

  VRi(vd).dword[0] = vecSaturate32(ppuState, static_cast<u64>(VRi(va).dword[0]) + static_cast<u64>(VRi(vb).dword[0]));
  VRi(vd).dword[1] = vecSaturate32(ppuState, static_cast<u64>(VRi(va).dword[1]) + static_cast<u64>(VRi(vb).dword[1]));
  VRi(vd).dword[2] = vecSaturate32(ppuState, static_cast<u64>(VRi(va).dword[2]) + static_cast<u64>(VRi(vb).dword[2]));
  VRi(vd).dword[3] = vecSaturate32(ppuState, static_cast<u64>(VRi(va).dword[3]) + static_cast<u64>(VRi(vb).dword[3]));
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

  CHECK_VXU;

  const f32 divisor = 1 << _instr.vuimm;

  VRi(vd).flt[0] = VRi(va).dword[0] / divisor;
  VRi(vd).flt[1] = VRi(va).dword[1] / divisor;
  VRi(vd).flt[2] = VRi(va).dword[2] / divisor;
  VRi(vd).flt[3] = VRi(va).dword[3] / divisor;
}

// Vector Compare Equal-to Unsigned Word (x'1000 0086')
void PPCInterpreter::PPCInterpreter_vcmpequwx(PPU_STATE* ppuState) {

  CHECK_VXU;

  VRi(vd).dword[0] = ((VRi(va).dword[0] == VRi(vb).dword[0]) ? 0xFFFFFFFF : 0x00000000);
  VRi(vd).dword[1] = ((VRi(va).dword[1] == VRi(vb).dword[1]) ? 0xFFFFFFFF : 0x00000000);
  VRi(vd).dword[2] = ((VRi(va).dword[2] == VRi(vb).dword[2]) ? 0xFFFFFFFF : 0x00000000);
  VRi(vd).dword[3] = ((VRi(va).dword[3] == VRi(vb).dword[3]) ? 0xFFFFFFFF : 0x00000000);

  if (_instr.vrc) {
    u8 crValue = 0;
    bool allEqual = false;
    bool allNotEqual = false;

    if (VRi(vd).dword[0] == 0xFFFFFFFF && VRi(vd).dword[1] == 0xFFFFFFFF
      && VRi(vd).dword[2] == 0xFFFFFFFF && VRi(vd).dword[3] == 0xFFFFFFFF) {
      allEqual = true;
    }

    if (VRi(vd).dword[0] == 0 && VRi(vd).dword[1] == 0 && VRi(vd).dword[2] == 0 && VRi(vd).dword[3] == 0) {
      allNotEqual = true;
    }

    crValue |= allEqual ? 0b1000 : 0;
    crValue |= allNotEqual ? 0b0010 : 0;

    ppcUpdateCR(ppuState, 6, crValue);
  }
}

// Vector128 Convert From Signed Fixed-Point Word to Floating-Point
void PPCInterpreter::PPCInterpreter_vcsxwfp128(PPU_STATE* ppuState) {
  CHECK_VXU;

  // (VD) <- float(VB as signed) / 2^uimm

  double div = (double)(1ULL << VMX128_3_IMM);
  VR(VMX128_3_VD128).flt[0] = (float)(VR(VMX128_3_VB128).dsword[0] / div);
  VR(VMX128_3_VD128).flt[1] = (float)(VR(VMX128_3_VB128).dsword[1] / div);
  VR(VMX128_3_VD128).flt[2] = (float)(VR(VMX128_3_VB128).dsword[2] / div);
  VR(VMX128_3_VD128).flt[3] = (float)(VR(VMX128_3_VB128).dsword[3] / div);
}

static inline s32 vcfpsxwsHelper(const double inFloat) {
  if (inFloat < (double)INT_MIN) { return INT_MIN; }
  else if (inFloat > (double)INT_MAX) { return INT_MAX; }
  else { return (s32)inFloat; }
}

// Vector128 Convert From Floating-Point to Signed Fixed - Point Word Saturate
void PPCInterpreter::PPCInterpreter_vcfpsxws128(PPU_STATE* ppuState) {
  // (VD) <- int_sat(VB as signed * 2^uimm)

  // TODO: Check correct behavior and if the SAT bit should be set, logic should be okay.

  CHECK_VXU;

  u32 uimm = VMX128_3_IMM;
  float fltuimm = static_cast<float>(std::exp2(uimm));

  VR(VMX128_3_VD128).flt[0] = vcfpsxwsHelper(static_cast<double>(VR(VMX128_3_VB128).flt[0] * fltuimm));
  VR(VMX128_3_VD128).flt[1] = vcfpsxwsHelper(static_cast<double>(VR(VMX128_3_VB128).flt[1] * fltuimm));
  VR(VMX128_3_VD128).flt[2] = vcfpsxwsHelper(static_cast<double>(VR(VMX128_3_VB128).flt[2] * fltuimm));
  VR(VMX128_3_VD128).flt[3] = vcfpsxwsHelper(static_cast<double>(VR(VMX128_3_VB128).flt[3] * fltuimm));
}

static inline float vexptefpHelper(const float inFloat) {
  if (inFloat == -std::numeric_limits<float>::infinity()) return 0.0f;
  if (inFloat == std::numeric_limits<float>::infinity()) return std::numeric_limits<float>::infinity();
  return powf(2.0f, inFloat);
}

// Vector 2 Raised to the Exponent Estimate Floating Point (x'1000 018A')
void PPCInterpreter::PPCInterpreter_vexptefp(PPU_STATE* ppuState) {
  // NOTE: Checked against Xenia's tests.
  
  CHECK_VXU;

  VRi(vd).flt[0] = vexptefpHelper(VRi(vb).flt[0]);
  VRi(vd).flt[1] = vexptefpHelper(VRi(vb).flt[1]);
  VRi(vd).flt[2] = vexptefpHelper(VRi(vb).flt[2]);
  VRi(vd).flt[3] = vexptefpHelper(VRi(vb).flt[3]);
}

// Vector128 2 Raised to the Exponent Estimate Floating Point
void PPCInterpreter::PPCInterpreter_vexptefp128(PPU_STATE* ppuState) {
  // (VD) <- pow2(VB)

  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;
  
  VR(VMX128_3_VD128).flt[0] = vexptefpHelper(VR(VMX128_3_VB128).flt[0]);
  VR(VMX128_3_VD128).flt[1] = vexptefpHelper(VR(VMX128_3_VB128).flt[1]);
  VR(VMX128_3_VD128).flt[2] = vexptefpHelper(VR(VMX128_3_VB128).flt[2]);
  VR(VMX128_3_VD128).flt[3] = vexptefpHelper(VR(VMX128_3_VB128).flt[3]);
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

// Vector128 Logical OR
void PPCInterpreter::PPCInterpreter_vor128(PPU_STATE* ppuState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] | VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] | VR(VMX128_VB128).dword[1];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] | VR(VMX128_VB128).dword[2];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] | VR(VMX128_VB128).dword[3];
}

// Vector Splat Word (x'1000 028C')
void PPCInterpreter::PPCInterpreter_vspltw(PPU_STATE *ppuState) {
  /*
   b <- UIMM*32
    do i=0 to 127 by 32
    vDi:i+31 <- (vB)b:b+31
    end
  */

  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  const u8 b = (_instr.vuimm & 0x3);

  for (u8 idx = 0; idx < 4; ++idx) {
    VRi(vd).dword[idx] = VRi(vb).dword[b];
  }
}

// Vector Splat Word 128
void PPCInterpreter::PPCInterpreter_vspltw128(PPU_STATE* ppuState) {
  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  const u8 b = (VMX128_3_IMM & 0x3);

  for (u8 idx = 0; idx < 4; ++idx) {
    VR(VMX128_3_VD128).dword[idx] = VR(VMX128_3_VB128).dword[b];
  }
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

// Vector Maximum Signed Word (x'1000 0182')
void PPCInterpreter::PPCInterpreter_vmaxsw(PPU_STATE* ppuState) {
  // TODO: Check behavior, logic seems ok.

  CHECK_VXU;

  VRi(vd).dword[0] = (VRi(va).dsword[0] > VRi(vb).dsword[0]) ? VRi(va).dsword[0] : VRi(vb).dsword[0];
  VRi(vd).dword[1] = (VRi(va).dsword[1] > VRi(vb).dsword[1]) ? VRi(va).dsword[1] : VRi(vb).dsword[1];
  VRi(vd).dword[2] = (VRi(va).dsword[2] > VRi(vb).dsword[2]) ? VRi(va).dsword[2] : VRi(vb).dsword[2];
  VRi(vd).dword[3] = (VRi(va).dsword[3] > VRi(vb).dsword[3]) ? VRi(va).dsword[3] : VRi(vb).dsword[3];
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

// Vector128 Maximum Floating-Point
void PPCInterpreter::PPCInterpreter_vmaxfp128(PPU_STATE* ppuState) {
  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = (VR(VMX128_VA128).flt[0] > VR(VMX128_VB128).flt[0]) ? VR(VMX128_VA128).flt[0] : VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = (VR(VMX128_VA128).flt[1] > VR(VMX128_VB128).flt[1]) ? VR(VMX128_VA128).flt[1] : VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = (VR(VMX128_VA128).flt[2] > VR(VMX128_VB128).flt[2]) ? VR(VMX128_VA128).flt[2] : VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = (VR(VMX128_VA128).flt[3] > VR(VMX128_VB128).flt[3]) ? VR(VMX128_VA128).flt[3] : VR(VMX128_VB128).flt[3];
}

// Vector128 Minimum Floating-Point
void PPCInterpreter::PPCInterpreter_vminfp128(PPU_STATE* ppuState) {
  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = (VR(VMX128_VA128).flt[0] < VR(VMX128_VB128).flt[0]) ? VR(VMX128_VA128).flt[0] : VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = (VR(VMX128_VA128).flt[1] < VR(VMX128_VB128).flt[1]) ? VR(VMX128_VA128).flt[1] : VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = (VR(VMX128_VA128).flt[2] < VR(VMX128_VB128).flt[2]) ? VR(VMX128_VA128).flt[2] : VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = (VR(VMX128_VA128).flt[3] < VR(VMX128_VB128).flt[3]) ? VR(VMX128_VA128).flt[3] : VR(VMX128_VB128).flt[3];
}

// Vector128 Merge Low Word
void PPCInterpreter::PPCInterpreter_vmrglw128(PPU_STATE* ppuState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[2];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VB128).dword[2];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[3];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VB128).dword[3];
}

static const u8 reIndex[32] = { 3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12, 19,18,17,16, 23,22,21,20, 27,26,25,24, 31,30,29,28 };

static inline u8 vpermHelper(u8 idx, Base::Vector128 vra, Base::Vector128 vrb) {
  return (idx & 16) ? vrb.bytes[idx & 0xF] : vra.bytes[idx & 0xF];
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

  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  auto bytes = VRi(vc).bytes;

  // Truncate in case of bigger than 32 value.
  for (auto& byte : bytes) {
    byte &= 0x1F;
  }

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vpermHelper(reIndex[bytes[reIndex[idx]]], VRi(va), VRi(rb));
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);
}

// Vector Permute 128
void PPCInterpreter::PPCInterpreter_vperm128(PPU_STATE* ppuState) {
  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  auto bytes = VR(VMX128_2_VC).bytes;

  // Truncate in case of bigger than 32 value.
  for (auto& byte : bytes) {
    byte &= 0x1F;
  }

  for (u8 idx = 0; idx < 16; idx++) {
    VR(VMX128_2_VD128).bytes[idx] = vpermHelper(reIndex[bytes[reIndex[idx]]], VR(VMX128_2_VA128), VR(VMX128_2_VB128));
  }

  VR(VMX128_2_VD128).dword[0] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[0]);
  VR(VMX128_2_VD128).dword[1] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[1]);
  VR(VMX128_2_VD128).dword[2] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[2]);
  VR(VMX128_2_VD128).dword[3] = byteswap_be<u32>(VR(VMX128_2_VD128).dword[3]);
}

// Vector128 Permutate Word Immediate
void PPCInterpreter::PPCInterpreter_vpermwi128(PPU_STATE* ppuState) {
  // (VD.x) = (VB.uimm[6-7])
  // (VD.y) = (VB.uimm[4-5])
  // (VD.z) = (VB.uimm[2-3])
  // (VD.w) = (VB.uimm[0-1])

  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  const u32 vrd = _instr.VMX128_P.VD128l | (_instr.VMX128_P.VD128h << 5);
  const u32 vrb = _instr.VMX128_P.VB128l | (_instr.VMX128_P.VB128h << 5);
  u32 uimm = _instr.VMX128_P.PERMl | (_instr.VMX128_P.PERMh << 5);

  VR(vrd).dword[0] = VR(vrb).dword[(uimm >> 6) & 3];
  VR(vrd).dword[1] = VR(vrb).dword[(uimm >> 4) & 3];
  VR(vrd).dword[2] = VR(vrb).dword[(uimm >> 2) & 3];
  VR(vrd).dword[3] = VR(vrb).dword[(uimm >> 0) & 3];
}

// Vector128 Rotate Left Immediate and Mask Insert
void PPCInterpreter::PPCInterpreter_vrlimi128(PPU_STATE* ppuState) {

  /*
  From Xenia:
  This is just a fancy permute.
  X Y Z W, rotated left by 2 = Z W X Y
  Then mask select the results into the dest.
  Sometimes rotation is zero, so fast path.
  */

  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  const uint32_t vd = _instr.VMX128_4.VD128l | (_instr.VMX128_4.VD128h << 5);
  const uint32_t vb = _instr.VMX128_4.VB128l | (_instr.VMX128_4.VB128h << 5);
  uint32_t blendMaskSource = _instr.VMX128_4.IMM;
  uint32_t blendMask = 0;
  blendMask |= (((blendMaskSource >> 3) & 0x1) ? 0 : 4) << 0;
  blendMask |= (((blendMaskSource >> 2) & 0x1) ? 1 : 5) << 8;
  blendMask |= (((blendMaskSource >> 1) & 0x1) ? 2 : 6) << 16;
  blendMask |= (((blendMaskSource >> 0) & 0x1) ? 3 : 7) << 24;
  uint32_t rotate = _instr.VMX128_4.z;

  Base::Vector128 result = {};

  if (rotate) {
    switch (rotate)
    {
    case 1: // X Y Z W -> Y Z W X
      result.flt[0] = VR(vb).flt[1];
      result.flt[1] = VR(vb).flt[2];
      result.flt[2] = VR(vb).flt[3];
      result.flt[3] = VR(vb).flt[0];
      break;
    case 2: // X Y Z W -> Z W X Y
      result.flt[0] = VR(vb).flt[2];
      result.flt[1] = VR(vb).flt[3];
      result.flt[2] = VR(vb).flt[0];
      result.flt[3] = VR(vb).flt[1];
      break;
    case 3: // X Y Z W -> W X Y Z
      result.flt[0] = VR(vb).flt[3];
      result.flt[1] = VR(vb).flt[0];
      result.flt[2] = VR(vb).flt[1];
      result.flt[3] = VR(vb).flt[2];
      break;
    default:
      LOG_ERROR(Xenos, "vrlimi128: Unhandled rotate amount.");
      break;
    }
  } else {
    result = VR(vb);
  }

  if (blendMask != 0x03020100) {
    result.dword[3] = ((blendMask & 0xFF000000) == 0x03000000 ? result.dword[3] : VR(vd).dword[3]);
    result.dword[2] = ((blendMask & 0x00FF0000) == 0x00020000 ? result.dword[2] : VR(vd).dword[2]);
    result.dword[1] = ((blendMask & 0x0000FF00) == 0x00000100 ? result.dword[1] : VR(vd).dword[1]);
    result.dword[0] = ((blendMask & 0x000000FF) == 0x00000000 ? result.dword[0] : VR(vd).dword[0]);
  }

  VR(vd) = result;
}

// Vector Round to Floating - Point Integer Nearest (x'1000 020A')
void PPCInterpreter::PPCInterpreter_vrfin(PPU_STATE* ppuState) {
  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  VRi(vd).flt[0] = round(VRi(vb).flt[0]);
  VRi(vd).flt[1] = round(VRi(vb).flt[1]);
  VRi(vd).flt[2] = round(VRi(vb).flt[2]);
  VRi(vd).flt[3] = round(VRi(vb).flt[3]);

}

// Vector128 Round to Floating - Point Integer Nearest
void PPCInterpreter::PPCInterpreter_vrfin128(PPU_STATE* ppuState) {
  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  VR(VMX128_3_VD128).flt[0] = round(VR(VMX128_3_VB128).flt[0]);
  VR(VMX128_3_VD128).flt[1] = round(VR(VMX128_3_VB128).flt[1]);
  VR(VMX128_3_VD128).flt[2] = round(VR(VMX128_3_VB128).flt[2]);
  VR(VMX128_3_VD128).flt[3] = round(VR(VMX128_3_VB128).flt[3]);
}

// Vector Multiply Add Floating Point (x'1000 002E')
void PPCInterpreter::PPCInterpreter_vmaddfp(PPU_STATE *ppuState) {
  /*
  do i=0 to 127 by 32
    vDi:i+31 <- RndToNearFP32(((vA)i:i+31 *fp (vC)i:i+31) +fp (vB)i:i+31)
  end
  */

  // NOTE: Checked against Xenia's tests.
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

  // NOTE: Checked against Xenia's tests.

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

  // NOTE: Checked against Xenia's tests.

  VRi(rd).dword[0] = VRi(ra).dword[0] << (VRi(rb).dword[0] & 31);
  VRi(rd).dword[1] = VRi(ra).dword[1] << (VRi(rb).dword[1] & 31);
  VRi(rd).dword[2] = VRi(ra).dword[2] << (VRi(rb).dword[2] & 31);
  VRi(rd).dword[3] = VRi(ra).dword[3] << (VRi(rb).dword[3] & 31);
}

// Vector128 Shift Left Word
void PPCInterpreter::PPCInterpreter_vslw128(PPU_STATE* ppuState) {
  CHECK_VXU;

  // NOTE: Checked against Xenia's tests.

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] << (VR(VMX128_VB128).dword[0] & 31);
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] << (VR(VMX128_VB128).dword[1] & 31);
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] << (VR(VMX128_VB128).dword[2] & 31);
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] << (VR(VMX128_VB128).dword[3] & 31);
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

  // NOTE: Checked against Xenia's tests.

  CHECK_VXU;

  const u8 sh = VRi(rb).bytes[15] & 0x7;

  Base::Vector128 res = VRi(va);

  for (int i = 15; i > 0; --i) {
    res.bytes[i ^ 0x3] = (res.bytes[i ^ 0x3] >> sh) |
      (res.bytes[(i - 1) ^ 0x3] << (8 - sh));
  }

  VRi(vd) = res;
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

// Vector128 Shift Right Word
void PPCInterpreter::PPCInterpreter_vsrw128(PPU_STATE* ppuState) {

  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] >> (VR(VMX128_VB128).dword[0] & 31);
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] >> (VR(VMX128_VB128).dword[1] & 31);
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] >> (VR(VMX128_VB128).dword[2] & 31);
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] >> (VR(VMX128_VB128).dword[3] & 31);
}

// Vector128 Shift Right Arithmetic Word
void PPCInterpreter::PPCInterpreter_vsraw128(PPU_STATE* ppuState) {
  // TODO: Verify behavior, logic seems ok.

  CHECK_VXU;

  VR(VMX128_VD128).dsword[0] = VR(VMX128_VA128).dsword[0] >> (VR(VMX128_VB128).dsword[0] & 31);
  VR(VMX128_VD128).dsword[1] = VR(VMX128_VA128).dsword[1] >> (VR(VMX128_VB128).dsword[1] & 31);
  VR(VMX128_VD128).dsword[2] = VR(VMX128_VA128).dsword[2] >> (VR(VMX128_VB128).dsword[2] & 31);
  VR(VMX128_VD128).dsword[3] = VR(VMX128_VA128).dsword[3] >> (VR(VMX128_VB128).dsword[3] & 31);
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

  CHECK_VXU;

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

  Base::Vector128 vra = VRi(va);
  Base::Vector128 vrb = VRi(vb);

#if defined(FIX) // Fix this, causes incorrect behavior.
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

  // NOTE: Checked against Xenia's tests.

  vra.dword[0] = byteswap_be<u32>(vra.dword[0]);
  vra.dword[1] = byteswap_be<u32>(vra.dword[1]);
  vra.dword[2] = byteswap_be<u32>(vra.dword[2]);
  vra.dword[3] = byteswap_be<u32>(vra.dword[3]);

  vrb.dword[0] = byteswap_be<u32>(vrb.dword[0]);
  vrb.dword[1] = byteswap_be<u32>(vrb.dword[1]);
  vrb.dword[2] = byteswap_be<u32>(vrb.dword[2]);
  vrb.dword[3] = byteswap_be<u32>(vrb.dword[3]);

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vsldoiHelper(sh + idx, vra, vrb);
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);
#endif
}

// Vector128 Shift Left Double by Octet Immediate
void PPCInterpreter::PPCInterpreter_vsldoi128(PPU_STATE* ppuState) {
  CHECK_VXU;

  const u8 sh = VMX128_5_SH;
  // No shift
  if (sh == 0) {
    VR(VMX128_5_VD128) = VR(VMX128_5_VA128);
    return;
  }
  else if (sh == 16) {
    // Don't touch VA
    VR(VMX128_5_VD128) = VR(VMX128_5_VB128);
    return;
  }

  Base::Vector128 vra = VR(VMX128_5_VA128);
  Base::Vector128 vrb = VR(VMX128_5_VB128);

#if defined(FIX)
  __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(VR(VMX128_5_VA128).bytes.data()));
  __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(VR(VMX128_5_VB128).bytes.data()));
  __m128i vd_sse = vsldoi_sse(va, vb, sh);

  // Store raw result before byte swap
  _mm_storeu_si128(reinterpret_cast<__m128i*>(VR(VMX128_5_VD128).bytes.data()), vd_sse);
  // Swap
  __m128i result = _mm_loadu_si128(reinterpret_cast<__m128i*>(VR(VMX128_5_VD128).bytes.data()));
  result = byteswap_be_u32x4_ssse3(vd_sse);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(VR(VMX128_5_VD128).bytes.data()), result);
#else
  // TODO: Ugly, super slow, fix.

  // NOTE: Checked against Xenia's tests.

  vra.dword[0] = byteswap_be<u32>(vra.dword[0]);
  vra.dword[1] = byteswap_be<u32>(vra.dword[1]);
  vra.dword[2] = byteswap_be<u32>(vra.dword[2]);
  vra.dword[3] = byteswap_be<u32>(vra.dword[3]);

  vrb.dword[0] = byteswap_be<u32>(vrb.dword[0]);
  vrb.dword[1] = byteswap_be<u32>(vrb.dword[1]);
  vrb.dword[2] = byteswap_be<u32>(vrb.dword[2]);
  vrb.dword[3] = byteswap_be<u32>(vrb.dword[3]);

  for (u8 idx = 0; idx < 16; idx++) {
    VR(VMX128_5_VD128).bytes[idx] = vsldoiHelper(sh + idx, vra, vrb);
  }

  VR(VMX128_5_VD128).dword[0] = byteswap_be<u32>(VR(VMX128_5_VD128).dword[0]);
  VR(VMX128_5_VD128).dword[1] = byteswap_be<u32>(VR(VMX128_5_VD128).dword[1]);
  VR(VMX128_5_VD128).dword[2] = byteswap_be<u32>(VR(VMX128_5_VD128).dword[2]);
  VR(VMX128_5_VD128).dword[3] = byteswap_be<u32>(VR(VMX128_5_VD128).dword[3]);
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

  // Need to byteswap becuase of byte endianness.
  Base::Vector128 vec = VRi(vb);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  const u8 uimm = _instr.vuimm;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vec.bytes[uimm];
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

// Vector128 Subtract Floating-Point
void PPCInterpreter::PPCInterpreter_vsubfp128(PPU_STATE* ppuState) {
  CHECK_VXU;

  // NOTE: Checked against Xenia's tests.
  // TODO: Rounding to Near.

  VR(VMX128_VD128).flt[0] = VR(VMX128_VA128).flt[0] - VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = VR(VMX128_VA128).flt[1] - VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = VR(VMX128_VA128).flt[2] - VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = VR(VMX128_VA128).flt[3] - VR(VMX128_VB128).flt[3];
}

// Vector128 Multiply Sum 4-way Floating-Point
void PPCInterpreter::PPCInterpreter_vmsum4fp128(PPU_STATE* ppuState) {
  CHECK_VXU;

  // Dot product XYZW.
  // (VD.xyzw) = (VA.x * VB.x) + (VA.y * VB.y) + (VA.z * VB.z) + (VA.w * VB.w)

  // NOTE: Checked against Xenia's tests.

  float dotProduct = (VR(VMX128_VA128).flt[0] * VR(VMX128_VB128).flt[0]) +
    (VR(VMX128_VA128).flt[1] * VR(VMX128_VB128).flt[1]) +
    (VR(VMX128_VA128).flt[2] * VR(VMX128_VB128).flt[2]) +
    (VR(VMX128_VA128).flt[3] * VR(VMX128_VB128).flt[3]);

  for (u8 idx = 0; idx < 4; idx++) {
    VR(VMX128_3_VD128).flt[idx] = dotProduct;
  }
}

enum ePackType : u32 {
  PACK_TYPE_D3DCOLOR = 0,
  PACK_TYPE_FLOAT16_2 = 1,
  PACK_TYPE_SHORT_4 = 2,
  PACK_TYPE_FLOAT16_4 = 3,
  PACK_TYPE_SHORT_2 = 4,
  PACK_TYPE_UINT_2101010 = 5,
};

static inline float MakePackedFloatUnsigned(const u32 x){
  union {
    float f;
    u32 u;
  } ret;

  ret.f = 1.0f;
  ret.u |= x;
  return ret.f;
}

// Vector128 Unpack D3Dtype
void PPCInterpreter::PPCInterpreter_vupkd3d128(PPU_STATE* ppuState) {
  //CHECK_VXU;

  // Research from Xenia:
  // Can't find many docs on this. Best reference is
  // http://worldcraft.googlecode.com/svn/trunk/src/qylib/math/xmmatrix.inl,
  // which shows how it's used in some cases. Since it's all intrinsics,
  // finding it in code is pretty easy.
  const uint32_t vrd = _instr.VMX128_3.VD128l | (_instr.VMX128_3.VD128h << 5);
  const uint32_t vrb = _instr.VMX128_3.VB128l | (_instr.VMX128_3.VB128h << 5);
  const uint32_t packType = _instr.VMX128_3.IMM >> 2;
  const u32 val = VR(vrb).dword[3];

  // NOTE: Implemented means it was tested against xenia's tests.

  switch (packType)
  {
  case PACK_TYPE_D3DCOLOR:
    LOG_DEBUG(Xenon, "VXU[vupkd3d128]: Pack type: PACK_TYPE_D3DCOLOR");
    VR(vrd).flt[0] = MakePackedFloatUnsigned((val >> 16) & 0xFF);
    VR(vrd).flt[1] = MakePackedFloatUnsigned((val >> 8) & 0xFF);
    VR(vrd).flt[2] = MakePackedFloatUnsigned((val >> 0) & 0xFF);
    VR(vrd).flt[3] = MakePackedFloatUnsigned((val >> 24) & 0xFF);
    break;
  case PACK_TYPE_FLOAT16_2:
    LOG_WARNING(Xenon, "VXU[vupkd3d128]: UNIMPLEMENTED Pack type: PACK_TYPE_FLOAT16_2");
    break;
  case PACK_TYPE_SHORT_4:
    LOG_WARNING(Xenon, "VXU[vupkd3d128]: UNIMPLEMENTED Pack type: PACK_TYPE_SHORT_4");
    break;
  case PACK_TYPE_FLOAT16_4:
    LOG_WARNING(Xenon, "VXU[vupkd3d128]: UNIMPLEMENTED Pack type: PACK_TYPE_FLOAT16_4");
    break;
  case PACK_TYPE_SHORT_2:
    LOG_WARNING(Xenon, "VXU[vupkd3d128]: UNIMPLEMENTED Pack type: PACK_TYPE_SHORT_2");
    break;
  case PACK_TYPE_UINT_2101010:
    LOG_WARNING(Xenon, "VXU[vupkd3d128]: UNIMPLEMENTED Pack type: PACK_TYPE_UINT_2101010");
    break;
  default:
    LOG_ERROR(Xenon, "VXU[vupkd3d128]: Unknown Pack Type. Please report to Xenon devs.");
    break;
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
