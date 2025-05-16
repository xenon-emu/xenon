// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "DummyTexture.h"

void Render::DummyTexture::CreateTextureHandle(u32 width, u32 height, s32 flags) {
  LOG_INFO(Render, "DummyTexture::CreateTextureHandle: {}, {}, {}", width, height, flags);
}

void Render::DummyTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) {
  LOG_INFO(Render, "DummyTexture::CreateTextureWithData: {}, {}, {}, {}, {}", width, height, static_cast<u8>(format), dataSize, flags);
}

void Render::DummyTexture::ResizeTexture(u32 width, u32 height) {
  LOG_INFO(Render, "DummyTexture::ResizeTexture: {}, {}", width, height);
}

void Render::DummyTexture::GenerateMipmaps() {
  LOG_INFO(Render, "DummyTexture::GenerateMipmaps");
}

void Render::DummyTexture::UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) {
  LOG_INFO(Render, "DummyTexture::UpdateSubRegion: {}, {}, {}, {}, {}", x, y, w, h, static_cast<u8>(format));
}

void Render::DummyTexture::Bind() {
  LOG_INFO(Render, "DummyTexture::Bind");
}

void Render::DummyTexture::Unbind() {
  LOG_INFO(Render, "DummyTexture::Unbind");
}

void Render::DummyTexture::DestroyTexture() {
  LOG_INFO(Render, "DummyTexture::DestroyTexture");
}