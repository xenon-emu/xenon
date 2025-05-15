// Copyright 2025 Xenon Emulator Project

#pragma once

#ifndef NO_GFX
namespace Render {

enum class eBufferUsage : u8 {
  StaticDraw,
  DynamicDraw,
  StreamDraw
};

enum class eBufferType : u8 {
  Vertex,
  Index,
  Uniform,
  Storage
};

class Buffer {
public:
  virtual ~Buffer() = default;
  virtual void CreateBuffer(u32 size, const void* data, eBufferUsage usage, eBufferType type) = 0;
  virtual void UpdateBuffer(u32 offset, u32 size, const void* data) = 0;
  virtual void Bind(u32 binding = 1) = 0;
  virtual void Unbind() = 0;
  virtual void DestroyBuffer() = 0;
  virtual void SetSize(u32 size) { Size = size; }
  virtual u32 GetSize() const { return Size; }
  virtual void SetType(eBufferType type) { Type = type; }
  virtual eBufferType GetType() const { return Type; }
protected:
  u32 Size = 0;
  eBufferType Type;
};

} // namespace Render
#endif
