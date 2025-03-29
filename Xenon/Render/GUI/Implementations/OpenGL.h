// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"

#ifndef NO_GFX
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>

#include "Render/GUI/GUI.h"

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
