// Copyright 2025 Xenon Emulator Project

#include "Renderer.h"

#ifndef NO_GFX
#include "Base/Config.h"
#include "Base/Version.h"
#include "Base/Thread.h"

#include "Core/XGPU/XGPU.h"
#include "Core/Xe_Main.h"

namespace Render {

Renderer::Renderer(RAM *ram) :
  ramPointer(ram),
  width(TILE(Config::rendering.window.width)),
  height(TILE(Config::rendering.window.height)),
  VSYNC(Config::rendering.vsync),
  fullscreen(Config::rendering.isFullscreen)
{
  thread = std::thread(&Renderer::Thread, this);
  thread.detach();
}

void Renderer::Start() {
  MICROPROFILE_SCOPEI("[Xe::Render]", "Start", MP_AUTO);
  // Init SDL Events, Video, Joystick, and Gamepad
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    LOG_ERROR(System, "Failed to initialize SDL: {}", SDL_GetError());
  }

  // SDL3 window properties.
  SDL_PropertiesID props = SDL_CreateProperties();
  const std::string title = fmt::format("Xenon {}", Base::Version);
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
  // Set starting X and Y position to be centered
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
  // Set width and height
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
  // Allow resizing
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  // Enable HiDPI
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  BackendSDLProperties(props);
  // Create window
  mainWindow = SDL_CreateWindowWithProperties(props);
  // Destroy (no longer used) properties
  SDL_DestroyProperties(props);
  // Set min size
  SDL_SetWindowMinimumSize(mainWindow, 640, 480);
  BackendSDLInit();
  // Set if we are in fullscreen mode or not
  SDL_SetWindowFullscreen(mainWindow, fullscreen);
  // Get current window ID
  windowID = SDL_GetWindowID(mainWindow);

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

void Renderer::Thread() {
  // Set thread name
  Base::SetCurrentThreadName("[Xe] Render");
  // Should we render?
  threadRunning = Config::rendering.enable;
  if (!threadRunning) {
    return;
  }

  // Start exec
  Start();

  // Main loop
  while (threadRunning && XeRunning) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::Render]", "Loop", MP_AUTO);
    // Process events.
    while (SDL_PollEvent(&windowEvent)) {
      if (Config::rendering.enableGui) {
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
    // Exit early if needed
    if (!threadRunning || !XeRunning)
      break;
    const SDL_WindowFlags flag = SDL_GetWindowFlags(mainWindow);
    bool inFocus = flag & SDL_WINDOW_INPUT_FOCUS;
    if (!Config::rendering.pauseOnFocusLoss) {
      inFocus = true;
    }

    // Framebuffer pointer from main memory
    fbPointer = ramPointer->getPointerToAddress(Xe_Main->xenos->fbSurfaceAddress);

    // Upload buffer
    if (fbPointer && Xe_Main.get() && !Xe_Main->renderHalt && inFocus) {
      // Profile
      MICROPROFILE_SCOPEI("[Xe::Render]", "Deswizle", MP_AUTO);
      const u32* ui_fbPointer = reinterpret_cast<u32*>(fbPointer);
      pixelSSBO->UpdateBuffer(0, pitch, ui_fbPointer);

      // Use the compute shader
      computeShaderProgram->Bind();
      pixelSSBO->Bind();
      computeShaderProgram->SetUniformInt("internalWidth", Xe_Main->xenos->internalWidth);
      computeShaderProgram->SetUniformInt("internalHeight", Xe_Main->xenos->internalHeight);
      computeShaderProgram->SetUniformInt("resWidth", width);
      computeShaderProgram->SetUniformInt("resHeight", height);
      OnCompute();
    }

    // Render the texture
    if (inFocus) {
      MICROPROFILE_SCOPEI("[Xe::Render]", "BindTexture", MP_AUTO);
      renderShaderPrograms->Bind();
      backbuffer->Bind();
      OnBind();
    }

    // Render the GUI
    if (Config::rendering.enableGui && gui.get() && Xe_Main.get() && !Xe_Main->renderHalt && inFocus) {
      MICROPROFILE_SCOPEI("[Xe::Render::GUI]", "Render", MP_AUTO);
      gui->Render(backbuffer.get());
    }

    // GL Swap
    if (Xe_Main.get() && !Xe_Main->renderHalt) {
      MICROPROFILE_SCOPEI("[Xe::Render]", "Swap", MP_AUTO);
      OnSwap(mainWindow);
    }
  }
}

} // namespace Render
#endif