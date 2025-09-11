// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanShaderFactory.h"
#include "Render/Vulkan/VulkanShader.h"

#include <fstream>
#include <sstream>

#ifndef NO_GFX
namespace Render {

VulkanShaderFactory::VulkanShaderFactory(VulkanRenderer *renderer) :
  device(renderer->device)
{}

void VulkanShaderFactory::Destroy() {
  for (const auto& [name, shader] : Shaders) {
    if (shader)
      shader->Destroy();
  }
  Shaders.clear();
}

std::shared_ptr<Shader> VulkanShaderFactory::CreateShader(const std::string &name) {
  auto shader = std::make_shared<VulkanShader>(device);
  Shaders[name] = shader;
  return shader;
}

std::shared_ptr<Shader> VulkanShaderFactory::GetShader(const std::string &name) {
  auto it = Shaders.find(name);
  return (it != Shaders.end()) ? it->second : nullptr;
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromBinary(
  const std::string &name,
  const std::unordered_map<eShaderType, std::vector<u32>> &sources)
{
  auto shader = std::make_shared<VulkanShader>(device);
  for (auto& [type, spirv] : sources) {
    shader->CompileFromBinary(type, reinterpret_cast<const u8 *>(spirv.data()), spirv.size());
  }
  Shaders[name] = shader;
  return shader;
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromFile(const std::string &name, const fs::path &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR(System, "Failed to open SPIR-V shader '{}'", path.string());
    return nullptr;
  }

  size_t fileSize = file.tellg();
  std::vector<u32> buffer(fileSize / sizeof(u32));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
  file.close();

  // TODO: Filter by metadat
  return LoadFromBinary(name, { { eShaderType::Compute, buffer } });
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromFiles(
  const std::string &name,
  const std::unordered_map<eShaderType, fs::path> &sources)
{
  auto shader = std::make_shared<VulkanShader>(device);
  bool deadFile = false;

  for (auto& [type, path] : sources) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
      LOG_ERROR(System, "Failed to open SPIR-V shader '{}'", path.string());
      deadFile = true;
      continue;
    }

    size_t fileSize = file.tellg();
    std::vector<u32> buffer(fileSize / sizeof(u32));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
    file.close();

    shader->CompileFromBinary(type, reinterpret_cast<const u8 *>(buffer.data()), buffer.size());
  }

  if (deadFile) {
    return nullptr;
  }

  Shaders[name] = shader;
  return shader;
}

std::shared_ptr<Shader> VulkanShaderFactory::LoadFromSource(
  const std::string &name,
  const std::unordered_map<eShaderType, std::string> &sources)
{
  LOG_ERROR(Render, "VulkanShaderFactory::LoadFromSource not supported at runtime (compile to SPIR-V first!)");
  return nullptr;
}

} // namespace Render
#endif
