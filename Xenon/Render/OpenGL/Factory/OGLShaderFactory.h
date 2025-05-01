// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Path_util.h"

#include "Render/Abstractions/Factory/ShaderFactory.h"

#ifndef NO_GFX
namespace Render {
  
class OGLShaderFactory : public ShaderFactory {
public:
  void Destroy() override;
  std::shared_ptr<Shader> CreateShader(const std::string &name) override;
  std::shared_ptr<Shader> LoadFromFile(const std::string &name, const fs::path &path) override;
  std::shared_ptr<Shader> LoadFromSource(const std::string &name, const std::unordered_map<eShaderType, std::string> &sources) override;
  std::shared_ptr<Shader> GetShader(const std::string &name) override;
};

} // namespace Render
#endif
