// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Dummy/Factory/DummyShaderFactory.h"
#include "Render/Dummy/DummyBuffer.h"
#include "Render/Dummy/DummyTexture.h"
#include "Render/GUI/Dummy.h"

#include "Base/Logging/Log.h"

namespace Render {

class DummyResourceFactory : public ResourceFactory {
public:
  std::unique_ptr<ShaderFactory> CreateShaderFactory() override {
    LOG_INFO(Render, "DummyResourceFactory::CreateShaderFactory");
    return std::make_unique<DummyShaderFactory>();
  }
  std::unique_ptr<Buffer> CreateBuffer() override {
    LOG_INFO(Render, "DummyResourceFactory::CreateBuffer");
    return std::make_unique<DummyBuffer>();
  }
  std::unique_ptr<Texture> CreateTexture() override {
    LOG_INFO(Render, "DummyResourceFactory::CreateTexture");
    return std::make_unique<DummyTexture>();
  }
  std::unique_ptr<GUI> CreateGUI() override {
    LOG_INFO(Render, "DummyResourceFactory::CreateGUI");
    return std::make_unique<DummyGUI>();
  }
};

} // namespace Render