// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Abstractions/Renderer.h"

#include <volk.h>
#include <vk_mem_alloc.h>

#if defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#define VK_ENABLE_BETA_EXTENSIONS

#ifndef NO_GFX

namespace Render {

class VulkanRenderer : public Renderer {
public:
  VulkanRenderer(RAM *ram, SDL_Window *mainWindow);
  ~VulkanRenderer();
  void BackendSDLProperties(SDL_PropertiesID properties) override;
  void BackendStart() override;
  void BackendShutdown() override;
  void BackendSDLInit() override;
  void BackendSDLShutdown() override;
  void BackendResize(s32 x, s32 y) override;
  void OnCompute() override;
  void OnBind() override;
  void OnSwap(SDL_Window *window) override;
  s32 GetBackbufferFlags() override;
  void *GetBackendContext() override;

  VmaAllocator allocator = nullptr;
  VkInstance instance = nullptr;
  VkDevice device = nullptr;
};

} // namespace Render

#endif