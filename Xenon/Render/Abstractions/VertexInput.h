/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "Buffer.h"

#ifndef NO_GFX
namespace Render {

// TODO: Expand this
enum class eVertexFormat : u8 {
  Float32x1,
  Float32x2,
  Float32x3,
  Float32x4,
  UInt8x4Norm
};

struct VertexBinding {
  u32 binding;
  u32 stride;
  bool inputRatePerInstance;
};

struct VertexAttribute {
  u32 location;
  u32 binding;
  eVertexFormat format;
  u32 offset;
};

class VertexInput {
public:
  virtual ~VertexInput() = default;
  virtual void SetBindings(const std::vector<VertexBinding> &bindings) = 0;
  virtual void SetAttributes(const std::vector<VertexAttribute> &attributes) = 0;
  virtual void BindVertexBuffer(u32 binding, std::shared_ptr<Buffer> buffer) = 0;
  virtual void SetIndexBuffer(std::shared_ptr<Buffer> buffer) = 0;
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
};

} // namespace Render
#endif
