// Copyright 2025 Xenon Emulator Project

#include "Base/Arch.h"
#include "Base/Logging/Log.h"
#include "PPCInterpreter.h"

// Updates needed fields from FPSCR and CR1 bits if requested
void PPCInterpreter::ppuUpdateFPSCR(PPU_STATE *ppuState, f64 op0, f64 op1, bool updateCR, u8 CR) {
  // TODO(bitsh1ft3r): Detect NaN's

  static_assert(std::endian::native == std::endian::little, "ppcUpdateFPSCR not implemented for Big-Endian arch.");

  const auto cmpResult = op0 <=> op1;
  u32 crValue = 0;
  if (cmpResult == std::partial_ordering::less) {
    curThread.FPSCR.FL = 1;
    crValue |= 0b1000;
  }
  if (cmpResult == std::partial_ordering::greater) {
    curThread.FPSCR.FG = 1;
    crValue |= 0b0100;
  }
  if (cmpResult == std::partial_ordering::equivalent) {
    curThread.FPSCR.FE = 1;
    crValue |= 0b0010;
  }
  if (cmpResult == std::partial_ordering::unordered) {
    curThread.FPSCR.FU = 1;
    crValue |= 0b0001;
  }

  // Update CRx bits if requested.
  if (updateCR) {
    switch (CR) {
    CR_CASE(0)
    CR_CASE(1)
    CR_CASE(2)
    CR_CASE(3)
    CR_CASE(4)
    CR_CASE(5)
    CR_CASE(6)
    CR_CASE(7)
    }
  }
}

// Floating Add (Double-Precision) (x'FC00 002A')
void PPCInterpreter::PPCInterpreter_faddx(PPU_STATE* ppuState) {
  /*
  frD <- (frA) + (frB)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = FPRi(fra).valueAsDouble + FPRi(frb).valueAsDouble;

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Absolute Value (x'FC00 0210')
void PPCInterpreter::PPCInterpreter_fabsx(PPU_STATE* ppuState) {
  /*
  The contents of frB with bit [0] cleared are placed into frD.
  */

 CHECK_FPU;
  
  FPRi(frd).valueAsDouble = std::fabs(FPRi(frb).valueAsDouble);

  if (_instr.rc) {
    ppuSetCR(ppuState, 1, curThread.FPSCR.FG, curThread.FPSCR.FL, curThread.FPSCR.FE, curThread.FPSCR.FU);
  }
}

// Floating Add Single (x'EC00 002A')
void PPCInterpreter::PPCInterpreter_faddsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) + (frB)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>(FPRi(fra).valueAsDouble + FPRi(frb).valueAsDouble);

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Compare Unordered (x'FC00 0000')
void PPCInterpreter::PPCInterpreter_fcmpu(PPU_STATE *ppuState) {
  /*
  if (frA) is a NaN or
     (frB) is a NaN then      c <- 0b0001
  else if (frA) < (frB) then  c <- 0b1000
  else if (frA) > (frB) then  c <- 0b0100
  else                        c <- 0b0010

  FPCC <- c
  CR[4 * crfD-4 * crfD + 3] <- c
  if (frA) is an SNaN or (frB) is an SNaN then
    VXSNAN <- 1
  */

 CHECK_FPU;

  const f64 fra = FPRi(fra).valueAsDouble;
  const f64 frb = FPRi(frb).valueAsDouble;

  ppuUpdateFPSCR(ppuState, fra, frb, true, _instr.crfd);
}

// Floating Convert to Integer Double Word with Round toward Zero (x'FC00 065E')
void PPCInterpreter::PPCInterpreter_fctidzx(PPU_STATE* ppuState) {
  // This was mostly taken from rpcs3's PPUInterpreter. 
  // TODO: Verify.

 CHECK_FPU;

#if defined(ARCH_X86_64)
  const auto val = _mm_set_sd(FPRi(frb).valueAsDouble);
  const auto res = _mm_xor_si128(_mm_set1_epi64x(_mm_cvttsd_si64(val)), _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(f64(1ull << 63)))));
  FPRi(frd).valueAsDouble = std::bit_cast<f64>(_mm_cvtsi128_si64(res));
#elif defined(ARCH_X86)
  const double input = FPRi(frb).valueAsDouble;
  s64 tmp = static_cast<s64>(FPRi(frb).valueAsDouble); // truncates just like _mm_cvttsd_si64
  const __m128i xor_mask = _mm_castpd_si128(_mm_cmpge_pd(_mm_set_sd(input), _mm_set1_pd(f64(1ull << 63))));
  const __m128i res = _mm_xor_si128(_mm_set_epi64x(0, tmp), xor_mask);
  s64 result = 0;
  memcpy(&result, &res, sizeof(result));
  FPRi(frd).valueAsDouble = std::bit_cast<f64>(result);
#else
  LOG_ERROR(Xenon, "fctidzx: Unsupported ARCH.");
#endif

  // TODO:
  ppuUpdateFPSCR(ppuState, 0.0, 0.0, _instr.rc);
}

// Floating Convert to Integer Word with Round toward Zero (x'FC00 001E')
void PPCInterpreter::PPCInterpreter_fctiwzx(PPU_STATE* ppuState) {
  // This was mostly taken from rpcs3's PPUInterpreter. 
  // TODO: Verify.

 CHECK_FPU;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
  const auto val = _mm_set_sd(FPRi(frb).valueAsDouble);
  const auto res = _mm_xor_si128(_mm_cvttpd_epi32(val), _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(0x80000000))));
  FPRi(frd).valueAsDouble = std::bit_cast<f64, s64>(_mm_cvtsi128_si32(res));
#else
  LOG_ERROR(Xenon, "fctiwzx: Unsupported ARCH.");
#endif

  // TODO:
  ppuUpdateFPSCR(ppuState, 0.0, 0.0, _instr.rc);
}

// Floating Convert from Integer Double Word (x'FC00 069C')
void PPCInterpreter::PPCInterpreter_fcfidx(PPU_STATE *ppuState) {
  /*
  frD <- signedInt64todouble(frB)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f64>(static_cast<s64>(FPRi(frb).valueAsDouble));

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Divide Single (x'EC00 0024')
void PPCInterpreter::PPCInterpreter_fdivsx(PPU_STATE *ppuState) {
  /*
  frD <- f32(frA) / (frC)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>(FPRi(fra).valueAsDouble / FPRi(frb).valueAsDouble);

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Multiply-Add (Double-Precision) (x'FC00 003A')
void PPCInterpreter::PPCInterpreter_fmaddx(PPU_STATE* ppuState) {
  /*
  frD <- (frA * frC) + frB
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = (FPRi(fra).valueAsDouble * FPRi(frc).valueAsDouble) + FPRi(frb).valueAsDouble;

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Multiply-Add Single (x'EC00 003A')
void PPCInterpreter::PPCInterpreter_fmaddsx(PPU_STATE* ppuState) {
  /*
  frD <- (frA * frC) + frB
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>((FPRi(fra).valueAsDouble * FPRi(frc).valueAsDouble) + FPRi(frb).valueAsDouble);

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Multiply (Double-Precision) (x'FC00 0032')
void PPCInterpreter::PPCInterpreter_fmulx(PPU_STATE* ppuState) {
  /*
  frD <- (frA) * (frC)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = FPRi(fra).valueAsDouble * FPRi(frc).valueAsDouble;

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Multiply Single (x'EC00 0032')
void PPCInterpreter::PPCInterpreter_fmulsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) * (frC)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>(FPRi(fra).valueAsDouble * FPRi(frc).valueAsDouble);

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Move Register (Double-Precision) (x'FC00 0090')
void PPCInterpreter::PPCInterpreter_fmrx(PPU_STATE *ppuState) {
  /*
  frD <- (frB)
  */

 CHECK_FPU;

  FPRi(frd) = FPRi(frb);

  if (_instr.rc) {
    ppuSetCR(ppuState, 1, curThread.FPSCR.FG, curThread.FPSCR.FL, curThread.FPSCR.FE, curThread.FPSCR.FU);
  }
}

// Floating Negate (x'FC00 0050')
void PPCInterpreter::PPCInterpreter_fnegx(PPU_STATE* ppuState) {
  /*
  frD <- ~ frB[0] || frB[1-63]
  */

  CHECK_FPU;

  FPRi(frd).valueAsU64 = ~FPRi(frb).valueAsU64;

  if (_instr.rc) {
    ppuSetCR(ppuState, 1, curThread.FPSCR.FG, curThread.FPSCR.FL, curThread.FPSCR.FE, curThread.FPSCR.FU);
  }
}

// Floating Negative Multiply-Subtract Single (x'EC00 003C')
void PPCInterpreter::PPCInterpreter_fnmsubsx(PPU_STATE* ppuState) {
  /*
  frD <- - ([frA * frC] - frB)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>(-((FPRi(fra).valueAsDouble * FPRi(frc).valueAsDouble) - FPRi(frb).valueAsDouble));

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Round to Single (x'FC00 0018')
void PPCInterpreter::PPCInterpreter_frspx(PPU_STATE *ppuState) {
  /*
  frD <- Round_single( frB )
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>(FPRi(frb).valueAsDouble);

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Subtract (Double-Precision) (x'FC00 0028')
void PPCInterpreter::PPCInterpreter_fsubx(PPU_STATE* ppuState) {
  /*
    frD <- (frA) - (frB)
    */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = FPRi(fra).valueAsDouble - FPRi(frb).valueAsDouble;

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

// Floating Subtract Single (x'EC00 0028')
void PPCInterpreter::PPCInterpreter_fsubsx(PPU_STATE *ppuState) {
  /*
  frD <- (frA) - (frB)
  */

 CHECK_FPU;

  FPRi(frd).valueAsDouble = static_cast<f32>(FPRi(fra).valueAsDouble - FPRi(frb).valueAsDouble);

  ppuUpdateFPSCR(ppuState, FPRi(frd).valueAsDouble, 0.0, _instr.rc);
}

void PPCInterpreter::PPCInterpreter_mffsx(PPU_STATE *ppuState) {
 CHECK_FPU;

  FPRi(frd).valueAsU64 = static_cast<u64>(GET_FPSCR);

  if (_instr.rc) {
    ppuSetCR(ppuState, 1, curThread.FPSCR.FG, curThread.FPSCR.FL, curThread.FPSCR.FE, curThread.FPSCR.FU);
  }
}

void PPCInterpreter::PPCInterpreter_mtfsfx(PPU_STATE *ppuState) {
 CHECK_FPU;

  u32 mask = 0;
  for (u32 b = 0x80; b; b >>= 1) {
    mask <<= 4;

    if (_instr.flm & b) {
      mask |= 0xF;
    }
  }

  SET_FPSCR((static_cast<u32>(FPRi(frb).valueAsU64 & mask)) | (GET_FPSCR & ~mask));

  if (_instr.rc) {
    ppuSetCR(ppuState, 1, curThread.FPSCR.FG, curThread.FPSCR.FL, curThread.FPSCR.FE, curThread.FPSCR.FU);
  }
}