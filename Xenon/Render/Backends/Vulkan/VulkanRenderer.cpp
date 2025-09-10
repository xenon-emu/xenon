// Copyright 2025 Xenon Emulator Project. All rights reserved.

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "Base/Config.h"

#include "Render/Backends/Vulkan/VulkanRenderer.h"

#include "Render/Vulkan/Factory/VulkanResourceFactory.h"

#ifndef NO_GFX
namespace Render {

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

  std::vector<const char*> instanceExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME
  };

#if defined(VK_USE_PLATFORM_XCB_KHR)
  instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  instanceExtensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
  instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.ppEnabledLayerNames = nullptr;
  createInfo.enabledExtensionCount = static_cast<u32>(instanceExtensions.size());
  createInfo.ppEnabledExtensionNames = instanceExtensions.data();
#ifdef __APPLE__
  createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  res = vkCreateInstance(&createInfo, nullptr, &instance);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateInstance failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  volkLoadInstance(instance);

  u32 gpuCount = 0;
  vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
  if (gpuCount == 0) {
    LOG_ERROR(Render, "No Vulkan-compatible GPU found!");
    return;
  }
  std::vector<VkPhysicalDevice> gpus(gpuCount);
  vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

  physicalDevice = (Config::rendering.gpuId != -1) ? gpus[Config::rendering.gpuId] : gpus[0];

  // Pick queue family
  u32 queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

  s32 graphicsQueueFamily = -1;
  for (u32 i = 0; i < queueFamilies.size(); i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphicsQueueFamily = i;
      break;
    }
  }

  if (graphicsQueueFamily == -1) {
    LOG_ERROR(Render, "No graphics-capable queue family found!");
    return;
  }

  f32 priority = 1.f;
  VkDeviceQueueCreateInfo queueCreateInfo = {};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &priority;

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

  res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateDevice failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

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
  allocatorInfo.physicalDevice = physicalDevice;
  allocatorInfo.device = device;
  allocatorInfo.pVulkanFunctions = &vmaFunctions;
  allocatorInfo.instance = instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

  res = vmaCreateAllocator(&allocatorInfo, &allocator);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vmaCreateAllocator failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  resourceFactory = std::make_unique<VulkanResourceFactory>(this);
  shaderFactory = resourceFactory->CreateShaderFactory();
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  shaderPath /= "vulkan";
  computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
    { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
  });
  renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
    { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
    { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
  });
}

void VulkanRenderer::BackendSDLProperties(SDL_PropertiesID properties) {
  // Enable Vulkan
  SDL_SetNumberProperty(properties, "flags", SDL_WINDOW_VULKAN);
  SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
}

void VulkanRenderer::BackendSDLInit() {
  LOG_INFO(Render, "VulkanRenderer::BackendSDLInit");
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

}

void VulkanRenderer::UpdateViewport(s32 x, s32 y, u32 width, u32 height) {

}

void VulkanRenderer::UpdateClearColor(u8 r, u8 b, u8 g, u8 a) {

}

void VulkanRenderer::UpdateClearDepth(f64 depth) {

}

void VulkanRenderer::Clear() {

}

void VulkanRenderer::UpdateViewportFromState(const Xe::XGPU::XenosState *state) {

}

void VulkanRenderer::VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized, const u32 fetchOffset, const u32 fetchStride) {

}

void VulkanRenderer::Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) {

}

void VulkanRenderer::DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {

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

s32 VulkanRenderer::GetXenosFlags() {
  return 0;
}

void* VulkanRenderer::GetBackendContext() {
  return reinterpret_cast<void*>(device);
}

u32 VulkanRenderer::GetBackendID() {
  return "Vulkan"_jLower;
}

} // namespace Render
#endif
