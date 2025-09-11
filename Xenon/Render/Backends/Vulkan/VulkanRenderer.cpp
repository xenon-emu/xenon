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
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
  instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
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

  graphicsQueueFamily = -1;
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

  std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  deviceCreateInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
  // Create the device
  res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateDevice failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  volkLoadDevice(device);

  // Create the device queue
  vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

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

  CreateCommandBuffer();

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

  if (!SDL_Vulkan_CreateSurface(mainWindow, instance, nullptr, &surface)) {
    LOG_ERROR(Render, "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
    return;
  }

  // Load KHR swapchain functions (volk does this automatically after vkDevice creation)
  if (!vkCreateSwapchainKHR || !vkAcquireNextImageKHR || !vkQueuePresentKHR) {
    LOG_ERROR(Render, "Vulkan swapchain functions not loaded!");
    return;
  }

  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

  chosenPresentMode = Config::rendering.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

  VkSwapchainCreateInfoKHR swapInfo{};
  swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapInfo.surface = surface;
  swapInfo.minImageCount = swapchainImageCount;
  swapInfo.imageFormat = chosenFormat.format;
  swapInfo.imageColorSpace = chosenFormat.colorSpace;
  VkExtent2D newExtent = caps.currentExtent;
  swapInfo.imageExtent = newExtent;
  width = newExtent.width;
  height = newExtent.height;
  swapInfo.imageArrayLayers = 1;
  swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapInfo.preTransform = caps.currentTransform;
  swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapInfo.presentMode = chosenPresentMode;
  swapInfo.clipped = VK_TRUE;
  swapInfo.oldSwapchain = VK_NULL_HANDLE;

  vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapchain);
}

void VulkanRenderer::CreateCommandBuffer() {
  VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  // Useful for resetting individual buffers
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = static_cast<uint32_t>(graphicsQueueFamily);

  VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateCommandPool failed: 0x{:x}", static_cast<u32>(res));
    return;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkAllocateCommandBuffers failed with error 0x{:x}", static_cast<u32>(res));
    return;
  }
}

void VulkanRenderer::BeginCommandBuffer() {
  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

void VulkanRenderer::EndCommandBuffer() {
  vkEndCommandBuffer(commandBuffer);
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
  if (swapchain == VK_NULL_HANDLE)
    return;

  // Create semaphores/fences if not already
  if (imageAvailableSemaphore == VK_NULL_HANDLE) {
    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphore);
    vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphore);

    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled to avoid initial wait
    vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);
  }

  // Wait for the previous frame to finish
  vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
  vkResetFences(device, 1, &inFlightFence);

  // Acquire next swapchain image
  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  } else if (result != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to acquire swapchain image!");
    return;
  }

  BeginCommandBuffer();

  // Render ImGui draw data to current swapchain image
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

  EndCommandBuffer();

  // Submit command buffer
  VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };

  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to submit command buffer!");
    return;
  }

  // Present swapchain image
  VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
  } else if (result != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to present swapchain image!");
  }
}

void VulkanRenderer::RecreateSwapchain() {
  vkDeviceWaitIdle(device);

  // Destroy old swapchain resources
  for (auto view : swapchainImageViews) {
    vkDestroyImageView(device, view, nullptr);
  }
  vkDestroySwapchainKHR(device, swapchain, nullptr);

  // Query new surface capabilities
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

  VkSwapchainCreateInfoKHR swapInfo{};
  swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapInfo.surface = surface;
  swapInfo.minImageCount = swapchainImageCount;
  swapInfo.imageFormat = chosenFormat.format;
  swapInfo.imageColorSpace = chosenFormat.colorSpace;
  swapInfo.imageExtent = { width, height };
  swapInfo.imageArrayLayers = 1;
  swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapInfo.preTransform = caps.currentTransform;
  swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapInfo.presentMode = chosenPresentMode;
  swapInfo.clipped = VK_TRUE;
  swapInfo.oldSwapchain = VK_NULL_HANDLE;

  vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapchain);

  u32 imageCount = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  std::vector<VkImage> swapchainImages(imageCount);
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

  swapchainImageViews.resize(imageCount);
  for (u32 i = 0; i != imageCount; ++i) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = swapchainImages[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = chosenFormat.format;
    viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);
  }
}

s32 VulkanRenderer::GetBackbufferFlags() {
  return 0;
}

s32 VulkanRenderer::GetXenosFlags() {
  return 0;
}

void* VulkanRenderer::GetBackendContext() {
  return reinterpret_cast<void *>(this);
}

u32 VulkanRenderer::GetBackendID() {
  return "Vulkan"_jLower;
}

} // namespace Render
#endif
