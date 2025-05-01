// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Abstractions/Renderer.h"

#ifndef NO_GFX
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_opengl3.h>

#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIe.h"
#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Abstractions/Factory/ShaderFactory.h"
#include "Render/GUI/GUI.h"

namespace Render {

class OGLRenderer : public Renderer {
public:
  OGLRenderer(RAM *ram);
  ~OGLRenderer();
  void BackendSDLProperties(SDL_PropertiesID properties) override;
  void BackendStart() override;
  void BackendShutdown() override;
  void BackendSDLInit() override;
  void BackendSDLShutdown() override;
  void BackendResize(s32 x, s32 y) override;
  void OnCompute() override;
  void OnBind() override;
  void OnSwap(SDL_Window* window) override;
  s32 GetBackbufferFlags() override;
  void* GetBackendContext() override;
private:
  // OpenGL Handles
  // Dummy VAO
  u32 dummyVAO;
  // SDL Context
  SDL_GLContext context;
};

} // namespace Render
#endif
