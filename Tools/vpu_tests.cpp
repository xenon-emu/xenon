// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include "Base/Param.h"
#include "Base/Types.h"
#include "Base/Vector128.h"
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#endif

Base::Vector128 VR[128]{};
struct DummyInstr {
  u8 vsh = 0xF;
  u8 va = 0;
  u8 vb = 1;
  u8 vc = 2;
  u8 vd = 3;
} _instr;
#define VRi(x) VR[_instr.x]

__attribute__((target("ssse3"))) __m128i vsldoi_sse(__m128i va, __m128i vb, u8 shb) {
  __m128i result = _mm_setzero_si128();
  switch (shb) {
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
  }
  return result;
}

static inline u8 vsldoiHelper(u8 sh, Base::Vector128 vra, Base::Vector128 vrb) {
  return (sh < 16) ? vra.bytes[sh] : vrb.bytes[sh & 0xF];
}

__attribute__((target("ssse3"))) inline __m128i byteswap_be_u32x4_ssse3(__m128i x) {
  // Reverses bytes in each 32-bit word using SSSE3 shuffle_epi8
  const __m128i shuffle = _mm_set_epi8(
    12, 13, 14, 15,
    8, 9, 10, 11,
    4, 5, 6, 7,
    0, 1, 2, 3
  );
  return _mm_shuffle_epi8(x, shuffle);
}

void vsldoi() {
  __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(VRi(va).bytes.data()));
  __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(VRi(vb).bytes.data()));
  __m128i vd = vsldoi_sse(va, vb, _instr.vsh);

  // Store raw result before byte swap
  _mm_storeu_si128(reinterpret_cast<__m128i *>(VRi(vd).bytes.data()), vd);
}

void test() {
  vsldoi();

  __m128i vd = _mm_loadu_si128(reinterpret_cast<__m128i*>(VRi(vd).bytes.data()));
  vd = byteswap_be_u32x4_ssse3(vd);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(VRi(vd).bytes.data()), vd);
}

/*
* sh=0xF
[Main] <Info> VD0: 0x0F101112
[Main] <Info> VD1: 0x13141516
[Main] <Info> VD2: 0x1718191A
[Main] <Info> VD3: 0x1B1C1D1E
* sh=0x0
* [Main] <Info> VD0: 0x00010203
[Main] <Info> VD1: 0x04050607
[Main] <Info> VD2: 0x08090A0B
[Main] <Info> VD3: 0x0C0D0E0F
*/

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

  test();

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