// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Renderer.h"

#ifndef NO_GFX
#include "Base/Config.h"
#include "Base/Version.h"
#include "Base/Thread.h"

#include "Core/XGPU/XGPU.h"
#include "Core/XGPU/ShaderConstants.h"
#include "Core/Xe_Main.h"

#include "Render/GUI/GUI.h"

namespace Render {

Renderer::Renderer(RAM *ram, SDL_Window *window) :
  ramPointer(ram),
  width(TILE(Config::rendering.window.width)),
  height(TILE(Config::rendering.window.height)),
  VSYNC(Config::rendering.vsync),
  fullscreen(Config::rendering.isFullscreen),
  mainWindow(window)
{
  SDLInit();
  thread = std::thread(&Renderer::Thread, this);
  thread.detach();
}

void Renderer::SDLInit() {
  MICROPROFILE_SCOPEI("[Xe::Render]", "Start", MP_AUTO);
  // Set if we are in fullscreen mode or not
  SDL_SetWindowFullscreen(mainWindow, fullscreen);
  // Get current window ID
  windowID = SDL_GetWindowID(mainWindow);
}

void Renderer::Create() {
  // Create factories
  BackendStart();
  shaderFactory = resourceFactory->CreateShaderFactory();

  // Init shader handles
  switch (GetBackendID()) {
  case "OpenGL"_j: {
    fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "opengl" };
    computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
      { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
    });
    if (!computeShaderProgram) {
      std::ofstream f{ shaderPath / "fb_deswizzle.comp" };
      f.write(computeShaderSource, sizeof(computeShaderSource));
      f.close();
      computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
        { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
      });
    }
    renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
      { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
      { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
    });
    if (!renderShaderPrograms) {
      std::ofstream vert{ shaderPath / "framebuffer.vert" };
      vert.write(vertexShaderSource, sizeof(vertexShaderSource));
      vert.close();
      std::ofstream frag{ shaderPath / "framebuffer.frag" };
      frag.write(fragmentShaderSource, sizeof(fragmentShaderSource));
      frag.close();
      renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
        { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
        { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
      });
    }
  } break;
  case "Dummy"_j: {
    fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "dummy" };
    computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
      { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
    });
    renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
      { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
      { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
    });
  } break;
  }

  // Create our backbuffer
  backbuffer = resourceFactory->CreateTexture();
  // Init texture
  backbuffer->CreateTextureHandle(width, height, GetBackbufferFlags());

  // Init pixel buffer
  pitch = width * height * sizeof(u32);
  pixels.resize(pitch, COLOR(30, 30, 30, 255)); // Init with dark grey
  pixelSSBO = resourceFactory->CreateBuffer();
  pixelSSBO->CreateBuffer(pixels.size(), pixels.data(), eBufferUsage::DynamicDraw, eBufferType::Storage);
  pixelSSBO->Bind();

  // Create our GUI
  if (Config::rendering.enableGui) {
    gui = resourceFactory->CreateGUI();
    gui->Init(mainWindow, reinterpret_cast<void*>(GetBackendContext()));
    imguiCreated = true;
  }
}

void Renderer::Shutdown() {
  threadRunning = false;
  if (Config::rendering.enableGui) {
    gui->Shutdown();
  }
  BackendShutdown();
  backbuffer->DestroyTexture();
  pixelSSBO->DestroyBuffer();
  shaderFactory->Destroy();
  shaderFactory.reset();
  resourceFactory.reset();
  backbuffer.reset();
  pixelSSBO.reset();
  gui.reset();
  BackendSDLShutdown();
  SDL_DestroyWindow(mainWindow);
  SDL_Quit();
}

void Renderer::Resize(s32 x, s32 y) {
  // Normalize our x and y for tiling
  width = TILE(x);
  height = TILE(y);
  // Resize backend
  BackendResize(x, y);
  // Recreate our texture with the new size
  backbuffer->ResizeTexture(width, height);
  // Set our new pitch
  pitch = width * height * sizeof(u32);
  // Resize our pixel buffer
  pixels.resize(pitch);
  pixelSSBO->UpdateBuffer(0, pixels.size(), pixels.data());
  LOG_DEBUG(Render, "Resized window to {}x{}", width, height);
}

void Renderer::HandleEvents() {
  if (Xe_Main.get()) {
    const SDL_WindowFlags flag = SDL_GetWindowFlags(Xe_Main->mainWindow);
    if (Config::rendering.pauseOnFocusLoss) {
      Xe_Main->renderHalt = flag & SDL_WINDOW_INPUT_FOCUS ? false : true;
    }
  }
  // Process events.
  while (XeRunning && SDL_PollEvent(&windowEvent)) {
    if (Config::rendering.enableGui && imguiCreated) {
      ImGui_ImplSDL3_ProcessEvent(&windowEvent);
    }
    switch (windowEvent.type) {
    case SDL_EVENT_WINDOW_RESIZED:
      if (windowEvent.window.windowID == windowID) {
        LOG_DEBUG(Render, "Resizing window...");
        Resize(windowEvent.window.data1, windowEvent.window.data2);
      }
      break;
    case SDL_EVENT_QUIT:
      if (Config::rendering.quitOnWindowClosure) {
        globalShutdownHandler();
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
  XeShaderFloatConsts &psConsts = state->psConsts;
  XeShaderFloatConsts &vsConsts = state->vsConsts;
  XeShaderBoolConsts &boolConsts = state->boolConsts;

  // Pixel shader constants
  for (u32 begin = static_cast<u32>(XeRegister::SHADER_CONSTANT_256_X), end = static_cast<u32>(XeRegister::SHADER_CONSTANT_511_W),
    idx = begin; idx != end; ++idx) {
    const u32 base = (idx - begin);
    const u64 mask = state->GetDirtyBlock(idx);
    if (mask) {
      f32 *ptr = reinterpret_cast<f32*>(state->GetRegisterPointer(static_cast<XeRegister>(idx)));
      f32 *dest = &psConsts.values[base];
      memcpy(dest, ptr, sizeof(f32));
    }
  }

  // Vertex shader constants
  for (u32 begin = static_cast<u32>(XeRegister::SHADER_CONSTANT_000_X), end = static_cast<u32>(XeRegister::SHADER_CONSTANT_255_W),
    idx = begin; idx != end; ++idx) {
    const u32 base = (idx - begin);
    const u64 mask = state->GetDirtyBlock(idx);
    if (mask) {
      f32 *ptr = reinterpret_cast<f32*>(state->GetRegisterPointer(static_cast<XeRegister>(idx)));
      f32 *dest = &vsConsts.values[base];
      memcpy(dest, ptr, sizeof(f32));
    }
  }

  // Boolean shader constants
  {
    //SHADER_CONSTANT_BOOL_000_031 - SHADER_CONSTANT_BOOL_224_255
    u32 begin = static_cast<u32>(XeRegister::SHADER_CONSTANT_BOOL_000_031);
    const u64 mask = state->GetDirtyBlock(begin);
    if (mask & 0xFF) {
      u32 *ptr = reinterpret_cast<u32 *>(state->GetRegisterPointer(static_cast<XeRegister>(begin)));
      u32 *dest = &boolConsts.values[begin];
      memcpy(dest, ptr, sizeof(u32) * 8);
    }
  }

  Render::BufferLoadJob pixelBufferJob = {
    "PixelConsts"_j,
    std::vector<u8>(
      reinterpret_cast<u8*>(&psConsts),
      reinterpret_cast<u8*>(&psConsts) + sizeof(psConsts)
    ),
    Render::eBufferType::Uniform,
    Render::eBufferUsage::DynamicDraw
  };
  Render::BufferLoadJob vertexBufferJob = {
    "VertexConsts"_j,
    std::vector<u8>(
      reinterpret_cast<u8*>(&vsConsts),
      reinterpret_cast<u8*>(&vsConsts) + sizeof(vsConsts)
    ),
    Render::eBufferType::Uniform,
    Render::eBufferUsage::DynamicDraw
  };
  Render::BufferLoadJob boolBufferJob = {
    "CommonBoolConsts"_j,
    std::vector<u8>(
      reinterpret_cast<u8*>(&boolConsts),
      reinterpret_cast<u8*>(&boolConsts) + sizeof(boolConsts)
    ),
    Render::eBufferType::Uniform,
    Render::eBufferUsage::DynamicDraw
  };

  bufferLoadQueue.push(pixelBufferJob);
  bufferLoadQueue.push(vertexBufferJob);
  bufferLoadQueue.push(boolBufferJob);
}

bool Renderer::IssueCopy(Xe::XGPU::XenosState *state) {
  // Master register
  const u32 copyCtrl = state->copyControl;
  // Which render targets are affected (0-3 = colorRT, 4=depth)
  const u32 copyRT = (copyCtrl & 0x7);
  // Should we clear after copy?
  const bool colorClearEnabled = (copyCtrl >> 8) & 1;
  const bool depthClearEnabled = (copyCtrl >> 9) & 1;
  // Actual copy command
  const Xe::eCopyCommand copyCommand = static_cast<Xe::eCopyCommand>((copyCtrl >> 20) & 3);
  // Target memory and format for the copy operation
  const u32 destInfo = state->copyDestInfo;
  const Xe::eEndianFormat endianFormat = static_cast<Xe::eEndianFormat>(destInfo & 7);
  const u32 destArray = (destInfo >> 3) & 1;
  const u32 destSlice = (destInfo >> 4) & 1;
  const Xe::eColorFormat destformat = static_cast<Xe::eColorFormat>((destInfo >> 7) & 0x3F);
  if (destformat == Xe::eColorFormat::Unknown) {
    LOG_ERROR(Xenos, "[CP] Invalid color format");
    return true;
  }
  const u32 destNumber = (destInfo >> 13) & 7;
  const u32 destBias = (destInfo >> 16) & 0x3F;
  const u32 destSwap = (destInfo >> 25) & 1;

  const u32 destBase = state->copyDestBase;
  const u32 destPitchRaw = state->copyDestPitch;
  const u32 destPitch = destPitchRaw & 0x3FFF;
  const u32 destHeight = (destPitchRaw >> 16) & 0x3FFF;
  // TODO: Actually pull the data
  const u32 copyVertexFetchSlot = 0;
  Xe::VertexFetchData vertexData{};
  vertexData.dword0 = state->ReadRegister(XeRegister::SHADER_CONSTANT_FETCH_00_0);
  vertexData.dword1 = state->ReadRegister(XeRegister::SHADER_CONSTANT_FETCH_00_1);
  u32 address = vertexData.address << 2;
  LOG_DEBUG(Xenos, "[CP] Copy state to EDRAM (address: 0x{:X}, size 0x{:X})", address, vertexData.size);
  // Clear
  if (colorClearEnabled) {
    u8 a = (state->clearColor >> 24) & 0xFF;
    u8 r = (state->clearColor >> 16) & 0xFF;
    u8 g = (state->clearColor >> 8) & 0xFF;
    u8 b = (state->clearColor >> 0) & 0xFF;
    UpdateClearColor(r, g, b, a);
    LOG_DEBUG(Xenos, "[CP] Clear color: {}, {}, {}, {}", r, g, b, a);
  }
  if (depthClearEnabled) {
    const f32 clearDepthValue = (state->depthClear & 0xFFFFFF00) / (f32)0xFFFFFF00;
    LOG_DEBUG(Xenos, "[CP] Clear depth: {}", clearDepthValue);
    UpdateClearDepth(clearDepthValue);
  }
  UpdateConstants(state);
  UpdateViewportFromState(state);
  return true;
}

void Renderer::Thread() {
  // Set thread name
  Base::SetCurrentThreadName("[Xe] Render");
  // Should we render?
  threadRunning = Config::rendering.enable;
  if (!threadRunning) {
    return;
  }

  // Setup SDL handles (thread-specific)
  BackendSDLInit();

  // Create all handles
  Create();

  // Main loop
  while (threadRunning && XeRunning) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::Render]", "Loop", MP_AUTO);

    // Exit early if needed
    if (!threadRunning || !XeRunning)
      break;

    while (!shaderLoadQueue.empty()) {
      ShaderLoadJob job = shaderLoadQueue.front();
      shaderLoadQueue.pop();

      Render::eShaderType shaderType = job.shaderType == Xe::eShaderType::Pixel
        ? Render::eShaderType::Fragment
        : Render::eShaderType::Vertex;

      std::shared_ptr<Shader> compiledShader = shaderFactory->LoadFromBinary(job.name, {
        { shaderType, job.binary }
      });

      convertedShaderPrograms.insert({ job.shaderCRC, compiledShader });
      LOG_DEBUG(Xenos, "Compiled {} shader (CRC: 0x{:08X})", job.name, job.shaderCRC);
    }

    while (!bufferLoadQueue.empty()) {
      BufferLoadJob job = bufferLoadQueue.front();
      bufferLoadQueue.pop();

      std::shared_ptr<Buffer> buffer = resourceFactory->CreateBuffer();
      buffer->CreateBuffer(static_cast<u32>(job.data.size()), job.data.data(), job.usage, job.type);

      createdBuffers[job.name] = buffer;
      LOG_DEBUG(Xenos, "Created buffer '{}', size: {}", job.name, job.data.size());
    }

    // Framebuffer pointer from main memory
    fbPointer = ramPointer->getPointerToAddress(Xe_Main->xenos->GetSurface());

    // Clear the display
    Clear();

    // Upload buffer
    if (fbPointer && Xe_Main.get() && !Xe_Main->renderHalt) {
      if (Xe_Main->xenos->RenderingTo2DFramebuffer()) {
        // Profile
        MICROPROFILE_SCOPEI("[Xe::Render]", "Deswizle", MP_AUTO);
        const u32 *ui_fbPointer = reinterpret_cast<u32*>(fbPointer);
        pixelSSBO->UpdateBuffer(0, pitch, ui_fbPointer);

        // Use the compute shader
        computeShaderProgram->Bind();
        pixelSSBO->Bind();
        if (Xe_Main.get()) {
          computeShaderProgram->SetUniformInt("internalWidth", Xe_Main->xenos->GetWidth());
          computeShaderProgram->SetUniformInt("internalHeight", Xe_Main->xenos->GetHeight());
          computeShaderProgram->SetUniformInt("resWidth", width);
          computeShaderProgram->SetUniformInt("resHeight", height);
        }
        OnCompute();
      }
    }

    // Render the texture
    if (Xe_Main.get() && !Xe_Main->renderHalt && Xe_Main->xenos->RenderingTo2DFramebuffer()) {
      MICROPROFILE_SCOPEI("[Xe::Render]", "BindTexture", MP_AUTO);
      renderShaderPrograms->Bind();
      backbuffer->Bind();
      OnBind();
    }

    // Render the GUI
    if (Config::rendering.enableGui && gui.get() && Xe_Main.get() && !Xe_Main->renderHalt) {
      MICROPROFILE_SCOPEI("[Xe::Render::GUI]", "Render", MP_AUTO);
      gui->Render(backbuffer.get());
    }

    // GL Swap
    if (Xe_Main.get()) {
      MICROPROFILE_SCOPEI("[Xe::Render]", "Swap", MP_AUTO);
      OnSwap(mainWindow);
    }
  }
}

bool Renderer::DebuggerActive() {
  if (!gui.get())
    return false;
  for (bool &a : gui.get()->ppcDebuggerActive) {
    if (a) {
      return true;
    }
  }
  return false;
}

void Renderer::SetDebuggerActive(s8 specificPPU) {
  if (gui.get()) {
    if (specificPPU != -1) {
      for (bool &a : gui.get()->ppcDebuggerActive) {
        a = true;
      }
    } else if (specificPPU <= 3) {
      gui.get()->ppcDebuggerActive[specificPPU - 1] = true;
    }
  }
}

} // namespace Render
#endif
