// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "OGLBuffer.h"

#ifndef NO_GFX

void Render::OGLBuffer::CreateBuffer(u64 size, const void *data, eBufferUsage usage, eBufferType type) {
  DestroyBuffer();
  GLTarget = ConvertBufferType(type);
  GLUsage = ConvertUsage(usage);
  glGenBuffers(1, &BufferHandle);
  glBindBuffer(GLTarget, BufferHandle);
  glBufferData(GLTarget, size, data, GLUsage);
  glBindBuffer(GLTarget, 0);
  SetSize(size);
  SetType(type);
}

void Render::OGLBuffer::UpdateBuffer(u64 offset, u64 size, const void *data) {
  if (size >= GetSize()) {
    CreateBuffer(size, data, ConvertGLUsage(GLUsage), ConvertGLBufferType(GLTarget));
  } else {
    SetSize(size);
    glBindBuffer(GLTarget, BufferHandle);
    glBufferSubData(GLTarget, offset, size, data);
    glBindBuffer(GLTarget, 0);
  }
}

void Render::OGLBuffer::Bind(u32 binding) {
  // Storage buffers use bind base
  if (GLTarget == GL_SHADER_STORAGE_BUFFER) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, BufferHandle);
  } else if (GLTarget == GL_UNIFORM_BUFFER) {
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, BufferHandle);
  } else {
    glBindBuffer(GLTarget, BufferHandle);
  }
}

void Render::OGLBuffer::Unbind() {
  glBindBuffer(GLTarget, 0);
}

void Render::OGLBuffer::DestroyBuffer() {
  if (BufferHandle) {
    glDeleteBuffers(1, &BufferHandle);
    BufferHandle = 0;
  }
}

u32 Render::OGLBuffer::ConvertBufferType(Render::eBufferType type) {
  switch (type) {
  case Render::eBufferType::Vertex: return GL_ARRAY_BUFFER;
  case Render::eBufferType::Index: return GL_ELEMENT_ARRAY_BUFFER;
  case Render::eBufferType::Uniform: return GL_UNIFORM_BUFFER;
  case Render::eBufferType::Storage: return GL_SHADER_STORAGE_BUFFER;
  default: return GL_ARRAY_BUFFER;
  }
}

u32 Render::OGLBuffer::ConvertUsage(Render::eBufferUsage usage) {
  switch (usage) {
  case Render::eBufferUsage::StaticDraw: return GL_STATIC_DRAW;
  case Render::eBufferUsage::DynamicDraw: return GL_DYNAMIC_DRAW;
  case Render::eBufferUsage::StreamDraw: return GL_STREAM_DRAW;
  default: return GL_STATIC_DRAW;
  }
}
Render::eBufferType Render::OGLBuffer::ConvertGLBufferType(u32 type) {
  switch (type) {
  case GL_ARRAY_BUFFER: return Render::eBufferType::Vertex;
  case GL_ELEMENT_ARRAY_BUFFER: return Render::eBufferType::Index;
  case GL_UNIFORM_BUFFER: return Render::eBufferType::Index;
  case GL_SHADER_STORAGE_BUFFER: return Render::eBufferType::Storage;
  default: return Render::eBufferType::Vertex;
  }
}
Render::eBufferUsage Render::OGLBuffer::ConvertGLUsage(u32 usage) {
  switch (usage) {
  case GL_STATIC_DRAW: return Render::eBufferUsage::StaticDraw;
  case GL_DYNAMIC_DRAW: return Render::eBufferUsage::DynamicDraw;
  case GL_STREAM_DRAW: return Render::eBufferUsage::StreamDraw;
  default: return Render::eBufferUsage::StaticDraw;
  }
}

#endif
