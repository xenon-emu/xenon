// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Dummy/Factory/DummyShaderFactory.h"
#include "Render/Dummy/DummyBuffer.h"
#include "Render/Dummy/DummyTexture.h"
#include "Render/GUI/Dummy.h"

namespace Render {

class DummyResourceFactory : public ResourceFactory {
public:
    std::unique_ptr<ShaderFactory> CreateShaderFactory() override {
        return std::make_unique<DummyShaderFactory>();
    }
    std::unique_ptr<Buffer> CreateBuffer() override {
        return std::make_unique<DummyBuffer>();
    }
    std::unique_ptr<Texture> CreateTexture() override {
        return std::make_unique<DummyTexture>();
    }
    std::unique_ptr<GUI> CreateGUI() override {
        return std::make_unique<DummyGUI>();
    }
};

} // namespace Render