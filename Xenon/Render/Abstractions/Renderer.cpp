// Copyright 2025 Xenon Emulator Project

#include "Renderer.h"

#ifndef NO_GFX
#include "Base/Config.h"
#include "Base/Version.h"
#include "Base/Thread.h"

#include "Core/XGPU/XGPU.h"
#include "Core/Xe_Main.h"

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

  // TODO(Vali0004): Pull shaders from a file, as right now, this will only pull the OpenGL Shaders
  // Init shader handles
  computeShaderProgram = shaderFactory->LoadFromSource("XeFbConvert", {
    { eShaderType::Compute, computeShaderSource }
  });
  renderShaderPrograms = shaderFactory->LoadFromSource("Render", {
    { eShaderType::Vertex, vertexShaderSource },
    { eShaderType::Fragment, fragmentShaderSource }
  });

  // Create our backbuffer
  backbuffer = resourceFactory->CreateTexture();
  // Init GL texture
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
  const SDL_WindowFlags flag = SDL_GetWindowFlags(Xe_Main->mainWindow);
  if (!Config::rendering.pauseOnFocusLoss) {
    Xe_Main->renderHalt = flag & SDL_WINDOW_INPUT_FOCUS ? false : true;
  }
  // Process events.
  while (SDL_PollEvent(&windowEvent)) {
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
        LOG_INFO(Render, "Attempting to soft shutdown...");
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

    // Framebuffer pointer from main memory
    fbPointer = ramPointer->getPointerToAddress(Xe_Main->xenos->GetSurface());

    // Upload buffer
    if (fbPointer && Xe_Main.get() && !Xe_Main->renderHalt) {
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

    // Render the texture
    if (!Xe_Main->renderHalt) {
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

} // namespace Render
#endif
