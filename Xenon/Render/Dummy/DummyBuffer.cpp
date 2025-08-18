// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "DummyBuffer.h"

#ifndef NO_GFX
void Render::DummyBuffer::CreateBuffer(u64 size, const void *data, eBufferUsage usage, eBufferType type) {
  LOG_INFO(Render, "DummyBuffer::CreateBuffer: {}, {}, {}", size, static_cast<u8>(usage), static_cast<u8>(size));
}

void Render::DummyBuffer::UpdateBuffer(u64 offset, u64 size, const void *data) {
  LOG_INFO(Render, "DummyBuffer::UpdateBuffer: {}, {}", offset, size);
}

void Render::DummyBuffer::Bind(u32 binding) {
  LOG_INFO(Render, "DummyBuffer::Bind");
}

void Render::DummyBuffer::Unbind() {
  LOG_INFO(Render, "DummyBuffer::Unbind");
}

void Render::DummyBuffer::DestroyBuffer() {
  LOG_INFO(Render, "DummyBuffer::DestroyBuffer");
}
#endif
