// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif

#include "Base/Logging/Log.h"

#ifndef NO_GFX

#if defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XCB_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#define VK_ENABLE_BETA_EXTENSIONS

#pragma push_macro("Bool")
#pragma push_macro("None")
#pragma push_macro("Always")
#undef Bool
#undef None
#undef Always

#include <volk.h>
#include <vk_mem_alloc.h>

#pragma pop_macro("Always")
#pragma pop_macro("None")
#pragma pop_macro("Bool")

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
