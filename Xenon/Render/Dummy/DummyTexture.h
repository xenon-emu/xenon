// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"
#include "Render/Abstractions/Texture.h"

namespace Render {

class DummyTexture : public Texture {
public:
  void CreateTextureHandle(u32 width, u32 height, s32 flags) override;
  void CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) override;
  void ResizeTexture(u32 width, u32 height) override;
  void GenerateMipmaps() override;
  void UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) override;
  void Bind() override;
  void Unbind() override;
  void DestroyTexture() override;
};

} // namespace Render