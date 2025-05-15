// Copyright 2025 Xenon Emulator Project

#include "VulkanShader.h"

#ifndef NO_GFX

namespace Render {

void VulkanShader::CompileFromSource(eShaderType type, const char *source) {

}

void VulkanShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {

}

s32 VulkanShader::GetUniformLocation(const std::string &name) {
  return 0;
}

void VulkanShader::SetUniformInt(const std::string &name, s32 value) {

}

void VulkanShader::SetUniformFloat(const std::string &name, f32 value) {

}

void VulkanShader::Link() {

}

void VulkanShader::Bind() {

}

void VulkanShader::Unbind() {

}

void VulkanShader::Destroy() {

}

} // namespace Render
#endif
