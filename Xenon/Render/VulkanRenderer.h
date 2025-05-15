// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Abstractions/Renderer.h"

#if defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

// For VK_KHR_portability_subset
#define VK_ENABLE_BETA_EXTENSIONS

#include <volk.h>
#include <vk_mem_alloc.h>

#ifndef NO_GFX

#include "Base/Hash.h"

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
  void UpdateScissor(s32 x, s32 y, u32 width, u32 height) override;
  void UpdateViewport(s32 x, s32 y, u32 width, u32 height) override;
  void UpdateClearColor(u8 r, u8 b, u8 g, u8 a) override;
  void Clear() override;

  void Draw() override;
  void DrawIndexed(Xe::XGPU::XeIndexBufferInfo indexBufferInfo) override;

  void OnCompute() override;
  void OnBind() override;
  void OnSwap(SDL_Window *window) override;
  s32 GetBackbufferFlags() override;
  void *GetBackendContext() override;
  u32 GetBackendID() override;

  VmaAllocator allocator = nullptr;
  VkInstance instance = nullptr;
  VkDevice device = nullptr;
};

} // namespace Render

#endif