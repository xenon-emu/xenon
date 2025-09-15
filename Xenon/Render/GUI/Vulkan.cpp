/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Config.h"

#include "Vulkan.h"

#ifndef NO_GFX
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Render/Abstractions/Renderer.h"
#include "Render/Vulkan/Factory/VulkanResourceFactory.h"
#include "Render/Vulkan/VulkanTexture.h"

void Render::VulkanGUI::InitBackend(void *context) {
  renderer = reinterpret_cast<VulkanRenderer *>(context);
  if (!ImGui_ImplSDL3_InitForOther(mainWindow)) {
    LOG_ERROR(System, "Failed to initialize ImGui's SDL3 implementation");
  }
  // Allow many descriptor sets of various types
  VkDescriptorPoolSize pool_sizes[] = {
    { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
  };

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  poolInfo.pPoolSizes = pool_sizes;

  if (vkCreateDescriptorPool(renderer->device, &poolInfo, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create ImGui descriptor pool!");
  }

  ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_2, [](const char *function_name, void *ctx) { return vkGetInstanceProcAddr(reinterpret_cast<VulkanRenderer *>(ctx)->instance, function_name); }, context);

  ImGui_ImplVulkan_InitInfo initInfo{};
  memset(&initInfo, 0, sizeof(initInfo));
  initInfo.Instance = renderer->instance;
  initInfo.PhysicalDevice = renderer->physicalDevice;
  initInfo.Device = renderer->device;
  initInfo.QueueFamily = renderer->graphicsQueueFamily;
  initInfo.Queue = renderer->graphicsQueue;
  initInfo.DescriptorPool = imguiDescriptorPool;
  initInfo.MinImageCount = 2;
  initInfo.ImageCount = renderer->swapchainImageCount > 0 ? renderer->swapchainImageCount : 2;
  initInfo.ImageCount = 4;
  initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  if (!ImGui_ImplVulkan_Init(&initInfo)) {
    LOG_ERROR(System, "Failed to initialize ImGui's Vulkan backend");
    return;
  }
}

void Render::VulkanGUI::ShutdownBackend() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
}

void Render::VulkanGUI::BeginSwap() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
}

void Render::VulkanGUI::EndSwap() {
  //ImGuiIO &io = ImGui::GetIO();
  //if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
  //  ImGui::UpdatePlatformWindows();
  //  ImGui::RenderPlatformWindowsDefault();
  //}
}
#endif
