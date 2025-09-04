// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanVertexInput.h"

#ifndef NO_GFX
void Render::VulkanVertexInput::SetBindings(const std::vector<VertexBinding> &bindings) {
  LOG_INFO(Render, "VulkanVertexInput::SetBindings: {}", bindings.size());
}

void Render::VulkanVertexInput::SetAttributes(const std::vector<VertexAttribute> &attributes) {
  LOG_INFO(Render, "VulkanVertexInput::SetAttributes: {}", attributes.size());
}

void Render::VulkanVertexInput::BindVertexBuffer(u32 binding, std::shared_ptr<Buffer> buffer) {
  LOG_INFO(Render, "VulkanVertexInput::BindVertexBuffer: {}", binding);
}

void Render::VulkanVertexInput::SetIndexBuffer(std::shared_ptr<Buffer> buffer) {
  LOG_INFO(Render, "VulkanVertexInput::SetIndexBuffer");
}

void Render::VulkanVertexInput::Bind() {
  LOG_INFO(Render, "VulkanVertexInput::Bind");
}

void Render::VulkanVertexInput::Unbind() {
  LOG_INFO(Render, "VulkanVertexInput::Unbind");
}
#endif