// Copyright 2025 Xenon Emulator Project

#include "PPCInterpreter.h"

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

// Vector Logical AND (x'1000 0404')
void PPCInterpreter::PPCInterpreter_vand(PPU_STATE *ppuState) {
  /*
  vD <- (vA) & (vB)
  */

  CHECK_VXU;

  VRi(vd) = VRi(va) & VRi(vb);
}

// Vector Logical AND with Complement (x'1000 0444')
void PPCInterpreter::PPCInterpreter_vandc(PPU_STATE *ppuState) {
  /*
  vD <- (vA) & ~(vB)
  */

  CHECK_VXU;

  VRi(vd) = VRi(va) & ~VRi(vb);
}

// Vector Logical NOR (x'1000 0504')
void PPCInterpreter::PPCInterpreter_vnor(PPU_STATE *ppuState) {
  /*
  vD <- ~((vA) | (vB))
  */

  CHECK_VXU;

  VRi(vd) = ~(VRi(va) | VRi(vb));
}

// Vector Logical OR (x'1000 0484')
void PPCInterpreter::PPCInterpreter_vor(PPU_STATE *ppuState) {
  /*
  vD <- (vA) | (vB)
  */

  CHECK_VXU;

  VRi(vd) = VRi(va) | VRi(vb);
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

  u8 b = (_instr.va & 0x3);

  VRi(vd).dword[0] = VRi(vb).dword[b];
  VRi(vd).dword[1] = VRi(vb).dword[b];
  VRi(vd).dword[2] = VRi(vb).dword[b];
  VRi(vd).dword[3] = VRi(vb).dword[b];
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

// Vector Shift Left Integer Byte (x'1000 0104')
void PPCInterpreter::PPCInterpreter_vslb(PPU_STATE* ppuState) {
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

// Vector Splat Byte (x'1000 020C')
void PPCInterpreter::PPCInterpreter_vspltb(PPU_STATE* ppuState) {
  /*
   b <- UIMM*8
  do i=0 to 127 by 8
  (vD)i:i+7 <- (vB)b:b+7
  end
  */

  CHECK_VXU;

  u8 uimm = _instr.vuimm;

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = VRi(vb).bytes[uimm];
  }
}

// Vector Splat Immediate Signed Halfword (x'1000 034C')
void PPCInterpreter::PPCInterpreter_vspltish(PPU_STATE* ppuState) {
  /*
  do i=0 to 127 by 16
    (vD)i:i+15 <- SignExtend(SIMM,16)
  end
  */

  CHECK_VXU;

  u32 simm = 0;

  if (_instr.vsimm) { simm = (_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm; }

  for (u8 idx = 0; idx < 8; idx++) {
    VRi(vd).word[idx] = static_cast<u16>(simm);
  }
}

// Vector Splat Immediate Signed Byte (x'1000 030C')
void PPCInterpreter::PPCInterpreter_vspltisb(PPU_STATE* ppuState) {
  /*
   do i = 0 to 127 by 8
   vDi:i+7 <- SignExtend(SIMM,8)
   end
  */

  CHECK_VXU;

  u32 simm = 0;

  if (_instr.vsimm) { simm = (_instr.vsimm & 0x10) ? (_instr.vsimm | 0xFFFFFFF0) : _instr.vsimm; }

  for (u8 idx = 0; idx < 16; idx++) {
    VRi(vd).bytes[idx] = static_cast<u8>(simm);
  }
}

// Vector128 Splat Immediate Signed Word
void PPCInterpreter::PPCInterpreter_vspltisw128(PPU_STATE* ppuState) {
  /*
  (VRD.xyzw) <- sign_extend(uimm)
  */

  CHECK_VXU;

  u32 simm = 0;

  if (VMX128_3_IMM) { simm = (VMX128_3_IMM & 0x10) ? (VMX128_3_IMM | 0xFFFFFFF0) : VMX128_3_IMM; }

  for (u8 idx = 0; idx < 4; idx++) {
    VR(VMX128_3_VD128).dword[idx] = simm;
  }
}

// Vector Logical XOR (x'1000 04C4')
void PPCInterpreter::PPCInterpreter_vxor(PPU_STATE *ppuState) {
  /*
  vD <- (vA) ^ (vB)
  */

  CHECK_VXU;

  VRi(vd) = VRi(va) ^ VRi(vb);
}
