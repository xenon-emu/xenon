// Copyright 2025 Xenon Emulator Project

#include "Render/Vulkan/VulkanShader.h"

#include "VulkanShaderFactory.h"BUFSIZ

#ifndef NO_GFX
namespace Render {

void VulkanShaderFactory::Destroy() {
  for (auto& [name, shader] : Shaders) {
    shader->Destroy();
  }
}
} // namespace Render
#endif
