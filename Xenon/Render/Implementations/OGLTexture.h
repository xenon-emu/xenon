// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"
#include "Render/Abstractions/Texture.h"

#ifndef NO_GFX

#define GL_GLEXT_PROTOTYPES
extern "C" {
#include <KHR/khrplatform.h>
#include <glad/glad.h>
}

namespace Render {

enum eCreationFlags : const s32 {
  glTextureWrapS_GL_CLAMP_TO_EDGE = (1 << 0),
  glTextureWrapT_GL_CLAMP_TO_EDGE = (1 << 1),
  glTextureMinFilter_GL_NEAREST = (1 << 2),
  glTextureMagFilter_GL_NEAREST = (1 << 3)
};

enum eTextureDepth : const s32 {
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

class OGLTexture : public Texture {
public:
  u32 GetDepthFromFlags(int flags);
  u32 GetOGLTextureFormat(eDataFormat format);
  void SetupTextureFlags(int flags);

  void CreateTextureHandle(u32 width, u32 height, int flags) override;
  void CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, int flags) override;
  void ResizeTexture(u32 width, u32 height) override;
  void Bind() override;
  void Unbind() override;
  void DestroyTexture() override;
private:
  u32 TextureHandle;
};

} // Namespace Render
#endif