// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Base/Types.h"
#include "Render/Abstractions/Buffer.h"

namespace Render {

class DummyBuffer : public Buffer {
public:
    DummyBuffer() = default;
    ~DummyBuffer() {
        DestroyBuffer();
    }

    void CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) override;
    void UpdateBuffer(u32 offset, u32 size, const void *data) override;
    void Bind() override;
    void Unbind() override;
    void DestroyBuffer() override;
};

} // namespace Render