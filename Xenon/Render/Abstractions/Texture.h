// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

#ifndef NO_GFX
namespace Render {

// TODO(Vali0004): Expand formats
enum eDataFormat : u8 {
  RGB = 0,
  RGBA = 1
};
class Texture {
public:
  virtual ~Texture() = default;
  virtual void CreateTextureHandle(u32 width, u32 height, int flags) = 0;
  virtual void CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, int flags) = 0;
  virtual void ResizeTexture(u32 width, u32 height) = 0;
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
  virtual void DestroyTexture() = 0;
  virtual void SetTexture(void* handle) { Texture = handle; }
  virtual void* GetTexture() { return Texture; }
  virtual void SetDepth(u32 depth) {  Depth = depth; }
  virtual u32 GetDepth() { return Depth; }
  virtual void SetWidth(u32 width) { Width = width; }
  virtual u32 GetWidth() { return Width; }
  virtual void SetHeight(u32 height) { Height = height; }
  virtual u32 GetHeight() { return Height; }
private:
  u32 Depth;
  u32 Width, Height;
  void* Texture;
};

} // Namespace Render
#endif
