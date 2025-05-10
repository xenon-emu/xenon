// Copyright 2025 Xenon Emulator Project

#pragma once

#include <unordered_map>
#include <vector>

#include "Render/Abstractions/Shader.h"

#ifndef NO_GFX

#define GL_GLEXT_PROTOTYPES
#include <KHR/khrplatform.h>
#include <glad/glad.h>

namespace Render {

class OGLShader : public Shader {
public:
  ~OGLShader() override { Destroy(); }

  void CompileFromSource(eShaderType type, const char *source) override;
  s32 GetUniformLocation(const std::string &name) override;
  void SetUniformInt(const std::string &name, s32 value) override;
  void SetUniformFloat(const std::string &name, f32 value) override;
  void Link() override;
  void Bind() override;
  void Unbind() override;
  void Destroy() override;
  void SetProgram(u32 prog) { Program = prog; }
private:
  u32 Program = 0;
  std::vector<u32> AttachedShaders;
  std::unordered_map<std::string, s32> UniformCache;

  u32 ToGLShaderType(eShaderType type);
};

} //namespace Render
#endif
