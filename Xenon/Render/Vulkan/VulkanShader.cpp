// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "VulkanShader.h"

#ifndef NO_GFX

void Render::VulkanShader::CompileFromSource(eShaderType type, const char *source) {

}

void Render::VulkanShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {

}

s32 Render::VulkanShader::GetUniformLocation(const std::string &name) {
  return 0;
}

void Render::VulkanShader::SetUniformInt(const std::string &name, s32 value) {

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

}

#endif
