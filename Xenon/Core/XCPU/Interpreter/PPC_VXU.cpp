#include "PPCInterpreter.h"

// Move from Vector Status and Control Register (x'1000 0604')
void PPCInterpreter::PPCInterpreter_mfvscr(PPU_STATE* ppuState) {
  /*
  vD <- 0 || (VSCR)
  */

  CHECK_VXU;

  VRi(vd).dword[3] = curThread.VSCR.hexValue;
}

// Move to Vector Status and Control Register (x'1000 0C44')
void PPCInterpreter::PPCInterpreter_mtvscr(PPU_STATE* ppuState) {
  /*
  VSCR <- (vB)96:127
  */

  CHECK_VXU;

  curThread.VSCR.hexValue = VRi(vb).dword[3];
}

// Vector Splat Word (x’1000 028C)
void PPCInterpreter::PPCInterpreter_vspltw(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_vmulfp128(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_vmrghw(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_vmrglw(PPU_STATE* ppuState) {
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
void PPCInterpreter::PPCInterpreter_vmrghw128(PPU_STATE* ppuState) {

  CHECK_VXU;

  VR(VMX128_VD128).dword[0] = VR(VMX128_VA128).dword[0];
  VR(VMX128_VD128).dword[1] = VR(VMX128_VB128).dword[0];
  VR(VMX128_VD128).dword[2] = VR(VMX128_VA128).dword[1];
  VR(VMX128_VD128).dword[3] = VR(VMX128_VB128).dword[1];
}
