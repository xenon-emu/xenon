// Copyright 2025 Xenon Emulator Project

#pragma once

#include <unordered_map>
#include <vector>

#include "Base/Types.h"
#include "Render/Abstractions/Shader.h"

#ifndef NO_GFX

namespace Render {

class VulkanShader : public Shader {
public:
  ~VulkanShader() override { Destroy(); }

  void CompileFromSource(eShaderType type, const char *source) override;
  void CompileFromBinary(eShaderType type, const u8 *data, u64 size) override;
  s32 GetUniformLocation(const std::string &name) override;
  void SetUniformInt(const std::string &name, s32 value) override;
  void SetUniformFloat(const std::string &name, f32 value) override;
  void Link() override;
  void Bind() override;
  void Unbind() override;
  void Destroy() override;
};
} // namespace Render
#endif