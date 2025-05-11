// Copyright 2025 Xenon Emulator Project

#include "VulkanTexture.h"

#ifndef NO_GFX

void Render::VulkanTexture::CreateTextureHandle(u32 width, u32 height, s32 flags) {
  DestroyTexture();
  SetWidth(width);
  SetHeight(height);

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = GetDepth();
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  // TODO: CreateTextureHandle needs to communicate desired format
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo createInfo = {};
  createInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkResult res = vmaCreateImage(renderer->allocator, &imageInfo, &createInfo, &image, &allocation, &allocationInfo);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "vmaCreateImage failed with error code {:x}", static_cast<u32>(res));
    return;
  }

  SetTexture(&image);
}

void Render::VulkanTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) {
  DestroyTexture();
  SetWidth(width);
  SetHeight(height);

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = GetDepth();
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = GetVulkanFormat(format);
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo createInfo = {};
  createInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkResult res = vmaCreateImage(renderer->allocator, &imageInfo, &createInfo, &image, &allocation, &allocationInfo);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "vmaCreateImage failed with error code {:x}", static_cast<u32>(res));
    return;
  }

  SetTexture(&image);

  // TODO: Copy data
}

VkFormat Render::VulkanTexture::GetVulkanFormat(eDataFormat format) {
  switch (format) {
    case eDataFormat::RGB:  return VK_FORMAT_R8G8B8_UNORM;
    case eDataFormat::RGBA: return VK_FORMAT_R8G8B8A8_UNORM;
    default: UNREACHABLE_MSG("Missing Format: {}", std::to_string(format));
  }
}


#endif