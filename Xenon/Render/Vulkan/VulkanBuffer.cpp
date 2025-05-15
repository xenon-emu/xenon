// Copyright 2025 Xenon Emulator Project

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "VulkanBuffer.h"

#ifndef NO_GFX

void Render::VulkanBuffer::CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) {
  DestroyBuffer();

  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = ConvertBufferType(type) | ConvertUsage(usage);

  VmaAllocationCreateInfo createInfo = {};
  createInfo.usage = VMA_MEMORY_USAGE_AUTO;
  createInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

  vmaCreateBuffer(renderer->allocator, &bufferInfo, &createInfo, &buffer, &allocation, &allocationInfo);
  memcpy(allocationInfo.pMappedData, data, size);
}

void Render::VulkanBuffer::UpdateBuffer(u32 offset, u32 size, const void *data) {
  memcpy(allocationInfo.pMappedData + offset, data, size);
}

void Render::VulkanBuffer::Bind() {

}

void Render::VulkanBuffer::Unbind() {

}

void Render::VulkanBuffer::DestroyBuffer() {
  if (buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(renderer->allocator, buffer, allocation);
  }
}

VkBufferUsageFlags Render::VulkanBuffer::ConvertBufferType(eBufferType type) {
  VkBufferUsageFlags flags = 0;

  switch (type) {
  case eBufferType::Vertex:
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    break;
  case eBufferType::Index:
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    break;
  case eBufferType::Uniform:
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    break;
  case eBufferType::Storage:
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    break;
  }

  return flags;
}

VkBufferUsageFlags Render::VulkanBuffer::ConvertUsage(eBufferUsage usage) {
  VkBufferUsageFlags flags = 0;

  switch (usage) {
  case eBufferUsage::StaticDraw:
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    break;
  case eBufferUsage::DynamicDraw:
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    break;
  case eBufferUsage::StreamDraw:
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    break;
  }

  return flags;
}

#endif
