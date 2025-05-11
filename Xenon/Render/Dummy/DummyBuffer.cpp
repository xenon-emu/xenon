// Copyright 2025 Xenon Emulator Project

#include "DummyBuffer.h"

void Render::DummyBuffer::CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) {
  LOG_INFO(Render, "DummyBuffer::CreateBuffer: {}, {}, {}", size, static_cast<u8>(usage), static_cast<u8>(size));
}

void Render::DummyBuffer::UpdateBuffer(u32 offset, u32 size, const void *data) {
  LOG_INFO(Render, "DummyBuffer::UpdateBuffer: {}, {}", offset, size);
}

void Render::DummyBuffer::Bind() {
  LOG_INFO(Render, "DummyBuffer::Bind");
}

void Render::DummyBuffer::Unbind() {
  LOG_INFO(Render, "DummyBuffer::Unbind");
}

void Render::DummyBuffer::DestroyBuffer() {
  LOG_INFO(Render, "DummyBuffer::DestroyBuffer");
}
