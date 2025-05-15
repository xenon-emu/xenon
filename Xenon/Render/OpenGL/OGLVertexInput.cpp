// Copyright 2025 Xenon Emulator Project

#include "OGLVertexInput.h"

#ifndef NO_GFX

#include "Base/Assert.h"

Render::OGLVertexInput::OGLVertexInput() {
  glGenVertexArrays(1, &vaoID);
}

Render::OGLVertexInput::~OGLVertexInput() {
  glDeleteVertexArrays(1, &vaoID);
}

void Render::OGLVertexInput::SetBindings(const std::vector<VertexBinding> &bindings) {
  bindingDescs = bindings;
}

void Render::OGLVertexInput::SetAttributes(const std::vector<VertexAttribute> &attributes) {
  attributeDescs = attributes;
}

void Render::OGLVertexInput::BindVertexBuffer(u32 binding, std::shared_ptr<Buffer> buffer) {
  vertexBuffers[binding] = buffer;
}

void Render::OGLVertexInput::SetIndexBuffer(std::shared_ptr<Buffer> buffer) {
  indexBuffer = buffer;
}

void Render::OGLVertexInput::Bind() {
  glBindVertexArray(vaoID);

  for (const auto &attr : attributeDescs) {
    auto it = vertexBuffers.find(attr.binding);
    if (it == vertexBuffers.end())
      continue;
    it->second->Bind();
    u32 glType;
    u8 normalized;
    s32 components;
    GetGLFormat(attr.format, glType, normalized, components);
    glEnableVertexAttribArray(attr.location);
    glVertexAttribPointer(attr.location, components, glType, normalized,
      bindingDescs[attr.binding].stride,
      reinterpret_cast<void*>(static_cast<u64>(attr.offset)));
  }

  if (indexBuffer) {
    indexBuffer->Bind(); // bind IBO
  }
}

void Render::OGLVertexInput::Unbind() {
  glBindVertexArray(0);
  for (const auto& [binding, buffer] : vertexBuffers) {
    buffer->Unbind();
  }
  if (indexBuffer) {
    indexBuffer->Unbind();
  }
}

s32 Render::OGLVertexInput::GetGLFormat(eVertexFormat format, u32 &type, u8 &normalized, s32 &components) {
  switch (format) {
  case eVertexFormat::Float32x1: type = GL_FLOAT; components = 1; normalized = GL_FALSE; break;
  case eVertexFormat::Float32x2: type = GL_FLOAT; components = 2; normalized = GL_FALSE; break;
  case eVertexFormat::Float32x3: type = GL_FLOAT; components = 3; normalized = GL_FALSE; break;
  case eVertexFormat::Float32x4: type = GL_FLOAT; components = 4; normalized = GL_FALSE; break;
  case eVertexFormat::UInt8x4Norm: type = GL_UNSIGNED_BYTE; components = 4; normalized = GL_TRUE; break;
  default: {
    type = GL_FLOAT;
    components = 4;
    normalized = GL_FALSE;
  } break;
  }
  return 0;
}

#endif
