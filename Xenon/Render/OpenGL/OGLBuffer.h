// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/Abstractions/Buffer.h"

#ifndef NO_GFX

#include <glad/glad.h>

namespace Render {

class OGLBuffer : public Buffer {
public:
  OGLBuffer() = default;
  ~OGLBuffer() {
    DestroyBuffer();
  }

  void CreateBuffer(u64 size, const void *data, eBufferUsage usage, eBufferType type) override;
  void UpdateBuffer(u64 offset, u64 size, const void *data) override;
  void Bind(u32 binding) override;
  void Unbind() override;
  void DestroyBuffer() override;
private:
  u32 ConvertBufferType(eBufferType type);
  u32 ConvertUsage(eBufferUsage usage);
  eBufferType ConvertGLBufferType(u32 type);
  eBufferUsage ConvertGLUsage(u32 usage);
  u32 BufferHandle = 0;
  u32 GLTarget = 0, GLUsage = 0;
};

} // namespace Render
#endif
