// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Buffer.h"

#include "Render/VulkanRenderer.h"
#include "Base/Types.h"

#ifndef NO_GFX

#include <vk_mem_alloc.h>
#include <volk.h>

namespace Render {

class VulkanBuffer : public Buffer {
public:
  VulkanBuffer(VulkanRenderer* renderer) : renderer(renderer) {}
  ~VulkanBuffer() {
    DestroyBuffer();
  }

  void CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) override;
  void UpdateBuffer(u32 offset, u32 size, const void *data) override;
  void Bind() override;
  void Unbind() override;
  void DestroyBuffer() override;
private:
  VkBufferUsageFlags ConvertBufferType(eBufferType type);
  VkBufferUsageFlags ConvertUsage(eBufferUsage usage);
  VulkanRenderer* renderer;
  VkBuffer buffer = nullptr;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocationInfo = {};
};

} // namespace Render
#endif