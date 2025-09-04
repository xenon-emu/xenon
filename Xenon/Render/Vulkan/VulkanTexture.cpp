// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanTexture.h"

#ifndef NO_GFX

void Render::VulkanTexture::CreateTextureHandle(u32 width, u32 height, s32 flags) {
  if (!renderer || renderer && !renderer->allocator) {
    LOG_ERROR(System, "VulkanTexture: renderer or allocator null!");
    return;
  }
  DestroyTexture();
  SetWidth(width);
  SetHeight(height);

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = { width, height, GetDepth() };
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkResult res = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &image, &allocation, &allocationInfo);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "vmaCreateImage failed with error code {:x}", static_cast<u32>(res));
    return;
  }

  CreateImageView(imageInfo.format);
}

void Render::VulkanTexture::CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8 *data, u32 dataSize, s32 flags) {
  DestroyTexture();
  SetWidth(width);
  SetHeight(height);

  VkFormat vkFormat = GetVulkanFormat(format);

  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent = { width, height, GetDepth() };
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = vkFormat;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkResult res = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &image, &allocation, &allocationInfo);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "vmaCreateImage failed with error code {:x}", static_cast<u32>(res));
    return;
  }

  CreateImageView(vkFormat);

  // Upload data with a staging buffer
  UploadData(data, dataSize, vkFormat, width, height);
}

void Render::VulkanTexture::ResizeTexture(u32 width, u32 height) {
  // Just recreate the texture
  CreateTextureHandle(width, height, 0);
}

void Render::VulkanTexture::GenerateMipmaps() {
  // TODO: Implement mipmap generation with vkCmdBlitImage
}

void Render::VulkanTexture::UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) {
  // Simple re-upload path
  VkFormat vkFormat = GetVulkanFormat(format);
  UploadSubData(data, w * h * GetBytesPerPixel(format), vkFormat, x, y, w, h);
}

void Render::VulkanTexture::Bind() {
  // Binding handled in descriptor sets (not here)
}

void Render::VulkanTexture::Unbind() {
  // No-op
}

void Render::VulkanTexture::DestroyTexture() {
  if (imageView != VK_NULL_HANDLE) {
    vkDestroyImageView(renderer->device, imageView, nullptr);
    imageView = VK_NULL_HANDLE;
  }
  if (image != VK_NULL_HANDLE) {
    vmaDestroyImage(renderer->allocator, image, allocation);
    image = VK_NULL_HANDLE;
    allocation = VK_NULL_HANDLE;
  }
}

VkFormat Render::VulkanTexture::GetVulkanFormat(eDataFormat format) {
  switch (format) {
    case eDataFormat::RGB:  return VK_FORMAT_R8G8B8_UNORM;
    case eDataFormat::RGBA: return VK_FORMAT_R8G8B8A8_UNORM;
    default: UNREACHABLE_MSG("Missing Format: {}", std::to_string(format));
  }
}

u32 Render::VulkanTexture::GetBytesPerPixel(eDataFormat format) {
  switch (format) {
  case eDataFormat::RGB:  return 3; // VK_FORMAT_R8G8B8_UNORM
  case eDataFormat::RGBA: return 4; // VK_FORMAT_R8G8B8A8_UNORM
  default: UNREACHABLE_MSG("Unsupported eDataFormat: {}", std::to_string(format));
  }
}

void Render::VulkanTexture::CreateImageView(VkFormat format) {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkResult res = vkCreateImageView(renderer->device, &viewInfo, nullptr, &imageView);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "vkCreateImageView failed with error code {:x}", static_cast<u32>(res));
  }
}

void Render::VulkanTexture::UploadData(u8 *data, u32 dataSize, VkFormat format, u32 width, u32 height) {
  // TODO: implement staging buffer upload
}

void Render::VulkanTexture::UploadSubData(u8 *data, u32 dataSize, VkFormat format, u32 x, u32 y, u32 w, u32 h) {
  // TODO: implement sub-region upload with staging buffer + vkCmdCopyBufferToImage
}

#endif
