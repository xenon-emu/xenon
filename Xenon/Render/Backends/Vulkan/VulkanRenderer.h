// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/Abstractions/Renderer.h"

#ifndef NO_GFX
#include <SDL3/SDL.h>
#define IMGUI_DEFINE_MATH_OPERATORS
//#include <backends/imgui_impl_vulkan.h>

#include "Base/Version.h"
#include "Base/Hash.h"
#include "Core/PCI/Devices/RAM/RAM.h"
#include "Core/PCI/PCIe.h"
#include "Render/Abstractions/Factory/ShaderFactory.h"
#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif

#if defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#define VK_ENABLE_BETA_EXTENSIONS

#include <volk.h>
#include <vk_mem_alloc.h>

namespace Render {

class VulkanRenderer : public Renderer {
public:
  VulkanRenderer(RAM *ram);
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
  void UpdateViewportFromState(const Xe::XGPU::XenosState *state) override;
  void Clear() override;

  void VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized, const u32 fetchOffset, const u32 fetchStride) override;
  void Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) override;
  void DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) override;

  void OnCompute() override;
  void OnBind() override;
  void OnSwap(SDL_Window *window) override;
  s32 GetBackbufferFlags() override;
  s32 GetXenosFlags() override;
  void *GetBackendContext() override;
  u32 GetBackendID() override;

  void CreateCommandBuffer();
  void EndCommandBuffer();

  void CheckActiveRenderPass();
  void EndActiveRenderPass();

  VmaAllocator allocator = VK_NULL_HANDLE;
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

} // namespace Render
#endif
