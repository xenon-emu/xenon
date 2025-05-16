// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Abstractions/Renderer.h"

#ifndef NO_GFX
#include <SDL3/SDL.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_opengl3.h>

#include "Base/Version.h"
#include "Base/Hash.h"
#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIe.h"
#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Abstractions/Factory/ShaderFactory.h"
#include "Render/GUI/GUI.h"

namespace Render {

class OGLRenderer : public Renderer {
public:
  OGLRenderer(RAM *ram, SDL_Window *mainWindow);
  ~OGLRenderer();
  void BackendSDLProperties(SDL_PropertiesID properties) override;
  void BackendStart() override;
  void BackendShutdown() override;
  void BackendSDLInit() override;
  void BackendSDLShutdown() override;
  void BackendResize(s32 x, s32 y) override;
  void UpdateScissor(s32 x, s32 y, u32 width, u32 height) override;
  void UpdateViewport(s32 x, s32 y, u32 width, u32 height) override;
  void UpdateClearColor(u8 r, u8 b, u8 g, u8 a) override;
  void UpdateClearDepth(f64 depth) override;
  void Clear() override;

  void Draw(Xe::XGPU::XenosState *state) override;
  void DrawIndexed(Xe::XGPU::XenosState *state, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) override;

  void OnCompute() override;
  void OnBind() override;
  void OnSwap(SDL_Window* window) override;
  s32 GetBackbufferFlags() override;
  void* GetBackendContext() override;
  u32 GetBackendID() override;
private:
  // OpenGL Handles
  // Dummy VAO
  u32 dummyVAO;
  // SDL Context
  SDL_GLContext context;

  // OpenGL Infos
  std::string gl_version() const;
  std::string gl_vendor() const;
  std::string gl_renderer() const;
};

} // namespace Render
#endif
