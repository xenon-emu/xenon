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

inline constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

struct FbConvertPC {
 s32 internalWidth;
 s32 internalHeight;
 s32 resWidth;
 s32 resHeight;
};
static_assert(sizeof(FbConvertPC) == 16);

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
  void BackendBindPixelBuffer(Buffer *buffer) override;
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

  void DestroyPipelines();

  void CreateCommandPoolAndBuffers();
  void CreateSyncObjects();
  void CreateDescriptorLayouts();
  void CreateDescriptorPoolAndSets();

  void CreateFbImage(u32 w, u32 h);

  void CreateComputePipeline();
  void CreateRenderPipeline();

  void UpdateDescriptors(VkBuffer pixelBuffer, VkDeviceSize pixelBufferSize);
  void RecreateSwapchain();

  // vk-bootstrap
  vkb::Instance vkbInstance{};
  vkb::InstanceDispatchTable instanceDispatch{};
  vkb::DispatchTable dispatch{};
  vkb::PhysicalDevice vkbPhys{};
  vkb::Swapchain vkbSwapchain{};
  vkb::Device vkbDevice{};

  // Core
  s32 graphicsQueueFamily = 0;
  VmaAllocator allocator = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;

  // Swapchain
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkSurfaceFormatKHR chosenFormat{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
  VkPresentModeKHR chosenPresentMode{};
  u32 swapchainImageCount = 0;
  std::vector<VkImage> swapchainImages{};
  std::vector<VkImageView> swapchainImageViews{};
  std::vector<VkFramebuffer> swapchainFramebuffers{};
  VkExtent2D swapchainExtent{ 0, 0 };

  // Command buffers
  VkCommandPool commandPool = VK_NULL_HANDLE;
  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers{};

  // Synchronization
  std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailable{};
  std::vector<VkSemaphore> renderFinishedPerImage{};
  std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlight{};
  std::vector<VkFence> imagesInFlight{};
  u32 currentFrame = 0;

  // Converted backbuffer
  VkImage fbImage = VK_NULL_HANDLE;
  VmaAllocation fbAlloc = VK_NULL_HANDLE;
  VkImageView fbView = VK_NULL_HANDLE;
  VkSampler fbSampler = VK_NULL_HANDLE;
  VkImageLayout fbLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // Descriptors
  VkDescriptorSetLayout computeSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout renderSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descPool = VK_NULL_HANDLE;
  VkDescriptorSet computeSet = VK_NULL_HANDLE;
  VkDescriptorSet renderSet  = VK_NULL_HANDLE;

  // Pipelines
  VkPipelineLayout computePL = VK_NULL_HANDLE;
  VkPipeline computePipe = VK_NULL_HANDLE;
  VkPipelineLayout renderPL = VK_NULL_HANDLE;
  VkPipeline renderPipe = VK_NULL_HANDLE;
};

} // namespace Render
#endif
