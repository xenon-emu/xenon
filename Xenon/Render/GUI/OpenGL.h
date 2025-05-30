// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/GUI/GUI.h"

#ifndef NO_GFX

#include <glad/glad.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>

namespace Render {

class OpenGLGUI : public GUI {
public:
  void InitBackend(void *context) override;
  void ShutdownBackend() override;
  void BeginSwap() override;
  void EndSwap() override;
};

} // namespace Render
#endif
