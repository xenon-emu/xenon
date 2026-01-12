/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "VulkanTexture.h"

#ifndef NO_GFX

VkCommandBuffer Render::VulkanTexture::BeginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = renderer->commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd{};
  renderer->dispatch.allocateCommandBuffers(&allocInfo, &cmd);

  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  renderer->dispatch.beginCommandBuffer(cmd, &beginInfo);
  return cmd;
}

void Render::VulkanTexture::EndSingleTimeCommands(VkCommandBuffer cmd) {
  renderer->dispatch.endCommandBuffer(cmd);
  VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;

  renderer->dispatch.queueSubmit(renderer->graphicsQueue, 1, &submit, VK_NULL_HANDLE);
  renderer->dispatch.queueWaitIdle(renderer->graphicsQueue);

  renderer->dispatch.freeCommandBuffers(renderer->commandPool, 1, &cmd);
}

void Render::VulkanTexture::TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
  VkFormat format, VkImageLayout oldLayout,
  VkImageLayout newLayout, u32 mipLevels, u32 layerCount)
{
  VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layerCount;

  VkPipelineStageFlags srcStage = 0;
  VkPipelineStageFlags dstStage = 0;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }

  renderer->dispatch.cmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Render::VulkanTexture::CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, u32 width, u32 height, u32 x, u32 y, u32 w, u32 h) {
  if (w == 0)
    w = width;
  if (h == 0)
    h = height;

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0; // tightly packed
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { (s32)x, (s32)y, 0 };
  region.imageExtent = { w, h, 1 };

  renderer->dispatch.cmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

bool Render::VulkanTexture::CreateBufferWithData(void *srcData, VkDeviceSize size, VkBufferUsageFlags usage,
  VkBuffer &outBuffer, VmaAllocation &outAlloc)
{
  VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufInfo.size = size;
  bufInfo.usage = usage;
  bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VkResult res = vmaCreateBuffer(renderer->allocator, &bufInfo, &allocInfo, &outBuffer, &outAlloc, nullptr);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "CreateBufferWithData: vmaCreateBuffer failed {:x}", static_cast<u32>(res));
    return false;
  }

  void *mapped = nullptr;
  vmaMapMemory(renderer->allocator, outAlloc, &mapped);
  memcpy(mapped, srcData, (size_t)size);
  vmaUnmapMemory(renderer->allocator, outAlloc);
  return true;
}

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
  SetDepth(1); // Hard-code for now
  imageInfo.extent = { width, height, GetDepth() };
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
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
    renderer->dispatch.destroyImageView(imageView, nullptr);
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

  VkResult res = renderer->dispatch.createImageView(&viewInfo, nullptr, &imageView);
  if (res != VK_SUCCESS) {
    LOG_ERROR(System, "vkCreateImageView failed with error code {:x}", static_cast<u32>(res));
  }
}

void Render::VulkanTexture::UploadData(u8 *data, u32 dataSize, VkFormat format, u32 width, u32 height) {
  if (!data || dataSize == 0)
    return;

  // Create staging buffer and copy
  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VmaAllocation stagingAlloc = VK_NULL_HANDLE;
  if (!CreateBufferWithData(data, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingAlloc)) {
    LOG_ERROR(System, "UploadData: failed to create staging buffer");
    return;
  }

  VkCommandBuffer cmd = BeginSingleTimeCommands();
  TransitionImageLayout(cmd, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
  CopyBufferToImage(cmd, stagingBuffer, image, width, height);
  TransitionImageLayout(cmd, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
  EndSingleTimeCommands(cmd);

  vmaDestroyBuffer(renderer->allocator, stagingBuffer, stagingAlloc);
}

void Render::VulkanTexture::UploadSubData(u8 *data, u32 dataSize, VkFormat format, u32 x, u32 y, u32 w, u32 h) {
  if (!data || dataSize == 0)
    return;

  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VmaAllocation stagingAlloc = VK_NULL_HANDLE;
  if (!CreateBufferWithData(data, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingAlloc)) {
    LOG_ERROR(System, "UploadSubData: failed to create staging buffer");
    return;
  }

  VkCommandBuffer cmd = BeginSingleTimeCommands();
  TransitionImageLayout(cmd, image, format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
  CopyBufferToImage(cmd, stagingBuffer, image, GetWidth(), GetHeight(), x, y, w, h);
  TransitionImageLayout(cmd, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
  EndSingleTimeCommands(cmd);

  vmaDestroyBuffer(renderer->allocator, stagingBuffer, stagingAlloc);
}

#endif
