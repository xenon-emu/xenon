// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Texture.h"
#include "Render/VulkanRenderer.h"

#ifndef NO_GFX

#include <vk_mem_alloc.h>
#include <volk.h>

#include "Base/Types.h"

namespace Render {

class VulkanTexture : public Texture {
public:
  void CreateTextureHandle(u32 width, u32 height, s32 flags) override;
  void CreateTextureWithData(u32 width, u32 height, eDataFormat format, u8* data, u32 dataSize, s32 flags) override;
  void ResizeTexture(u32 width, u32 height) override;
  void GenerateMipmaps() override;
  void UpdateSubRegion(u32 x, u32 y, u32 w, u32 h, eDataFormat format, u8 *data) override;
  void Bind() override;
  void Unbind() override;
  void DestroyTexture() override;
private:
  VkFormat GetVulkanFormat(eDataFormat format);

  VulkanRenderer* renderer;
  VkImage image = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkFormat imageFormat = VK_FORMAT_UNDEFINED;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VmaAllocationInfo allocationInfo = {};
};

} // namespace Render
#endif