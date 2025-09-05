// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/Abstractions/Texture.h"

#ifndef NO_GFX

#include <glad/glad.h>

namespace Render {
  
struct DepthFormatMapping {
  const s32 flag;
  const u32 glFormat;
};
struct TextureParamFlag {
  const s32 flag;
  const GLenum pname;
  const GLint param;
};

enum eCreationFlags : s32 {
  glTextureWrapS_GL_CLAMP_TO_EDGE = (1 << 0),
  glTextureWrapT_GL_CLAMP_TO_EDGE = (1 << 1),
  glTextureMinFilter_GL_NEAREST = (1 << 2),
  glTextureMagFilter_GL_NEAREST = (1 << 3),
  glTextureSwizzleR_GL_ALPHA = (1 << 4),
  glTextureSwizzleG_GL_RED = (1 << 5),
  glTextureSwizzleB_GL_GREEN = (1 << 6),
  glTextureSwizzleA_GL_BLUE = (1 << 7),
};

enum eTextureDepth : s32 {
  RG = (1 << 4),
  RGI = (1 << 5),
  R8 = (1 << 6),
  R8I = (1 << 7),
  R8U = (1 << 8),
  R16 = (1 << 9),
  R16F = (1 << 10),
  R16I = (1 << 11),
  R16U = (1 << 12),
  R32 = (1 << 13),
  R32F = (1 << 14),
  R32I = (1 << 15),
  R32U = (1 << 16)
};

static constexpr DepthFormatMapping DepthMappings[] = {
  { eTextureDepth::RG,  GL_RG },
  { eTextureDepth::RGI, GL_RG_INTEGER },
  { eTextureDepth::R8, GL_R8 },
  { eTextureDepth::R8I, GL_R8I },
  { eTextureDepth::R8U, GL_R8UI },
  { eTextureDepth::R16, GL_R16 },
  { eTextureDepth::R16F, GL_R16F },
  { eTextureDepth::R16I, GL_R16I },
  { eTextureDepth::R16U, GL_R16UI },
  { eTextureDepth::R32,  GL_R32I },
  { eTextureDepth::R32F, GL_R32F },
  { eTextureDepth::R32I, GL_R32I },
  { eTextureDepth::R32U, GL_R32UI },
};
static constexpr TextureParamFlag TextureFlags[] = {
  { glTextureWrapS_GL_CLAMP_TO_EDGE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE },
  { glTextureWrapT_GL_CLAMP_TO_EDGE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE },
  { glTextureMinFilter_GL_NEAREST, GL_TEXTURE_MIN_FILTER, GL_NEAREST },
  { glTextureMagFilter_GL_NEAREST, GL_TEXTURE_MAG_FILTER, GL_NEAREST },
  { glTextureSwizzleR_GL_ALPHA, GL_TEXTURE_SWIZZLE_R, GL_ALPHA },
  { glTextureSwizzleG_GL_RED,   GL_TEXTURE_SWIZZLE_G, GL_RED },
  { glTextureSwizzleB_GL_GREEN, GL_TEXTURE_SWIZZLE_B, GL_GREEN },
  { glTextureSwizzleA_GL_BLUE,  GL_TEXTURE_SWIZZLE_A, GL_BLUE },
};

class OGLTexture : public Texture {
public:
  u32 GetDepthFromFlags(s32 flags);
  u32 GetOGLTextureFormat(eDataFormat format);
  void SetupTextureFlags(s32 flags);

  void CreateTextureHandle(u32 width, u32 height, s32 flags) override;
  void CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) override;
  void ResizeTexture(u32 width, u32 height) override;
  void GenerateMipmaps() override;
  void UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) override;
  void Bind() override;
  void Unbind() override;
  void DestroyTexture() override;
private:
  u32 TextureHandle = 0;
};

} // namespace Render
#endif
