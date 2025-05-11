// Copyright 2025 Xenon Emulator Project

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

} // namespace Render