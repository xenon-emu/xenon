// Copyright 2025 Xenon Emulator Project

#include "DummyRenderer.h"

#include "Dummy/Factory/DummyResourceFactory.h"

namespace Render {
DummyRenderer::DummyRenderer(RAM *ram, SDL_Window *mainWindow) :
    Renderer(ram, mainWindow) {
    LOG_WARNING(Render, "Using dummy renderer!");
}

DummyRenderer::~DummyRenderer() {
    Shutdown();
}

void DummyRenderer::BackendStart() {
    resourceFactory = std::make_unique<DummyResourceFactory>();
}

void DummyRenderer::BackendSDLProperties(SDL_PropertiesID properties) {}

void DummyRenderer::BackendSDLInit() {}

void DummyRenderer::BackendShutdown() {}

void DummyRenderer::BackendSDLShutdown() {}

void DummyRenderer::BackendResize(s32 x, s32 y) {}

void DummyRenderer::OnCompute() {}

void DummyRenderer::OnBind() {}

void DummyRenderer::OnSwap(SDL_Window* window) {}

s32 DummyRenderer::GetBackbufferFlags() {
    return 0;
}

void* DummyRenderer::GetBackendContext() {
    return nullptr;
}

} // namespace Render