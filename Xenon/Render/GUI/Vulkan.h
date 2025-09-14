// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif

#include "Base/Logging/Log.h"

#ifndef NO_GFX

#include "Render/Backends/Vulkan/VulkanRenderer.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_IMPL_VULKAN_USE_VOLK
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl3.h>

namespace Render {

class VulkanRenderer;
class VulkanGUI : public GUI {
public:
  void InitBackend(void *context) override;
  void ShutdownBackend() override;
  void BeginSwap() override;
  void EndSwap() override;
private:
  VulkanRenderer *renderer{};
  VkDescriptorPool imguiDescriptorPool{};
};

} // namespace Render#
#endif
