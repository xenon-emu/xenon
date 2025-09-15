/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "Base/PathUtil.h"

#include "Render/Abstractions/Shader.h"

#ifndef NO_GFX
namespace Render {

class ShaderFactory {
public:
  virtual ~ShaderFactory() = default;
  virtual void Destroy() = 0;
  virtual std::shared_ptr<Shader> CreateShader(const std::string &name) = 0;
  virtual std::shared_ptr<Shader> LoadFromFile(const std::string &name, const fs::path &path) = 0;
  virtual std::shared_ptr<Shader> LoadFromFiles(const std::string &name, const std::unordered_map<eShaderType, fs::path> &sources) = 0;
  virtual std::shared_ptr<Shader> LoadFromSource(const std::string &name, const std::unordered_map<eShaderType, std::string> &sources) = 0;
  virtual std::shared_ptr<Shader> LoadFromBinary(const std::string &name, const std::unordered_map<eShaderType, std::vector<u32>> &sources) = 0;
  virtual std::shared_ptr<Shader> GetShader(const std::string &name) = 0;

  std::unordered_map<std::string, std::shared_ptr<Shader>> Shaders;
};

} // namespace Render
#endif
