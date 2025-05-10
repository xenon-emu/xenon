// Copyright 2025 Xenon Emulator Project

#include "DummyTexture.h"

void Render::DummyTexture::CreateTextureHandle(u32 width, u32 height, s32 flags) {}

void Render::DummyTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) {}

void Render::DummyTexture::ResizeTexture(u32 width, u32 height) {}

void Render::DummyTexture::GenerateMipmaps() {}

void Render::DummyTexture::UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) {}

void Render::DummyTexture::Bind() {}

void Render::DummyTexture::Unbind() {}

void Render::DummyTexture::DestroyTexture() {}