// Copyright 2025 Xenon Emulator Project

#include "OGLBuffer.h"

#ifndef NO_GFX

void Render::OGLBuffer::CreateBuffer(u32 size, const void *data, eBufferUsage usage, eBufferType type) {
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

void Render::OGLBuffer::UpdateBuffer(u32 offset, u32 size, const void *data) {
  glBindBuffer(GLTarget, BufferHandle);
  glBufferSubData(GLTarget, offset, size, data);
  glBindBuffer(GLTarget, 0);
}

void Render::OGLBuffer::Bind() {
  // Storage buffers use bind base
  if (GLTarget == GL_SHADER_STORAGE_BUFFER) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, BufferHandle);
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

u32 Render::OGLBuffer::ConvertBufferType(eBufferType type) {
  switch (type) {
  case eBufferType::Vertex: return GL_ARRAY_BUFFER;
  case eBufferType::Index: return GL_ELEMENT_ARRAY_BUFFER;
  case eBufferType::Uniform: return GL_UNIFORM_BUFFER;
  case eBufferType::Storage: return GL_SHADER_STORAGE_BUFFER;
  default: return GL_ARRAY_BUFFER;
  }
}

u32 Render::OGLBuffer::ConvertUsage(eBufferUsage usage) {
  switch (usage) {
  case eBufferUsage::StaticDraw: return GL_STATIC_DRAW;
  case eBufferUsage::DynamicDraw: return GL_DYNAMIC_DRAW;
  case eBufferUsage::StreamDraw: return GL_STREAM_DRAW;
  default: return GL_STATIC_DRAW;
  }
}

#endif
