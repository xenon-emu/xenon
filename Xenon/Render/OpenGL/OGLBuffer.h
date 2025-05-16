// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/Abstractions/Buffer.h"

#ifndef NO_GFX

#define GL_GLEXT_PROTOTYPES
#include <KHR/khrplatform.h>
#include <glad/glad.h>

namespace Render {

class OGLBuffer : public Buffer {
public:
  OGLBuffer() = default;
  ~OGLBuffer() {
    DestroyBuffer();
  }

  void CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) override;
  void UpdateBuffer(u32 offset, u32 size, const void *data) override;
  void Bind(u32 binding) override;
  void Unbind() override;
  void DestroyBuffer() override;
private:
  u32 ConvertBufferType(eBufferType type);
  u32 ConvertUsage(eBufferUsage usage);
  u32 BufferHandle = 0;
  u32 GLTarget = 0, GLUsage = 0;
};

} // namespace Render
#endif
