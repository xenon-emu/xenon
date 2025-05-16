// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "DummyVertexInput.h"

void Render::DummyVertexInput::SetBindings(const std::vector<VertexBinding> &bindings) {
  LOG_INFO(Render, "DummyVertexInput::SetBindings: {}", bindings.size());
}

void Render::DummyVertexInput::SetAttributes(const std::vector<VertexAttribute> &attributes) {
  LOG_INFO(Render, "DummyVertexInput::SetAttributes: {}", attributes.size());
}

void Render::DummyVertexInput::BindVertexBuffer(u32 binding, std::shared_ptr<Buffer> buffer) {
  LOG_INFO(Render, "DummyVertexInput::BindVertexBuffer: {}", binding);
}

void Render::DummyVertexInput::SetIndexBuffer(std::shared_ptr<Buffer> buffer) {
  LOG_INFO(Render, "DummyVertexInput::SetIndexBuffer");
}

void Render::DummyVertexInput::Bind() {
  LOG_INFO(Render, "DummyVertexInput::Bind");
}

void Render::DummyVertexInput::Unbind() {
  LOG_INFO(Render, "DummyVertexInput::Unbind");
}