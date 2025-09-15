/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

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

enum class eConstType : u32 {
  InvalidTexture = 0,
  InvalidVertex = 1,
  Texture = 2,
  Vertex = 3,
};

// No need to swap these fields, we can handle it where the data is parsed.
// Just something to keep in mind
struct TextureSize1D {
  u32 Width : 24;
  u32 : 8;
};

struct TextureSize2D {
  u32 Width : 13;
  u32 Height : 13;
  u32 : 6;
};

struct TextureSize3D {
  u32 Width : 11;
  u32 Height : 11;
  u32 Depth : 10;
};

struct TextureSizeStack {
  u32 Width : 13;
  u32 Height : 13;
  u32 Depth : 6;
};

union TextureFetchConstant {
  struct {
    // u32[0]
    eConstType Type : 2;
    u32 SignX : 2;
    u32 SignY : 2;
    u32 SignZ : 2;
    u32 SignW : 2;
    u32 ClampX : 3;
    u32 ClampY : 3;
    u32 ClampZ : 3; 
    u32 : 2;
    u32 : 1;
    u32 Pitch : 9;
    u32 Tiled : 1;
    // u32[1]
    u32 DataFormat : 6;
    u32 Endian : 2;
    u32 RequestSize : 2;
    u32 Stacked : 1;
    u32 ClampPolicy : 1;
    u32 BaseAddress : 20;
    // u32[2]
    union {
      TextureSize1D OneD;
      TextureSize2D TwoD;
      TextureSize3D ThreeD;
      TextureSizeStack Stack;
    } Size;
    // u32[3]
    u32 NumFormat : 1;
    u32 SwizzleX : 3;
    u32 SwizzleY : 3;
    u32 SwizzleZ : 3;
    u32 SwizzleW : 3;
    s32 ExpAdjust : 6;
    u32 MagFilter : 2;
    u32 MinFilter : 2;
    u32 MipFilter : 2;
    u32 AnisoFilter : 3;
    u32 : 3;
    u32 BorderSize : 1;
    // u32[4]
    u32 VolMagFilter : 1;
    u32 VolMinFilter : 1;
    u32 MinMipLevel : 4;
    u32 MaxMipLevel : 4;
    u32 MagAnisoWalk : 1;
    u32 MinAnisoWalk : 1;
    s32 LODBias : 10;
    s32 GradExpAdjustH : 5;
    s32 GradExpAdjustV : 5;
    // u32[5]
    u32 BorderColor : 2;
    u32 ForceBCWToMax : 1;
    u32 TriClamp : 2;
    s32 AnisoBias : 4;
    u32 Dimension : 2;
    u32 PackedMips : 1;
    u32 MipAddress : 20;
  };
  u32 rawHex[6];
};

union VertexFetchConstant {
  struct {
    // u32[0]
    eConstType Type : 2;
    u32 BaseAddress : 30;
    // u32[1]
    u32 Endian : 2;
    u32 Size : 24;
    u32 AddressClamp : 1;
    u32 : 1;
    u32 RequestSize : 2;
    u32 ClampDisable : 2;
  };
  u32 rawHex[2];
};

union ShaderConstantFetch {
  TextureFetchConstant Texture;
  VertexFetchConstant Vertex[3];
  u32 rawHex[6];
};

} // namespace Xe
