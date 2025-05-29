// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"
#include "Base/Vector128.h"
#include <emmintrin.h>

Base::Vector128 VR[128]{};
struct DummyInstr {
  u8 vsh = 0xF;
  u8 va = 0;
  u8 vb = 1;
  u8 vc = 2;
  u8 vd = 3;
} _instr;
#define VRi(x) VR[_instr.x]

__m128i swap_bytes_4words(__m128i v) {
  __m128i mask_ff = _mm_set1_epi32(0xFF);

  __m128i b0 = _mm_and_si128(v, mask_ff);
  __m128i b1 = _mm_and_si128(_mm_srli_epi32(v, 8), mask_ff);
  __m128i b2 = _mm_and_si128(_mm_srli_epi32(v, 16), mask_ff);
  __m128i b3 = _mm_and_si128(_mm_srli_epi32(v, 24), mask_ff);

  __m128i r = _mm_or_si128(
    _mm_slli_epi32(b0, 24),
    _mm_or_si128(
      _mm_slli_epi32(b1, 16),
      _mm_or_si128(_mm_slli_epi32(b2, 8), b3)
    )
  );
  return r;
}

__m128i vsldoi_sse(__m128i va, __m128i vb, u8 shb) {
  __m128i result;
  switch (shb) {
  #define CASE(i) case i: result = _mm_or_si128(_mm_srli_si128(va, i), _mm_slli_si128(vb, 15 - i)); break
    case 0: result = va; break;
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
    default: result = _mm_setzero_si128(); break;
  }
  return swap_bytes_4words(result);
}

void vsldoi() {
  __m128i va = _mm_loadu_si128(reinterpret_cast<__m128i*>(VRi(va).bytes.data()));
  __m128i vb = _mm_loadu_si128(reinterpret_cast<__m128i*>(VRi(vb).bytes.data()));
  __m128i vd = vsldoi_sse(va, vb, _instr.vsh);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(VRi(vd).bytes.data()), vd);
}

s32 ToolMain() {
  VRi(va).dword[0] = 0x00010203;
  VRi(va).dword[1] = 0x04050607;
  VRi(va).dword[2] = 0x08090A0B;
  VRi(va).dword[3] = 0x0C0D0E0F;

  VRi(vb).dword[0] = 0x10111213;
  VRi(vb).dword[1] = 0x14151617;
  VRi(vb).dword[2] = 0x18191A1B;
  VRi(vb).dword[3] = 0x1C1D1E1F;

  VRi(va).dword[0] = byteswap_be<u32>(VRi(va).dword[0]);
  VRi(va).dword[1] = byteswap_be<u32>(VRi(va).dword[1]);
  VRi(va).dword[2] = byteswap_be<u32>(VRi(va).dword[2]);
  VRi(va).dword[3] = byteswap_be<u32>(VRi(va).dword[3]);

  VRi(vb).dword[0] = byteswap_be<u32>(VRi(vb).dword[0]);
  VRi(vb).dword[1] = byteswap_be<u32>(VRi(vb).dword[1]);
  VRi(vb).dword[2] = byteswap_be<u32>(VRi(vb).dword[2]);
  VRi(vb).dword[3] = byteswap_be<u32>(VRi(vb).dword[3]);

  vsldoi();

  //VRi(vd).dword[0] = byteswap_be<u32>(VRi(vd).dword[0]);
  //VRi(vd).dword[1] = byteswap_be<u32>(VRi(vd).dword[1]);
  //VRi(vd).dword[2] = byteswap_be<u32>(VRi(vd).dword[2]);
  //VRi(vd).dword[3] = byteswap_be<u32>(VRi(vd).dword[3]);

  for (u8 i = 0; i != 4; ++i) {
    LOG_INFO(Main, "VD{}: 0x{:08X}", i, VRi(vd).dword[i]);
  }

  return 0;
}

extern s32 ToolMain();
PARAM(help, "Prints this message", false);

s32 main(s32 argc, char *argv[]) {
  // Init params
  Base::Param::Init(argc, argv);
  // Handle help param
  if (PARAM_help.Present()) {
    ::Base::Param::Help();
    return 0;
  }
  return ToolMain();
}