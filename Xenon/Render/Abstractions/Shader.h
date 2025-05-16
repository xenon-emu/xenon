// Copyright 2025 Xenon Emulator Project. All rights reserved.

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
  virtual void CompileFromBinary(eShaderType type, const u8 *data, u64 size) = 0;
  virtual s32 GetUniformLocation(const std::string &name) = 0;
  virtual void SetUniformInt(const std::string &name, s32 value) = 0;
  virtual void SetUniformFloat(const std::string &name, f32 value) = 0;
  virtual void SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) = 0;
  virtual void SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) = 0;
  virtual void SetBooleanConstants(const u32 *data) = 0;
  virtual bool Link() = 0;
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
  virtual void Destroy() = 0;
};

} // namespace Render
#endif
