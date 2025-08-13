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
  virtual void VertexFetch(const u32 location, const u32 components, bool isFloat, bool isNormalized, const u32 fetchOffset, const u32 fetchStride) = 0;
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
  std::vector<DrawJob> previousJobs{};
  std::queue<DrawJob> drawQueue{};

  // Shader load queue
  std::mutex bufferQueueMutex{};
  std::queue<BufferLoadJob> bufferLoadQueue{};
  std::unordered_map<u64, std::shared_ptr<Buffer>> createdBuffers{};

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
  std::atomic<bool> readyForFrame = false;
  std::mutex copyQueueMutex{};
  std::queue<Xe::XGPU::XenosState*> copyQueue{};
  // Frame wait
  std::atomic<bool> waiting = false;
  std::atomic<u32> waitTime = 0;
  // Internal swap counter
  std::atomic<u32> swapCount;
  std::mutex frameReadyMutex;
  std::condition_variable frameReadyCondVar;
  bool frameReady = false;

  // Shaders
  std::shared_ptr<Shader> computeShaderProgram{};
  std::shared_ptr<Shader> renderShaderPrograms{};
private:
  // Thread handle
  std::thread thread;

  // GUI handle
  std::unique_ptr<GUI> gui{};

  // Pixel buffer
  std::unique_ptr<Buffer> pixelSSBO{};
  std::vector<u32> pixels{};
};

} // namespace Render
#endif
