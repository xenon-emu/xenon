// Copyright 2025 Xenon Emulator Project

#include "OGLTexture.h"
#include "Base/Logging/Log.h"

u32 Render::OGLTexture::GetDepthFromFlags(int flags) {
  if ((flags & Render::eTextureDepth::RG) != 0) {
    return GL_RG;
  }
  if ((flags & Render::eTextureDepth::RGI) != 0) {
    return GL_RG_INTEGER;
  }

  if ((flags & Render::eTextureDepth::R8) != 0) {
    return GL_R8;
  }
  if ((flags & Render::eTextureDepth::R8I) != 0) {
    return GL_R8I;
  }
  if ((flags & Render::eTextureDepth::R8U) != 0) {
    return GL_R8UI;
  }

  if ((flags & Render::eTextureDepth::R16) != 0) {
    return GL_R16;
  }
  if ((flags & Render::eTextureDepth::R16F) != 0) {
    return GL_R16F;
  }
  if ((flags & Render::eTextureDepth::R16I) != 0) {
    return GL_R16I;
  }
  if ((flags & Render::eTextureDepth::R16U) != 0) {
    return GL_R16UI;
  }
  
  if ((flags & Render::eTextureDepth::R32) != 0) {
    return GL_R16;
  }
  if ((flags & Render::eTextureDepth::R32F) != 0) {
    return GL_R32F;
  }
  if ((flags & Render::eTextureDepth::R32I) != 0) {
    return GL_R32I;
  }
  if ((flags & Render::eTextureDepth::R32U) != 0) {
    return GL_R32UI;
  }

  return 0;
} 
u32 Render::OGLTexture::GetOGLTextureFormat(Render::eDataFormat format) {
 switch (format) {
    case Render::eDataFormat::RGB:
      return GL_RGB;
      break;  
    case Render::eDataFormat::RGBA:
      return GL_RGBA;
      break;
  }
  return 0;
}
void Render::OGLTexture::SetupTextureFlags(int flags) {
  if ((flags & glTextureWrapS_GL_CLAMP_TO_EDGE) != 0) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  }
  if ((flags & glTextureWrapT_GL_CLAMP_TO_EDGE) != 0) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  if ((flags & glTextureMinFilter_GL_NEAREST) != 0) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  if ((flags & glTextureMagFilter_GL_NEAREST) != 0) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
}

void Render::OGLTexture::CreateTextureHandle(u32 width, u32 height, int flags) {
  SetTexture(&TextureHandle);
  SetWidth(width);
  SetHeight(height);
  u32 depth = GetDepthFromFlags(flags);
  SetDepth(depth);
  glGenTextures(1, (u32*)GetTexture());
  Bind();
  glTexStorage2D(GL_TEXTURE_2D, 1, depth, GetWidth(), GetHeight());
  SetupTextureFlags(flags);
  glBindImageTexture(0, TextureHandle, 0, GL_FALSE, 0, GL_READ_WRITE, depth);
  Unbind();
}
void Render::OGLTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, int flags) {
  SetTexture(&TextureHandle);
  SetWidth(width);
  SetHeight(height);
  u32 depth = GetDepthFromFlags(flags);
  SetDepth(depth);
  SetTexture(&TextureHandle);
  glGenTextures(1, (u32*)GetTexture());
  Bind();
  glTexStorage2D(GL_TEXTURE_2D, 1, depth, GetWidth(), GetHeight());
  SetupTextureFlags(flags);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  u32 textureFormat = GetOGLTextureFormat(format);
  u32 internalTextureFormat = GetOGLTextureFormat(format);
  glTexImage2D(GL_TEXTURE_2D, 0, textureFormat, GetWidth(), GetHeight(), 0, internalTextureFormat, GL_UNSIGNED_BYTE, data);
  Unbind();
}
void Render::OGLTexture::ResizeTexture(u32 width, u32 height) {
  Bind();
  glTexStorage2D(GL_TEXTURE_2D, 1, GetDepth(), width, height);
  // Vali0004: We may not need to reset these params
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindImageTexture(0, TextureHandle, 0, GL_FALSE, 0, GL_READ_WRITE, GetDepth());
  Unbind();
}

void Render::OGLTexture::Bind() {
  glBindTexture(GL_TEXTURE_2D, TextureHandle);
}      
void Render::OGLTexture::Unbind() {
  glBindTexture(GL_TEXTURE_2D, 0);
} 

void Render::OGLTexture::DestroyTexture() {
  glDeleteTextures(1, (u32*)GetTexture());
  SetTexture(nullptr);
}