/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include <memory>

#include "Render/Abstractions/Buffer.h"
#include "Render/Abstractions/Texture.h"
#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif

#ifndef NO_GFX
namespace Render {

class ShaderFactory;
class Buffer;
class Texture;
class ResourceFactory {
public:
  virtual ~ResourceFactory() = default;
  virtual std::unique_ptr<ShaderFactory> CreateShaderFactory() = 0;
  virtual std::unique_ptr<Buffer> CreateBuffer() = 0;
  virtual std::unique_ptr<Texture> CreateTexture() = 0;
#ifndef TOOL
  virtual std::unique_ptr<GUI> CreateGUI() = 0;
#endif
};

} // namespace Render
#endif
