// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/Abstractions/Buffer.h"
#include "Render/Backends/Vulkan/VulkanRenderer.h"

#include "Base/Types.h"
#include "Base/Logging/Log.h"

#ifndef NO_GFX

#include <vk_mem_alloc.h>
#include <volk.h>

namespace Render {

class VulkanRenderer;

class VulkanBuffer : public Buffer {
public:
  VulkanBuffer(VulkanRenderer *renderer) : renderer(renderer) {}
  ~VulkanBuffer() {
    DestroyBuffer();
  }

  void CreateBuffer(u64 size, const void *data, eBufferUsage usage, eBufferType type) override;
  void UpdateBuffer(u64 offset, u64 size, const void *data) override;
  void Bind(u32 binding) override;
  void Unbind() override;
  void DestroyBuffer() override;
private:
  VkBufferUsageFlags ConvertBufferType(eBufferType type);
  VkBufferUsageFlags ConvertUsage(eBufferUsage usage);

  VulkanRenderer *renderer = nullptr;
  VkBuffer buffer = nullptr;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocationInfo = {};
};

} // namespace Render

#endif
