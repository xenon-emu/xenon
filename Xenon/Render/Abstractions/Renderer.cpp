/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Renderer.h"

#ifndef NO_GFX
#include "Base/Config.h"
#include "Base/Version.h"
#include "Base/Thread.h"

#include "Core/XGPU/XGPU.h"
#include "Core/XGPU/ShaderConstants.h"
#include "Core/XeMain.h"

#include <glad/glad.h>
#ifndef TOOL
#include "Render/GUI/GUI.h"
#endif

namespace Render {

Renderer::Renderer() :
  width(TILE(Config::rendering.window.width)),
  height(TILE(Config::rendering.window.height)),
  VSYNC(Config::rendering.vsync),
  fullscreen(Config::rendering.isFullscreen)
{}

void Renderer::SDLInit() {
  // Init SDL Events, Video, Joystick, and Gamepad
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    LOG_ERROR(Xenon, "Failed to initialize SDL: {}", SDL_GetError());
  }

  // SDL3 window properties.
  SDL_PropertiesID props = SDL_CreateProperties();
  const std::string title = FMT("Xenon {}", Base::Version);
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
  // Set starting X and Y position to be centered
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
  // Set width and height
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, Config::rendering.window.width);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, Config::rendering.window.height);
  // Allow resizing
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  // Enable HiDPI
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  BackendSDLProperties(props);
  // Create window
  mainWindow = SDL_CreateWindowWithProperties(props);

  // Set min size
  SDL_SetWindowMinimumSize(mainWindow, 640, 480);

  if (!mainWindow) {
    LOG_ERROR(Render, "Failed to create window: {}", SDL_GetError());
  }

  // Destroy (no longer used) properties  
  SDL_DestroyProperties(props);
  // Set if we are in fullscreen mode or not
  SDL_SetWindowFullscreen(mainWindow, fullscreen);
  // Get current window ID
  windowID = SDL_GetWindowID(mainWindow);
}

void Renderer::Start(RAM *ram) {
  ramPointer = ram;
  // Should we render?
  threadRunning = Config::rendering.enable && XeRunning;
  if (threadRunning) {
    SDLInit();
    thread = std::thread(&Renderer::Thread, this);
    thread.detach();
  }
}

void Renderer::CreateHandles() {
  // Create factories
  BackendStart();

  // Create our backbuffer
  backbuffer = resourceFactory->CreateTexture();
  // Init texture
  backbuffer->CreateTextureHandle(width, height, GetBackbufferFlags());

  // Init pixel buffer
  pixels.resize(width * height, COLOR(30, 30, 30, 255)); // Init with dark grey
  pitch = pixels.size() * sizeof(u32);
  pixelSSBO = resourceFactory->CreateBuffer();
  pixelSSBO->CreateBuffer(pitch, pixels.data(), eBufferUsage::DynamicDraw, eBufferType::Storage);
  pixelSSBO->Bind();
  BackendBindPixelBuffer(pixelSSBO.get());

  // Create our GUI
  gui = resourceFactory->CreateGUI();
  gui->Init(mainWindow, reinterpret_cast<void *>(GetBackendContext()));
}

void Renderer::Shutdown() {
  if (gui)
    gui->Shutdown();
  backbuffer->DestroyTexture();
  pixelSSBO->DestroyBuffer();
  shaderFactory->Destroy();
  shaderFactory.reset();
  resourceFactory.reset();
  backbuffer.reset();
  pixelSSBO.reset();
  gui.reset();
  BackendShutdown();
  BackendSDLShutdown();
  SDL_DestroyWindow(mainWindow);
  SDL_Quit();
}

void Renderer::Resize(u32 x, u32 y) {
  // Normalize our x and y for tiling
  u32 newWidth = TILE(x);
  u32 newHeight = TILE(y);
  // Save old size
  u32 oldWidth = width;
  u32 oldHeight = height;

  // Just don't do anything, no need to resize if it's the same
  if (newWidth == oldWidth && newHeight == oldHeight)
    return;

  std::vector<u32> oldPixels = std::move(pixels); // Move to avoid copy
  // Resize backend
  BackendResize(x, y);
  // Recreate our texture with the new size
  backbuffer->ResizeTexture(newWidth, newHeight);
  // Allocate new pixel buffer, initialized to grey
  pixels.resize(newWidth * newHeight, COLOR(205, 205, 205, 205));
  // Copy old pixels into the new buffer at (0, 0)
  for (u32 row = 0; row < std::min(oldHeight, newHeight); ++row) {
    std::memcpy(
      &pixels[row * newWidth],
      &oldPixels[row * oldWidth],
      std::min(oldWidth, newWidth) * sizeof(u32)
    );
  }
  // Update size
  width = newWidth;
  height = newHeight;
  // Update pitch
  pitch = width * height * sizeof(u32);
  // Update buffer
  pixelSSBO->UpdateBuffer(0, pitch, pixels.data());
  BackendBindPixelBuffer(pixelSSBO.get());
  LOG_DEBUG(Render, "Resized window to {}x{}", width, height);
}

void Renderer::HandleEvents() {
  const SDL_WindowFlags flag = SDL_GetWindowFlags(mainWindow);
  if (Config::rendering.pauseOnFocusLoss) {
    focusLost = flag & SDL_WINDOW_INPUT_FOCUS ? false : true;
  }
  // Process events.
  while (threadRunning && SDL_PollEvent(&windowEvent)) {
    ImGui_ImplSDL3_ProcessEvent(&windowEvent);
    switch (windowEvent.type) {
    case SDL_EVENT_WINDOW_RESIZED:
      if (windowEvent.window.windowID == windowID) {
        LOG_DEBUG(Render, "Resizing window...");
        Resize(windowEvent.window.data1, windowEvent.window.data2);
      }
      break;
    case SDL_EVENT_QUIT:
      if (Config::rendering.quitOnWindowClosure) {
        XeRunning = false;
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (windowEvent.key.key == SDLK_F11) {
        SDL_WindowFlags flag = SDL_GetWindowFlags(mainWindow);
        bool fullscreenMode = flag & SDL_WINDOW_FULLSCREEN;
        SDL_SetWindowFullscreen(mainWindow, !fullscreenMode);
      }
      break;
    default:
      break;
    }
  }
}

void Renderer::UpdateConstants(Xe::XGPU::XenosState *state) {
  XeShaderFloatConsts &floatConsts = state->floatConsts;
  XeShaderBoolConsts &boolConsts = state->boolConsts;

  // Vertex shader constants
  constexpr u64 FLOAT_CONST_WORDS = sizeof(floatConsts.values) / sizeof(floatConsts.values[0]);
  u32 *regPtr = reinterpret_cast<u32 *>(state->GetRegisterPointer(XeRegister::SHADER_CONSTANT_000_X));
  for (u64 i = 0; i != FLOAT_CONST_WORDS; ++i) {
    u32 word = regPtr[i];
    f32 f;
    ::memcpy(&f, &word, sizeof(word));
    floatConsts.values[i] = f;
  }

  // Boolean shader constants
  {
    // SHADER_CONSTANT_BOOL_000_031 - SHADER_CONSTANT_BOOL_224_255
    u32 begin = static_cast<u32>(XeRegister::SHADER_CONSTANT_BOOL_000_031);
    const u64 mask = state->GetDirtyBlock(begin);
    if (mask & 0xFF) {
      u32 *ptr = reinterpret_cast<u32 *>(state->GetRegisterPointer(static_cast<XeRegister>(begin)));
      u32 *dest = &boolConsts.values[begin];
      std::memcpy(dest, ptr, sizeof(u32) * 8);
    }
  }

  // Upload float constants
  {
    RenderCommand cmd{};
    cmd.type = RenderCommandType::UploadBuffer;
    cmd.payload = RenderCommand::UploadBufferCmd{
      "FloatConsts"_j,
      std::vector<u8>(
        reinterpret_cast<u8*>(floatConsts.values),
        reinterpret_cast<u8*>(floatConsts.values) + sizeof(floatConsts.values)
      ),
      eBufferType::Storage,
      eBufferUsage::DynamicDraw
    };

    std::lock_guard<Base::FutexMutex> lock(renderQueueMutex);
    renderQueue.push(std::move(cmd));
  }

  // Upload bool constants
  {
    RenderCommand cmd{};
    cmd.type = RenderCommandType::UploadBuffer;
    cmd.payload = RenderCommand::UploadBufferCmd{
      "CommonBoolConsts"_j,
      std::vector<u8>(
        reinterpret_cast<u8*>(boolConsts.values),
        reinterpret_cast<u8*>(boolConsts.values) + sizeof(boolConsts.values)
      ),
      eBufferType::Storage,
      eBufferUsage::DynamicDraw
    };

    std::lock_guard<Base::FutexMutex> lock(renderQueueMutex);
    renderQueue.push(std::move(cmd));
  }
}

bool Renderer::IssueCopy(Xe::XGPU::XenosState *state) {
  // Which render targets are affected (0-3 = colorRT, 4=depth)
  const u32 copyRT = state->copyControl.copySrcSelect;
  // Should we clear after copy?
  const bool colorClearEnabled = state->copyControl.colorClearEnable;
  const bool depthClearEnabled = state->copyControl.depthClearEnable;
  // Actual copy command
  const eCopyCommand copyCommand = state->copyControl.copyCommand;

  // Target memory and format for the copy operation
  const eEndian128 endianFormat = state->copyDestInfo.copyDestEndian;
  const u32 destArray = state->copyDestInfo.copyDestArray;
  const u32 destSlice = state->copyDestInfo.copyDestSlice;
  const eColorFormat destFormat = state->copyDestInfo.copyDestFormat;
  const eSurfaceNumberFormat destNumber = state->copyDestInfo.copyDestNumber;
  const u32 destBias = state->copyDestInfo.copyDestExpBias;
  const u32 destSwap = state->copyDestInfo.copyDestSwap;
  const u32 destBase = state->copyDestBase;

  const u32 destPitch = state->copyDestPitch.copyDestPitch;
  const u32 destHeight = state->copyDestPitch.copyDestHeight;

  Xe::XGPU::XeShader *shader = activeShader;
  if (shader && shader->vertexShader) {
    for (const auto *fetch : shader->vertexShader->vertexFetches) {
      u32 fetchSlot = fetch->fetchSlot;
      u32 regBase = static_cast<u32>(XeRegister::SHADER_CONSTANT_FETCH_00_0) + fetchSlot * 2;

      Xe::VertexFetchConstant fetchData{};
      fetchData.rawHex[0] = byteswap_be<u32>(state->ReadRegister(static_cast<XeRegister>(regBase + 0)));
      fetchData.rawHex[1] = byteswap_be<u32>(state->ReadRegister(static_cast<XeRegister>(regBase + 1)));

      if (fetchData.Size == 0 || fetchData.BaseAddress == 0)
        continue;

      u32 byteAddress = fetchData.BaseAddress << 2;
      u32 byteSize = fetchData.Size << 2;

      u8 *data = ramPointer->GetPointerToAddress(byteAddress);
      if (!data) {
        LOG_WARNING(Xenos, "VertexFetch: Invalid memory for slot {} (addr=0x{:X})", fetchSlot, byteAddress);
        continue;
      }

      const u64 wordCount = fetchData.Size;
      std::vector<u32> rawWords(wordCount);
      memcpy(rawWords.data(), data, wordCount * sizeof(u32));

      std::vector<u8> uploadBytes(wordCount * 4);
      for (u64 i = 0; i != wordCount; ++i) {
        u32 w = rawWords[i];
        ::memcpy(&uploadBytes[i * 4], &w, 4);
      }

      RenderCommand cmd{};
      cmd.type = RenderCommandType::UploadBuffer;
      cmd.payload = RenderCommand::UploadBufferCmd{
        "VertexFetch"_j,          // or some hash based on slot/address if you want reuse
        std::move(uploadBytes),
        eBufferType::Vertex,
        eBufferUsage::StaticDraw
      };

      {
        std::lock_guard<Base::FutexMutex> lock(renderQueueMutex);
        renderQueue.push(std::move(cmd));
      }
      LOG_INFO(Xenos, "Uploaded vertex fetch buffer: slot={}, addr=0x{:X}, size={} bytes", fetchSlot, byteAddress, byteSize);
    }
  }
  // Clear
  if (colorClearEnabled) {
    u8 a = (state->clearColor >> 24) & 0xFF;
    u8 g = (state->clearColor >> 16) & 0xFF;
    u8 b = (state->clearColor >> 8) & 0xFF;
    u8 r = (state->clearColor >> 0) & 0xFF;
    UpdateClearColor(r, g, b, a);
#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "[CP] Clear color: {}, {}, {}, {}", r, g, b, a);
#endif
  }
  if (depthClearEnabled) {
    const f32 clearDepthValue = (state->depthClear & 0xFFFFFF00) / (f32)0xFFFFFF00;
#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "[CP] Clear depth: {}", clearDepthValue);
#endif
    UpdateClearDepth(clearDepthValue);
  }
  UpdateConstants(state);
  UpdateViewportFromState(state);
  return true;
}

Xe::XGPU::XeShader *Renderer::GetOrCreateShader(u32 vsHash, u32 psHash) {
  if (!vsHash || !psHash)
    return nullptr;

  const u64 combinedHash = (static_cast<u64>(vsHash) << 32) | psHash;

  // Fast path: already linked
  if (auto it = linkedShaderPrograms.find(combinedHash); it != linkedShaderPrograms.end()) {
    return &it->second;
  }

  // Slow path: need to link, protect shared maps
  std::lock_guard<Base::FutexMutex> lock(programLinkMutex);

  // Double-check now that we hold the lock
  if (auto it = linkedShaderPrograms.find(combinedHash); it != linkedShaderPrograms.end()) {
    return &it->second;
  }

  auto vsIt = pendingVertexShaders.find(vsHash);
  auto psIt = pendingPixelShaders.find(psHash);

  if (vsIt == pendingVertexShaders.end() || psIt == pendingPixelShaders.end()) {
    // Shader(s) not ready yet
    return nullptr;
  }

  // This is basically the guts of TryLinkShaderPair, but localized
  std::shared_ptr<Shader> shader = shaderFactory->LoadFromBinary(
    FMT("VS{:08X}_PS{:08X}", vsHash, psHash),
    {
      { Render::eShaderType::Vertex,   vsIt->second.second },
      { Render::eShaderType::Fragment, psIt->second.second }
    }
  );

  if (!shader) {
    LOG_ERROR(Xenos, "Failed to link shader program 0x{:08X}_0x{:08X}", vsHash, psHash);
    return nullptr;
  }

  Xe::XGPU::XeShader xeShader{};
  xeShader.program = std::move(shader);
  xeShader.pixelShader = psIt->second.first;
  xeShader.pixelShaderHash = psIt->first;
  xeShader.vertexShader = vsIt->second.first;
  xeShader.vertexShaderHash = vsIt->first;

  // Create texture handles
  for (u64 i = 0; i != xeShader.pixelShader->usedTextures.size(); ++i)
    xeShader.textures.push_back(resourceFactory->CreateTexture());
  for (u64 i = 0; i != xeShader.vertexShader->usedTextures.size(); ++i)
    xeShader.textures.push_back(resourceFactory->CreateTexture());
  for (auto &texture : xeShader.textures)
    texture->CreateTextureHandle(width, height, GetXenosFlags());

  // Vertex attrib setup
  if (xeShader.vertexShader) {
    OnBind(); // bind VAO / context-specific stuff

    for (const auto &[fetch_key, location] : xeShader.vertexShader->attributeLocationMap) {
      const Xe::Microcode::AST::VertexFetch *fetch = nullptr;
      for (const auto *f : xeShader.vertexShader->vertexFetches) {
        if (f->fetchSlot   == fetch_key.slot &&
            f->fetchOffset == fetch_key.offset &&
            f->fetchStride == fetch_key.stride) {
          fetch = f;
          break;
        }
      }
      if (!fetch)
        continue;

      u32 fetchSlot = fetch->fetchSlot;
      u32 regBase   = static_cast<u32>(XeRegister::SHADER_CONSTANT_FETCH_00_0) + fetchSlot * 2;

      Xe::ShaderConstantFetch fetchData{};
      for (u32 i = 0; i != 6; ++i)
        fetchData.rawHex[i] = byteswap_be<u32>(
          XeMain::xenos->xenosState->ReadRegister(static_cast<XeRegister>(regBase + i))
        );

      if (fetchData.Vertex[0].Type == Xe::eConstType::Texture) {
        // Vertex shader texture fetches skipped for now
        continue;
      } else if (fetchData.Vertex[0].Type == Xe::eConstType::Vertex) {
        u32 fetchAddress = fetchData.Vertex[0].BaseAddress << 2;
        u32 fetchSize    = fetchData.Vertex[0].Size        << 2;

        u8 *data = ramPointer->GetPointerToAddress(fetchAddress);
        if (!data) {
          LOG_WARNING(Xenos, "VertexFetch: Invalid memory for slot {} (addr=0x{:X})",
                      fetchSlot, fetchAddress);
          continue;
        }

        const u64 wordCount = fetchSize / 4;
        std::vector<u32> rawWords(wordCount);
        memcpy(rawWords.data(), data, wordCount * sizeof(u32));

        std::vector<f32> dataVec;
        dataVec.resize(wordCount);
        for (u64 i = 0; i != wordCount; ++i) {
          dataVec[i] = std::bit_cast<f32, u32>(rawWords[i]);
        }

        u64 bufferKey = (static_cast<u64>(fetchAddress) << 32) | fetchSize;

        std::shared_ptr<Buffer> buffer = nullptr;
        auto it = createdBuffers.find(bufferKey);
        if (it != createdBuffers.end()) {
          buffer = it->second;
          if (buffer->GetSize() < fetchSize) {
            buffer->DestroyBuffer();
            buffer->CreateBuffer(static_cast<u32>(fetchSize), dataVec.data(),
                                 Render::eBufferUsage::StaticDraw, Render::eBufferType::Vertex);
          } else {
            buffer->UpdateBuffer(0, static_cast<u32>(fetchSize), dataVec.data());
          }
        } else {
          buffer = resourceFactory->CreateBuffer();
          buffer->CreateBuffer(static_cast<u32>(fetchSize), dataVec.data(),
                               Render::eBufferUsage::StaticDraw, Render::eBufferType::Vertex);
          createdBuffers.insert({ bufferKey, buffer });
        }

        // Bind the buffer to the current VAO
        buffer->Bind();

        const u32 components = fetch->GetComponentCount();
        const u32 offset = fetch->fetchOffset * 4;
        const u32 stride = fetch->fetchStride * 4;
        VertexFetch(location, components, fetch->isFloat, fetch->isNormalized, offset, stride);
      }
    }
  }

  auto [it, inserted] =
    linkedShaderPrograms.emplace(combinedHash, std::move(xeShader));

  return &it->second;
}

void Renderer::Thread() {
  // Set thread name
  Base::SetCurrentThreadName("[Xe] Render");

  // Setup SDL handles (thread-specific)
  BackendSDLInit();

  // Create all handles
  CreateHandles();

  // Main loop
  while (threadRunning) {
    threadRunning = Config::rendering.enable && XeRunning;
    // Exit early if needed
    if (!threadRunning)
      break;

    // Clear the display
    if (XeMain::xenos)
      Clear();

    if (XeMain::xenos && XeMain::xenos->RenderingTo2DFramebuffer()) {
      if (ramPointer) {
        fbPointer = ramPointer->GetPointerToAddress(XeMain::xenos->GetSurface());
        const u32 *ui_fbPointer = reinterpret_cast<const u32 *>(fbPointer);
        pixelSSBO->UpdateBuffer(0, pitch, ui_fbPointer);

        if (computeShaderProgram.get()) {
          computeShaderProgram->Bind();
          pixelSSBO->Bind();

          if (XeMain::xenos) {
            internalWidth = XeMain::xenos->GetWidth();
            internalHeight = XeMain::xenos->GetHeight();
          }
          computeShaderProgram->SetUniformInt("internalWidth", internalWidth);
          computeShaderProgram->SetUniformInt("internalHeight", internalHeight);
          computeShaderProgram->SetUniformInt("resWidth", width);
          computeShaderProgram->SetUniformInt("resHeight", height);
          OnCompute();
        }
        if (renderShaderPrograms.get()) {
          renderShaderPrograms->Bind();
          backbuffer->Bind();
          OnBind();
          backbuffer->Unbind();
          renderShaderPrograms->Unbind();
        }
      }
    }

    std::vector<RenderCommand> frameCommands;
    {
      std::lock_guard<Base::FutexMutex> lock(renderQueueMutex);
      while (!renderQueue.empty()) {
        frameCommands.push_back(std::move(renderQueue.front()));
        renderQueue.pop();
      }
    }

    for (auto &cmd : frameCommands) {
      switch (cmd.type) {
        case RenderCommandType::BindShader: {
          auto &c = std::get<RenderCommand::BindShaderCmd>(cmd.payload);
          activeShader = GetOrCreateShader(c.vsHash, c.psHash);
          break;
        }

        case RenderCommandType::UploadBuffer: {
          auto &c = std::get<RenderCommand::UploadBufferCmd>(cmd.payload);

          auto &buffer = createdBuffers[c.bufferHash];
          if (!buffer) {
            buffer = resourceFactory->CreateBuffer();
            buffer->CreateBuffer(c.data.size(), c.data.data(), c.usage, c.type);
          } else {
            buffer->UpdateBuffer(0, c.data.size(), c.data.data());
          }
          break;
        }

        case RenderCommandType::CopyResolve: {
          auto &c = std::get<RenderCommand::CopyResolveCmd>(cmd.payload);
          IssueCopy(c.state);
          break;
        }

        case RenderCommandType::ClearColor: {
          auto &c = std::get<RenderCommand::ClearColorCmd>(cmd.payload);
          UpdateClearColor(c.r, c.g, c.b, c.a);
          break;
        }

        case RenderCommandType::ClearDepth: {
          auto &c = std::get<RenderCommand::ClearDepthCmd>(cmd.payload);
          UpdateClearDepth(c.depth);
          break;
        }

        case RenderCommandType::Draw: {
          auto &c = std::get<RenderCommand::DrawCmd>(cmd.payload);
          if (activeShader && activeShader->program)
            Draw(*activeShader, c.params);
          break;
        }

        case RenderCommandType::DrawIndexed: {
          auto &c = std::get<RenderCommand::DrawIndexedCmd>(cmd.payload);
          if (activeShader && activeShader->program)
            DrawIndexed(*activeShader, c.params, c.indexInfo);
          break;
        }

        default: {
          // Do nothing for other cases right now.
          break;
        }
      }
    }

    if (waiting) {
      waiting = false;
      if (waitTime >= 0x100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(waitTime / 0x100));
      } else {
        std::this_thread::yield();
      }
    }

    // Render the GUI
    if (gui.get() && !focusLost) {
      gui->Render(backbuffer.get());
    }

    // Swap
    swapCount.fetch_add(1);
    OnSwap(mainWindow);
  }
}

bool Renderer::DebuggerActive() {
  for (s32 i = gui.get() ? 2 : 0; i; --i) {
    if (gui->ppcDebuggerActive[i])
      return true;
  }
  return false;
}

void Renderer::SetDebuggerActive(s8 specificPPU) {
  if (specificPPU != -1) {
    for (s32 i = gui.get() ? 2 : 0; i; --i) {
      gui->ppcDebuggerActive[i] = true;
    }
  }

  if (gui && (specificPPU <= 3 && specificPPU > 0)) {
    gui->ppcDebuggerActive[specificPPU - 1] = true;
  }
}

} // namespace Render
#endif
