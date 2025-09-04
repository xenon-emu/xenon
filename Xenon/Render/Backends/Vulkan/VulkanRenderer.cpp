// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Render/Backends/Vulkan/VulkanRenderer.h"

#include "Render/Vulkan/Factory/VulkanResourceFactory.h"

#ifndef NO_GFX
namespace Render {

VulkanRenderer::VulkanRenderer(RAM *ram) :
  Renderer(ram)
{}

void VulkanRenderer::BackendStart() {
  resourceFactory = std::make_unique<VulkanResourceFactory>();
  shaderFactory = resourceFactory->CreateShaderFactory();
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  shaderPath /= "vulkan";
  computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
    { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
  });
  renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
    { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
    { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
  });
}

void VulkanRenderer::BackendSDLProperties(SDL_PropertiesID properties) {
  LOG_INFO(Render, "VulkanRenderer::BackendSDLProperties");
}

void VulkanRenderer::BackendSDLInit() {
  LOG_INFO(Render, "VulkanRenderer::BackendSDLInit");
}

void VulkanRenderer::BackendShutdown() {
  LOG_INFO(Render, "VulkanRenderer::BackendShutdown");
}

void VulkanRenderer::BackendSDLShutdown() {
  LOG_INFO(Render, "VulkanRenderer::BackendSDLShutdown");
}

void VulkanRenderer::BackendResize(s32 x, s32 y) {
  LOG_INFO(Render, "VulkanRenderer::BackendResize: {}, {}", x, y);
}

void VulkanRenderer::UpdateScissor(s32 x, s32 y, u32 width, u32 height) {
  LOG_INFO(Render, "VulkanRenderer::UpdateScissor: {}, {}, {}, {}", x, y, width, height);
}

void VulkanRenderer::UpdateViewport(s32 x, s32 y, u32 width, u32 height) {
  LOG_INFO(Render, "VulkanRenderer::UpdateViewport: {}, {}, {}, {}", x, y, width, height);
}

void VulkanRenderer::UpdateClearColor(u8 r, u8 b, u8 g, u8 a) {
  LOG_INFO(Render, "VulkanRenderer::UpdateClearColor: {}, {}, {}, {}", r, g, b, a);
}

void VulkanRenderer::UpdateClearDepth(f64 depth) {
  LOG_INFO(Render, "VulkanRenderer::UpdateClearDepth: {}", depth);
}

void VulkanRenderer::Clear() {
  LOG_INFO(Render, "VulkanRenderer::Clear");
}

void VulkanRenderer::UpdateViewportFromState(const Xe::XGPU::XenosState *state) {
  LOG_INFO(Render, "VulkanRenderer::UpdateViewportFromState");
}

void VulkanRenderer::VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized, const u32 fetchOffset, const u32 fetchStride) {
  LOG_INFO(Render, "VulkanRenderer::VertexFetch: loc:{}, comps:{}, float:{}, normalized:{}, offset:{}, stride:{}", location, components, isFloat ? "yes" : "no",
    isNormalized ? "yes" : "no", fetchOffset, fetchStride);
}

void VulkanRenderer::Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) {
  LOG_INFO(Render, "VulkanRenderer::Draw");
}

void VulkanRenderer::DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
  LOG_INFO(Render, "VulkanRenderer::DrawIndexed: {}, {}, {}, {}, {}",
    indexBufferInfo.count,
    static_cast<u32>(indexBufferInfo.endianness),
    indexBufferInfo.guestBase,
    static_cast<u32>(indexBufferInfo.indexFormat),
    indexBufferInfo.length);
}

void VulkanRenderer::OnCompute() {
  LOG_INFO(Render, "VulkanRenderer::OnCompute");
}

void VulkanRenderer::OnBind() {
  LOG_INFO(Render, "VulkanRenderer::OnBind");
}

void VulkanRenderer::OnSwap(SDL_Window* window) {
  LOG_INFO(Render, "VulkanRenderer::OnSwap");
}

s32 VulkanRenderer::GetBackbufferFlags() {
  LOG_INFO(Render, "VulkanRenderer::GetBackbufferFlags");
  return 0;
}

s32 VulkanRenderer::GetXenosFlags() {
  LOG_INFO(Render, "VulkanRenderer::GetXenosFlags");
  return 0;
}

void* VulkanRenderer::GetBackendContext() {
  return nullptr;
}

u32 VulkanRenderer::GetBackendID() {
  return "Vulkan"_jLower;
}

} // namespace Render
#endif
