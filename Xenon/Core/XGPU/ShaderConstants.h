// Copyright 2025 Xenon Emulator Project

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

} // namespace Xe