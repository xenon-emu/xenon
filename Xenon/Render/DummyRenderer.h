// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Abstractions/Renderer.h"

#include "Base/Logging/Log.h"

namespace Render {

class DummyRenderer : public Renderer {
public:
  DummyRenderer(RAM *ram, SDL_Window *mainWindow);
  ~DummyRenderer();
  void BackendSDLProperties(SDL_PropertiesID properties) override;
  void BackendStart() override;
  void BackendShutdown() override;
  void BackendSDLInit() override;
  void BackendSDLShutdown() override;
  void BackendResize(s32 x, s32 y) override;
  void UpdateScissor(s32 x, s32 y, u32 width, u32 height) override;
  void UpdateViewport(s32 x, s32 y, u32 width, u32 height) override;
  void UpdateClearColor(u8 r, u8 b, u8 g, u8 a) override;
  void OnCompute() override;
  void OnBind() override;
  void OnSwap(SDL_Window* window) override;
  s32 GetBackbufferFlags() override;
  void* GetBackendContext() override;
};

} // namespace Render