/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Render/Abstractions/Shader.h"

#include "Base/Types.h"
#include "Base/Logging/Log.h"

#ifndef NO_GFX
namespace Render {

class DummyShader : public Shader {
public:
  ~DummyShader() override { Destroy(); }

  void CompileFromSource(eShaderType type, const char *source) override;
  void CompileFromBinary(eShaderType type, const u8 *data, u64 size) override;
  s32 GetUniformLocation(const std::string &name) override;
  void SetUniformInt(const std::string &name, s32 value) override;
  void SetUniformFloat(const std::string &name, f32 value) override;
  void SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) override;
  void SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) override;
  void SetBooleanConstants(const u32 *data) override;
  bool Link() override;
  void Bind() override;
  void Unbind() override;
  void Destroy() override;
};

} // namespace Render
#endif
