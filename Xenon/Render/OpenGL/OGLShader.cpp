// Copyright 2025 Xenon Emulator Project

#include "OGLShader.h"

#include "Base/Assert.h"
#include "Base/Logging/Log.h"

#ifndef NO_GFX

GLenum Render::OGLShader::ToGLShaderType(eShaderType type) {
  switch (type) {
    case eShaderType::Vertex: return GL_VERTEX_SHADER;
    case eShaderType::Fragment: return GL_FRAGMENT_SHADER;
    case eShaderType::Compute: return GL_COMPUTE_SHADER;
    default: UNREACHABLE_MSG("Unknown Shader Type");
  }
}

void Render::OGLShader::CompileFromSource(eShaderType type, const char *source) {
  u32 shader = glCreateShader(ToGLShaderType(type));
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  s32 success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    LOG_ERROR(System, "Shader compilation failed:\n{}", log);
    glDeleteShader(shader);
    return;
  }

  AttachedShaders.push_back(shader);
}

s32 Render::OGLShader::GetUniformLocation(const std::string &name) {
  auto it = UniformCache.find(name);
  if (it != UniformCache.end()) {
    return it->second;
  }
  s32 location = glGetUniformLocation(Program, name.c_str());
  UniformCache[name] = location;
  return location;
}

void Render::OGLShader::SetUniformInt(const std::string &name, s32 value) {
  glUniform1i(GetUniformLocation(name), value);
}

void Render::OGLShader::SetUniformFloat(const std::string &name, f32 value) {
  glUniform1f(GetUniformLocation(name), value);
}

void Render::OGLShader::Link() {
  Program = glCreateProgram();
  for (auto shader : AttachedShaders) {
    glAttachShader(Program, shader);
  }

  glLinkProgram(Program);

  s32 success = 0;
  glGetProgramiv(Program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[1024];
    glGetProgramInfoLog(Program, sizeof(log), nullptr, log);
    UNREACHABLE_MSG("Shader link failed:\n{}", log);
  }

  for (auto shader : AttachedShaders) {
    glDeleteShader(shader);
  }
  AttachedShaders.clear();
}

void Render::OGLShader::Bind() {
  glUseProgram(Program);
}

void Render::OGLShader::Unbind() {
  glUseProgram(0);
}

void Render::OGLShader::Destroy() {
  AttachedShaders.clear();
  UniformCache.clear();
  if (Program) {
    glDeleteProgram(Program);
    Program = 0;
  }
}

#endif