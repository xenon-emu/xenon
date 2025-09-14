// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Render/Abstractions/Texture.h"
#include "Render/Backends/Vulkan/VulkanRenderer.h"

#include "Base/Logging/Log.h"
#include "Base/Assert.h"
#include "Base/Types.h"

#ifndef NO_GFX

namespace Render {

class VulkanTexture : public Texture {
public:
  VulkanTexture(VulkanRenderer *renderer) : renderer(renderer) {}
  void CreateTextureHandle(u32 width, u32 height, s32 flags) override;
  void CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) override;
  void ResizeTexture(u32 width, u32 height) override;
  void GenerateMipmaps() override;
  void UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) override;
  void Bind() override;
  void Unbind() override;
  void DestroyTexture() override;

  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer cmd);
private:
  void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
    VkFormat format, VkImageLayout oldLayout,
    VkImageLayout newLayout, u32 mipLevels, u32 layerCount = 1);
  void CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, u32 width, u32 height, u32 x = 0, u32 y = 0, u32 w = 0, u32 h = 0);
  bool CreateBufferWithData(void *srcData, VkDeviceSize size, VkBufferUsageFlags usage,
    VkBuffer &outBuffer, VmaAllocation &outAlloc);
  VkFormat GetVulkanFormat(eDataFormat format);
  u32 GetBytesPerPixel(eDataFormat format);
  void CreateImageView(VkFormat format);
  void UploadData(u8 *data, u32 dataSize, VkFormat format, u32 width, u32 height);
  void UploadSubData(u8 *data, u32 dataSize, VkFormat format, u32 x, u32 y, u32 w, u32 h);

  VulkanRenderer *renderer = nullptr;
  VkImage image = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkFormat imageFormat = VK_FORMAT_UNDEFINED;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocationInfo = {};
};

} // namespace Render

#endif
