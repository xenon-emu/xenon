/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Render/Backends/Dummy/DummyRenderer.h"

#include "Render/Dummy/Factory/DummyResourceFactory.h"

#ifndef NO_GFX
namespace Render {

void DummyRenderer::BackendStart() {
  LOG_INFO(Render, "DummyRenderer::BackendStart");
  resourceFactory = std::make_unique<DummyResourceFactory>();
  shaderFactory = resourceFactory->CreateShaderFactory();
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  shaderPath /= "dummy";
  computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
    { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
  });
  renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
    { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
    { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
  });
}

void DummyRenderer::BackendSDLProperties(SDL_PropertiesID properties) {
  LOG_INFO(Render, "DummyRenderer::BackendSDLProperties");
}

void DummyRenderer::BackendSDLInit() {
  LOG_INFO(Render, "DummyRenderer::BackendSDLInit");
}

void DummyRenderer::BackendShutdown() {
  LOG_INFO(Render, "DummyRenderer::BackendShutdown");
}

void DummyRenderer::BackendSDLShutdown() {
  LOG_INFO(Render, "DummyRenderer::BackendSDLShutdown");
}

void DummyRenderer::BackendResize(s32 x, s32 y) {
  LOG_INFO(Render, "DummyRenderer::BackendResize: {}, {}", x, y);
}

void DummyRenderer::UpdateScissor(s32 x, s32 y, u32 width, u32 height) {
  LOG_INFO(Render, "DummyRenderer::UpdateScissor: {}, {}, {}, {}", x, y, width, height);
}

void DummyRenderer::UpdateViewport(s32 x, s32 y, u32 width, u32 height) {
  LOG_INFO(Render, "DummyRenderer::UpdateViewport: {}, {}, {}, {}", x, y, width, height);
}

void DummyRenderer::UpdateClearColor(u8 r, u8 b, u8 g, u8 a) {
  LOG_INFO(Render, "DummyRenderer::UpdateClearColor: {}, {}, {}, {}", r, g, b, a);
}

void DummyRenderer::UpdateClearDepth(f64 depth) {
  LOG_INFO(Render, "DummyRenderer::UpdateClearDepth: {}", depth);
}

void DummyRenderer::Clear() {
  LOG_INFO(Render, "DummyRenderer::Clear");
}

void DummyRenderer::UpdateViewportFromState(const Xe::XGPU::XenosState *state) {
  LOG_INFO(Render, "DummyRenderer::UpdateViewportFromState");
}

void DummyRenderer::BackendBindPixelBuffer(Buffer *buffer) {
  LOG_INFO(Render, "DummyRenderer::BackendBindPixelBuffer");
}

void DummyRenderer::VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized, const u32 fetchOffset, const u32 fetchStride) {
  LOG_INFO(Render, "DummyRenderer::VertexFetch: loc:{}, comps:{}, float:{}, normalized:{}, offset:{}, stride:{}", location, components, isFloat ? "yes" : "no",
    isNormalized ? "yes" : "no", fetchOffset, fetchStride);
}

void DummyRenderer::Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) {
  LOG_INFO(Render, "DummyRenderer::Draw");
}

void DummyRenderer::DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
  LOG_INFO(Render, "DummyRenderer::DrawIndexed: {}, {}, {}, {}, {}",
    indexBufferInfo.count,
    static_cast<u32>(indexBufferInfo.endianness),
    indexBufferInfo.guestBase,
    static_cast<u32>(indexBufferInfo.indexFormat),
    indexBufferInfo.length);
}

void DummyRenderer::OnCompute() {
  LOG_INFO(Render, "DummyRenderer::OnCompute");
}

void DummyRenderer::OnBind() {
  LOG_INFO(Render, "DummyRenderer::OnBind");
}

void DummyRenderer::OnSwap(SDL_Window *window) {
  LOG_INFO(Render, "DummyRenderer::OnSwap");
}

s32 DummyRenderer::GetBackbufferFlags() {
  LOG_INFO(Render, "DummyRenderer::GetBackbufferFlags");
  return 0;
}

s32 DummyRenderer::GetXenosFlags() {
  LOG_INFO(Render, "DummyRenderer::GetXenosFlags");
  return 0;
}

void* DummyRenderer::GetBackendContext() {
  LOG_INFO(Render, "DummyRenderer::GetBackendContext");
  return nullptr;
}

u32 DummyRenderer::GetBackendID() {
  LOG_INFO(Render, "DummyRenderer::GetBackendContext");
  return "Dummy"_jLower;
}

} // namespace Render
#endif
