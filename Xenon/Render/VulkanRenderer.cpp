// Copyright 2025 Xenon Emulator Project

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "VulkanRenderer.h"

#ifndef NO_GFX
namespace Render {

VulkanRenderer::VulkanRenderer(RAM *ram, SDL_Window *mainWindow) :
  Renderer(ram, mainWindow)
{}

VulkanRenderer::~VulkanRenderer() {
  Shutdown();
}

void VulkanRenderer::BackendStart() {
  VkResult res = volkInitialize();
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "volkInitialize failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Xenon";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Xenon";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.ppEnabledLayerNames = nullptr;
  createInfo.enabledLayerCount = 0;
  // TODO: Add extension support
  createInfo.ppEnabledExtensionNames = nullptr;
  createInfo.enabledExtensionCount = 0;

#ifdef __APPLE__
  createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  res = vkCreateInstance(&createInfo, nullptr, &instance);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateInstance failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  volkLoadInstance(instance);

  LOG_ERROR(Render, "SUCCESS!!!");

  VmaVulkanFunctions vmaFunctions = {};
  vmaFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vmaFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  vmaFunctions.vkAllocateMemory = vkAllocateMemory;
  vmaFunctions.vkBindBufferMemory = vkBindBufferMemory;
  vmaFunctions.vkBindImageMemory = vkBindImageMemory;
  vmaFunctions.vkCreateBuffer = vkCreateBuffer;
  vmaFunctions.vkCreateImage = vkCreateImage;
  vmaFunctions.vkDestroyBuffer = vkDestroyBuffer;
  vmaFunctions.vkDestroyImage = vkDestroyImage;
  vmaFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
  vmaFunctions.vkFreeMemory = vkFreeMemory;
  vmaFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
  vmaFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
  vmaFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
  vmaFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
  vmaFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
  vmaFunctions.vkMapMemory = vkMapMemory;
  vmaFunctions.vkUnmapMemory = vkUnmapMemory;
  vmaFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.flags = 0;
  allocatorInfo.physicalDevice = nullptr;
  allocatorInfo.device = nullptr;
  allocatorInfo.pVulkanFunctions = &vmaFunctions;
  allocatorInfo.instance = instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

  res = vmaCreateAllocator(&allocatorInfo, &allocator);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vmaCreateAllocator failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }
}

void VulkanRenderer::BackendSDLProperties(SDL_PropertiesID properties) {
  // Enable Vulkan
  SDL_SetNumberProperty(properties, "flags", SDL_WINDOW_VULKAN);
  SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
}

void VulkanRenderer::BackendSDLInit() {

}

void VulkanRenderer::BackendShutdown() {
  if (instance != nullptr) {
    vkDestroyInstance(instance, nullptr);
  }
}

void VulkanRenderer::BackendSDLShutdown() {

}

void VulkanRenderer::BackendResize(s32 x, s32 y) {

}

void VulkanRenderer::UpdateScissor(s32 x, s32 y, u32 width, u32 height) {
  VkRect2D scissor = {};
  scissor.extent.width = width;
  scissor.extent.height = height;
  scissor.offset.x = x;
  scissor.offset.y = y;

  // vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void VulkanRenderer::UpdateViewport(s32 x, s32 y, u32 width, u32 height) {
  VkViewport viewport = {};
  viewport.x = static_cast<f32>(x);
  viewport.y = static_cast<f32>(y);
  viewport.width = static_cast<f32>(width);
  viewport.height = static_cast<f32>(height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  // vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void VulkanRenderer::Clear() {

}

void VulkanRenderer::Draw() {

}

void VulkanRenderer::DrawIndexed(Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {

}

void VulkanRenderer::UpdateClearColor(u8 r, u8 b, u8 g, u8 a) {
  VkClearAttachment attachment = {};
  auto &rgba = attachment.clearValue.color.float32;
  rgba[0] = static_cast<f32>(r) / 255.0f;
  rgba[1] = static_cast<f32>(g) / 255.0f;
  rgba[2] = static_cast<f32>(b) / 255.0f;
  rgba[3] = static_cast<f32>(a) / 255.0f;
  attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  // attachment.colorAttachment = 0;
  // vkCmdClearAttachments(commandBuffer, 1, &attachment, 1, &clearRect);
}

void VulkanRenderer::OnCompute() {

}

void VulkanRenderer::OnBind() {

}

void VulkanRenderer::OnSwap(SDL_Window *window) {

}

s32 VulkanRenderer::GetBackbufferFlags() {
  return 0;
}

void *VulkanRenderer::GetBackendContext() {
  return nullptr;
}

u32 VulkanRenderer::GetBackendID() {
  return "Vulkan"_j;
}

} // namespace Render
#endif