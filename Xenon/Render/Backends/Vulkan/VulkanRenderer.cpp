/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

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

  vkb::InstanceBuilder builder{};
  auto instRet = builder
    .set_app_name("Xenon")
    .require_api_version(1, 2, 0)
    .use_default_debug_messenger()
    .request_validation_layers(true)
    .build();

  if (!instRet) {
    LOG_ERROR(Render, "Failed to create Vulkan instance: {}", instRet.error().message());
    return;
  }

  vkbInstance = instRet.value();
  instance = vkbInstance.instance;
  volkLoadInstance(instance);

  if (!SDL_Vulkan_CreateSurface(mainWindow, instance, nullptr, &surface)) {
    LOG_ERROR(Render, "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
    return;
  }

  // Pick GPU
  vkb::PhysicalDeviceSelector selector{ vkbInstance };
  auto physRet = selector
    .set_surface(surface)
    .set_minimum_version(1, 2)
    .select();

  if (!physRet) {
    LOG_ERROR(Render, "Failed to select Vulkan GPU: {}", physRet.error().message());
    return;
  }

  vkbPhys = physRet.value();
  physicalDevice = vkbPhys.physical_device;

  // Create device
  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRendering{};
  dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
  dynamicRendering.dynamicRendering = VK_TRUE;
  vkb::DeviceBuilder deviceBuilder{ vkbPhys };
  deviceBuilder.add_pNext(&dynamicRendering);
  auto devRet = deviceBuilder.build();

  if (!devRet) {
    LOG_ERROR(Render, "Failed to create Vulkan device: {}", devRet.error().message());
    return;
  }

  vkbDevice = devRet.value();
  dispatchTable = vkbDevice.make_table();
  device = vkbDevice.device;
  graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  volkLoadDevice(device);

  // Initialize VMA
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

  // Load KHR swapchain functions (volk does this automatically after vkDevice creation)
  if (!vkCreateSwapchainKHR || !vkAcquireNextImageKHR || !vkQueuePresentKHR) {
    LOG_ERROR(Render, "Vulkan swapchain functions not loaded!");
    return;
  }

  // Swapchain setup
  auto swapchainRet = vkb::SwapchainBuilder{ vkbPhys, device, surface }
    .use_default_format_selection()
    .set_desired_present_mode(Config::rendering.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR)
    .set_desired_extent(width, height)
    .build();

  if (!swapchainRet) {
    LOG_ERROR(Render, "Failed to create Vulkan swapchain: {}", swapchainRet.error().message());
    return;
  }

  vkbSwapchain = swapchainRet.value();
  swapchain = vkbSwapchain.swapchain;

  auto imageViewsRet = vkbSwapchain.get_image_views();
  if (!imageViewsRet) {
    LOG_ERROR(Render, "Failed to get swapchain image views: {}", imageViewsRet.error().message());
    return;
  }
  swapchainImageViews = imageViewsRet.value();

  // TODO: Create render pass

  //framebuffers.resize(swapchainImageViews.size(), VK_NULL_HANDLE);
  //for (u64 i = 0; i != data.framebuffers.size(); ++i) {
  //  VkImageView attachments[] = { data.swapchainImageViews[i] };
  //  VkFramebufferCreateInfo framebuffer_info = {};
  //  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  //  framebufferInfo.renderPass = renderPass;
  //  framebufferInfo.attachmentCount = 1;
  //  framebufferInfo.pAttachments = attachments;
  //  framebufferInfo.width = width;
  //  framebufferInfo.height = height;
  //  framebufferInfo.layers = 1;
  //  if (dispatchTable.createFramebuffer(&framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
  //      return -1;
  //  }
  //}

  // Initialize imagesInFlight
  imagesInFlight.resize(swapchainImageViews.size(), VK_NULL_HANDLE);

  chosenFormat.format = vkbSwapchain.image_format;
  chosenFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  VkExtent2D extent = vkbSwapchain.extent;
  width = extent.width;
  height = extent.height;

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
}

void VulkanRenderer::CreateCommandBuffer() {
  VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  // Useful for resetting individual buffers
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = static_cast<u32>(graphicsQueueFamily);

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
  dispatchTable.beginCommandBuffer(commandBuffer, &beginInfo);
}

void VulkanRenderer::EndCommandBuffer() {
  dispatchTable.endCommandBuffer(commandBuffer);
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
  for (auto &fence : imagesInFlight) {
    dispatchTable.destroyFence(fence, nullptr);
  }
  dispatchTable.destroyCommandPool(commandPool, nullptr);
  for (auto &framebuffer : framebuffers) {
    dispatchTable.destroyFramebuffer(framebuffer, nullptr);
  }
  vkbSwapchain.destroy_image_views(swapchainImageViews);
  vkb::destroy_device(vkbDevice);
  vkb::destroy_surface(vkbInstance, surface);
  vkb::destroy_instance(vkbInstance);
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

void VulkanRenderer::OnSwap(SDL_Window* window) {
  if (swapchain == VK_NULL_HANDLE)
    return;

  // Create semaphores/fences if not already
  if (imageAvailableSemaphore == VK_NULL_HANDLE) {
    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphore);
    vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphore);

    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);
  }

  // Acquire next image
  uint32_t imageIndex = 0;
  VkResult result = dispatchTable.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  } else if (result != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to acquire swapchain image! Error: {}", (u32)result);
    return;
  }

  if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  imagesInFlight[imageIndex] = inFlightFence;

  BeginCommandBuffer();

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

  EndCommandBuffer();

  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

  dispatchTable.resetFences(1, &inFlightFence);

  if (dispatchTable.queueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to submit command buffer!");
    return;
  }

  // Present swapchain image
  VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
  VkSwapchainKHR swapchains[] = { swapchain };
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;

  result = dispatchTable.queuePresentKHR(graphicsQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  } else if (result != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to present swapchain image! Error: {}", (u32)result);
  }
}

void VulkanRenderer::RecreateSwapchain() {
  vkDeviceWaitIdle(device);

  // Rebuild swapchain using vk-bootstrap
  vkb::SwapchainBuilder swapchainBuilder{ vkbPhys, device, surface };
  auto swapchainRet = swapchainBuilder
    .use_default_format_selection()
    .set_desired_present_mode(Config::rendering.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR)
    .set_desired_extent(width, height)
    .build();

  if (!swapchainRet) {
    LOG_ERROR(Render, "Failed to create Vulkan swapchain: {}", swapchainRet.error().message());
    return;
  }

  vkbSwapchain = swapchainRet.value();
  swapchain = vkbSwapchain.swapchain;

  auto imageViewsRet = vkbSwapchain.get_image_views();
  if (!imageViewsRet) {
    LOG_ERROR(Render, "Failed to get swapchain image views: {}", imageViewsRet.error().message());
    return;
  }
  swapchainImageViews = imageViewsRet.value();

  chosenFormat.format = vkbSwapchain.image_format;

  CreateCommandBuffer();
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
