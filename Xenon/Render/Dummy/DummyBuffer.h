/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Render/Abstractions/Buffer.h"

#include "Base/Types.h"
#include "Base/Logging/Log.h"

#ifndef NO_GFX
namespace Render {

class DummyBuffer : public Buffer {
public:
  DummyBuffer() = default;
  ~DummyBuffer() {
    DestroyBuffer();
  }

  void CreateBuffer(u64 size, const void *data, eBufferUsage usage, eBufferType type) override;
  void UpdateBuffer(u64 offset, u64 size, const void *data) override;
  void Bind(u32 binding) override;
  void Unbind() override;
  void DestroyBuffer() override;
  void *GetBackendHandle() override;
private:
  u32 DummyHandle = 0;
};

} // namespace Render
#endif
