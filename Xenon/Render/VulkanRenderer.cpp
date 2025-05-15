// Copyright 2025 Xenon Emulator Project

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "VulkanRenderer.h"

#include <unordered_set>

#ifndef NO_GFX
namespace Render {
  static const std::unordered_set<std::string> RequiredInstanceExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
  #   if defined(_WIN64)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
  #   elif defined(__linux__)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
  #   elif defined(__APPLE__)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
  #   endif
  };

  static const std::unordered_set<std::string> OptionalInstanceExtensions = {
#   if defined(__APPLE__)
    // Tells the system Vulkan loader to enumerate portability drivers, if supported.
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#   endif
};

static const std::unordered_set<std::string> RequiredDeviceExtensions = {
  // VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
};

static const std::unordered_set<std::string> OptionalDeviceExtensions = {};

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

  u32 extensionCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

  std::unordered_set<std::string> requiredExtensions = RequiredInstanceExtensions;
  std::unordered_set<std::string> supportedOptionalExtensions;

  std::unordered_set<std::string> missingRequiredExtensions = requiredExtensions;

  for (uint32_t i = 0; i < extensionCount; i++) {
    const std::string extensionName(availableExtensions[i].extensionName);
    missingRequiredExtensions.erase(extensionName);

    if (OptionalInstanceExtensions.find(extensionName) != OptionalInstanceExtensions.end()) {
      supportedOptionalExtensions.insert(extensionName);
    }
  }

  if (!missingRequiredExtensions.empty()) {
    for (const std::string &extension : missingRequiredExtensions) {
      LOG_ERROR(Render, "Missing required Vulkan extension: {}", extension);
    }

    LOG_ERROR(Render, "Unable to create instance. Required extensions are missing");
    return;
  }

  std::vector<const char *> enabledExtensions;
  for (const std::string &extension : requiredExtensions) {
    enabledExtensions.push_back(extension.c_str());
  }

  for (const std::string &extension : supportedOptionalExtensions) {
    enabledExtensions.push_back(extension.c_str());
  }

  createInfo.ppEnabledExtensionNames = enabledExtensions.data();
  createInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());

#ifdef __APPLE__
  createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  res = vkCreateInstance(&createInfo, nullptr, &instance);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateInstance failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  volkLoadInstance(instance);


  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    LOG_ERROR(Render, "Unable to find devices that support Vulkan.");
    return;
  }

  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

  uint32_t currentDeviceTypeScore = 0;
  uint32_t deviceTypeScoreTable[] = {
    0,  // VK_PHYSICAL_DEVICE_TYPE_OTHER
    3,  // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
    4,  // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
    2,  // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
    1   // VK_PHYSICAL_DEVICE_TYPE_CPU
  };

  VkPhysicalDevice physicalDevice;

  for (uint32_t i = 0; i < deviceCount; i++) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);

    uint32_t deviceTypeIndex = deviceProperties.deviceType;
    if (deviceTypeIndex > 4) {
      continue;
    }

    std::string deviceName(deviceProperties.deviceName);
    uint32_t deviceTypeScore = deviceTypeScoreTable[deviceTypeIndex];
    bool preferDeviceTypeScore = (deviceTypeScore > currentDeviceTypeScore);

    if (preferDeviceTypeScore) {
      physicalDevice = physicalDevices[i];
      currentDeviceTypeScore = deviceTypeScore;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    LOG_ERROR(Render, "Unable to find a device with the required features.");
    return;
  }

  // Check for extensions.
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableDeviceExtensions.data());

  std::unordered_set<std::string> missingRequiredDeviceExtensions = RequiredDeviceExtensions;
  std::unordered_set<std::string> supportedOptionalDeviceExtensions;

  for (uint32_t i = 0; i < extensionCount; i++) {
    const std::string extensionName(availableExtensions[i].extensionName);
    missingRequiredDeviceExtensions.erase(extensionName);

    if (OptionalDeviceExtensions.find(extensionName) != OptionalDeviceExtensions.end()) {
      supportedOptionalDeviceExtensions.insert(extensionName);
    }
  }

  if (!missingRequiredDeviceExtensions.empty()) {
    for (const std::string &extension : missingRequiredDeviceExtensions) {
      LOG_ERROR(Render, "Missing required Vulkan extension: {}", extension);
    }

    LOG_ERROR(Render, "Unable to create device. Required extensions are missing");
    return;
  }

  std::vector<const char *> enabledDeviceExtensions;
  for (const std::string &extension : RequiredDeviceExtensions) {
    enabledDeviceExtensions.push_back(extension.c_str());
  }

  for (const std::string &extension : supportedOptionalExtensions) {
    enabledDeviceExtensions.push_back(extension.c_str());
  }

  VkDeviceCreateInfo createDeviceInfo = {};
  createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.ppEnabledExtensionNames = enabledExtensions.data();
  createInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());

  res = vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device);
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
  allocatorInfo.flags = 0;
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
}

void VulkanRenderer::BackendSDLProperties(SDL_PropertiesID properties) {
  // Enable Vulkan
  SDL_SetNumberProperty(properties, "flags", SDL_WINDOW_VULKAN);
  SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
}

void VulkanRenderer::BackendSDLInit() {

}

void VulkanRenderer::CreateCommandBuffer() {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  // TODO: Properly do queues
  poolInfo.queueFamilyIndex = 0;

  VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateCommandPool failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkAllocateCommandBuffers failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkBeginCommandBuffer failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }
}

void VulkanRenderer::EndCommandBuffer() {
  VkResult res = vkEndCommandBuffer(commandBuffer);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkEndCommandBuffer failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }
}

void VulkanRenderer::CheckActiveRenderPass() {
  EndActiveRenderPass();

  if (renderPass != VK_NULL_HANDLE) {
    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }
}

void VulkanRenderer::EndActiveRenderPass() {
  if (renderPass != VK_NULL_HANDLE) {
    vkCmdEndRenderPass(commandBuffer);
    renderPass = VK_NULL_HANDLE;
  }
}

void VulkanRenderer::BackendShutdown() {
  if (instance != VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
  }

  if (commandBuffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  }

  if (commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, commandPool, nullptr);
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

  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void VulkanRenderer::UpdateViewport(s32 x, s32 y, u32 width, u32 height) {
  VkViewport viewport = {};
  viewport.x = static_cast<f32>(x);
  viewport.y = static_cast<f32>(y);
  viewport.width = static_cast<f32>(width);
  viewport.height = static_cast<f32>(height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void VulkanRenderer::Clear() {

}

void VulkanRenderer::Draw() {
  // TODO: Vertex count
  // vkCmdDraw(commandBuffer, 0, 1, 0, 0);
}

void VulkanRenderer::DrawIndexed(Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
  vkCmdDrawIndexed(commandBuffer, indexBufferInfo.count, 1, 0, 0, 0);
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
  VkPipelineShaderStageCreateInfo stageInfo = {};
  stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage = stageInfo;

  VkPipeline computePipeline;
  VkResult res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
  if (res != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateComputePipelines failed with error code 0x{:x}", static_cast<u32>(res));
    return;
  }

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
  vkCmdDispatch(commandBuffer, width / 16, height / 16, 1);
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

  // TODO: Pipeline Caching
  if (computePipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, computePipeline, nullptr);
  }
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