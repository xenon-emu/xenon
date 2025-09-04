// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanTexture.h"

#ifndef NO_GFX
void Render::VulkanTexture::CreateTextureHandle(u32 width, u32 height, s32 flags) {
  LOG_INFO(Render, "VulkanTexture::CreateTextureHandle: {}, {}, {}", width, height, flags);
}

void Render::VulkanTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) {
  LOG_INFO(Render, "VulkanTexture::CreateTextureWithData: {}, {}, {}, {}, {}", width, height, static_cast<u8>(format), dataSize, flags);
}

void Render::VulkanTexture::ResizeTexture(u32 width, u32 height) {
  LOG_INFO(Render, "VulkanTexture::ResizeTexture: {}, {}", width, height);
}

void Render::VulkanTexture::GenerateMipmaps() {
  LOG_INFO(Render, "VulkanTexture::GenerateMipmaps");
}

void Render::VulkanTexture::UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) {
  LOG_INFO(Render, "VulkanTexture::UpdateSubRegion: {}, {}, {}, {}, {}", x, y, w, h, static_cast<u8>(format));
}

void Render::VulkanTexture::Bind() {
  LOG_INFO(Render, "VulkanTexture::Bind");
}

void Render::VulkanTexture::Unbind() {
  LOG_INFO(Render, "VulkanTexture::Unbind");
}

void Render::VulkanTexture::DestroyTexture() {
  LOG_INFO(Render, "VulkanTexture::DestroyTexture");
}
#endif
