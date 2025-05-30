// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <memory>
#include <unordered_map>

#include "Render/Abstractions/VertexInput.h"
#include "Render/Abstractions/Buffer.h"

#ifndef NO_GFX

#include <glad/glad.h>

namespace Render {

class OGLVertexInput : public VertexInput {
public:
  OGLVertexInput();
  ~OGLVertexInput();

  void SetBindings(const std::vector<VertexBinding>& bindings) override;
  void SetAttributes(const std::vector<VertexAttribute>& attributes) override;
  void BindVertexBuffer(u32 binding, std::shared_ptr<Buffer> buffer) override;
  void SetIndexBuffer(std::shared_ptr<Buffer> buffer) override;
  void Bind() override;
  void Unbind() override;
private:
  s32 GetGLFormat(eVertexFormat format, u32 &type, u8 &normalized, s32 &components);
  u32 vaoID = 0;
  std::vector<VertexBinding> bindingDescs = {};
  std::vector<VertexAttribute> attributeDescs = {};
  std::unordered_map<u32, std::shared_ptr<Buffer>> vertexBuffers = {};
  std::shared_ptr<Buffer> indexBuffer = nullptr;
};

} // namespace Render
#endif
