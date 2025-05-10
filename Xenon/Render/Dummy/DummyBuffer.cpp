// Copyright 2025 Xenon Emulator Project

#include "DummyBuffer.h"

void Render::DummyBuffer::CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) {}

void Render::DummyBuffer::UpdateBuffer(u32 offset, u32 size, const void *data) {}

void Render::DummyBuffer::Bind() {}

void Render::DummyBuffer::Unbind() {}

void Render::DummyBuffer::DestroyBuffer() {}
