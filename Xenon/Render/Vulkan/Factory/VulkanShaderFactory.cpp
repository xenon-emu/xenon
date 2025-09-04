// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanShaderFactory.h"

#include "Render/Vulkan/VulkanShader.h"

#ifndef NO_GFX
namespace Render {

void VulkanShaderFactory::Destroy() {}

std::shared_ptr<Shader> VulkanShaderFactory::CreateShader(const std::string &name) {
  LOG_INFO(Render, "VulkanShaderFactory::CreateShader: {}", name);
  return std::make_unique<VulkanShader>();
}

std::shared_ptr<Shader> VulkanShaderFactory::GetShader(const std::string &name) {
  LOG_INFO(Render, "VulkanShaderFactory::GetShader: {}", name);
  return std::make_unique<VulkanShader>();
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromSource(const std::string &name, const std::unordered_map<eShaderType, std::string> &sources) {
  LOG_INFO(Render, "VulkanShaderFactory::LoadFromSource: {}", name);
  return std::make_unique<VulkanShader>();
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromFile(const std::string &name, const fs::path &path) {
  LOG_INFO(Render, "VulkanShaderFactory::LoadFromFile: {}", name);
  return std::make_unique<VulkanShader>();
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromFiles(const std::string &name, const std::unordered_map<eShaderType, fs::path> &sources) {
  LOG_INFO(Render, "VulkanShaderFactory::LoadFromFiles: {}", name);
  return std::make_unique<VulkanShader>();
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromBinary(const std::string &name, const std::unordered_map<eShaderType, std::vector<u32>> &sources) {
  LOG_INFO(Render, "VulkanShaderFactory::LoadFromBinary: {}", name);
  return std::make_unique<VulkanShader>();
}

} // namespace Render
#endif
