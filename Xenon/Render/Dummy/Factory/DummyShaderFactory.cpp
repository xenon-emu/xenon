// Copyright 2025 Xenon Emulator Project

#include "DummyShaderFactory.h"

#include "Render/Dummy/DummyShader.h"

namespace Render {

void DummyShaderFactory::Destroy() {}

std::shared_ptr<Shader> DummyShaderFactory::CreateShader(const std::string &name) {
  LOG_INFO(Render, "DummyShaderFactory::CreateShader: {}", name);
  return std::make_unique<DummyShader>();
}

std::shared_ptr<Shader> DummyShaderFactory::GetShader(const std::string &name) {
  LOG_INFO(Render, "DummyShaderFactory::GetShader: {}", name);
  return std::make_unique<DummyShader>();
}

std::shared_ptr<Shader> DummyShaderFactory::LoadFromSource(const std::string &name, const std::unordered_map<eShaderType, std::string> &sources) {
  LOG_INFO(Render, "DummyShaderFactory::LoadFromSource: {}", name);
  return std::make_unique<DummyShader>();
}

std::shared_ptr<Shader> DummyShaderFactory::LoadFromFile(const std::string &name, const fs::path &path) {
  LOG_INFO(Render, "DummyShaderFactory::LoadFromFile: {}", name);
  return std::make_unique<DummyShader>();
}

} // namespace Render