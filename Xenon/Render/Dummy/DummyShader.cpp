// Copyright 2025 Xenon Emulator Project

#include "DummyShader.h"

void Render::DummyShader::CompileFromSource(eShaderType type, const char *source) {
  LOG_INFO(Render, "DummyShader::CompileFromSource: {}", static_cast<u8>(type));
}

s32 Render::DummyShader::GetUniformLocation(const std::string &name) {
  LOG_INFO(Render, "DummyShader::GetUniformLocation: {}", name);
  return 0;
}

void Render::DummyShader::SetUniformInt(const std::string &name, s32 value) {
  LOG_INFO(Render, "DummyShader::SetUniformInt: {}, {}", name, value);
}

void Render::DummyShader::SetUniformFloat(const std::string &name, f32 value) {
  LOG_INFO(Render, "DummyShader::SetUniformFloat: {}, {}", name, value);
}

void Render::DummyShader::Link() {
  LOG_INFO(Render, "DummyShader::Link");
}

void Render::DummyShader::Bind() {
  LOG_INFO(Render, "DummyShader::Bind");
}

void Render::DummyShader::Unbind() {
  LOG_INFO(Render, "DummyShader::Unbind");
}

void Render::DummyShader::Destroy() {
  LOG_INFO(Render, "DummyShader::Destroy");
}



