// Copyright 2025 Xenon Emulator Project

#include <fstream>
#include <sstream>

#include "Base/Logging/Log.h"
#include "Base/Hash.h"

#include "Render/OpenGL/OGLShader.h"

#include "OGLShaderFactory.h"

#ifndef NO_GFX
namespace Render {

void OGLShaderFactory::Destroy() {
  for (const auto& [name, shader] : Shaders) {
    shader->Destroy();
  }
}

std::shared_ptr<Shader> OGLShaderFactory::CreateShader(const std::string &name) {
  auto shader = std::make_shared<OGLShader>();
  Shaders[name] = shader;
  return shader;
}

std::shared_ptr<Shader> OGLShaderFactory::GetShader(const std::string &name) {
  auto it = Shaders.find(name);
  return (it != Shaders.end()) ? it->second : nullptr;
}

std::shared_ptr<Shader> OGLShaderFactory::LoadFromSource(const std::string &name,
  const std::unordered_map<eShaderType, std::string> &sources) {
  auto shader = std::make_shared<OGLShader>();
  for (const auto& [type, src] : sources) {
    shader->CompileFromSource(type, src.c_str());
  }
  shader->Link();
  Shaders[name] = shader;
  return shader;
}

eShaderType GetShaderType(const std::string &line) {
  switch (Base::joaatStringHash(line)) {
  case "#vertex"_j: return eShaderType::Vertex;
  case "#fragment"_j: return eShaderType::Fragment;
  case "#compute"_j: return eShaderType::Compute;
  default: return eShaderType::Invalid;
  }
  return eShaderType::Invalid;
}

std::shared_ptr<Shader> OGLShaderFactory::LoadFromFile(const std::string &name, const fs::path &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR(System, "Failed to open shader file: {}", path.string());
    return nullptr;
  }

  std::unordered_map<eShaderType, std::string> shaderSources;
  std::stringstream buffer;
  std::string line;
  eShaderType currentType;
  bool insideShader = false;

  while (std::getline(file, line)) {
    eShaderType type = GetShaderType(line);
    if (type != eShaderType::Invalid) {
      if (insideShader) {
        shaderSources[currentType] = buffer.str();
        buffer.str("");
        buffer.clear();
      }
      currentType = type;
      insideShader = true;
    } else if (insideShader) {
      buffer << line << '\n';
    }
  }

  if (insideShader) {
    shaderSources[currentType] = buffer.str();
  }

  return LoadFromSource(name, shaderSources);
}

} // namespace Render
#endif