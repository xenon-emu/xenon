// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "DummyShader.h"

void Render::DummyShader::CompileFromSource(eShaderType type, const char *source) {
  LOG_INFO(Render, "DummyShader::CompileFromSource: {}", static_cast<u8>(type));
}

void Render::DummyShader::CompileFromBinary(eShaderType type, const u8 *data, u64 size) {
  LOG_INFO(Render, "DummyShader::CompileFromBinary: {}", static_cast<u8>(type));
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

void Render::DummyShader::SetVertexShaderConsts(u32 baseVector, u32 count, const f32 *data) {
  LOG_INFO(Render, "DummyShader::SetVertexShaderConsts: {}, {}", baseVector, count);
}

void Render::DummyShader::SetPixelShaderConsts(u32 baseVector, u32 count, const f32 *data) {
  LOG_INFO(Render, "DummyShader::SetPixelShaderConsts: {}, {}", baseVector, count);
}

void Render::DummyShader::SetBooleanConstants(const u32 *data) {
  LOG_INFO(Render, "DummyShader::SetBooleanConstants");
}

bool Render::DummyShader::Link() {
  LOG_INFO(Render, "DummyShader::Link");
  return true;
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



