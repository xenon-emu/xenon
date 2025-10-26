/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Render/Abstractions/Renderer.h"

#ifndef NO_GFX

#if defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XCB_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#define VK_ENABLE_BETA_EXTENSIONS

#include <vk_mem_alloc.h>
#include <VkBootstrap.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_vulkan.h>

#include "Base/Version.h"
#include "Base/Hash.h"
#include "Core/RAM/RAM.h"
#include "Core/PCI/PCIe.h"
#include "Render/Abstractions/Factory/ShaderFactory.h"
#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif
#include "Render/GUI/Vulkan.h"

namespace Render {

class VulkanRenderer : public Renderer {
public:
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
  void BeginCommandBuffer();
  void EndCommandBuffer();

  void RecreateSwapchain();

  // vk-bootstrap
  vkb::Instance vkbInstance{};
  vkb::InstanceDispatchTable instanceDispatch{};
  vkb::DispatchTable dispatch{};
  vkb::PhysicalDevice vkbPhys{};
  vkb::Swapchain vkbSwapchain{};
  vkb::Device vkbDevice{};

  // Core
  u64 currentFrame = 0;
  s32 graphicsQueueFamily = 0;
  VmaAllocator allocator = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;

  // Swapchain
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkSurfaceFormatKHR chosenFormat{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
  VkPresentModeKHR chosenPresentMode{};
  u32 swapchainImageCount = 2;
  std::vector<VkImage> swapchainImages{};
  std::vector<VkImageView> swapchainImageViews{};
  std::vector<VkFramebuffer> swapchainFramebuffers{};
  VkExtent2D swapchainExtent{ 0, 0 };

  // Command buffers
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  // Synchronization
  std::vector<VkSemaphore> availableSemaphores{};
  std::vector<VkSemaphore> finishedSemaphores{};
  std::vector<VkFence> inFlightFences{};
  std::vector<VkFence> imagesInFlight{};

  // Render pass
  VkRenderPass renderPass = VK_NULL_HANDLE;

  // Framebuffers
  std::vector<VkFramebuffer> framebuffers{};
};

} // namespace Render
#endif
