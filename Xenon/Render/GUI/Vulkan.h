// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif

#include "Base/Logging/Log.h"

#ifndef NO_GFX
#define IMGUI_DEFINE_MATH_OPERATORS
//#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl3.h>

namespace Render {

class VulkanGUI : public GUI {
public:
  void InitBackend(void *context) override;
  void ShutdownBackend() override;
  void BeginSwap() override;
  void EndSwap() override;
};

} // namespace Render#
#endif
