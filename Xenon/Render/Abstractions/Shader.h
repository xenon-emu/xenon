// Copyright 2025 Xenon Emulator Project

#pragma once

#include <string>

#ifndef NO_GFX
namespace Render {
  
enum class eShaderType : u8 {
  Vertex,
  Fragment,
  Compute,
  Invalid
};

class Shader {
public:
  virtual ~Shader() = default;
  virtual void CompileFromSource(eShaderType type, const char *source) = 0;
  virtual s32 GetUniformLocation(const std::string &name) = 0;
  virtual void SetUniformInt(const std::string &name, s32 value) = 0;
  virtual void SetUniformFloat(const std::string &name, f32 value) = 0;
  virtual void Link() = 0;
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
  virtual void Destroy() = 0;
};

} // namespace Render
#endif