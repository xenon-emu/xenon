/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "Base/Config.h"

#include "Render/Backends/Vulkan/VulkanRenderer.h"
#include "Render/Backends/Vulkan/VulkanShaders.h"

#include "Render/Vulkan/Factory/VulkanResourceFactory.h"
#include "Render/Vulkan/VulkanShader.h"

#define VK_CHECK(...) { \
  VkResult r = __VA_ARGS__; \
  if (r != VK_SUCCESS) { \
    LOG_ERROR(Render, "Failed Vulkan call: 0x{:x}", (u32)r); \
    THROW(true); \
    return; \
  } \
}

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
    .add_required_extensions({
      // Explicitly request the extension when not using Vulkan 1.3 core
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
      // We want to set vertex input layours per-draw
      // For various reasons, this makes life easier with Xenos
      VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME
    })
    .select();

  if (!physRet) {
    LOG_ERROR(Render, "Failed to select Vulkan GPU: {}", physRet.error().message());
    return;
  }

  // Create device
  vkbPhys = physRet.value();

  // Enable VK_KHR_dynamic_rendering
  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynFeat{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
    .pNext = nullptr,
    .dynamicRendering = VK_TRUE,
  };

  // Enable VK_EXT_vertex_input_dynamic_state
  VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vidFeat{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
    .pNext = nullptr,
    .vertexInputDynamicState = VK_TRUE,
  };

  vkb::DeviceBuilder deviceBuilder{ vkbPhys };
  deviceBuilder.add_pNext(&dynFeat);
  deviceBuilder.add_pNext(&vidFeat);

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

  auto imagesRet = vkbSwapchain.get_images();
  if (!imagesRet) {
    LOG_ERROR(Render, "Failed to get swapchain images: {}", imagesRet.error().message());
    return;
  }
  swapchainImages = imagesRet.value();

  chosenFormat.format = vkbSwapchain.image_format;

  width = vkbSwapchain.extent.width;
  height = vkbSwapchain.extent.height;

  swapchainImageCount = swapchainImageViews.size();

  swapchainImageLayouts.assign(swapchainImageCount, VK_IMAGE_LAYOUT_UNDEFINED);

  imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);

  renderFinishedPerImage.resize(swapchainImageCount);

  VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  for (u32 i = 0; i < swapchainImageCount; ++i) {
    VkResult r = dispatch.createSemaphore(&si, nullptr, &renderFinishedPerImage[i]);
    if (r != VK_SUCCESS) {
      LOG_ERROR(Render, "Failed to get create a sempaphore: 0x{:x}", (u32)r);
      return;
    }
  }

  resourceFactory = std::make_unique<VulkanResourceFactory>(this);
  shaderFactory = resourceFactory->CreateShaderFactory();
  // Uses GLSL code, which isn't valid yet.
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  shaderPath /= "vulkan";
  computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
    { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
  });
  if (!computeShaderProgram) {
    std::ofstream f{ shaderPath / "fb_deswizzle.comp" };
    f.write(Vulkan::computeShaderSource, sizeof(Vulkan::computeShaderSource));
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
    vert.write(Vulkan::vertexShaderSource, sizeof(Vulkan::vertexShaderSource));
    vert.close();
    std::ofstream frag{ shaderPath / "framebuffer.frag" };
    frag.write(Vulkan::fragmentShaderSource, sizeof(Vulkan::fragmentShaderSource));
    frag.close();
    renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
      { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
      { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
    });
  }

  CreateCommandPoolAndBuffers();

  CreateSyncObjects();

  CreateDescriptorLayouts();
  CreateDescriptorPoolAndSets();

  CreateFbImage(width, height);

  CreateComputePipeline();
  CreateRenderPipeline();
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
  dispatch.deviceWaitIdle();

  DestroyPipelines();

  // destroy guest pipelines / layout / shader modules / vb
  for (auto& it : guestPipelines_) dispatch.destroyPipeline(it.second, nullptr);
  guestPipelines_.clear();

  if (guestPL_) { dispatch.destroyPipelineLayout(guestPL_, nullptr); guestPL_ = VK_NULL_HANDLE; }

  for (auto& it : vsModules_) dispatch.destroyShaderModule(it.second, nullptr);
  for (auto& it : psModules_) dispatch.destroyShaderModule(it.second, nullptr);
  vsModules_.clear();
  psModules_.clear();

  if (guestVB_) {
    if (guestVBMap_) { vmaUnmapMemory(allocator, guestVBAlloc_); guestVBMap_ = nullptr; }
    vmaDestroyBuffer(allocator, guestVB_, guestVBAlloc_);
    guestVB_ = VK_NULL_HANDLE;
    guestVBAlloc_ = VK_NULL_HANDLE;
    guestVBCap_ = 0;
  }

  for (auto sem : renderFinishedPerImage) {
    if (sem)
      dispatch.destroySemaphore(sem, nullptr);
  }
  renderFinishedPerImage.clear();

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (imageAvailable[i])
      dispatch.destroySemaphore(imageAvailable[i], nullptr);
    imageAvailable[i] = VK_NULL_HANDLE;

    if (inFlight[i])
      dispatch.destroyFence(inFlight[i], nullptr);
    inFlight[i] = VK_NULL_HANDLE;
  }

  if (commandPool) {
    dispatch.destroyCommandPool(commandPool, nullptr);
    commandPool = VK_NULL_HANDLE;
  }

  if (!swapchainImageViews.empty())
    vkbSwapchain.destroy_image_views(swapchainImageViews);
  swapchainImageViews.clear();
  swapchainImages.clear();

  if (allocator) {
    vmaDestroyAllocator(allocator);
    allocator = VK_NULL_HANDLE;
  }

  vkb::destroy_device(vkbDevice);
  vkb::destroy_surface(vkbInstance, surface);
  vkb::destroy_instance(vkbInstance);
}

static void CmdImageBarrier(VkCommandBuffer cmd, vkb::DispatchTable &dispatch,
                            VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
  VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  b.oldLayout = oldLayout;
  b.newLayout = newLayout;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.baseMipLevel = 0;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.baseArrayLayer = 0;
  b.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  // default for UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL (or any "first use")
  b.srcAccessMask = 0;
  b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  // COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
  if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
      newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
  {
    srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.dstAccessMask = 0;
  }

  // PRESENT_SRC_KHR -> COLOR_ATTACHMENT_OPTIMAL (re-acquired image)
  if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR &&
      newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
  {
    srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    b.srcAccessMask = 0;
    b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }

  dispatch.cmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

static void CmdImageBarrier2(
  vkb::DispatchTable &d, VkCommandBuffer cmd,
  VkImage img,
  VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkImageLayout oldL,
  VkPipelineStageFlags dstStage, VkAccessFlags dstAccess, VkImageLayout newL)
{
  VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  b.srcAccessMask = srcAccess;
  b.dstAccessMask = dstAccess;
  b.oldLayout = oldL;
  b.newLayout = newL;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = img;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.baseMipLevel = 0;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.baseArrayLayer = 0;
  b.subresourceRange.layerCount = 1;

  d.cmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

void VulkanRenderer::CreateCommandPoolAndBuffers() {
  VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = (u32)graphicsQueueFamily;

  VkResult r = dispatch.createCommandPool(&poolInfo, nullptr, &commandPool);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateCommandPool failed: 0x{:x}", (u32)r);
    return;
  }

  VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  ai.commandPool = commandPool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

  r = dispatch.allocateCommandBuffers(&ai, commandBuffers.data());
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkAllocateCommandBuffers failed: 0x{:x}", (u32)r);
    return;
  }
}

void VulkanRenderer::CreateSyncObjects() {
  VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkResult r = dispatch.createSemaphore(&si, nullptr, &imageAvailable[i]);
    if (r != VK_SUCCESS) {
      LOG_ERROR(Render, "createSemaphore(imageAvailable) failed: 0x{:x}", (u32)r);
      return;
    }

    r = dispatch.createFence(&fi, nullptr, &inFlight[i]);
    if (r != VK_SUCCESS) {
      LOG_ERROR(Render, "createFence(inFlight) failed: 0x{:x}", (u32)r);
      return;
    }
  }

  imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);
}

void VulkanRenderer::CreateDescriptorLayouts() {
  // Compute layout
  VkDescriptorSetLayoutBinding c0{};
  c0.binding = 0;
  c0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  c0.descriptorCount = 1;
  c0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding c1{};
  c1.binding = 1;
  c1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  c1.descriptorCount = 1;
  c1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding computeBinds[] = { c0, c1 };
  VkDescriptorSetLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  ci.bindingCount = 2;
  ci.pBindings = computeBinds;
  VK_CHECK(dispatch.createDescriptorSetLayout(&ci, nullptr, &computeSetLayout));

  // Render layout (usampler2D)
  VkDescriptorSetLayoutBinding r0{};
  r0.binding = 0;
  r0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  r0.descriptorCount = 1;
  r0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo ri{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  ri.bindingCount = 1;
  ri.pBindings = &r0;
  VK_CHECK(dispatch.createDescriptorSetLayout(&ri, nullptr, &renderSetLayout));
}

void VulkanRenderer::CreateDescriptorPoolAndSets() {
  VkDescriptorPoolSize sizes[] = {
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  4 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
  };

  VkDescriptorPoolCreateInfo pi{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  pi.maxSets = 8;
  pi.poolSizeCount = (u32)std::size(sizes);
  pi.pPoolSizes = sizes;

  VK_CHECK(dispatch.createDescriptorPool(&pi, nullptr, &descPool));

  VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  ai.descriptorPool = descPool;

  VkDescriptorSetLayout one = computeSetLayout;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &one;
  VK_CHECK(dispatch.allocateDescriptorSets(&ai, &computeSet));

  one = renderSetLayout;
  ai.pSetLayouts = &one;
  VK_CHECK(dispatch.allocateDescriptorSets(&ai, &renderSet));
}

void VulkanRenderer::CreateFbImage(u32 w, u32 h) {
  // Destroy old
  if (fbView) {
    dispatch.destroyImageView(fbView, nullptr);
    fbView = VK_NULL_HANDLE;
  }
  if (fbImage) {
    vmaDestroyImage(allocator, fbImage, fbAlloc);
    fbImage = VK_NULL_HANDLE;
    fbAlloc = VK_NULL_HANDLE;
  }
  if (fbSampler) {
    dispatch.destroySampler(fbSampler, nullptr);
    fbSampler = VK_NULL_HANDLE;
  }

  fbLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImageCreateInfo ii{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = VK_FORMAT_R32_UINT;
  ii.extent = { w, h, 1 };
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo ai{};
  ai.usage = VMA_MEMORY_USAGE_AUTO;

  VkResult r = vmaCreateImage(allocator, &ii, &ai, &fbImage, &fbAlloc, nullptr);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vmaCreateImage(fbImage) failed: 0x{:x}", (u32)r);
    return;
  }

  VkImageViewCreateInfo vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  vi.image = fbImage;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R32_UINT;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;

  r = dispatch.createImageView(&vi, nullptr, &fbView);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "createImageView(fbView) failed: 0x{:x}", (u32)r);
    return;
  }

  VkSamplerCreateInfo si{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
  si.magFilter = VK_FILTER_NEAREST;
  si.minFilter = VK_FILTER_NEAREST;
  si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.minLod = 0.f;
  si.maxLod = 0.f;

  r = dispatch.createSampler(&si, nullptr, &fbSampler);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "createSampler(fbSampler) failed: 0x{:x}", (u32)r);
    return;
  }
}

void VulkanRenderer::CreateComputePipeline() {
  VulkanShader *vkShader = reinterpret_cast<VulkanShader *>(computeShaderProgram.get());
  VkShaderModule cs = vkShader->computeShader;
  if (!cs)
    return;

  // Descriptor set + push constants
  VkPushConstantRange pcr{};
  pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pcr.offset = 0;
  pcr.size = sizeof(FbConvertPC);

  VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pl.setLayoutCount = 1;
  pl.pSetLayouts = &computeSetLayout;
  pl.pushConstantRangeCount = 1;
  pl.pPushConstantRanges = &pcr;

  VkResult r = dispatch.createPipelineLayout(&pl, nullptr, &computePL);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreatePipelineLayout(compute) failed: 0x{:x}", (u32)r);
    dispatch.destroyShaderModule(cs, nullptr);
    return;
  }

  VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = cs;
  stage.pName = "main";

  VkComputePipelineCreateInfo ci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
  ci.stage = stage;
  ci.layout = computePL;

  r = dispatch.createComputePipelines(VK_NULL_HANDLE, 1, &ci, nullptr, &computePipe);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateComputePipelines failed: 0x{:x}", (u32)r);
  }

  dispatch.destroyShaderModule(cs, nullptr);
}

void VulkanRenderer::CreateRenderPipeline() {
  VulkanShader *vkShader = reinterpret_cast<VulkanShader *>(renderShaderPrograms.get());
  VkShaderModule vs = vkShader->vertexShader;
  VkShaderModule fs = vkShader->fragmentShader;
  if (!fs || !vs)
    return;

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs;
  stages[0].pName  = "main";
  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fs;
  stages[1].pName  = "main";

  // No vertex buffers
  VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

  VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dyn.dynamicStateCount = 2;
  dyn.pDynamicStates = dynStates;

  VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  vp.viewportCount = 1;
  vp.scissorCount  = 1;

  VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState cbAtt{};
  cbAtt.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
  cb.attachmentCount = 1;
  cb.pAttachments = &cbAtt;

  // Layout
  VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pl.setLayoutCount = 1;
  pl.pSetLayouts = &renderSetLayout;

  VkResult r = dispatch.createPipelineLayout(&pl, nullptr, &renderPL);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreatePipelineLayout(render) failed: 0x{:x}", (u32)r);
    dispatch.destroyShaderModule(vs, nullptr);
    dispatch.destroyShaderModule(fs, nullptr);
    return;
  }

  // Dynamic rendering info
  VkPipelineRenderingCreateInfoKHR rendering{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .pNext = nullptr,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &chosenFormat.format,
    .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  gp.stageCount = 2;
  gp.pStages = stages;
  gp.pVertexInputState = &vi;
  gp.pInputAssemblyState = &ia;
  gp.pViewportState = &vp;
  gp.pRasterizationState = &rs;
  gp.pMultisampleState = &ms;
  gp.pColorBlendState = &cb;
  gp.pDynamicState = &dyn;
  gp.layout = renderPL;
#if defined(VK_PIPELINE_CREATE_RENDERING_BIT)
  gp.flags |= VK_PIPELINE_CREATE_RENDERING_BIT;
#elif defined(VK_PIPELINE_CREATE_RENDERING_BIT_KHR)
  gp.flags |= VK_PIPELINE_CREATE_RENDERING_BIT_KHR;
#endif
  gp.pNext = &rendering;
  gp.renderPass = VK_NULL_HANDLE;
  gp.subpass = 0;

  r = dispatch.createGraphicsPipelines(VK_NULL_HANDLE, 1, &gp, nullptr, &renderPipe);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateGraphicsPipelines(render) failed: 0x{:x}", (u32)r);
  }

  dispatch.destroyShaderModule(vs, nullptr);
  dispatch.destroyShaderModule(fs, nullptr);
}

void VulkanRenderer::DestroyPipelines() {
  if (computePipe) {
    dispatch.destroyPipeline(computePipe, nullptr);
    computePipe = VK_NULL_HANDLE;
  }

  if (computePL) {
    dispatch.destroyPipelineLayout(computePL, nullptr);
    computePL = VK_NULL_HANDLE;
  }

  if (renderPipe) {
    dispatch.destroyPipeline(renderPipe, nullptr);
    renderPipe = VK_NULL_HANDLE;
  }

  if (renderPL) {
    dispatch.destroyPipelineLayout(renderPL, nullptr);
    renderPL = VK_NULL_HANDLE;
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

void VulkanRenderer::BackendBindPixelBuffer(Buffer *buffer) {
  VulkanBuffer *vkBuffer = reinterpret_cast<VulkanBuffer *>(buffer);
  UpdateDescriptors((VkBuffer)vkBuffer->GetBackendHandle(), (VkDeviceSize)vkBuffer->GetSize());
}

static VkFormat VkFormatForFloatComps(u32 comps) {
  switch (comps) {
    case 1: return VK_FORMAT_R32_SFLOAT;
    case 2: return VK_FORMAT_R32G32_SFLOAT;
    case 3: return VK_FORMAT_R32G32B32_SFLOAT;
    default: return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
}

static VkPrimitiveTopology TopologyFromXenos(u32 primType /*Xe::XGPU::ePrimitiveType*/) {
  // Most games will start with TRIANGLE_LIST.
  (void)primType;
  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

static u64 MakeGuestPipeKey(u32 vs, u32 ps, u32 primType) {
  // pack primType in low 8 bits (hack)
  return ((u64)vs << 32) ^ (u64)ps ^ ((u64)primType & 0xFFull);
}

void VulkanRenderer::EnsureGuestVB(VkDeviceSize minBytes) {
  if (guestVB_ && guestVBCap_ >= minBytes)
    return;

  // destroy old
  if (guestVB_) {
    if (guestVBMap_) {
      vmaUnmapMemory(allocator, guestVBAlloc_);
      guestVBMap_ = nullptr;
    }
    vmaDestroyBuffer(allocator, guestVB_, guestVBAlloc_);
    guestVB_ = VK_NULL_HANDLE;
    guestVBAlloc_ = VK_NULL_HANDLE;
    guestVBCap_ = 0;
  }

  VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bi.size = minBytes;
  bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo ai{};
  ai.usage = VMA_MEMORY_USAGE_AUTO;
  ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
             VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo ainfo{};
  VkResult r = vmaCreateBuffer(allocator, &bi, &ai, &guestVB_, &guestVBAlloc_, &ainfo);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vmaCreateBuffer(guestVB) failed: 0x{:x}", (u32)r);
    return;
  }

  guestVBCap_ = minBytes;
  guestVBMap_ = ainfo.pMappedData;
  if (!guestVBMap_) {
    // if VMA didn't map it, map now
    vmaMapMemory(allocator, guestVBAlloc_, &guestVBMap_);
  }
}

void VulkanRenderer::BuildVertexBuffer_HardcodedTriangle() {
  // layout: location0 = vec4 position
  struct Vtx { float x,y,z,w; };
  const Vtx tri[3] = {
    { -0.8f, -0.8f, 0.0f, 1.0f },
    {  0.8f, -0.8f, 0.0f, 1.0f },
    {  0.0f,  0.8f, 0.0f, 1.0f },
  };

  EnsureGuestVB(sizeof(tri));
  if (!guestVBMap_) return;

  memcpy(guestVBMap_, tri, sizeof(tri));
  // host coherent due to VMA flags; otherwise you'd flush here
}

VkShaderModule VulkanRenderer::GetOrCreateShaderModule(u32 hash, bool isVertex) {
  auto& map = isVertex ? vsModules_ : psModules_;
  if (!hash) return VK_NULL_HANDLE;

  if (auto it = map.find(hash); it != map.end())
    return it->second;

  std::vector<u32> spv;

  {
    std::lock_guard<Base::FutexMutex> lock(programLinkMutex);

    if (isVertex) {
      auto it = pendingVertexShaders.find(hash);
      if (it == pendingVertexShaders.end()) {
        LOG_WARNING(Render, "VS hash {:08X} not found in pendingVertexShaders", hash);
        return VK_NULL_HANDLE;
      }
      spv = it->second.second;
    } else {
      auto it = pendingPixelShaders.find(hash);
      if (it == pendingPixelShaders.end()) {
        LOG_WARNING(Render, "PS hash {:08X} not found in pendingPixelShaders", hash);
        return VK_NULL_HANDLE;
      }
      spv = it->second.second;
    }
  }

  if (spv.empty()) {
    LOG_WARNING(Render, "Shader {:08X} SPIR-V is empty", hash);
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  ci.codeSize = spv.size() * sizeof(u32);
  ci.pCode = spv.data();

  VkShaderModule mod = VK_NULL_HANDLE;
  VkResult r = dispatch.createShaderModule(&ci, nullptr, &mod);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateShaderModule failed: 0x{:x}", (u32)r);
    return VK_NULL_HANDLE;
  }

  map.emplace(hash, mod);
  return mod;
}

void VulkanRenderer::EnsureGuestPipelineLayout() {
  if (guestPL_) return;

  // HACK: no descriptor sets, no push constants, nothing.
  // this is just to get *something* executing.
  VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pl.setLayoutCount = 0;
  pl.pushConstantRangeCount = 0;

  VkResult r = dispatch.createPipelineLayout(&pl, nullptr, &guestPL_);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreatePipelineLayout(guest) failed: 0x{:x}", (u32)r);
    guestPL_ = VK_NULL_HANDLE;
  }
}

VkPipeline VulkanRenderer::CreateGuestGraphicsPipeline(VkShaderModule vs, VkShaderModule ps, u32 primType) {
  EnsureGuestPipelineLayout();
  if (!guestPL_) return VK_NULL_HANDLE;

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps;
  stages[1].pName = "main";

  // Vertex input state can be empty because we use VK_EXT_vertex_input_dynamic_state.
  VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

  VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  ia.topology = TopologyFromXenos(primType);
  ia.primitiveRestartEnable = VK_FALSE;

  // enable dynamic viewport/scissor + dynamic vertex input
  VkDynamicState dynStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_VERTEX_INPUT_EXT
  };
  VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dyn.dynamicStateCount = (u32)std::size(dynStates);
  dyn.pDynamicStates = dynStates;

  VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  vp.viewportCount = 1;
  vp.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState cbAtt{};
  cbAtt.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
  cb.attachmentCount = 1;
  cb.pAttachments = &cbAtt;

  // dynamic rendering format
  VkPipelineRenderingCreateInfoKHR rendering{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .pNext = nullptr,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &chosenFormat.format,
    .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  gp.pNext = &rendering;
  gp.stageCount = 2;
  gp.pStages = stages;
  gp.pVertexInputState = &vi;
  gp.pInputAssemblyState = &ia;
  gp.pViewportState = &vp;
  gp.pRasterizationState = &rs;
  gp.pMultisampleState = &ms;
  gp.pColorBlendState = &cb;
  gp.pDynamicState = &dyn;
  gp.layout = guestPL_;
  gp.renderPass = VK_NULL_HANDLE;
  gp.subpass = 0;

#if defined(VK_PIPELINE_CREATE_RENDERING_BIT)
  gp.flags |= VK_PIPELINE_CREATE_RENDERING_BIT;
#elif defined(VK_PIPELINE_CREATE_RENDERING_BIT_KHR)
  gp.flags |= VK_PIPELINE_CREATE_RENDERING_BIT_KHR;
#endif

  VkPipeline pipe = VK_NULL_HANDLE;
  VkResult r = dispatch.createGraphicsPipelines(VK_NULL_HANDLE, 1, &gp, nullptr, &pipe);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "vkCreateGraphicsPipelines(guest) failed: 0x{:x}", (u32)r);
    return VK_NULL_HANDLE;
  }
  return pipe;
}

VkPipeline VulkanRenderer::EnsureGuestPipeline(u32 vsHash, u32 psHash, u32 primType) {
  VkShaderModule vs = GetOrCreateShaderModule(vsHash, true);
  VkShaderModule ps = GetOrCreateShaderModule(psHash, false);
  if (!vs || !ps) return VK_NULL_HANDLE;

  u64 key = MakeGuestPipeKey(vsHash, psHash, primType);
  if (auto it = guestPipelines_.find(key); it != guestPipelines_.end())
    return it->second;

  VkPipeline p = CreateGuestGraphicsPipeline(vs, ps, primType);
  if (!p) return VK_NULL_HANDLE;

  guestPipelines_.emplace(key, p);
  return p;
}

void VulkanRenderer::EmitGuestDraws(VkCommandBuffer cmd) {
  if (pendingDraws_.empty())
    return;

  // HACK: Always upload a triangle to prove shader + pipeline path.
  BuildVertexBuffer_HardcodedTriangle();

  // HACK: Always assume location 0 position vec4 float.
  VkVertexInputBindingDescription2EXT bind{ VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT };
  bind.binding = 0;
  bind.stride = sizeof(float) * 4;
  bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  bind.divisor = 1;

  VkVertexInputAttributeDescription2EXT attr{ VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT };
  attr.location = 0;
  attr.binding = 0;
  attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attr.offset = 0;

  // Apply once for all draws
  dispatch.cmdSetVertexInputEXT(cmd, 1, &bind, 1, &attr);

  VkDeviceSize vbOff = 0;
  dispatch.cmdBindVertexBuffers(cmd, 0, 1, &guestVB_, &vbOff);

  for (auto& d : pendingDraws_) {
    const u32 vsHash = d.shader.vertexShaderHash;
    const u32 psHash = d.shader.pixelShaderHash;

    const u32 primType = (u32)d.params.vgtDrawInitiator.primitiveType;
    VkPipeline pipe = EnsureGuestPipeline(vsHash, psHash, primType);
    if (!pipe) continue;

    dispatch.cmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

    if (!d.indexed) {
      // Auto-index draw: use numIndices as vertex count (hack).
      u32 vcount = d.params.vgtDrawInitiator.numIndices;
      if (!vcount) vcount = 3;
      dispatch.cmdDraw(cmd, vcount, 1, 0, 0);
    } else {
      // HACK: treat indexed as non-indexed for now.
      // Implement real index upload + cmdBindIndexBuffer later.
      u32 vcount = d.params.vgtDrawInitiator.numIndices;
      if (!vcount) vcount = 3;
      dispatch.cmdDraw(cmd, vcount, 1, 0, 0);
    }
  }

  pendingDraws_.clear();
  pendingVFetches_.clear();
}

void VulkanRenderer::VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized,
                                 const u32 fetchOffset, const u32 fetchStride) {
  PendingVFetch f{};
  f.location = location;
  f.components = components;
  f.isFloat = isFloat;
  f.isNormalized = isNormalized;
  f.fetchOffset = fetchOffset;
  f.fetchStride = fetchStride;
  pendingVFetches_.push_back(f);
}

void VulkanRenderer::Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) {
  PendingDraw d{};
  d.shader = shader;
  d.params = params;
  d.indexed = false;
  pendingDraws_.push_back(std::move(d));
}

void VulkanRenderer::DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params,
                                 Xe::XGPU::XeIndexBufferInfo indexBufferInfo) {
  PendingDraw d{};
  d.shader = shader;
  d.params = params;
  d.indexed = true;
  d.indexInfo = indexBufferInfo;
  pendingDraws_.push_back(std::move(d));
}

void VulkanRenderer::OnCompute() {

}

void VulkanRenderer::OnBind() {

}

static inline void CmdBeginRendering(vkb::DispatchTable &d, VkCommandBuffer cmd, const VkRenderingInfo *ri) {
  if (d.fp_vkCmdBeginRendering)
    d.cmdBeginRendering(cmd, ri);
  else
    d.cmdBeginRenderingKHR(cmd, reinterpret_cast<const VkRenderingInfoKHR*>(ri));
}

static inline void CmdEndRendering(vkb::DispatchTable& d, VkCommandBuffer cmd) {
  if (d.fp_vkCmdEndRendering)
    d.cmdEndRendering(cmd);
  else
    d.cmdEndRenderingKHR(cmd);
}

void VulkanRenderer::OnSwap(SDL_Window *window) {
  (void)window;
  if (!swapchain || swapchainImageViews.empty() || swapchainImages.empty())
    return;

  if (!computePipe || !renderPipe || !fbImage || !fbView || !fbSampler) {
    LOG_WARNING(Render, "OnSwap early-out: computePipe={} renderPipe={} fbImage={} fbView={} fbSampler={}",
      (void*)computePipe, (void*)renderPipe, (void*)fbImage, (void*)fbView, (void*)fbSampler);
    return;
  }

  dispatch.waitForFences(1, &inFlight[currentFrame], VK_TRUE, UINT64_MAX);

  u32 imageIndex = 0;
  VkResult r = dispatch.acquireNextImageKHR(
    swapchain, UINT64_MAX,
    imageAvailable[currentFrame],
    VK_NULL_HANDLE,
    &imageIndex
  );

  if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapchain();
    return;
  } else if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR(Render, "acquireNextImageKHR failed: 0x{:x}", (u32)r);
    return;
  }

  if (imageIndex >= swapchainImageCount)
    return;

  if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    dispatch.waitForFences(1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  imagesInFlight[imageIndex] = inFlight[currentFrame];

  dispatch.resetFences(1, &inFlight[currentFrame]);
  dispatch.resetCommandBuffer(commandBuffers[currentFrame], 0);

  VkCommandBuffer cmd = commandBuffers[currentFrame];
  VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  dispatch.beginCommandBuffer(cmd, &bi);

  VkImage image = swapchainImages[imageIndex];
  VkImageView view = swapchainImageViews[imageIndex];

  VkImageLayout old = swapchainImageLayouts[imageIndex];
  CmdImageBarrier(cmd, dispatch, image, old, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  swapchainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkClearValue clear{};
  clear.color = {{0.f, 0.f, 0.f, 1.f}};

  VkRenderingAttachmentInfo colorAtt{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
  colorAtt.imageView = view;
  colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAtt.clearValue = clear;

  VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
  ri.renderArea = { {0,0}, {(u32)width, (u32)height} };
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &colorAtt;

  // Compute pass
  CmdImageBarrier2(dispatch, cmd, fbImage,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, fbLayout,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
  fbLayout = VK_IMAGE_LAYOUT_GENERAL;

  dispatch.cmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipe);
  dispatch.cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePL, 0, 1, &computeSet, 0, nullptr);

  FbConvertPC pc{};
  pc.internalWidth  = (s32)internalWidth;
  pc.internalHeight = (s32)internalHeight;
  pc.resWidth = (s32)width;
  pc.resHeight = (s32)height;

  dispatch.cmdPushConstants(cmd, computePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FbConvertPC), &pc);

  u32 gx = ((u32)pc.resWidth  + 15u) / 16u;
  u32 gy = ((u32)pc.resHeight + 15u) / 16u;
  dispatch.cmdDispatch(cmd, gx, gy, 1);

  // Make compute writes visible to fragment sampling
  CmdImageBarrier2(dispatch, cmd, fbImage,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, fbLayout,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  fbLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // Render pass
  CmdBeginRendering(dispatch, cmd, &ri);

  VkRect2D sc{
    { 0, 0 },
    { (u32)width, (u32)height }
  };

  VkViewport vp{
    0.f, 0.f,
    (float)width, (float)height,
    0.f, 1.f
  };

  dispatch.cmdSetScissor(cmd, 0, 1, &sc);
  dispatch.cmdSetViewport(cmd, 0, 1, &vp);

  dispatch.cmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipe);
  dispatch.cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPL, 0, 1, &renderSet, 0, nullptr);
  dispatch.cmdDraw(cmd, 3, 1, 0, 0);

  EmitGuestDraws(cmd);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  CmdEndRendering(dispatch, cmd);

  CmdImageBarrier(cmd, dispatch, image,
                  swapchainImageLayouts[imageIndex],
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  swapchainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  dispatch.endCommandBuffer(cmd);

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSemaphore signal = renderFinishedPerImage[imageIndex];

  VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &imageAvailable[currentFrame];
  si.pWaitDstStageMask = &waitStage;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &signal;

  r = dispatch.queueSubmit(graphicsQueue, 1, &si, inFlight[currentFrame]);
  if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "queueSubmit failed: 0x{:x}", (u32)r);
    return;
  }

  VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &signal;
  pi.swapchainCount = 1;
  pi.pSwapchains = &swapchain;
  pi.pImageIndices = &imageIndex;

  r = dispatch.queuePresentKHR(graphicsQueue, &pi);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
  } else if (r != VK_SUCCESS) {
    LOG_ERROR(Render, "queuePresentKHR failed: 0x{:x}", (u32)r);
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::UpdateDescriptors(VkBuffer pixelBuffer, VkDeviceSize pixelBufferSize) {
  if (pixelBuffer == VK_NULL_HANDLE || pixelBufferSize == 0) {
    LOG_WARNING(Render, "UpdateDescriptors: pixel buffer not ready (buf={}, size={})",
             (void*)pixelBuffer, (u64)pixelBufferSize);
    return;
  }
  // Compute: storage image
  VkDescriptorImageInfo outImg{};
  outImg.imageView = fbView;
  outImg.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkWriteDescriptorSet w0{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  w0.dstSet = computeSet;
  w0.dstBinding = 0;
  w0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  w0.descriptorCount = 1;
  w0.pImageInfo = &outImg;

  // Compute: SSBO
  VkDescriptorBufferInfo ssbo{};
  ssbo.buffer = pixelBuffer;
  ssbo.offset = 0;
  ssbo.range  = pixelBufferSize;

  VkWriteDescriptorSet w1{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  w1.dstSet = computeSet;
  w1.dstBinding = 1;
  w1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  w1.descriptorCount = 1;
  w1.pBufferInfo = &ssbo;

  VkDescriptorImageInfo samp{};
  samp.sampler = fbSampler;
  samp.imageView = fbView;
  samp.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet w2{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  w2.dstSet = renderSet;
  w2.dstBinding = 0;
  w2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  w2.descriptorCount = 1;
  w2.pImageInfo = &samp;

  VkWriteDescriptorSet writes[] = { w0, w1, w2 };
  dispatch.updateDescriptorSets((u32)std::size(writes), writes, 0, nullptr);
}

void VulkanRenderer::RecreateSwapchain() {
  dispatch.deviceWaitIdle();

  // Destroy per-image render-finished semaphores
  for (auto sem : renderFinishedPerImage) {
    if (sem)
      dispatch.destroySemaphore(sem, nullptr);
  }
  renderFinishedPerImage.clear();

  // Destroy old image views
  if (!swapchainImageViews.empty())
    vkbSwapchain.destroy_image_views(swapchainImageViews);
  swapchainImageViews.clear();
  swapchainImages.clear();

  // Rebuild swapchain
  auto swapchainRet = vkb::SwapchainBuilder{ vkbPhys, vkbDevice, surface }
    .use_default_format_selection()
    .set_desired_present_mode(Config::rendering.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR)
    .set_desired_extent(width, height)
    .build();

  if (!swapchainRet) {
    LOG_ERROR(Render, "swapchain rebuild failed: {}", swapchainRet.error().message());
    return;
  }

  vkbSwapchain = swapchainRet.value();
  swapchain = vkbSwapchain.swapchain;

  auto imageViewsRet = vkbSwapchain.get_image_views();
  if (!imageViewsRet) {
    LOG_ERROR(Render, "get_image_views failed: {}", imageViewsRet.error().message());
    return;
  }
  swapchainImageViews = imageViewsRet.value();

  auto imagesRet = vkbSwapchain.get_images();
  if (!imagesRet) {
    LOG_ERROR(Render, "get_images failed: {}", imagesRet.error().message());
    return;
  }
  swapchainImages = imagesRet.value();

  chosenFormat.format = vkbSwapchain.image_format;
  width = vkbSwapchain.extent.width;
  height = vkbSwapchain.extent.height;

  swapchainImageCount = (u32)swapchainImageViews.size();
  imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);

  swapchainImageLayouts.assign(swapchainImageCount, VK_IMAGE_LAYOUT_UNDEFINED);

  // Recreate per-image render-finished semaphores
  renderFinishedPerImage.resize(swapchainImageCount, VK_NULL_HANDLE);
  VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  for (u32 i = 0; i < swapchainImageCount; ++i) {
    VkResult r = dispatch.createSemaphore(&si, nullptr, &renderFinishedPerImage[i]);
    if (r != VK_SUCCESS) {
      LOG_ERROR(Render, "createSemaphore(renderFinishedPerImage[{}]) failed: 0x{:x}", i, (u32)r);
      return;
    }
  }

  CreateFbImage(width, height);

  // Recreate pipelines that depend on swapchain format
  CreateComputePipeline();
  CreateRenderPipeline();
}

s32 VulkanRenderer::GetBackbufferFlags() {
  return 0;
}

s32 VulkanRenderer::GetXenosFlags() {
  return 0;
}

void *VulkanRenderer::GetBackendContext() {
  return reinterpret_cast<void *>(this);
}

u32 VulkanRenderer::GetBackendID() {
  return "Vulkan"_jLower;
}

} // namespace Render
#endif
