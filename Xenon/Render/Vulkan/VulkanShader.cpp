// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanShader.h"

#ifndef NO_GFX

void Render::VulkanShader::CompileFromSource(eShaderType type, const char *source) {

}

void Render::VulkanShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = size;
  createInfo.pCode = reinterpret_cast<const u32 *>(data);

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    LOG_ERROR(Render, "Failed to create Vulkan shader module");
    return;
  }

  switch (type) {
  case eShaderType::Vertex:
    vertexShader = shaderModule;
    break;
  case eShaderType::Fragment:
    fragmentShader = shaderModule;
    break;
  default:
    LOG_ERROR(Render, "Unsupported shader type.");
    vkDestroyShaderModule(device, shaderModule, nullptr);
    break;
  }
}

s32 Render::VulkanShader::GetUniformLocation(const std::string &name) {
  LOG_WARNING(Render, "GetUniformLocation is not supported in Vulkan");
  return 0;
}

void Render::VulkanShader::SetUniformInt(const std::string &name, s32 value) {
  LOG_WARNING(Render, "SetUniformInt is not supported in Vulkan (use push constants/UBOs)");
}

void Render::VulkanShader::SetUniformFloat(const std::string &name, f32 value) {

}

void Render::VulkanShader::SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) {

}

void Render::VulkanShader::SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) {

}

void Render::VulkanShader::SetBooleanConstants(const u32 *data) {

}

bool Render::VulkanShader::Link() {
  return true;
}

void Render::VulkanShader::Bind() {

}

void Render::VulkanShader::Unbind() {

}

void Render::VulkanShader::Destroy() {
  if (vertexShader) {
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vertexShader = VK_NULL_HANDLE;
  }
  if (fragmentShader) {
    vkDestroyShaderModule(device, fragmentShader, nullptr);
    fragmentShader = VK_NULL_HANDLE;
  }
}

#endif
