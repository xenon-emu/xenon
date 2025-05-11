// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Shader.h"

#include "Base/Types.h"
#include "Base/Logging/Log.h"

namespace Render {

class DummyShader : public Shader {
public:
  ~DummyShader() override { Destroy(); }

  void CompileFromSource(eShaderType type, const char *source) override;
  s32 GetUniformLocation(const std::string &name) override;
  void SetUniformInt(const std::string &name, s32 value) override;
  void SetUniformFloat(const std::string &name, f32 value) override;
  void Link() override;
  void Bind() override;
  void Unbind() override;
  void Destroy() override;
};

} // namespace Render