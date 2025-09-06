// Copyright 2025 Xenon Emulator Project. All rights reserved.
#include <cmath>

#include "PPCInterpreter.h"

// Data Stream Touch for Store
void PPCInterpreter::PPCInterpreter_dss(sPPEState *ppeState) {
  CHECK_VXU;

  // We don't really need to do anything here, as it's handling cache. We mostly ignore it
}

// Data Stream Touch for Store
void PPCInterpreter::PPCInterpreter_dst(sPPEState *ppeState) {
  CHECK_VXU;

  // We don't really need to do anything here, as it's handling cache. We mostly ignore it
}

// Data Stream Touch for Store
void PPCInterpreter::PPCInterpreter_dstst(sPPEState *ppeState) {
  CHECK_VXU;

  // We don't really need to do anything here, as it's handling cache. We mostly ignore it
}

// Move from Vector Status and Control Register (x'1000 0604')
void PPCInterpreter::PPCInterpreter_mfvscr(sPPEState *ppeState) {
  /*
  vD <- 0 || (VSCR)
  */

  CHECK_VXU;

  VRi(vd).dword[3] = curThread.VSCR.hexValue;
}

// Move to Vector Status and Control Register (x'1000 0C44')
void PPCInterpreter::PPCInterpreter_mtvscr(sPPEState *ppeState) {
  /*
  VSCR <- (vB)96:127
  */

  CHECK_VXU;

  curThread.VSCR.hexValue = VRi(vb).dword[3];
}

// Vector Add Floating Point (x'1000 000A')
void PPCInterpreter::PPCInterpreter_vaddfp(sPPEState *ppeState) {
  // TODO: Rounding and NJ mode check.

  CHECK_VXU;

  VRi(vd).flt[0] = VRi(va).flt[0] + VRi(vb).flt[0];
  VRi(vd).flt[1] = VRi(va).flt[1] + VRi(vb).flt[1];
  VRi(vd).flt[2] = VRi(va).flt[2] + VRi(vb).flt[2];
  VRi(vd).flt[3] = VRi(va).flt[3] + VRi(vb).flt[3];
}

// Vector128 Add Floating Point
void PPCInterpreter::PPCInterpreter_vaddfp128(sPPEState* ppeState) {
  /*
    do i = 0,127,32
      (vD)i:i+31 <- RndToNearFP32((vA)i:i+31 + fp (vB)i:i+31)
    end
  */

  // TODO: Rounding and NJ mode check.

  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = VR(VMX128_VA128).flt[0] + VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = VR(VMX128_VA128).flt[1] + VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = VR(VMX128_VA128).flt[2] + VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = VR(VMX128_VA128).flt[3] + VR(VMX128_VB128).flt[3];
}

static inline u8 vecSaturateU8(sPPEState *ppeState, u32 inValue) {
  if (inValue > 255) {
    // Set SAT bit in VSCR and truncate to 255.
    curThread.VSCR.SAT = 1;
    inValue = 255;
  }
  return static_cast<u8>(inValue);
}

static inline s16 vecSaturateS16(sPPEState* ppeState, s32 inValue) {
  if (inValue > 32767) {
    // Set SAT bit in VSCR and truncate to 32767.
    curThread.VSCR.SAT = 1;
    inValue = 32767;
  } else if(inValue < -32768){
    // Set SAT bit in VSCR and truncate to -32768.
    curThread.VSCR.SAT = 1;
    inValue = -32768;
  }
  return static_cast<s16>(inValue);
}

static inline s32 vecSaturateS32(sPPEState* ppeState, s64 inValue) {
  if (inValue < INT32_MIN) {
    curThread.VSCR.SAT = 1;
    inValue = INT32_MIN;
  } else if (inValue > INT32_MAX) {
    curThread.VSCR.SAT = 1;
    inValue = INT32_MAX;
  }
  return static_cast<s32>(inValue);
}

static inline u32 vecSaturateU32(sPPEState* ppeState, u64 inValue) {
  if (inValue > UINT_MAX) {
    // Set SAT bit in VSCR and truncate to 2^32 - 1.
    curThread.VSCR.SAT = 1;
    inValue = UINT_MAX;
  }
  return static_cast<u32>(inValue);
}

// Vector Add Unsigned Byte Saturate ('x1000 0200')
void PPCInterpreter::PPCInterpreter_vaddubs(sPPEState *ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = vecSaturateU8(ppeState, static_cast<u32>(VRi(va).bytes[idx]) + static_cast<u32>(VRi(vb).bytes[idx]));
  }
}

// Vector Add Unsigned Halfword Modulo (0x1000 0040)
void PPCInterpreter::PPCInterpreter_vadduhm(sPPEState* ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).word[idx] = VRi(va).word[idx] + VRi(vb).word[idx];
  }
}

// Vector Add Unsigned Word Saturate (x'1000 0280')
void PPCInterpreter::PPCInterpreter_vadduws(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = vecSaturateU32(ppeState, static_cast<u64>(VRi(va).dword[0]) + static_cast<u64>(VRi(vb).dword[0]));
  VRi(vd).dword[1] = vecSaturateU32(ppeState, static_cast<u64>(VRi(va).dword[1]) + static_cast<u64>(VRi(vb).dword[1]));
  VRi(vd).dword[2] = vecSaturateU32(ppeState, static_cast<u64>(VRi(va).dword[2]) + static_cast<u64>(VRi(vb).dword[2]));
  VRi(vd).dword[3] = vecSaturateU32(ppeState, static_cast<u64>(VRi(va).dword[3]) + static_cast<u64>(VRi(vb).dword[3]));
}

// Vector Add Signed Halfword Saturate(0x1000 0340)
void PPCInterpreter::PPCInterpreter_vaddshs(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).sword[0] = vecSaturateS16(ppeState, static_cast<u32>(VRi(va).sword[0]) + static_cast<u32>(VRi(vb).sword[0]));
  VRi(vd).sword[1] = vecSaturateS16(ppeState, static_cast<u32>(VRi(va).sword[1]) + static_cast<u32>(VRi(vb).sword[1]));
  VRi(vd).sword[2] = vecSaturateS16(ppeState, static_cast<u32>(VRi(va).sword[2]) + static_cast<u32>(VRi(vb).sword[2]));
  VRi(vd).sword[3] = vecSaturateS16(ppeState, static_cast<u32>(VRi(va).sword[3]) + static_cast<u32>(VRi(vb).sword[3]));
}

// Vector Average Unsigned Halfword (x'1000 0442')
void PPCInterpreter::PPCInterpreter_vavguh(sPPEState* ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).word[idx] = static_cast<u16>((static_cast<u32>(VRi(va).word[idx]) + static_cast<u32>(VRi(vb).word[idx]) + 1) >> 1);
  }
}

// Vector Logical AND (x'1000 0404')
void PPCInterpreter::PPCInterpreter_vand(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] & VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] & VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] & VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] & VRi(vb).dword[3];
}

// Vector128 Logical AND
void PPCInterpreter::PPCInterpreter_vand128(sPPEState* ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] & VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] & VR(VMX128_VB128).dword[1];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] & VR(VMX128_VB128).dword[2];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] & VR(VMX128_VB128).dword[3];
}

// Vector Logical AND with Complement (x'1000 0444')
void PPCInterpreter::PPCInterpreter_vandc(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] & ~VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] & ~VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] & ~VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] & ~VRi(vb).dword[3];
}

// Vector128 Logical AND with Complement
void PPCInterpreter::PPCInterpreter_vandc128(sPPEState* ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] & ~VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] & ~VR(VMX128_VB128).dword[1];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] & ~VR(VMX128_VB128).dword[2];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] & ~VR(VMX128_VB128).dword[3];
}

// Vector Convert to Signed Fixed-Point Word Saturate (x'1000 03CA')
void PPCInterpreter::PPCInterpreter_vctsxs(sPPEState* ppeState) {
  CHECK_VXU;

  f32 fuimm = static_cast<f32>(std::exp2(static_cast<u32>(_instr.vuimm)));
  VRi(vd).dsword[0] = vecSaturateS32(ppeState, VRi(vb).flt[0] * fuimm);
  VRi(vd).dsword[1] = vecSaturateS32(ppeState, VRi(vb).flt[1] * fuimm);
  VRi(vd).dsword[2] = vecSaturateS32(ppeState, VRi(vb).flt[2] * fuimm);
  VRi(vd).dsword[3] = vecSaturateS32(ppeState, VRi(vb).flt[3] * fuimm);
}

// Vector Convert to Unsigned Fixed-Point Word Saturate (x'1000 038A')
void PPCInterpreter::PPCInterpreter_vctuxs(sPPEState* ppeState) {
  CHECK_VXU;

  f32 fuimm = static_cast<f32>(std::exp2(static_cast<u32>(_instr.vuimm)));
  VRi(vd).dsword[0] = vecSaturateU32(ppeState, static_cast<s64>(static_cast<f64>(VRi(vb).flt[0]) * fuimm));
  VRi(vd).dsword[1] = vecSaturateU32(ppeState, static_cast<s64>(static_cast<f64>(VRi(vb).flt[1]) * fuimm));
  VRi(vd).dsword[2] = vecSaturateU32(ppeState, static_cast<s64>(static_cast<f64>(VRi(vb).flt[2]) * fuimm));
  VRi(vd).dsword[3] = vecSaturateU32(ppeState, static_cast<s64>(static_cast<f64>(VRi(vb).flt[3]) * fuimm));
}

// Vector convert from Signed Fixed-Point Word (x'1000 034A')
void PPCInterpreter::PPCInterpreter_vcfsx(sPPEState* ppeState) {
  CHECK_VXU;

  float fltUimm = std::ldexp(1.0f, -int(_instr.vuimm));
  VRi(vd).flt[0] = static_cast<f32>(VRi(vb).dsword[0]) * fltUimm;
  VRi(vd).flt[1] = static_cast<f32>(VRi(vb).dsword[1]) * fltUimm;
  VRi(vd).flt[2] = static_cast<f32>(VRi(vb).dsword[2]) * fltUimm;
  VRi(vd).flt[3] = static_cast<f32>(VRi(vb).dsword[3]) * fltUimm;
}

// Vector Convert from Unsigned Fixed-Point Word (x'1000 030A')
void PPCInterpreter::PPCInterpreter_vcfux(sPPEState *ppeState) {
  // TODO: Rounding

  CHECK_VXU;

  const f32 divisor = 1 << _instr.vuimm;

  VRi(vd).flt[0] = VRi(va).dword[0] / divisor;
  VRi(vd).flt[1] = VRi(va).dword[1] / divisor;
  VRi(vd).flt[2] = VRi(va).dword[2] / divisor;
  VRi(vd).flt[3] = VRi(va).dword[3] / divisor;
}

static u32 vcmpbfpHelper(const f32 fra, const f32 frb) {
  u32 returnValue = 0;
  static const u32 retLE = 0x80000000;
  static const u32 retGE = 0x40000000;
  if (std::isnan(fra) || std::isnan(frb))
    return retLE | retGE;

  returnValue |= (fra <= frb ? 0 : retLE);
  returnValue |= (fra >= -frb ? 0 : retGE);

  return returnValue;
}

// Vector Compare Bounds Floating Point (x'1000 03C6')
void PPCInterpreter::PPCInterpreter_vcmpbfp(sPPEState* ppeState) {
  CHECK_VXU;

  u32 regMask = 0;
  regMask |= VRi(vd).dword[0] = vcmpbfpHelper(VRi(va).flt[0], VRi(vb).flt[0]);
  regMask |= VRi(vd).dword[1] = vcmpbfpHelper(VRi(va).flt[1], VRi(vb).flt[1]);
  regMask |= VRi(vd).dword[2] = vcmpbfpHelper(VRi(va).flt[2], VRi(vb).flt[2]);
  regMask |= VRi(vd).dword[3] = vcmpbfpHelper(VRi(va).flt[3], VRi(vb).flt[3]);

  if (_instr.vrc) {
    u8 crValue = 0;
    crValue = (regMask == 0) ? 0b0010 : 0b0000;
    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector128 Compare Bounds Floating Point
void PPCInterpreter::PPCInterpreter_vcmpbfp128(sPPEState* ppeState) {
  CHECK_VXU;

  u32 regMask = 0;
  regMask |= VR(VMX128_R_VD128).dword[0] = vcmpbfpHelper(VR(VMX128_R_VA128).flt[0], VR(VMX128_R_VB128).flt[0]);
  regMask |= VR(VMX128_R_VD128).dword[1] = vcmpbfpHelper(VR(VMX128_R_VA128).flt[1], VR(VMX128_R_VB128).flt[1]);
  regMask |= VR(VMX128_R_VD128).dword[2] = vcmpbfpHelper(VR(VMX128_R_VA128).flt[2], VR(VMX128_R_VB128).flt[2]);
  regMask |= VR(VMX128_R_VD128).dword[3] = vcmpbfpHelper(VR(VMX128_R_VA128).flt[3], VR(VMX128_R_VB128).flt[3]);

  if (_instr.v128rc) {
    u8 crValue = 0;
    crValue = (regMask == 0) ? 0b0010 : 0b0000;
    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector Compare Equal-to-Floating Point (x'1000 00C6')
void PPCInterpreter::PPCInterpreter_vcmpeqfp(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = ((VRi(va).flt[0] == VRi(vb).flt[0]) ? 0xFFFFFFFF : 0x00000000);
  VRi(vd).dword[1] = ((VRi(va).flt[1] == VRi(vb).flt[1]) ? 0xFFFFFFFF : 0x00000000);
  VRi(vd).dword[2] = ((VRi(va).flt[2] == VRi(vb).flt[2]) ? 0xFFFFFFFF : 0x00000000);
  VRi(vd).dword[3] = ((VRi(va).flt[3] == VRi(vb).flt[3]) ? 0xFFFFFFFF : 0x00000000);

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

    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector128 Compare Equal-to Floating Point
void PPCInterpreter::PPCInterpreter_vcmpeqfp128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_R_VD128).dword[0] = ((VR(VMX128_R_VA128).flt[0] == VR(VMX128_R_VB128).flt[0]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[1] = ((VR(VMX128_R_VA128).flt[1] == VR(VMX128_R_VB128).flt[1]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[2] = ((VR(VMX128_R_VA128).flt[2] == VR(VMX128_R_VB128).flt[2]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[3] = ((VR(VMX128_R_VA128).flt[3] == VR(VMX128_R_VB128).flt[3]) ? 0xFFFFFFFF : 0x00000000);

  if (_instr.v128rc) {
    u8 crValue = 0;
    bool allEqual = false;
    bool allNotEqual = false;

    if (VR(VMX128_R_VD128).dword[0] == 0xFFFFFFFF && VR(VMX128_R_VD128).dword[1] == 0xFFFFFFFF
      && VR(VMX128_R_VD128).dword[2] == 0xFFFFFFFF && VR(VMX128_R_VD128).dword[3] == 0xFFFFFFFF) {
      allEqual = true;
    }

    if (VR(VMX128_R_VD128).dword[0] == 0 && VR(VMX128_R_VD128).dword[1] == 0
      && VR(VMX128_R_VD128).dword[2] == 0 && VR(VMX128_R_VD128).dword[3] == 0) {
      allNotEqual = true;
    }

    crValue |= allEqual ? 0b1000 : 0;
    crValue |= allNotEqual ? 0b0010 : 0;

    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector Compare Equal-to Unsigned Word (x'1000 0086')
void PPCInterpreter::PPCInterpreter_vcmpequwx(sPPEState *ppeState) {
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

    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector128 Compare Equal-to Unsigned Word
void PPCInterpreter::PPCInterpreter_vcmpequw128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_R_VD128).dword[0] = ((VR(VMX128_R_VA128).dword[0] == VR(VMX128_R_VB128).dword[0]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[1] = ((VR(VMX128_R_VA128).dword[1] == VR(VMX128_R_VB128).dword[1]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[2] = ((VR(VMX128_R_VA128).dword[2] == VR(VMX128_R_VB128).dword[2]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[3] = ((VR(VMX128_R_VA128).dword[3] == VR(VMX128_R_VB128).dword[3]) ? 0xFFFFFFFF : 0x00000000);

  if (_instr.v128rc) {
    u8 crValue = 0;
    bool allEqual = false;
    bool allNotEqual = false;

    if (VR(VMX128_R_VD128).dword[0] == 0xFFFFFFFF && VR(VMX128_R_VD128).dword[1] == 0xFFFFFFFF
      && VR(VMX128_R_VD128).dword[2] == 0xFFFFFFFF && VR(VMX128_R_VD128).dword[3] == 0xFFFFFFFF) {
      allEqual = true;
    }

    if (VR(VMX128_R_VD128).dword[0] == 0 && VR(VMX128_R_VD128).dword[1] == 0 && VR(VMX128_R_VD128).dword[2] == 0 && VR(VMX128_R_VD128).dword[3] == 0) {
      allNotEqual = true;
    }

    crValue |= allEqual ? 0b1000 : 0;
    crValue |= allNotEqual ? 0b0010 : 0;

    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector128 Compare Greater-Than-or-Equal-to Floating-Point
void PPCInterpreter::PPCInterpreter_vcmpgefp128(sPPEState* ppeState) {
  CHECK_VXU;

  VR(VMX128_R_VD128).dword[0] = ((VR(VMX128_R_VA128).flt[0] >= VR(VMX128_R_VB128).flt[0]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[1] = ((VR(VMX128_R_VA128).flt[1] >= VR(VMX128_R_VB128).flt[1]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[2] = ((VR(VMX128_R_VA128).flt[2] >= VR(VMX128_R_VB128).flt[2]) ? 0xFFFFFFFF : 0x00000000);
  VR(VMX128_R_VD128).dword[3] = ((VR(VMX128_R_VA128).flt[3] >= VR(VMX128_R_VB128).flt[3]) ? 0xFFFFFFFF : 0x00000000);

  if (_instr.v128rc) {
    u8 crValue = 0;
    bool allEqual = false;
    bool allNotEqual = false;

    if (VR(VMX128_R_VD128).dword[0] == 0xFFFFFFFF && VR(VMX128_R_VD128).dword[1] == 0xFFFFFFFF
      && VR(VMX128_R_VD128).dword[2] == 0xFFFFFFFF && VR(VMX128_R_VD128).dword[3] == 0xFFFFFFFF) {
      allEqual = true;
    }

    if (VR(VMX128_R_VD128).dword[0] == 0 && VR(VMX128_R_VD128).dword[1] == 0 && VR(VMX128_R_VD128).dword[2] == 0 && VR(VMX128_R_VD128).dword[3] == 0) {
      allNotEqual = true;
    }

    crValue |= allEqual ? 0b1000 : 0;
    crValue |= allNotEqual ? 0b0010 : 0;

    ppcUpdateCR(ppeState, 6, crValue);
  }
}

// Vector128 Convert From Signed Fixed-Point Word to Floating-Point
void PPCInterpreter::PPCInterpreter_vcsxwfp128(sPPEState *ppeState) {
  // (VD) <- float(VB as signed) / 2^uimm

  CHECK_VXU;

  f64 div = (f64)(1ULL << VMX128_3_IMM);
  VR(VMX128_3_VD128).flt[0] = (float)(VR(VMX128_3_VB128).dsword[0] / div);
  VR(VMX128_3_VD128).flt[1] = (float)(VR(VMX128_3_VB128).dsword[1] / div);
  VR(VMX128_3_VD128).flt[2] = (float)(VR(VMX128_3_VB128).dsword[2] / div);
  VR(VMX128_3_VD128).flt[3] = (float)(VR(VMX128_3_VB128).dsword[3] / div);
}

static inline s32 vcfpsxwsHelper(const f64 inFloat) {
  if (inFloat < (f64)INT_MIN) {
    return INT_MIN;
  } else if (inFloat > (f64)INT_MAX) {
    return INT_MAX;
  } else {
    return (s32)inFloat;
  }
}

// Vector128 Convert From Floating-Point to Signed Fixed - Point Word Saturate
void PPCInterpreter::PPCInterpreter_vcfpsxws128(sPPEState *ppeState) {
  // (VD) <- int_sat(VB as signed * 2^uimm)

  // TODO: Check if the SAT bit should be set.

  CHECK_VXU;

  u32 uimm = VMX128_3_IMM;
  f32 fltuimm = static_cast<f32>(std::exp2(uimm));

  VR(VMX128_3_VD128).flt[0] = vcfpsxwsHelper(static_cast<f64>(VR(VMX128_3_VB128).flt[0] * fltuimm));
  VR(VMX128_3_VD128).flt[1] = vcfpsxwsHelper(static_cast<f64>(VR(VMX128_3_VB128).flt[1] * fltuimm));
  VR(VMX128_3_VD128).flt[2] = vcfpsxwsHelper(static_cast<f64>(VR(VMX128_3_VB128).flt[2] * fltuimm));
  VR(VMX128_3_VD128).flt[3] = vcfpsxwsHelper(static_cast<f64>(VR(VMX128_3_VB128).flt[3] * fltuimm));
}

static inline f32 vexptefpHelper(const f32 inFloat) {
  if (inFloat == -std::numeric_limits<f32>::infinity())
    return 0.f;
  if (inFloat == std::numeric_limits<f32>::infinity())
    return std::numeric_limits<f32>::infinity();
  return powf(2.f, inFloat);
}

// Vector 2 Raised to the Exponent Estimate Floating Point (x'1000 018A')
void PPCInterpreter::PPCInterpreter_vexptefp(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).flt[0] = vexptefpHelper(VRi(vb).flt[0]);
  VRi(vd).flt[1] = vexptefpHelper(VRi(vb).flt[1]);
  VRi(vd).flt[2] = vexptefpHelper(VRi(vb).flt[2]);
  VRi(vd).flt[3] = vexptefpHelper(VRi(vb).flt[3]);
}

// Vector128 2 Raised to the Exponent Estimate Floating Point
void PPCInterpreter::PPCInterpreter_vexptefp128(sPPEState *ppeState) {
  // (VD) <- pow2(VB)

  CHECK_VXU;

  VR(VMX128_3_VD128).flt[0] = vexptefpHelper(VR(VMX128_3_VB128).flt[0]);
  VR(VMX128_3_VD128).flt[1] = vexptefpHelper(VR(VMX128_3_VB128).flt[1]);
  VR(VMX128_3_VD128).flt[2] = vexptefpHelper(VR(VMX128_3_VB128).flt[2]);
  VR(VMX128_3_VD128).flt[3] = vexptefpHelper(VR(VMX128_3_VB128).flt[3]);
}

static f32 vNaN(f32 inFloat) {
  const u32 posNaN = 0x7FC00000;
  return (f32 &)posNaN;
}

static f32 vectorNegate(f32 inFloat) {
  (u32 &)inFloat ^= 0x80000000; // Invert the sign of the result.
  return inFloat;
}

static u32 vnmsubfpHelper(f32 fra, f32 frb, f32 frc) {
  return vectorNegate((fra * frb) - frc);
}

// Vector Negative Multiply-Subtract Floating Point (x'1000 002F')
void PPCInterpreter::PPCInterpreter_vnmsubfp(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).flt[0] = vnmsubfpHelper(VRi(va).flt[0], VRi(vb).flt[0], VRi(vc).flt[0]);
  VRi(vd).flt[1] = vnmsubfpHelper(VRi(va).flt[1], VRi(vb).flt[1], VRi(vc).flt[1]);
  VRi(vd).flt[2] = vnmsubfpHelper(VRi(va).flt[2], VRi(vb).flt[2], VRi(vc).flt[2]);
  VRi(vd).flt[3] = vnmsubfpHelper(VRi(va).flt[3], VRi(vb).flt[3], VRi(vc).flt[3]);
}

// Vector128 Negative Multiply-Subtract Floating Point
void PPCInterpreter::PPCInterpreter_vnmsubfp128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = vnmsubfpHelper(VR(VMX128_VA128).flt[0], VR(VMX128_VD128).flt[0], VR(VMX128_VB128).flt[0]);
  VR(VMX128_VD128).flt[1] = vnmsubfpHelper(VR(VMX128_VA128).flt[1], VR(VMX128_VD128).flt[1], VR(VMX128_VB128).flt[1]);
  VR(VMX128_VD128).flt[2] = vnmsubfpHelper(VR(VMX128_VA128).flt[2], VR(VMX128_VD128).flt[2], VR(VMX128_VB128).flt[2]);
  VR(VMX128_VD128).flt[3] = vnmsubfpHelper(VR(VMX128_VA128).flt[3], VR(VMX128_VD128).flt[3], VR(VMX128_VB128).flt[3]);
}

// Vector Logical NOR (x'1000 0504')
void PPCInterpreter::PPCInterpreter_vnor(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = ~(VRi(va).dword[0] | VRi(vb).dword[0]);
  VRi(vd).dword[1] = ~(VRi(va).dword[1] | VRi(vb).dword[1]);
  VRi(vd).dword[2] = ~(VRi(va).dword[2] | VRi(vb).dword[2]);
  VRi(vd).dword[3] = ~(VRi(va).dword[3] | VRi(vb).dword[3]);
}

// Vector Logical OR (x'1000 0484')
void PPCInterpreter::PPCInterpreter_vor(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] | VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] | VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] | VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] | VRi(vb).dword[3];
}

// Vector128 Logical OR
void PPCInterpreter::PPCInterpreter_vor128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] | VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] | VR(VMX128_VB128).dword[1];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] | VR(VMX128_VB128).dword[2];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] | VR(VMX128_VB128).dword[3];
}

// Vector Splat Word (x'1000 028C')
void PPCInterpreter::PPCInterpreter_vspltw(sPPEState *ppeState) {
  CHECK_VXU;

  const u8 b = (_instr.vuimm & 0x3);

  for (u8 idx = 0; idx < 4; ++idx) {
    VRi(vd).dword[idx] = VRi(vb).dword[b];
  }
}

// Vector128 Splat Word
void PPCInterpreter::PPCInterpreter_vspltw128(sPPEState *ppeState) {
  CHECK_VXU;

  const u8 b = (VMX128_3_IMM & 0x3);

  for (u8 idx = 0; idx < 4; ++idx) {
    VR(VMX128_3_VD128).dword[idx] = VR(VMX128_3_VB128).dword[b];
  }
}

// Vector128 Multiply Sum 3-way Floating-Point
void PPCInterpreter::PPCInterpreter_vmsum3fp128(sPPEState* ppeState) {
  CHECK_VXU;

  // Dot product XYZ.
  // (VD.xyzw) = (VA.x * VB.x) + (VA.y * VB.y) + (VA.z * VB.z)

  f32 dotProduct = (VR(VMX128_VA128).flt[0] * VR(VMX128_VB128).flt[0]) +
    (VR(VMX128_VA128).flt[1] * VR(VMX128_VB128).flt[1]) +
    (VR(VMX128_VA128).flt[2] * VR(VMX128_VB128).flt[2]);

  for (u8 idx = 0; idx < 4; idx++) {
    VR(VMX128_3_VD128).flt[idx] = dotProduct;
  }
}

// Vector Maximum Unsigned Word (x'1000 0082')
void PPCInterpreter::PPCInterpreter_vmaxuw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = (VRi(va).dword[0] > VRi(vb).dword[0]) ? VRi(va).dword[0] : VRi(vb).dword[0];
  VRi(vd).dword[1] = (VRi(va).dword[1] > VRi(vb).dword[1]) ? VRi(va).dword[1] : VRi(vb).dword[1];
  VRi(vd).dword[2] = (VRi(va).dword[2] > VRi(vb).dword[2]) ? VRi(va).dword[2] : VRi(vb).dword[2];
  VRi(vd).dword[3] = (VRi(va).dword[3] > VRi(vb).dword[3]) ? VRi(va).dword[3] : VRi(vb).dword[3];
}

// Vector Maximum Signed Halfword (x'1000 0142')
void PPCInterpreter::PPCInterpreter_vmaxsh(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).word[0] = (VRi(va).sword[0] > VRi(vb).sword[0]) ? VRi(va).sword[0] : VRi(vb).sword[0];
  VRi(vd).word[1] = (VRi(va).sword[1] > VRi(vb).sword[1]) ? VRi(va).sword[1] : VRi(vb).sword[1];
  VRi(vd).word[2] = (VRi(va).sword[2] > VRi(vb).sword[2]) ? VRi(va).sword[2] : VRi(vb).sword[2];
  VRi(vd).word[3] = (VRi(va).sword[3] > VRi(vb).sword[3]) ? VRi(va).sword[3] : VRi(vb).sword[3];
}

// Vector Maximum Unsigned Halfword (0x1000 0042)
void PPCInterpreter::PPCInterpreter_vmaxuh(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).word[0] = (VRi(va).word[0] > VRi(vb).word[0]) ? VRi(va).word[0] : VRi(vb).word[0];
  VRi(vd).word[1] = (VRi(va).word[1] > VRi(vb).word[1]) ? VRi(va).word[1] : VRi(vb).word[1];
  VRi(vd).word[2] = (VRi(va).word[2] > VRi(vb).word[2]) ? VRi(va).word[2] : VRi(vb).word[2];
  VRi(vd).word[3] = (VRi(va).word[3] > VRi(vb).word[3]) ? VRi(va).word[3] : VRi(vb).word[3];
}

// Vector Maximum Signed Word (x'1000 0182')
void PPCInterpreter::PPCInterpreter_vmaxsw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dsword[0] = (VRi(va).dsword[0] > VRi(vb).dsword[0]) ? VRi(va).dsword[0] : VRi(vb).dsword[0];
  VRi(vd).dsword[1] = (VRi(va).dsword[1] > VRi(vb).dsword[1]) ? VRi(va).dsword[1] : VRi(vb).dsword[1];
  VRi(vd).dsword[2] = (VRi(va).dsword[2] > VRi(vb).dsword[2]) ? VRi(va).dsword[2] : VRi(vb).dsword[2];
  VRi(vd).dsword[3] = (VRi(va).dsword[3] > VRi(vb).dsword[3]) ? VRi(va).dsword[3] : VRi(vb).dsword[3];
}

// Vector Minimum Signed Halfword (x'1000 0342')
void PPCInterpreter::PPCInterpreter_vminsh(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).sword[0] = (VRi(va).sword[0] < VRi(vb).sword[0]) ? VRi(va).sword[0] : VRi(vb).sword[0];
  VRi(vd).sword[1] = (VRi(va).sword[1] < VRi(vb).sword[1]) ? VRi(va).sword[1] : VRi(vb).sword[1];
  VRi(vd).sword[2] = (VRi(va).sword[2] < VRi(vb).sword[2]) ? VRi(va).sword[2] : VRi(vb).sword[2];
  VRi(vd).sword[3] = (VRi(va).sword[3] < VRi(vb).sword[3]) ? VRi(va).sword[3] : VRi(vb).sword[3];
}

// Vector Minimum Unsigned Halfword (x'1000 0242')
void PPCInterpreter::PPCInterpreter_vminuh(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).word[0] = (VRi(va).word[0] < VRi(vb).word[0]) ? VRi(va).word[0] : VRi(vb).word[0];
  VRi(vd).word[1] = (VRi(va).word[1] < VRi(vb).word[1]) ? VRi(va).word[1] : VRi(vb).word[1];
  VRi(vd).word[2] = (VRi(va).word[2] < VRi(vb).word[2]) ? VRi(va).word[2] : VRi(vb).word[2];
  VRi(vd).word[3] = (VRi(va).word[3] < VRi(vb).word[3]) ? VRi(va).word[3] : VRi(vb).word[3];
}

// Vector Minimum Unsigned Word (x'1000 0282')
void PPCInterpreter::PPCInterpreter_vminuw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = (VRi(va).dword[0] < VRi(vb).dword[0]) ? VRi(va).dword[0] : VRi(vb).dword[0];
  VRi(vd).dword[1] = (VRi(va).dword[1] < VRi(vb).dword[1]) ? VRi(va).dword[1] : VRi(vb).dword[1];
  VRi(vd).dword[2] = (VRi(va).dword[2] < VRi(vb).dword[2]) ? VRi(va).dword[2] : VRi(vb).dword[2];
  VRi(vd).dword[3] = (VRi(va).dword[3] < VRi(vb).dword[3]) ? VRi(va).dword[3] : VRi(vb).dword[3];
}

// Vector Maximum Floating Point (x'1000 040A')
void PPCInterpreter::PPCInterpreter_vmaxfp(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).flt[0] = (VRi(va).flt[0] > VRi(vb).flt[0]) ? VRi(va).flt[0] : VRi(vb).flt[0];
  VRi(vd).flt[1] = (VRi(va).flt[1] > VRi(vb).flt[1]) ? VRi(va).flt[1] : VRi(vb).flt[1];
  VRi(vd).flt[2] = (VRi(va).flt[2] > VRi(vb).flt[2]) ? VRi(va).flt[2] : VRi(vb).flt[2];
  VRi(vd).flt[3] = (VRi(va).flt[3] > VRi(vb).flt[3]) ? VRi(va).flt[3] : VRi(vb).flt[3];
}

// Vector 128 Multiply Floating Point
void PPCInterpreter::PPCInterpreter_vmulfp128(sPPEState *ppeState) {
  /*
  vD <- vA * vB (4 x FP)
  */

  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = VR(VMX128_VA128).flt[0] * VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = VR(VMX128_VA128).flt[1] * VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = VR(VMX128_VA128).flt[2] * VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = VR(VMX128_VA128).flt[3] * VR(VMX128_VB128).flt[3];
}

// Vector128 Multiply Add Floating Point
void PPCInterpreter::PPCInterpreter_vmaddcfp128(sPPEState *ppeState) {
  // (VD) <- ((VA) * (VD)) + (VB)

  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = (VR(VMX128_VA128).flt[0] * VR(VMX128_VD128).flt[0]) + VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = (VR(VMX128_VA128).flt[1] * VR(VMX128_VD128).flt[1]) + VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = (VR(VMX128_VA128).flt[2] * VR(VMX128_VD128).flt[2]) + VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = (VR(VMX128_VA128).flt[3] * VR(VMX128_VD128).flt[3]) + VR(VMX128_VB128).flt[3];
}

// Vector Merge High Byte (x'1000 000C')
void PPCInterpreter::PPCInterpreter_vmrghb(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).bytes[5] = VRi(va).bytes[0];
  VRi(vd).bytes[4] = VRi(vb).bytes[0];
  VRi(vd).bytes[7] = VRi(va).bytes[1];
  VRi(vd).bytes[6] = VRi(vb).bytes[1];
  VRi(vd).bytes[1] = VRi(va).bytes[2];
  VRi(vd).bytes[0] = VRi(vb).bytes[2];
  VRi(vd).bytes[3] = VRi(va).bytes[3];
  VRi(vd).bytes[2] = VRi(vb).bytes[3];
  VRi(vd).bytes[13] = VRi(va).bytes[4];
  VRi(vd).bytes[12] = VRi(vb).bytes[4];
  VRi(vd).bytes[15] = VRi(va).bytes[5];
  VRi(vd).bytes[14] = VRi(vb).bytes[5];
  VRi(vd).bytes[9] = VRi(va).bytes[6];
  VRi(vd).bytes[8] = VRi(vb).bytes[6];
  VRi(vd).bytes[11] = VRi(va).bytes[7];
  VRi(vd).bytes[10] = VRi(vb).bytes[7];
}

// Vector Merge High Halfword (x'1000 004C')
void PPCInterpreter::PPCInterpreter_vmrghh(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).word[3] = VRi(va).word[0];
  VRi(vd).word[2] = VRi(vb).word[0];
  VRi(vd).word[1] = VRi(va).word[1];
  VRi(vd).word[0] = VRi(vb).word[1];
  VRi(vd).word[7] = VRi(va).word[2];
  VRi(vd).word[6] = VRi(vb).word[2];
  VRi(vd).word[5] = VRi(va).word[3];
  VRi(vd).word[4] = VRi(vb).word[3];
}

// Vector Merge High Word (x'1000 008C')
void PPCInterpreter::PPCInterpreter_vmrghw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0];
  VRi(vd).dword[1] = VRi(vb).dword[0];
  VRi(vd).dword[2] = VRi(va).dword[1];
  VRi(vd).dword[3] = VRi(vb).dword[1];
}

// Vector Merge Low Word (x'1000 018C')
void PPCInterpreter::PPCInterpreter_vmrglw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[2];
  VRi(vd).dword[1] = VRi(vb).dword[2];
  VRi(vd).dword[2] = VRi(va).dword[3];
  VRi(vd).dword[3] = VRi(vb).dword[3];
}

// Vector128 Merge High Word
void PPCInterpreter::PPCInterpreter_vmrghw128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[1];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VB128).dword[1];
}

// Vector Merge Low Byte (x'1000 010C')
void PPCInterpreter::PPCInterpreter_vmrglb(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).bytes[5] = VRi(va).bytes[8];
  VRi(vd).bytes[4] = VRi(vb).bytes[8];
  VRi(vd).bytes[7] = VRi(va).bytes[9];
  VRi(vd).bytes[6] = VRi(vb).bytes[9];
  VRi(vd).bytes[1] = VRi(va).bytes[10];
  VRi(vd).bytes[0] = VRi(vb).bytes[10];
  VRi(vd).bytes[3] = VRi(va).bytes[11];
  VRi(vd).bytes[2] = VRi(vb).bytes[11];
  VRi(vd).bytes[13] = VRi(va).bytes[12];
  VRi(vd).bytes[12] = VRi(vb).bytes[12];
  VRi(vd).bytes[15] = VRi(va).bytes[13];
  VRi(vd).bytes[14] = VRi(vb).bytes[13];
  VRi(vd).bytes[9] = VRi(va).bytes[14];
  VRi(vd).bytes[8] = VRi(vb).bytes[14];
  VRi(vd).bytes[11] = VRi(va).bytes[15];
  VRi(vd).bytes[10] = VRi(vb).bytes[15];
}

// Vector Merge Low Halfword (x'1000 014C')
void PPCInterpreter::PPCInterpreter_vmrglh(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).word[3] = VRi(va).word[4];
  VRi(vd).word[2] = VRi(vb).word[4];
  VRi(vd).word[1] = VRi(va).word[5];
  VRi(vd).word[0] = VRi(vb).word[5];
  VRi(vd).word[7] = VRi(va).word[6];
  VRi(vd).word[6] = VRi(vb).word[6];
  VRi(vd).word[5] = VRi(va).word[7];
  VRi(vd).word[4] = VRi(vb).word[7];
}

// Vector128 Maximum Floating-Point
void PPCInterpreter::PPCInterpreter_vmaxfp128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = (VR(VMX128_VA128).flt[0] > VR(VMX128_VB128).flt[0]) ? VR(VMX128_VA128).flt[0] : VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = (VR(VMX128_VA128).flt[1] > VR(VMX128_VB128).flt[1]) ? VR(VMX128_VA128).flt[1] : VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = (VR(VMX128_VA128).flt[2] > VR(VMX128_VB128).flt[2]) ? VR(VMX128_VA128).flt[2] : VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = (VR(VMX128_VA128).flt[3] > VR(VMX128_VB128).flt[3]) ? VR(VMX128_VA128).flt[3] : VR(VMX128_VB128).flt[3];
}

// Vector Minimum Floating Point (x'1000 044A')
void PPCInterpreter::PPCInterpreter_vminfp(sPPEState* ppeState) {
  CHECK_VXU;

  VRi(vd).flt[0] = (VRi(va).flt[0] < VRi(vb).flt[0]) ? VRi(va).flt[0] : VRi(vb).flt[0];
  VRi(vd).flt[1] = (VRi(va).flt[1] < VRi(vb).flt[1]) ? VRi(va).flt[1] : VRi(vb).flt[1];
  VRi(vd).flt[2] = (VRi(va).flt[2] < VRi(vb).flt[2]) ? VRi(va).flt[2] : VRi(vb).flt[2];
  VRi(vd).flt[3] = (VRi(va).flt[3] < VRi(vb).flt[3]) ? VRi(va).flt[3] : VRi(vb).flt[3];
}

// Vector128 Minimum Floating-Point
void PPCInterpreter::PPCInterpreter_vminfp128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).flt[0] = (VR(VMX128_VA128).flt[0] < VR(VMX128_VB128).flt[0]) ? VR(VMX128_VA128).flt[0] : VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = (VR(VMX128_VA128).flt[1] < VR(VMX128_VB128).flt[1]) ? VR(VMX128_VA128).flt[1] : VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = (VR(VMX128_VA128).flt[2] < VR(VMX128_VB128).flt[2]) ? VR(VMX128_VA128).flt[2] : VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = (VR(VMX128_VA128).flt[3] < VR(VMX128_VB128).flt[3]) ? VR(VMX128_VA128).flt[3] : VR(VMX128_VB128).flt[3];
}

// Vector128 Merge Low Word
void PPCInterpreter::PPCInterpreter_vmrglw128(sPPEState *ppeState) {
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
void PPCInterpreter::PPCInterpreter_vperm(sPPEState *ppeState) {
  CHECK_VXU;

  auto bytes = VRi(vc).bytes;

  Base::Vector128 vector{};

  // Truncate in case of bigger than 32 value.
  for (auto& byte : bytes) {
    byte &= 0x1F;
  }

  for (u8 idx = 0; idx < 16; idx++) {
    vector.bytes[idx] = vpermHelper(reIndex[bytes[reIndex[idx]]], VRi(va), VRi(rb));
  }

  VRi(vd).dword[0] = byteswap_be<u32>(vector.dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(vector.dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(vector.dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(vector.dword[3]);
}

// Vector128 Permute
void PPCInterpreter::PPCInterpreter_vperm128(sPPEState *ppeState) {
  CHECK_VXU;

  auto bytes = VR(VMX128_2_VC).bytes;

  Base::Vector128 vector{};

  // Truncate in case of bigger than 32 value.
  for (auto& byte : bytes) {
    byte &= 0x1F;
  }

  for (u8 idx = 0; idx < 16; idx++) {
    vector.bytes[idx] = vpermHelper(reIndex[bytes[reIndex[idx]]], VR(VMX128_2_VA128), VR(VMX128_2_VB128));
  }

  VR(VMX128_2_VD128).dword[0] = byteswap_be<u32>(vector.dword[0]);
  VR(VMX128_2_VD128).dword[1] = byteswap_be<u32>(vector.dword[1]);
  VR(VMX128_2_VD128).dword[2] = byteswap_be<u32>(vector.dword[2]);
  VR(VMX128_2_VD128).dword[3] = byteswap_be<u32>(vector.dword[3]);
}

// Vector128 Permutate Word Immediate
void PPCInterpreter::PPCInterpreter_vpermwi128(sPPEState *ppeState) {
  CHECK_VXU;

  const u32 vrd = _instr.VMX128_P.VD128l | (_instr.VMX128_P.VD128h << 5);
  const u32 vrb = _instr.VMX128_P.VB128l | (_instr.VMX128_P.VB128h << 5);
  u32 uimm = _instr.VMX128_P.PERMl | (_instr.VMX128_P.PERMh << 5);

  VR(vrd).dword[0] = VR(vrb).dword[(uimm >> 6) & 3];
  VR(vrd).dword[1] = VR(vrb).dword[(uimm >> 4) & 3];
  VR(vrd).dword[2] = VR(vrb).dword[(uimm >> 2) & 3];
  VR(vrd).dword[3] = VR(vrb).dword[(uimm >> 0) & 3];
}

// Vector Rotate Left Integer Halfword (x'1000 0044')
void PPCInterpreter::PPCInterpreter_vrlh(sPPEState* ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 8; ++idx) {
    VRi(vd).word[idx] = std::rotl<u16>(VRi(va).word[idx], (VRi(vb).word[idx] & 0xF));
  }
}

// Vector128 Rotate Left Immediate and Mask Insert
void PPCInterpreter::PPCInterpreter_vrlimi128(sPPEState *ppeState) {
  /*
  From Xenia:
  This is just a fancy permute.
  X Y Z W, rotated left by 2 = Z W X Y
  Then mask select the results into the dest.
  Sometimes rotation is zero, so fast path.
  */

  CHECK_VXU;

  const u32 vd = _instr.VMX128_4.VD128l | (_instr.VMX128_4.VD128h << 5);
  const u32 vb = _instr.VMX128_4.VB128l | (_instr.VMX128_4.VB128h << 5);
  u32 blendMaskSource = _instr.VMX128_4.IMM;
  u32 blendMask = 0;
  blendMask |= (((blendMaskSource >> 3) & 0x1) ? 0 : 4) << 0;
  blendMask |= (((blendMaskSource >> 2) & 0x1) ? 1 : 5) << 8;
  blendMask |= (((blendMaskSource >> 1) & 0x1) ? 2 : 6) << 16;
  blendMask |= (((blendMaskSource >> 0) & 0x1) ? 3 : 7) << 24;
  u32 rotate = _instr.VMX128_4.z;

  Base::Vector128 result = {};

  if (rotate) {
    switch (rotate) {
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
void PPCInterpreter::PPCInterpreter_vrfin(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).flt[0] = round(VRi(vb).flt[0]);
  VRi(vd).flt[1] = round(VRi(vb).flt[1]);
  VRi(vd).flt[2] = round(VRi(vb).flt[2]);
  VRi(vd).flt[3] = round(VRi(vb).flt[3]);

}

// Vector128 Round to Floating - Point Integer Nearest
void PPCInterpreter::PPCInterpreter_vrfin128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_3_VD128).flt[0] = round(VR(VMX128_3_VB128).flt[0]);
  VR(VMX128_3_VD128).flt[1] = round(VR(VMX128_3_VB128).flt[1]);
  VR(VMX128_3_VD128).flt[2] = round(VR(VMX128_3_VB128).flt[2]);
  VR(VMX128_3_VD128).flt[3] = round(VR(VMX128_3_VB128).flt[3]);
}

static f32 vrefpHelper(const f32 inFloat) {
  if (inFloat == 0.f)
    return std::numeric_limits<f32>::infinity();
  if (inFloat == -0.f)
    return -std::numeric_limits<f32>::infinity();
  return 1.f / inFloat;
}

// Vector Reciprocal Estimate Floating Point (x'1000 010A')
void PPCInterpreter::PPCInterpreter_vrefp(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).flt[0] = vrefpHelper(VRi(vb).flt[0]);
  VRi(vd).flt[1] = vrefpHelper(VRi(vb).flt[1]);
  VRi(vd).flt[2] = vrefpHelper(VRi(vb).flt[2]);
  VRi(vd).flt[3] = vrefpHelper(VRi(vb).flt[3]);
}

void PPCInterpreter::PPCInterpreter_vrefp128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_3_VD128).flt[0] = vrefpHelper(VR(VMX128_3_VB128).flt[0]);
  VR(VMX128_3_VD128).flt[1] = vrefpHelper(VR(VMX128_3_VB128).flt[1]);
  VR(VMX128_3_VD128).flt[2] = vrefpHelper(VR(VMX128_3_VB128).flt[2]);
  VR(VMX128_3_VD128).flt[3] = vrefpHelper(VR(VMX128_3_VB128).flt[3]);
}

static f32 vrsqrtefpHelper(const f32 inFloat) {
  if (inFloat == 0.f)
    return std::numeric_limits<f32>::infinity();
  if (inFloat == -0.f)
    return -std::numeric_limits<f32>::infinity();
  if (inFloat < 0.f)
    return std::numeric_limits<f32>::quiet_NaN();
  return 1.f / sqrtf(inFloat);
}

// Vector Reciprocal Square Root Estimate Floating Point (x'1000 014A')
void PPCInterpreter::PPCInterpreter_vrsqrtefp(sPPEState *ppeState) {
  CHECK_VXU;

  // TODO: Check for handling of infinity, minus infinity and NaN's.

  VRi(vd).flt[0] = vrsqrtefpHelper(VRi(vb).flt[0]);
  VRi(vd).flt[1] = vrsqrtefpHelper(VRi(vb).flt[1]);
  VRi(vd).flt[2] = vrsqrtefpHelper(VRi(vb).flt[2]);
  VRi(vd).flt[3] = vrsqrtefpHelper(VRi(vb).flt[3]);
}

// Vector128 Reciprocal Square Root Estimate Floating Point
void PPCInterpreter::PPCInterpreter_vrsqrtefp128(sPPEState *ppeState) {
  CHECK_VXU;

  // TODO: Check for handling of infinity, minus infinity and NaN's.

  VR(VMX128_3_VD128).flt[0] = vrsqrtefpHelper(VR(VMX128_3_VB128).flt[0]);
  VR(VMX128_3_VD128).flt[1] = vrsqrtefpHelper(VR(VMX128_3_VB128).flt[1]);
  VR(VMX128_3_VD128).flt[2] = vrsqrtefpHelper(VR(VMX128_3_VB128).flt[2]);
  VR(VMX128_3_VD128).flt[3] = vrsqrtefpHelper(VR(VMX128_3_VB128).flt[3]);
}

// Vector Conditional Select (x'1000 002A')
void PPCInterpreter::PPCInterpreter_vsel(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = (VRi(va).dword[0] & ~VRi(vc).dword[0]) | (VRi(vb).dword[0] & VRi(vc).dword[0]);
  VRi(vd).dword[1] = (VRi(va).dword[1] & ~VRi(vc).dword[1]) | (VRi(vb).dword[1] & VRi(vc).dword[1]);
  VRi(vd).dword[2] = (VRi(va).dword[2] & ~VRi(vc).dword[2]) | (VRi(vb).dword[2] & VRi(vc).dword[2]);
  VRi(vd).dword[3] = (VRi(va).dword[3] & ~VRi(vc).dword[3]) | (VRi(vb).dword[3] & VRi(vc).dword[3]);
}

// Vector128 Conditional Select
void PPCInterpreter::PPCInterpreter_vsel128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = (VR(VMX128_VA128).dword[0] & ~VR(VMX128_VD128).dword[0]) | (VR(VMX128_VB128).dword[0] & VR(VMX128_VD128).dword[0]);
  VR(VMX128_VD128).dword[1] = (VR(VMX128_VA128).dword[1] & ~VR(VMX128_VD128).dword[1]) | (VR(VMX128_VB128).dword[1] & VR(VMX128_VD128).dword[1]);
  VR(VMX128_VD128).dword[2] = (VR(VMX128_VA128).dword[2] & ~VR(VMX128_VD128).dword[2]) | (VR(VMX128_VB128).dword[2] & VR(VMX128_VD128).dword[2]);
  VR(VMX128_VD128).dword[3] = (VR(VMX128_VA128).dword[3] & ~VR(VMX128_VD128).dword[3]) | (VR(VMX128_VB128).dword[3] & VR(VMX128_VD128).dword[3]);
}

// Vector Shift Left (x'1000 01C4') STUB!
void PPCInterpreter::PPCInterpreter_vsl(sPPEState* ppeState) {
  CHECK_VXU;

  const u8 sh = VRi(rb).bytes[15] & 0x7;

  Base::Vector128 res = VRi(va);

  for (s32 i = 0; i < 15; ++i) {
    res.bytes[i ^ 0x3] = (res.bytes[i ^ 0x3] >> sh) | (res.bytes[(i + 1) ^ 0x3] << (8 - sh));
  }

  VRi(vd) = res;
}

// Vector Shift Left by Octet (x'1000 040C')
void PPCInterpreter::PPCInterpreter_vslo(sPPEState* ppeState) {
  CHECK_VXU;

  // Shift amount in bytes.
  u32 shift = (VRi(vb).bytes[15] >> 3) & 0xF;

  // Fast path, should return if there's nothing to shift.
  if (shift == 0) { return; }

  // Need to byteswap becuase of byte endianness.
  Base::Vector128 vec = VRi(va);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  for (u8 idx = 0; idx < 16; ++idx) {
    VRi(vd).bytes[idx] = (idx + shift < 16) ? vec.bytes[idx + shift] : 0;
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);
}

// Vector Multiply Add Floating Point (x'1000 002E')
void PPCInterpreter::PPCInterpreter_vmaddfp(sPPEState *ppeState) {
  // TODO: Rounding.

  CHECK_VXU;

  VRi(vd).flt[0] = (VRi(va).flt[0] * VRi(vc).flt[0]) + VRi(vb).flt[0];
  VRi(vd).flt[1] = (VRi(va).flt[1] * VRi(vc).flt[1]) + VRi(vb).flt[1];
  VRi(vd).flt[2] = (VRi(va).flt[2] * VRi(vc).flt[2]) + VRi(vb).flt[2];
  VRi(vd).flt[3] = (VRi(va).flt[3] * VRi(vc).flt[3]) + VRi(vb).flt[3];
}

// Vector Shift Left Integer Byte (x'1000 0104')
void PPCInterpreter::PPCInterpreter_vslb(sPPEState *ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = VRi(va).bytes[idx] << (VRi(vb).bytes[idx] & 0x7);
  }
}

// Vector Shift Left Half Word (x'1000 0104')
void PPCInterpreter::PPCInterpreter_vslh(sPPEState* ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).word[idx] = VRi(va).word[idx] << (VRi(vb).word[idx] & 15);
  }
}

// Vector Shift Left Integer Word (x'1000 0184')
void PPCInterpreter::PPCInterpreter_vslw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(rd).dword[0] = VRi(ra).dword[0] << (VRi(rb).dword[0] & 31);
  VRi(rd).dword[1] = VRi(ra).dword[1] << (VRi(rb).dword[1] & 31);
  VRi(rd).dword[2] = VRi(ra).dword[2] << (VRi(rb).dword[2] & 31);
  VRi(rd).dword[3] = VRi(ra).dword[3] << (VRi(rb).dword[3] & 31);
}

// Vector128 Shift Left Word
void PPCInterpreter::PPCInterpreter_vslw128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] << (VR(VMX128_VB128).dword[0] & 31);
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] << (VR(VMX128_VB128).dword[1] & 31);
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] << (VR(VMX128_VB128).dword[2] & 31);
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] << (VR(VMX128_VB128).dword[3] & 31);
}

// Vector Shift Right (x'1000 02C4')
void PPCInterpreter::PPCInterpreter_vsr(sPPEState *ppeState) {
  CHECK_VXU;

  const u8 sh = VRi(rb).bytes[15] & 0x7;

  Base::Vector128 res = VRi(va);

  for (s32 i = 15; i > 0; --i) {
    res.bytes[i ^ 0x3] = (res.bytes[i ^ 0x3] >> sh) |
      (res.bytes[(i - 1) ^ 0x3] << (8 - sh));
  }

  VRi(vd) = res;
}

// Vector Shift Right Halfword (x'1000 0244')
void PPCInterpreter::PPCInterpreter_vsrh(sPPEState* ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).word[idx] = VRi(va).word[idx] >> (VRi(vb).word[idx] & 15);
  }
}

// Vector Shift Right Algebraic Halfword (x'1000 0344')
void PPCInterpreter::PPCInterpreter_vsrah(sPPEState* ppeState) {
  CHECK_VXU;

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).sword[idx] = VRi(va).sword[idx] >> (VRi(vb).word[idx] & 15);
  }
}

// Vector Shift Right Word (x'1000 0284')
void PPCInterpreter::PPCInterpreter_vsrw(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(rd).dword[0] = VRi(ra).dword[0] >> (VRi(rb).dword[0] & 31);
  VRi(rd).dword[1] = VRi(ra).dword[1] >> (VRi(rb).dword[1] & 31);
  VRi(rd).dword[2] = VRi(ra).dword[2] >> (VRi(rb).dword[2] & 31);
  VRi(rd).dword[3] = VRi(ra).dword[3] >> (VRi(rb).dword[3] & 31);
}

// Vector128 Shift Right Word
void PPCInterpreter::PPCInterpreter_vsrw128(sPPEState *ppeState) {

  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] >> (VR(VMX128_VB128).dword[0] & 31);
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] >> (VR(VMX128_VB128).dword[1] & 31);
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] >> (VR(VMX128_VB128).dword[2] & 31);
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] >> (VR(VMX128_VB128).dword[3] & 31);
}

// Vector128 Shift Right Arithmetic Word
void PPCInterpreter::PPCInterpreter_vsraw128(sPPEState *ppeState) {
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
void PPCInterpreter::PPCInterpreter_vsldoi(sPPEState *ppeState) {
  CHECK_VXU;

  const u8 sh = _instr.vsh;
  // No shift
  if (sh == 0) {
    VRi(vd) = VRi(va);
    return;
  }
  if (sh == 16) {
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
void PPCInterpreter::PPCInterpreter_vsldoi128(sPPEState *ppeState) {
  CHECK_VXU;

  const u8 sh = VMX128_5_SH;
  // No shift
  if (sh == 0) {
    VR(VMX128_5_VD128) = VR(VMX128_5_VA128);
    return;
  }
  if (sh == 16) {
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
void PPCInterpreter::PPCInterpreter_vspltb(sPPEState *ppeState) {
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

// Vector Splat Halfword (x'1000 024C')
void PPCInterpreter::PPCInterpreter_vsplth(sPPEState* ppeState) {
  // Need to byteswap becuase of byte endianness.
  Base::Vector128 vec = VRi(vb);
  vec.dword[0] = byteswap_be<u32>(vec.dword[0]);
  vec.dword[1] = byteswap_be<u32>(vec.dword[1]);
  vec.dword[2] = byteswap_be<u32>(vec.dword[2]);
  vec.dword[3] = byteswap_be<u32>(vec.dword[3]);

  const u8 uimm = _instr.vuimm;

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).word[idx] = vec.word[uimm];
  }

  VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);
}

// Vector Splat Immediate Signed Halfword (x'1000 034C')
void PPCInterpreter::PPCInterpreter_vspltish(sPPEState *ppeState) {
  CHECK_VXU;

  s32 simm = 0;

  if (_instr.vsimm) {
    simm = ((_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm);
  }

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).sword[idx] = static_cast<s16>(simm);
  }
}

// Vector Splat Immediate Signed Word (x'1000 038C)
void PPCInterpreter::PPCInterpreter_vspltisw(sPPEState *ppeState) {
  CHECK_VXU;

  s32 simm = 0;

  if (_instr.vsimm) {
    simm = ((_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm);
  }

  for (u8 idx = 0; idx < 4; idx++) {
    VRi(vd).dsword[idx] = simm;
  }
}

// Vector Splat Immediate Signed Byte (x'1000 030C')
void PPCInterpreter::PPCInterpreter_vspltisb(sPPEState *ppeState) {
  CHECK_VXU;

  s32 simm = 0;

  if (_instr.vsimm) {
    simm = ((_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm);
  }

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = static_cast<s8>(simm);
  }
}

// Vector128 Splat Immediate Signed Word
void PPCInterpreter::PPCInterpreter_vspltisw128(sPPEState *ppeState) {
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
void PPCInterpreter::PPCInterpreter_vsubfp128(sPPEState *ppeState) {
  CHECK_VXU;

  // TODO: Rounding to Near.

  VR(VMX128_VD128).flt[0] = VR(VMX128_VA128).flt[0] - VR(VMX128_VB128).flt[0];
  VR(VMX128_VD128).flt[1] = VR(VMX128_VA128).flt[1] - VR(VMX128_VB128).flt[1];
  VR(VMX128_VD128).flt[2] = VR(VMX128_VA128).flt[2] - VR(VMX128_VB128).flt[2];
  VR(VMX128_VD128).flt[3] = VR(VMX128_VA128).flt[3] - VR(VMX128_VB128).flt[3];
}

// Vector128 Multiply Sum 4-way Floating-Point
void PPCInterpreter::PPCInterpreter_vmsum4fp128(sPPEState *ppeState) {
  CHECK_VXU;

  // Dot product XYZW.
  // (VD.xyzw) = (VA.x * VB.x) + (VA.y * VB.y) + (VA.z * VB.z) + (VA.w * VB.w)

  f32 dotProduct = (VR(VMX128_VA128).flt[0] * VR(VMX128_VB128).flt[0]) +
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

static inline f32 MakePackedFloatUnsigned(const u32 x) {
  union {
    f32 f;
    u32 u;
  } ret;

  ret.f = 1.f;
  ret.u |= x;
  return ret.f;
}

static inline f32 MakePackedFloatSigned(const s32 x) {
  union {
    f32 f;
    u32 i;
  } ret;

  ret.f = 3.f;
  ret.i += x;
  return ret.f;
}

// Vector128 Unpack D3Dtype
void PPCInterpreter::PPCInterpreter_vupkd3d128(sPPEState *ppeState) {
  CHECK_VXU;

  // Research from Xenia:
  // Can't find many docs on this. Best reference is
  // http://worldcraft.googlecode.com/svn/trunk/src/qylib/math/xmmatrix.inl,
  // which shows how it's used in some cases. Since it's all intrinsics,
  // finding it in code is pretty easy.
  const u32 vrd = _instr.VMX128_3.VD128l | (_instr.VMX128_3.VD128h << 5);
  const u32 vrb = _instr.VMX128_3.VB128l | (_instr.VMX128_3.VB128h << 5);
  const u32 packType = _instr.VMX128_3.IMM >> 2;
  const u32 val = VR(vrb).dword[3];

  switch (packType) {
  case PACK_TYPE_D3DCOLOR:
    LOG_DEBUG(Xenon, "VXU[vupkd3d128]: Pack type: PACK_TYPE_D3DCOLOR");
    VR(vrd).flt[0] = MakePackedFloatUnsigned((val >> 16) & 0xFF);
    VR(vrd).flt[1] = MakePackedFloatUnsigned((val >> 8) & 0xFF);
    VR(vrd).flt[2] = MakePackedFloatUnsigned((val >> 0) & 0xFF);
    VR(vrd).flt[3] = MakePackedFloatUnsigned((val >> 24) & 0xFF);
    break;
  case PACK_TYPE_FLOAT16_2: // Untested.
    LOG_DEBUG(Xenon, "VXU[vupkd3d128]:Pack type: PACK_TYPE_FLOAT16_2");
    VR(vrd).flt[0] = MakePackedFloatSigned(VR(vrb).sword[6]);
    VR(vrd).flt[1] = MakePackedFloatSigned(VR(vrb).sword[7]);
    VR(vrd).flt[2] = 0.0f;
    VR(vrd).flt[3] = 1.0f;
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
void PPCInterpreter::PPCInterpreter_vxor(sPPEState *ppeState) {
  CHECK_VXU;

  VRi(vd).dword[0] = VRi(va).dword[0] ^ VRi(vb).dword[0];
  VRi(vd).dword[1] = VRi(va).dword[1] ^ VRi(vb).dword[1];
  VRi(vd).dword[2] = VRi(va).dword[2] ^ VRi(vb).dword[2];
  VRi(vd).dword[3] = VRi(va).dword[3] ^ VRi(vb).dword[3];
}

// Vector128 Logical XOR
void PPCInterpreter::PPCInterpreter_vxor128(sPPEState *ppeState) {
  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0] ^ VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VA128).dword[1] ^ VR(VMX128_VB128).dword[1];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[2] ^ VR(VMX128_VB128).dword[2];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VA128).dword[3] ^ VR(VMX128_VB128).dword[3];
}
