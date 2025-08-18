// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <fstream>
#include <thread>
#include <unordered_map>
#include <vector>
#ifndef NO_GFX
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_sdl3.h>
#endif

#include "Base/Hash.h"
#include "Base/Types.h"

#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/PCIe.h"
#include "Core/XGPU/Microcode/ASTBlock.h"
#include "Core/XGPU/CommandProcessor.h"
#include "Core/XGPU/ShaderConstants.h"
#include "Render/Abstractions/Factory/ResourceFactory.h"
#include "Render/Abstractions/Factory/ShaderFactory.h"

#ifndef NO_GFX
// ARGB (Console is BGRA)
#define COLOR(r, g, b, a) ((a) << 24 | (r) << 16 | (g) << 8 | (b) << 0)
#define TILE(x) ((x + 31) >> 5) << 5
namespace Render {

class GUI;

struct DrawJob {
  bool indexed = false;
  Xe::XGPU::XeDrawParams params = {};
  u32 shaderVS = 0, shaderPS = 0;
  u64 shaderHash = 0;
};

struct BufferLoadJob {
  BufferLoadJob(const std::string &name, const std::vector<u8> &data, const eBufferType type, const eBufferUsage usage) :
    name(name), data(data),
    type(type), usage(usage) {
    hash = Base::JoaatStringHash(name);
  }
  std::string name = {};
  u32 hash = 0;
  std::vector<u8> data = {};
  eBufferType type;
  eBufferUsage usage;
};

class Renderer {
public:
  Renderer(RAM *ram);
  virtual ~Renderer() = default;
  virtual void BackendSDLProperties(SDL_PropertiesID properties) = 0;
  virtual void BackendStart() = 0;
  virtual void BackendShutdown() = 0;
  virtual void BackendSDLInit() = 0;
  virtual void BackendSDLShutdown() = 0;
  virtual void BackendResize(s32 x, s32 y) = 0;
  virtual void UpdateScissor(s32 x, s32 y, u32 width, u32 height) = 0;
  virtual void UpdateViewport(s32 x, s32 y, u32 width, u32 height) = 0;
  virtual void UpdateClearColor(u8 r, u8 b, u8 g, u8 a) = 0;
  virtual void UpdateClearDepth(f64 depth) = 0;
  virtual void Clear() = 0;

  virtual void UpdateViewportFromState(const Xe::XGPU::XenosState *state) = 0;
  virtual void Draw(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params) = 0;
  virtual void DrawIndexed(Xe::XGPU::XeShader shader, Xe::XGPU::XeDrawParams params, Xe::XGPU::XeIndexBufferInfo indexBufferInfo) = 0;

  virtual void OnCompute() = 0;
  virtual void OnBind() = 0;
  virtual void OnSwap(SDL_Window *window) = 0;
  virtual s32 GetBackbufferFlags() = 0;
  virtual s32 GetXenosFlags() = 0;
  virtual void* GetBackendContext() = 0;
  virtual u32 GetBackendID() = 0;
  void SDLInit();

  void Start();
  void CreateHandles();
  void Shutdown();
  void Resize(u32 x, u32 y);

  void UpdateConstants(Xe::XGPU::XenosState *state);

  bool IssueCopy(Xe::XGPU::XenosState *state);

  void TryLinkShaderPair(u32 vsHash, u32 psHash);

  void HandleEvents();

  void Thread();

  u32 VAO;
  // CPU Handles
  RAM *ramPointer{};
  u8 *fbPointer{};

  // Window Resolution
  u32 width = 1280;
  u32 height = 720;

  // ImGui Created
  bool imguiCreated = false;

  // Render Lost Lost
  bool focusLost = false;
  // Vertical SYNC
  bool VSYNC = true;
  // Is Fullscreen
  bool fullscreen = false;
  // Thread Running
  volatile bool threadRunning = true;

  // FB Pitch
  u32 pitch = 0;
  // SDL Window data
  SDL_Window *mainWindow = nullptr;
  SDL_Event windowEvent = {};
  SDL_WindowID windowID = {};
  // Factories
  std::unique_ptr<ResourceFactory> resourceFactory{};
  std::unique_ptr<ShaderFactory> shaderFactory{};

  // Backbuffer texture
  std::unique_ptr<Texture> backbuffer{};

  // Shader texture queue
  std::mutex drawQueueMutex{};
  std::queue<DrawJob> drawQueue{};

  // Shader load queue
  std::mutex bufferQueueMutex{};
  std::queue<BufferLoadJob> bufferLoadQueue{};
  std::unordered_map<u32, std::shared_ptr<Buffer>> createdBuffers{};

  // GUI Helpers
  bool DebuggerActive();

  void SetDebuggerActive(s8 specificPPU = -1);
  
  // Recompiled shaders
  std::mutex programLinkMutex{};
  std::unordered_map<u32, std::pair<Xe::Microcode::AST::Shader*, std::vector<u32>>> pendingVertexShaders{};
  std::unordered_map<u32, std::pair<Xe::Microcode::AST::Shader*, std::vector<u32>>> pendingPixelShaders{};
  std::unordered_map<u64, Xe::XGPU::XeShader> linkedShaderPrograms{};
  std::atomic<u32> currentVertexShader = 0;
  u32 pendingVertexShader = 0;
  std::atomic<u32> currentPixelShader = 0;
  u32 pendingPixelShader = 0;
  std::atomic<bool> readyToLink = false;
  std::mutex copyQueueMutex{};
  std::queue<Xe::XGPU::XenosState*> copyQueue{};
private:
  // Thread handle
  std::thread thread;

  // GUI handle
  std::unique_ptr<GUI> gui{};

  // Pixel buffer
  std::unique_ptr<Buffer> pixelSSBO{};
  std::vector<u32> pixels{};

  // Shaders
  std::shared_ptr<Shader> computeShaderProgram{};
  std::shared_ptr<Shader> renderShaderPrograms{};
};

// Shaders

inline constexpr const char vertexShaderSource[] = R"glsl(
out vec2 o_texture_coord;

void main() {
  o_texture_coord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(o_texture_coord * vec2(2.0f, -2.0f) + vec2(-1.0f, 1.0f), 0.0f, 1.0f);
})glsl";

inline constexpr const char fragmentShaderSource[] = R"glsl(
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp usampler2D;
precision highp uimage2D;

in vec2 o_texture_coord;

out vec4 o_color;

uniform usampler2D u_texture;
void main() {
  uint pixel = texture(u_texture, o_texture_coord).r;
  // Gotta love BE vs LE (X360 works in BGRA, so we work in ARGB)
  float a = float((pixel >> 24u) & 0xFFu) / 255.0;
  float r = float((pixel >> 16u) & 0xFFu) / 255.0;
  float g = float((pixel >> 8u) & 0xFFu) / 255.0;
  float b = float((pixel >> 0u) & 0xFFu) / 255.0;
  o_color = vec4(r, g, b, a);
})glsl";

inline constexpr const char computeShaderSource[] = R"glsl(
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp usampler2D;
precision highp uimage2D;

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

  // Scale accordingly
  float scaleX = float(internalWidth) / float(resWidth);
  float scaleY = float(internalHeight) / float(resHeight);

  // Map to source resolution
  int srcX = int(float(texel_pos.x) * scaleX);
  int srcY = int(float(texel_pos.y) * scaleY);

  // God only knows how this indexing works
  int stdIndex = (srcY * internalWidth + srcX);
  int xeIndex = xeFbConvert(internalWidth, stdIndex * 4);

  uint packedColor = pixel_data[xeIndex];
  imageStore(o_texture, texel_pos, uvec4(packedColor, 0, 0, 0));
})glsl";

} // namespace Render
#endif
