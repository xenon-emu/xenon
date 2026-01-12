/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#ifndef NO_GFX
namespace Render {

enum class eBufferUsage : u8 {
  StaticDraw,
  DynamicDraw,
  StreamDraw,
  ReadOnly
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
  virtual void CreateBuffer(u64 size, const void *data, eBufferUsage usage, eBufferType type) = 0;
  virtual void UpdateBuffer(u64 offset, u64 size, const void *data) = 0;
  virtual void Bind(u32 binding = 1) = 0;
  virtual void Unbind() = 0;
  virtual void DestroyBuffer() = 0;
  virtual void *GetBackendHandle() = 0;
  virtual void SetSize(u64 size) { Size = size; }
  virtual u64 GetSize() const { return Size; }
  virtual void SetType(eBufferType type) { Type = type; }
  virtual eBufferType GetType() const { return Type; }
protected:
  u64 Size = 0;
  eBufferType Type;
};

} // namespace Render
#endif
