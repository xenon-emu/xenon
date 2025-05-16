// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "DummyRenderer.h"

#include "Dummy/Factory/DummyResourceFactory.h"

namespace Render {
DummyRenderer::DummyRenderer(RAM *ram, SDL_Window *mainWindow) :
  Renderer(ram, mainWindow) {
  LOG_WARNING(Render, "DummyRenderer::DummyRenderer: Using dummy renderer!");
}

DummyRenderer::~DummyRenderer() {
  LOG_INFO(Render, "DummyRenderer::~DummyRenderer");
  Shutdown();
}

void DummyRenderer::BackendStart() {
  LOG_INFO(Render, "DummyRenderer::BackendStart");
  resourceFactory = std::make_unique<DummyResourceFactory>();
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

void DummyRenderer::Draw(Xe::XGPU::XenosState *state) {
  LOG_INFO(Render, "DummyRenderer::Draw");
}

void DummyRenderer::DrawIndexed(Xe::XGPU::XenosState *state, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
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

void DummyRenderer::OnSwap(SDL_Window* window) {
  LOG_INFO(Render, "DummyRenderer::OnSwap");
}

s32 DummyRenderer::GetBackbufferFlags() {
  LOG_INFO(Render, "DummyRenderer::GetBackbufferFlags");
  return 0;
}

void* DummyRenderer::GetBackendContext() {
  LOG_INFO(Render, "DummyRenderer::GetBackendContext");
  return nullptr;
}

u32 DummyRenderer::GetBackendID() {
  LOG_INFO(Render, "DummyRenderer::GetBackendContext");
  return "Dummy"_j;
}

} // namespace Render