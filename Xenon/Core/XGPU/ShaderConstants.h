// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Base/Types.h"

namespace Xe {
  
enum class eSwizzle : u8 {
  // Read/Write
  X = 0,
  Y,
  Z,
  W,

  // Write Only
  Zero,    // 0: Component is forced to zero
  One,     // 1: Component is forced to one
  Ignored, // Don't care about this component
  Unused   // Masked out and not modified
};

enum class eEndianFormat : u32 {
  Unspecified,
  Format8in16,
  Format8in32,
  Format16in32,
  Format8in64,
  Format8in128
};

enum class eShaderType : u32 {
  Vertex,
  Pixel,
  Unknown
};

enum class eRenderTargetColorFormat : u32 {
  D24S8,
  D24FS8
};

enum class eRenderTargetDepthFormat : u32 {
  Format_A8_R8_G8_B8, // D3DFMT_A8R8G8B8
  Format_A8_R8_G8_B8_GAMA, // D3DFMT_A8R8G8B8 with gamma correction
  Format_2_10_10_10,
  Format_2_10_10_10_FLOAT,
  Format_16_16,
  Format_16_16_16_16,
  Format_16_16_FLOAT,
  Format_16_16_16_16_FLOAT,
  Format_2_10_10_10_UNK = 10,
  Format_2_10_10_10_FLOAT_UNK = 12,
  Format_32_FLOAT = 14,
  Format_32_32_FLOAT = 15,
};

enum class eCmpFunc : u32 {
  Never,
  Less,
  Equal,
  LessEqual,
  Greater,
  NotEqual,
  GreaterEqual,
  Always
};

enum class eStencilOp : u32 {
  Keep,
  Zero,
  Replace,
  IncrWrap,
  DecrWrap,
  Invert,
  Incr,
  Decr
};

enum class eBlendArg : u32 {
  Zero,
  One,
  Unknown2,
  Unknown3,
  SrcColor,
  OneMinusSrcColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DestColor,
  OneMinusDestColor,
  DestAlpha,
  OneMinusDestAlpha,
  ConstColor,
  OneMinusConstColor,
  ConstAlpha,
  OneMinusConstAlpha,
  SrcAlphaSaturate
};

enum class eBlendOp : u32 {
  Add,
  Subtract,
  Min,
  Max,
  ReverseSubtract
};

enum class eCullMode : u32 {
  None,
  Front,
  Back
};

enum class eFrontFace : u32 {
  CW,
  CCW
};

enum class eFillMode : u32 {
  Point,
  Line,
  Solid
};

enum class eTextureFormatType : u32 {
  Uncompressed,
  Compressed
};

enum class eBorderColor : u32 {
  AGBR_Black,
  AGBR_White,
  ACBYCR_BLACK,
  ACBCRY_BLACK
};

union ShaderConstantFetch {
  u64 rawHex;
#ifdef __LITTLE_ENDIAN__
  struct {
    u32 type : 2;
    u32 address : 29;
    u32 unk1 : 1;
    u32 endian : 2;
    u32 size : 24;
    u32 unk2 : 6;
  };
  struct {
    u32 dword0;
    u32 dword1;
  };
#else
  struct {
    u32 unk1 : 6;
    u32 size : 24;
    u32 endian : 2;
    u32 unk1 : 1;
    u32 address : 29;
    u32 type : 2;
  };
  struct {
    u32 dword0;
    u32 dword1;
  };
#endif
};

} // namespace Xe
