/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/OpenGL/Factory/OGLShaderFactory.h"
#include "Render/OpenGL/OGLBuffer.h"
#include "Render/OpenGL/OGLTexture.h"
#ifndef TOOL
#include "Render/GUI/OpenGL.h"
#endif

#ifndef NO_GFX
namespace Render {

class OGLResourceFactory : public ResourceFactory {
public:
  std::unique_ptr<ShaderFactory> CreateShaderFactory() override {
    return std::make_unique<OGLShaderFactory>();
  }
  std::unique_ptr<Buffer> CreateBuffer() override {
    return std::make_unique<OGLBuffer>();
  }
  std::unique_ptr<Texture> CreateTexture() override {
    return std::make_unique<OGLTexture>();
  }
#ifndef TOOL
  std::unique_ptr<GUI> CreateGUI() override {
    return std::make_unique<OpenGLGUI>();
  }
#endif
};

} // namespace Render
#endif
