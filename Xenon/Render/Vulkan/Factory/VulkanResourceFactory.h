// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Vulkan/Factory/VulkanShaderFactory.h"
#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanTexture.h"
// #include "Render/GUI/Vulkan.h"

#ifndef NO_GFX
namespace Render {

class VulkanResourceFactory : public ResourceFactory {
public:
  std::unique_ptr<ShaderFactory> CreateShaderFactory() override {
    return std::make_unique<VulkanShaderFactory>();
  }
  std::unique_ptr<Buffer> CreateBuffer() override {
    return std::make_unique<VulkanBuffer>();
  }
  std::unique_ptr<Texture> CreateTexture() override {
    return std::make_unique<VulkanTexture>();
  }
  std::unique_ptr<GUI> CreateGUI() override {
    // TODO: Vk GUI
    return nullptr;
  }
};

} // namespace Render
#endif