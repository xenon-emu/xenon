/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "OGLTexture.h"

#ifndef NO_GFX

#include "Base/Assert.h"

u32 Render::OGLTexture::GetDepthFromFlags(s32 flags) {
  for (const auto &mapping : DepthMappings) {
    if ((flags & mapping.flag) != 0) {
      return mapping.glFormat;
    }
  }

  UNREACHABLE_MSG("Missing Depth Format flag: {}", flags);
}

u32 Render::OGLTexture::GetOGLTextureFormat(eDataFormat format) {
  switch (format) {
    case eDataFormat::RGB:  return GL_RGB;
    case eDataFormat::RGBA: return GL_RGBA;
    case eDataFormat::ARGB:
    case eDataFormat::BGRA: return GL_BGRA;
    default: UNREACHABLE_MSG("Missing Format: {}", std::to_string(format));
  }
}

void Render::OGLTexture::SetupTextureFlags(s32 flags) {
  for (const auto& tf : TextureFlags) {
    if (flags & tf.flag) {
      glTexParameteri(GL_TEXTURE_2D, tf.pname, tf.param);
    }
  }
}

void Render::OGLTexture::CreateTextureHandle(u32 width, u32 height, s32 flags) {
  DestroyTexture();
  SetTexture(&TextureHandle);
  SetWidth(width);
  SetHeight(height);
  SetDepth(GetDepthFromFlags(flags));

  glGenTextures(1, &TextureHandle);
  Bind();
  glTexStorage2D(GL_TEXTURE_2D, 1, GetDepth(), width, height);
  SetupTextureFlags(flags);
  glBindImageTexture(0, TextureHandle, 0, GL_FALSE, 0, GL_READ_WRITE, GetDepth());
  Unbind();
}

void Render::OGLTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8 *data, u32 /*dataSize*/, s32 flags) {
  DestroyTexture();
  SetTexture(&TextureHandle);
  SetWidth(width);
  SetHeight(height);
  SetDepth(GetDepthFromFlags(flags));

  glGenTextures(1, &TextureHandle);
  Bind();
  glTexStorage2D(GL_TEXTURE_2D, 1, GetDepth(), width, height);
  SetupTextureFlags(flags);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  u32 glFormat = GetOGLTextureFormat(format);
  if (Type != 0) {
    Type = GL_UNSIGNED_BYTE;
    if (format == eDataFormat::ARGB)
      Type = GL_UNSIGNED_INT_8_8_8_8_REV;
  }
  glTexImage2D(GL_TEXTURE_2D, 0, GetDepth(), width, height, 0, glFormat, GetType(), data);
  Unbind();
}

void Render::OGLTexture::ResizeTexture(u32 width, u32 height) {
  DestroyTexture();
  SetTexture(&TextureHandle);
  glGenTextures(1, &TextureHandle);
  Bind();
  glTexStorage2D(GL_TEXTURE_2D, 1, GetDepth(), width, height);
  SetupTextureFlags(glTextureWrapS_GL_CLAMP_TO_EDGE | glTextureWrapT_GL_CLAMP_TO_EDGE |
                    glTextureMinFilter_GL_NEAREST | glTextureMagFilter_GL_NEAREST);
  glBindImageTexture(0, TextureHandle, 0, GL_FALSE, 0, GL_READ_WRITE, GetDepth());
  Unbind();
}

void Render::OGLTexture::GenerateMipmaps() {
  Bind();
  glGenerateMipmap(GL_TEXTURE_2D);
  Unbind();
}

void Render::OGLTexture::UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) {
  Bind();
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GetOGLTextureFormat(format), GL_UNSIGNED_BYTE, data);
  Unbind();
}

void Render::OGLTexture::Bind() {
  glBindTexture(GL_TEXTURE_2D, TextureHandle);
}

void Render::OGLTexture::Unbind() {
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Render::OGLTexture::DestroyTexture() {
  if (TextureHandle != 0)
    glDeleteTextures(1, &TextureHandle);
  SetTexture(nullptr);
}

#endif
