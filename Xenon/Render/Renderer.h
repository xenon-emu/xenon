// Copyright 2025 Xenon Emulator Project

#pragma once

#include <thread>
#include <vector>
#include <fstream>

#include "Base/Types.h"

#ifndef NO_GFX
#include <SDL3/SDL.h>

#define GL_GLEXT_PROTOTYPES
extern "C" {
#include <KHR/khrplatform.h>
#include <glad/glad.h>
}

#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>

#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIe.h"
#include "Render/Abstractions/Texture.h"
#include "Render/GUI/GUI.h"

// ARGB (Console is BGRA)
#define COLOR(r, g, b, a) ((a) << 24 | (r) << 16 | (g) << 8 | (b) << 0)
#define TILE(x) ((x + 31) >> 5) << 5

namespace Render {

class Renderer {
public:
  Renderer(RAM *ram);
  ~Renderer();
  void Start();
  void Shutdown();

  void Resize(int x, int y, bool resizeViewport = true);

  void Thread();

  // Debugger Pointers (RAM, and FrameBuffer)
  RAM *ramPointer{};
  u8 *fbPointer{};

  // Initial Internal rendering width/height.
  u32 internalWidth = 1280;
  u32 internalHeight = 720;

  // Window Resolution
  u32 width = 1280;
  u32 height = 720;

  // Vertical SYNC
  bool VSYNC = true;
  // Is Fullscreen
  bool fullscreen = false;
  // Thread Running
  volatile bool threadRunning = true;

  bool DebuggerActive() {
    return gui.get() && gui.get()->ppcDebuggerActive;
  }

  // FB Pitch
  int pitch = 0;
private:
  // Thread handle
  std::thread thread;

  // Backbuffer texture
  std::unique_ptr<Texture> backbuffer;

  // GUI handle
  std::unique_ptr<GUI> gui;

  // Pixel buffer
  std::vector<u32> pixels{};
  // SDL Window data
  SDL_Window *mainWindow{};
  SDL_GLContext context;
  SDL_Event windowEvent;
  SDL_WindowID windowID;
public:
  // OpenGL Handles
  // XeFB Pixel Buffer
  GLuint pixelBuffer;
  // Texture handles
  GLuint texture, dummyVAO;
  // Shaders
  GLuint shaderProgram, renderShaderProgram;
};

// Shaders

inline constexpr const char* vertexShaderSource = R"(
#version 430 core

out vec2 o_texture_coord;

void main() {
  o_texture_coord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(o_texture_coord * vec2(2.0f, -2.0f) + vec2(-1.0f, 1.0f), 0.0f, 1.0f);
})";

inline constexpr const char* fragmentShaderSource = R"(
#version 430 core

in vec2 o_texture_coord;

out vec4 o_color;

uniform usampler2D u_texture;
void main() {
  uint pixel = texture(u_texture, o_texture_coord).r;
  // Gotta love BE vs LE (X360 works in BGRA, so we work in ARGB)
  const float a = float((pixel >> 24) & 0xFF) / 255.0;
  const float r = float((pixel >> 16) & 0xFF) / 255.0;
  const float g = float((pixel >> 8) & 0xFF) / 255.0;
  const float b = float((pixel >> 0) & 0xFF) / 255.0;
  o_color = vec4(r, g, b, a);
})";

inline constexpr const char* computeShaderSource = R"(
#version 430 core

layout (local_size_x = 16, local_size_y = 16) in;

layout (r32ui, binding = 0) uniform writeonly uimage2D o_texture;
layout (std430, binding = 1) buffer pixel_buffer {
  uint pixel_data[];
};

uniform int internalWidth;
uniform int internalHeight;

uniform int resWidth;
uniform int resHeight;

// This is black magic to convert tiles to linear, just don't touch it
int xeFbConvert(int width, int addr) {
  int y = addr / (width * 4);
  int x = (addr % (width * 4)) / 4;
  return ((((y & ~31) * width) + (x & ~31) * 32) +
         (((x & 3) + ((y & 1) << 2) + ((x & 28) << 1) + ((y & 30) << 5)) ^
         ((y & 8) << 2)));
}

void main() {
  ivec2 texel_pos = ivec2(gl_GlobalInvocationID.xy);
  // OOB check, but shouldn't be needed
  if (texel_pos.x >= resWidth || texel_pos.y >= resHeight)
    return;

  // Precalc whatever it would be with extra sizing for 32x32 tiles
  const int tiledWidth = TILE(internalWidth);
  const int tiledHeight = TILE(internalHeight);

  // Scale accordingly
  float scaleX = internalWidth / float(resWidth);
  float scaleY = internalHeight / float(resHeight);

  // Map to source resolution
  const int srcX = int(float(texel_pos.x) * scaleX);
  const int srcY = int(float(texel_pos.y) * scaleY);

  // God only knows how this indexing works
  int stdIndex = (srcY * internalWidth + srcX);
  int xeIndex = xeFbConvert(internalWidth, stdIndex * 4);

  uint packedColor = pixel_data[xeIndex];
  imageStore(o_texture, texel_pos, uvec4(packedColor, 0, 0, 0));
})";

} // namespace Render

#endif
