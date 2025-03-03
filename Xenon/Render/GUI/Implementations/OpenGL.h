// Copyright 2025 Xenon Emulator Project

#pragma once

#define GL_GLEXT_PROTOTYPES
extern "C" {
#include <KHR/khrplatform.h>
#include <glad/glad.h>
}
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>

#include "Render/GUI/GUI.h"

namespace Render {

class OpenGLGUI : public GUI {
public:
  void InitBackend(SDL_Window* window, void* context) override;
  void ShutdownBackend() override;
  void BeginSwap() override;
  void OnSwap() override;
  void EndSwap() override;
};

} // namespace Render