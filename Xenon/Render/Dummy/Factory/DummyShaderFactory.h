// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Factory/ShaderFactory.h"

#include "Base/Logging/Log.h"

namespace Render {

class DummyShaderFactory : public ShaderFactory {
public:
  void Destroy() override;
  std::shared_ptr<Shader> CreateShader(const std::string &name) override;
  std::shared_ptr<Shader> LoadFromFile(const std::string &name, const fs::path &path) override;
  std::shared_ptr<Shader> LoadFromFiles(const std::string &name, const std::unordered_map<eShaderType, fs::path> &sources) override;
  std::shared_ptr<Shader> LoadFromSource(const std::string &name, const std::unordered_map<eShaderType, std::string> &sources) override;
  std::shared_ptr<Shader> LoadFromBinary(const std::string &name, const std::unordered_map<eShaderType, std::vector<u32>> &sources) override;
  std::shared_ptr<Shader> GetShader(const std::string &name) override;
};

} // namespace Render