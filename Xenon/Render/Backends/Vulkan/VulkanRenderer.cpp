/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "Base/Config.h"

#include "Render/Backends/Vulkan/VulkanRenderer.h"
#include "Render/Backends/Vulkan/VulkanShaders.h"

#include "Render/Vulkan/Factory/VulkanResourceFactory.h"

#ifndef NO_GFX
namespace Render {

void VulkanRenderer::BackendStart() {
  vkb::InstanceBuilder builder{};

  builder.set_debug_callback([](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) -> VkBool32
  {
		auto severity = vkb::to_string_message_severity(messageSeverity);
		auto type = vkb::to_string_message_type(messageType);
    LOG_INFO(Render, "[{}: {}]", severity, type, pCallbackData->pMessage);
		return VK_FALSE;
	});

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

  instanceDispatch = vkbInstance.make_table();

  if (!SDL_Vulkan_CreateSurface(mainWindow, vkbInstance.instance, nullptr, &surface)) {
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

  // Create device
  vkbPhys = physRet.value();
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
  dispatch = vkbDevice.make_table();
  graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  // Initialize VMA
  VmaVulkanFunctions vmaFunctions = {};
  vmaFunctions.vkGetInstanceProcAddr = instanceDispatch.fp_vkGetInstanceProcAddr;
  vmaFunctions.vkGetDeviceProcAddr = vkbDevice.fp_vkGetDeviceProcAddr;
  vmaFunctions.vkGetPhysicalDeviceMemoryProperties = instanceDispatch.fp_vkGetPhysicalDeviceMemoryProperties;
  vmaFunctions.vkGetPhysicalDeviceProperties = instanceDispatch.fp_vkGetPhysicalDeviceProperties;
  vmaFunctions.vkAllocateMemory = dispatch.fp_vkAllocateMemory;
  vmaFunctions.vkBindBufferMemory = dispatch.fp_vkBindBufferMemory;
  vmaFunctions.vkBindImageMemory = dispatch.fp_vkBindImageMemory;
  vmaFunctions.vkCreateBuffer = dispatch.fp_vkCreateBuffer;
  vmaFunctions.vkCreateImage = dispatch.fp_vkCreateImage;
  vmaFunctions.vkDestroyBuffer = dispatch.fp_vkDestroyBuffer;
  vmaFunctions.vkDestroyImage = dispatch.fp_vkDestroyImage;
  vmaFunctions.vkFlushMappedMemoryRanges = dispatch.fp_vkFlushMappedMemoryRanges;

  vmaFunctions.vkGetBufferMemoryRequirements = dispatch.fp_vkGetBufferMemoryRequirements;
  vmaFunctions.vkGetImageMemoryRequirements = dispatch.fp_vkGetImageMemoryRequirements;
  vmaFunctions.vkInvalidateMappedMemoryRanges = dispatch.fp_vkInvalidateMappedMemoryRanges;
  vmaFunctions.vkFreeMemory = dispatch.fp_vkFreeMemory;
  vmaFunctions.vkMapMemory = dispatch.fp_vkMapMemory;
  vmaFunctions.vkUnmapMemory = dispatch.fp_vkUnmapMemory;
  vmaFunctions.vkCmdCopyBuffer = dispatch.fp_vkCmdCopyBuffer;

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = vkbPhys.physical_device;
  allocatorInfo.device = vkbDevice.device;
  allocatorInfo.pVulkanFunctions = &vmaFunctions;
  allocatorInfo.instance = vkbInstance.instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

  VkResult res = vmaCreateAllocator(&allocatorInfo, &allocator);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vmaCreateAllocator failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  // Swapchain setup
  auto swapchainRet = vkb::SwapchainBuilder{ vkbPhys, vkbDevice, surface }
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
  //  if (dispatch.createFramebuffer(&framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
  //      return -1;
  //  }
  //}

  chosenFormat.format = vkbSwapchain.image_format;
  chosenFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  VkExtent2D extent = vkbSwapchain.extent;
  width = extent.width;
  height = extent.height;

  // Initialize syncros
  availableSemaphores.resize(swapchainImageViews.size(), VK_NULL_HANDLE);
  finishedSemaphores.resize(swapchainImageViews.size(), VK_NULL_HANDLE);
  inFlightFences.resize(swapchainImageViews.size(), VK_NULL_HANDLE);
  imagesInFlight.resize(swapchainImageViews.size(), VK_NULL_HANDLE);

  // Create semaphores/fences
  VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

  VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u64 i = 0; i != swapchainImageViews.size(); ++i) {
    VkResult result = dispatch.createSemaphore(&semaphoreInfo, nullptr, &availableSemaphores[i]);
    if (result != VK_SUCCESS) {
      LOG_ERROR(Render, "vkCreateSemaphore[i] failed with error code 0x{:x}", i, static_cast<u32>(result));
      return;
    }
    result = dispatch.createFence(&fenceInfo, nullptr, &inFlightFences[i]);
    if (result != VK_SUCCESS) {
      LOG_ERROR(Render, "createFence[i] failed with error code 0x{:x}", i, static_cast<u32>(result));
      return;
    }
  }

  for (u64 i = 0; i != vkbSwapchain.image_count; ++i) {
    VkResult result = dispatch.createSemaphore(&semaphoreInfo, nullptr, &finishedSemaphores[i]);
    if (result != VK_SUCCESS) {
      LOG_ERROR(Render, "vkCreateSemaphore[i] failed with error code 0x{:x}", i, static_cast<u32>(result));
      return;
    }
  }

  CreateCommandBuffer();

  resourceFactory = std::make_unique<VulkanResourceFactory>(this);
  shaderFactory = resourceFactory->CreateShaderFactory();
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  shaderPath /= "vulkan";
  computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
    { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
  });
  if (!computeShaderProgram) {
    std::ofstream f{ shaderPath / "fb_deswizzle.comp" };
    f.write(computeShaderSource, sizeof(computeShaderSource));
    f.close();
    computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
      { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
    });
  }
  renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
    { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
    { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
  });
  if (!renderShaderPrograms) {
    std::ofstream vert{ shaderPath / "framebuffer.vert" };
    vert.write(vertexShaderSource, sizeof(vertexShaderSource));
    vert.close();
    std::ofstream frag{ shaderPath / "framebuffer.frag" };
    frag.write(fragmentShaderSource, sizeof(fragmentShaderSource));
    frag.close();
    renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
      { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
      { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
    });
  }
}

void VulkanRenderer::CreateCommandBuffer() {
  VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  // Useful for resetting individual buffers
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = static_cast<u32>(graphicsQueueFamily);

  VkResult res = dispatch.createCommandPool(&poolInfo, nullptr, &commandPool);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateCommandPool failed: 0x{:x}", static_cast<u32>(res));
    return;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  res = dispatch.allocateCommandBuffers(&allocInfo, &commandBuffer);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkAllocateCommandBuffers failed with error 0x{:x}", static_cast<u32>(res));
    return;
  }
}

void VulkanRenderer::BeginCommandBuffer() {
  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  dispatch.beginCommandBuffer(commandBuffer, &beginInfo);
}

void VulkanRenderer::EndCommandBuffer() {
  dispatch.endCommandBuffer(commandBuffer);
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
    dispatch.destroyFence(fence, nullptr);
  }
  dispatch.destroyCommandPool(commandPool, nullptr);
  for (auto &framebuffer : framebuffers) {
    dispatch.destroyFramebuffer(framebuffer, nullptr);
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

  // Acquire next image
  u32 imageIndex{};
  VkResult result = dispatch.acquireNextImageKHR(swapchain, UINT64_MAX, availableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  } else if (result != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to acquire swapchain image! Error: {}", (u32)result);
    return;
  }

  if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    dispatch.waitForFences(1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  imagesInFlight[imageIndex] = inFlightFences[currentFrame];

  BeginCommandBuffer();

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

  EndCommandBuffer();

  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &availableSemaphores[currentFrame];
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &finishedSemaphores[imageIndex];

  dispatch.resetFences(1, &inFlightFences[currentFrame]);

  if (dispatch.queueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to submit command buffer!");
    return;
  }

  // Present swapchain image
  VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &finishedSemaphores[imageIndex];
  VkSwapchainKHR swapchains[] = { swapchain };
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;

  result = dispatch.queuePresentKHR(graphicsQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
  } else if (result != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to present swapchain image! Error: {}", (u32)result);
  }
  currentFrame = (currentFrame + 1) % swapchainImageCount;
}

void VulkanRenderer::RecreateSwapchain() {
  dispatch.deviceWaitIdle();

  // Rebuild swapchain using vk-bootstrap
  vkb::SwapchainBuilder swapchainBuilder{ vkbPhys, vkbDevice, surface };
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
