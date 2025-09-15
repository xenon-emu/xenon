/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Render/Abstractions/VertexInput.h"
#include "Render/Abstractions/Buffer.h"

#include "Base/Types.h"
#include "Base/Logging/Log.h"

#ifndef NO_GFX
namespace Render {

class DummyVertexInput : public VertexInput {
public:
  void SetBindings(const std::vector<VertexBinding> &bindings) override;
  void SetAttributes(const std::vector<VertexAttribute> &attributes) override;
  void BindVertexBuffer(u32 binding, std::shared_ptr<Buffer> buffer) override;
  void SetIndexBuffer(std::shared_ptr<Buffer> buffer) override;
  void Bind() override;
  void Unbind() override;
};

} // namespace Render
#endif
