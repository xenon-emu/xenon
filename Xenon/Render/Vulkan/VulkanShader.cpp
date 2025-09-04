// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanShader.h"

#ifndef NO_GFX
void Render::VulkanShader::CompileFromSource(eShaderType type, const char *source) {
  LOG_INFO(Render, "VulkanShader::CompileFromSource: {}", static_cast<u8>(type));
}

void Render::VulkanShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {
  LOG_INFO(Render, "VulkanShader::CompileFromBinary: {}", static_cast<u8>(type));
}

s32 Render::VulkanShader::GetUniformLocation(const std::string &name) {
  LOG_INFO(Render, "VulkanShader::GetUniformLocation: {}", name);
  return 0;
}

void Render::VulkanShader::SetUniformInt(const std::string &name, s32 value) {
  LOG_INFO(Render, "VulkanShader::SetUniformInt: {}, {}", name, value);
}

void Render::VulkanShader::SetUniformFloat(const std::string &name, f32 value) {
  LOG_INFO(Render, "VulkanShader::SetUniformFloat: {}, {}", name, value);
}

void Render::VulkanShader::SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) {
  LOG_INFO(Render, "VulkanShader::SetVertexShaderConsts: {}, {}", baseVector, count);
}

void Render::VulkanShader::SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) {
  LOG_INFO(Render, "VulkanShader::SetPixelShaderConsts: {}, {}", baseVector, count);
}

void Render::VulkanShader::SetBooleanConstants(const u32 *data) {
  LOG_INFO(Render, "VulkanShader::SetBooleanConstants");
}

bool Render::VulkanShader::Link() {
  LOG_INFO(Render, "VulkanShader::Link");
  return true;
}

void Render::VulkanShader::Bind() {
  LOG_INFO(Render, "VulkanShader::Bind");
}

void Render::VulkanShader::Unbind() {
  LOG_INFO(Render, "VulkanShader::Unbind");
}

void Render::VulkanShader::Destroy() {
  LOG_INFO(Render, "VulkanShader::Destroy");
}
#endif
