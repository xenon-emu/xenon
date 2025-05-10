// Copyright 2025 Xenon Emulator Project

#include "DummyShaderFactory.h"

namespace Render {

void DummyShaderFactory::Destroy() {}

std::shared_ptr<Shader> DummyShaderFactory::CreateShader(const std::string &name) {
    return nullptr;
}

std::shared_ptr<Shader> DummyShaderFactory::GetShader(const std::string &name) {
    return nullptr;
}

std::shared_ptr<Shader> DummyShaderFactory::LoadFromSource(const std::string &name, const std::unordered_map<eShaderType, std::string> &sources) {
    return nullptr;
}

std::shared_ptr<Shader> DummyShaderFactory::LoadFromFile(const std::string &name, const fs::path &path) {
    return nullptr;
}

} // namespace Render