// Copyright 2025 Xenon Emulator Project. All rights reserved.

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

void Render::OGLShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {
  u32 shader = glCreateShader(ToGLShaderType(type));
  glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, data, size);
  glSpecializeShaderARB(shader, "main", 0, 0, 0);
  
  s32 success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    s32 logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
      std::vector<char> log(logLength);
      glGetShaderInfoLog(shader, logLength, NULL, log.data());
      LOG_CRITICAL(System, "Shader compilation failed! {}", log.data());
    }
    LOG_CRITICAL(System, "Shader compilation failed! No message present, likely SPIR-V");
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

void Render::OGLShader::SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) {

}

void Render::OGLShader::SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) {

}

void Render::OGLShader::SetBooleanConstants(const u32 *data) {

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
    s32 logLength = 0;
    glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
      std::vector<char> log(logLength);
      glGetProgramInfoLog(Program, logLength, NULL, log.data());
      LOG_CRITICAL(System, "Shader linking failed! {}", log.data());
      return;
    }
    LOG_CRITICAL(System, "Shader linking failed! No message present, likely SPIR-V");
  } else if (AttachedShaders.empty()) {
    LOG_CRITICAL(System, "Shader linking failed! No shaders to link!");
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
