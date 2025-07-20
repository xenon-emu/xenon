// Copyright 2025 Xenon Emulator Project. All rights reserved.

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

Renderer::Renderer(RAM *ram) :
  ramPointer(ram),
  width(TILE(Config::rendering.window.width)),
  height(TILE(Config::rendering.window.height)),
  VSYNC(Config::rendering.vsync),
  fullscreen(Config::rendering.isFullscreen)
{}

void Renderer::SDLInit() {
  MICROPROFILE_SCOPEI("[Xe::Render]", "SDLInit", MP_AUTO);
  // Init SDL Events, Video, Joystick, and Gamepad
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    LOG_ERROR(Xenon, "Failed to initialize SDL: {}", SDL_GetError());
  }

  // SDL3 window properties.
  SDL_PropertiesID props = SDL_CreateProperties();
  const std::string title = fmt::format("Xenon {}", Base::Version);
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
  SDL_SetNumberProperty(props, "flags", SDL_WINDOW_OPENGL);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
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

void Renderer::Start() {
  MICROPROFILE_SCOPEI("[Xe::Render]", "Start", MP_AUTO);
  // Create thread and SDL
  SDLInit();
  thread = std::thread(&Renderer::Thread, this);
  thread.detach();
}

void Renderer::CreateHandles() {
  MICROPROFILE_SCOPEI("[Xe::Render]", "Create", MP_AUTO);
  // Create factories
  BackendStart();
  shaderFactory = resourceFactory->CreateShaderFactory();

  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) };
  // Init shader handles
  switch (GetBackendID()) {
  case "GLES"_jLower:
  case "OpenGL"_jLower: {
    bool gles = GetBackendID() == "GLES"_jLower;
    std::string versionString = FMT("#version {} {}\n", gles ? 310 : 430, gles ? "es" : "compatibility");
    shaderPath /= "opengl";
    computeShaderProgram = shaderFactory->LoadFromFiles("XeFbConvert", {
      { eShaderType::Compute, shaderPath / "fb_deswizzle.comp" }
    });
    if (!computeShaderProgram) {
      std::ofstream f{ shaderPath / "fb_deswizzle.comp" };
      f.write(versionString.data(), versionString.size());
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
      vert.write(versionString.data(), versionString.size());
      vert.write(vertexShaderSource, sizeof(vertexShaderSource));
      vert.close();
      std::ofstream frag{ shaderPath / "framebuffer.frag" };
      frag.write(versionString.data(), versionString.size());
      frag.write(fragmentShaderSource, sizeof(fragmentShaderSource));
      frag.close();
      renderShaderPrograms = shaderFactory->LoadFromFiles("Render", {
        { eShaderType::Vertex, shaderPath / "framebuffer.vert" },
        { eShaderType::Fragment, shaderPath / "framebuffer.frag" }
      });
    }
  } break;
  case "Dummy"_jLower: {
    shaderPath /= "dummy";
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
  if (Config::rendering.enableGui) {
    gui->Shutdown();
  }
  threadRunning = false;
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
  pixelSSBO->UpdateBuffer(0, pixels.size(), pixels.data());
  LOG_DEBUG(Render, "Resized window to {}x{}", width, height);
}

void Renderer::HandleEvents() {
  const SDL_WindowFlags flag = SDL_GetWindowFlags(mainWindow);
  if (Config::rendering.pauseOnFocusLoss) {
    focusLost = flag & SDL_WINDOW_INPUT_FOCUS ? false : true;
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
  f32 *ptr = reinterpret_cast<f32*>(state->GetRegisterPointer(XeRegister::SHADER_CONSTANT_000_X));
  memcpy(floatConsts.values, ptr, sizeof(floatConsts.values));

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

  Render::BufferLoadJob floatBufferJob = {
    "FloatConsts",
    std::vector<u8>(
      reinterpret_cast<u8*>(floatConsts.values),
      reinterpret_cast<u8*>(floatConsts.values) + sizeof(floatConsts.values)
    ),
    Render::eBufferType::Storage,
    Render::eBufferUsage::DynamicDraw
  };
  Render::BufferLoadJob boolBufferJob = {
    "CommonBoolConsts",
    std::vector<u8>(
      reinterpret_cast<u8*>(boolConsts.values),
      reinterpret_cast<u8*>(boolConsts.values) + sizeof(boolConsts.values)
    ),
    Render::eBufferType::Storage,
    Render::eBufferUsage::DynamicDraw
  };

  {
    std::lock_guard<std::mutex> lock(bufferQueueMutex);
    bufferLoadQueue.push(floatBufferJob);
  }
  {
    std::lock_guard<std::mutex> lock(bufferQueueMutex);
    bufferLoadQueue.push(boolBufferJob);
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

  u64 combinedHash = (static_cast<u64>(currentVertexShader.load()) << 32) | currentPixelShader.load();
  auto shader = linkedShaderPrograms[combinedHash];
  if (shader.vertexShader) {
    for (const auto *fetch : shader.vertexShader->vertexFetches) {
      u32 fetchSlot = fetch->fetchSlot;
      u32 regBase = static_cast<u32>(XeRegister::SHADER_CONSTANT_FETCH_00_0) + fetchSlot * 2;

      Xe::VertexFetchData fetchData{};
      fetchData.dword0 = byteswap_be<u32>(state->ReadRegister(static_cast<XeRegister>(regBase + 0)));
      fetchData.dword1 = byteswap_be<u32>(state->ReadRegister(static_cast<XeRegister>(regBase + 1)));

      if (fetchData.size == 0 || fetchData.address == 0)
        continue;

      u32 byteAddress = fetchData.address << 2;
      u32 byteSize = fetchData.size << 2; // Size in DWORDS.

      u8 *data = ramPointer->GetPointerToAddress(byteAddress);
      if (!data) {
        LOG_WARNING(Xenos, "VertexFetch: Invalid memory for slot {} (addr=0x{:X})", fetchSlot, byteAddress);
        continue;
      }

      std::vector<u32> floatVec{};
      floatVec.resize(fetchData.size);
      memcpy(floatVec.data(), data, byteSize);
      for (auto &f : floatVec) {
        LOG_INFO(Xenos, "TEST: 0x{:X}, 0x{:X}", f, std::byteswap<u32>(f));
      }
      std::vector<u8> dataVec{};
      dataVec.resize(byteSize);
      memcpy(dataVec.data(), data, byteSize);
      Render::BufferLoadJob fetchBufferJob = {
        "VertexFetch",
        std::move(dataVec),
        Render::eBufferType::Vertex,
        Render::eBufferUsage::StaticDraw
      };

      {
        std::lock_guard<std::mutex> lock(bufferQueueMutex);
        bufferLoadQueue.push(fetchBufferJob);
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

std::vector<u32> CreateVertexShader() {
  Sirit::Module module{ 0x00010000 };
  module.AddCapability(spv::Capability::Shader);
  module.AddExtension("SPV_KHR_storage_buffer_storage_class");
  module.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);

  // Types
  auto void_type = module.TypeVoid();
  auto func_type = module.TypeFunction(void_type);
  auto float_type = module.TypeFloat(32);
  auto vec4_type = module.TypeVector(float_type, 4);

  // Input and output
  auto input_ptr = module.TypePointer(spv::StorageClass::Input, vec4_type);
  auto output_ptr = module.TypePointer(spv::StorageClass::Output, vec4_type);

  // Variables
  auto aPos = module.AddGlobalVariable(input_ptr, spv::StorageClass::Input);
  module.Name(aPos, "aPos");
  module.Decorate(aPos, spv::Decoration::Location, 0);

  auto gl_Position = module.AddGlobalVariable(output_ptr, spv::StorageClass::Output);
  module.Name(gl_Position, "gl_Position");
  module.Decorate(gl_Position, spv::Decoration::BuiltIn, spv::BuiltIn::Position);

  // Function
  auto func = module.OpFunction(void_type, spv::FunctionControlMask::MaskNone, func_type);
  module.Name(func, "main");
  module.AddEntryPoint(spv::ExecutionModel::Vertex, func, "main", aPos, gl_Position);

  auto label = module.AddLabel();

  // Load input
  auto pos_val = module.OpLoad(vec4_type, aPos);

  // Write to gl_Position
  module.OpStore(gl_Position, pos_val);

  // Return
  module.OpReturn();
  module.OpFunctionEnd();
  return module.Assemble();
}

void Renderer::TryLinkShaderPair(u32 vsHash, u32 psHash) {
  auto vsIt = pendingVertexShaders.find(vsHash);
  auto psIt = pendingPixelShaders.find(psHash);

  if (vsIt != pendingVertexShaders.end() && psIt != pendingPixelShaders.end()) {
    u64 combinedHash = (static_cast<u64>(vsHash) << 32) | psHash;
    std::shared_ptr<Shader> shader = shaderFactory->LoadFromBinary(fmt::format("VS{:08X}_PS{:08X}", vsHash, psHash), {
      { Render::eShaderType::Vertex, vsIt->second.second },
      { Render::eShaderType::Fragment, psIt->second.second }
    });

    if (shader) {
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

      if (xeShader.vertexShader) {
        // Bind VAO once for this shader program's attributes setup.
        // Assuming OnBind creates/binds an appropriate VAO for the current context/shader.
        OnBind();

        for (const auto &[fetch_key, location] : xeShader.vertexShader->attributeLocationMap) {
          // Find the original full fetch definition from the shader's list
          const Xe::Microcode::AST::VertexFetch *fetch = nullptr;
          for (const auto *f : xeShader.vertexShader->vertexFetches) {
            if (f->fetchSlot == fetch_key.slot && f->fetchOffset == fetch_key.offset && f->fetchStride == fetch_key.stride) {
              fetch = f;
              break;
            }
          }
          if (!fetch)
            continue;

          u32 fetchSlot = fetch->fetchSlot;
          u32 regBase = static_cast<u32>(XeRegister::SHADER_CONSTANT_FETCH_00_0) + fetchSlot * 2;

          Xe::ShaderConstantFetch fetchData{};
          for (u32 i = 0; i != 6; ++i)
            fetchData.rawHex[i] = byteswap_be<u32>(XeMain::xenos->xenosState->ReadRegister(static_cast<XeRegister>(regBase + i)));

          if (fetchData.Vertex[0].Type == Xe::eConstType::Texture) {
              // Texture fetches for vertex shaders are currently skipped as per original code.
            continue;
          } else if (fetchData.Vertex[0].Type == Xe::eConstType::Vertex) {
            u32 fetchAddress = fetchData.Vertex[0].BaseAddress << 2;
            u32 fetchSize = fetchData.Vertex[0].Size << 2;

            u8 *data = ramPointer->GetPointerToAddress(fetchAddress);
            if (!data) {
              LOG_WARNING(Xenos, "VertexFetch: Invalid memory for slot {} (addr=0x{:X})", fetchSlot, fetchAddress);
              continue;
            }

            // Prepare data for OpenGL: byteswap and copy to a float vector
            std::vector<f32> dataVec(fetchSize / sizeof(f32)); // Resize based on number of floats
            memcpy(dataVec.data(), data, fetchSize);
            for (u32 i = 0; i < dataVec.size(); ++i) {
              dataVec[i] = std::bit_cast<f32>(byteswap_be(std::bit_cast<u32>(dataVec[i])));
            }

            // Use a unique key for the buffer based on its memory address and size.
            // This allows for proper caching and updates of specific vertex buffers.
            u64 bufferKey = (static_cast<u64>(fetchAddress) << 32) | fetchSize;

            std::shared_ptr<Buffer> buffer = nullptr;
            auto it = createdBuffers.find(bufferKey);
            if (it != createdBuffers.end()) {
              buffer = it->second;
              if (buffer->GetSize() < fetchSize) {
                  // Existing buffer is too small, destroy and recreate
                buffer->DestroyBuffer();
                buffer->CreateBuffer(static_cast<u32>(fetchSize), dataVec.data(), Render::eBufferUsage::StaticDraw, Render::eBufferType::Vertex);
              } else {
                  // Existing buffer is large enough, just update its content
                buffer->UpdateBuffer(0, static_cast<u32>(fetchSize), dataVec.data());
              }
            } else {
              // Create a new buffer
              buffer = resourceFactory->CreateBuffer();
              buffer->CreateBuffer(static_cast<u32>(fetchSize), dataVec.data(), Render::eBufferUsage::StaticDraw, Render::eBufferType::Vertex);
              createdBuffers.insert({ bufferKey, buffer });
            }

            // Bind the buffer to the current VAO for attribute setup
            buffer->Bind();

            // Setup vertex attributes for this fetch.
            // `GetComponentCount()` gives the number of components (1-4).
            // `fetchOffset` and `fetchStride` are in DWords (4 bytes), so multiply by 4 for byte offsets/strides.
            const u32 components = fetch->GetComponentCount();
            const u32 type = fetch->isFloat?GL_FLOAT:GL_UNSIGNED_INT;
            const u8 normalized = fetch->isNormalized?GL_TRUE:GL_FALSE;
            const u32 offset = fetch->fetchOffset * 4;
            const u32 stride = fetch->fetchStride * 4;

            // Assign consecutive attribute locations starting from `currentAttributeLocation`.
            if (location <= 32) { // Check against a reasonable maximum for vertex attributes
              glEnableVertexAttribArray(location);
              glVertexAttribPointer(
                location,
                components,
                type,
                normalized,
                stride,
                reinterpret_cast<void *>((uintptr_t)offset)
              );
            }
          }
        }
      }

      linkedShaderPrograms[combinedHash] = xeShader;
      //LOG_DEBUG(Xenos, "Linked shader program 0x{:016X} (VS:0x{:08X}, PS:0x{:08X})", combinedHash, vsHash, psHash);
    } else {
      LOG_ERROR(Xenos, "Failed to link shader program 0x{:016X} (VS:0x{:08X}, PS:0x{:08X})", combinedHash, vsHash, psHash);
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
  CreateHandles();

  // Main loop
  while (threadRunning && XeRunning) {
    // Start Profile
    MICROPROFILE_SCOPEI("[Xe::Render]", "Loop", MP_AUTO);

    // Exit early if needed
    if (!threadRunning || !XeRunning)
      break;

    // Buffer load
    {
      std::lock_guard<std::mutex> lock(bufferQueueMutex);
      while (!bufferLoadQueue.empty()) {
        BufferLoadJob job = bufferLoadQueue.front();
        bufferLoadQueue.pop();

        auto &bufferEntry = createdBuffers[job.hash];
        if (bufferEntry) {
          bufferEntry->UpdateBuffer(0, static_cast<u32>(job.data.size()), job.data.data());
#ifdef XE_DEBUG
          LOG_DEBUG(Xenos, "Updated buffer '{}', size: {}", job.name, job.data.size());
#endif
        } else {
          bufferEntry = resourceFactory->CreateBuffer();
          bufferEntry->CreateBuffer(static_cast<u32>(job.data.size()), job.data.data(), job.usage, job.type);
#ifdef XE_DEBUG
          LOG_DEBUG(Xenos, "Created buffer '{}', size: {}", job.name, job.data.size());
#endif
        }
      }
    }

    if (readyToLink.load()) {
      const bool hasVS = pendingVertexShaders.contains(pendingVertexShader);
      const bool hasPS = pendingPixelShaders.contains(pendingPixelShader);

      if (hasVS && hasPS) {
        u64 combined = (static_cast<u64>(pendingVertexShader) << 32) | pendingPixelShader;
#ifdef XE_DEBUG
        LOG_DEBUG(Xenos, "Linking VS: 0x{:08X}, PS: 0x{:08X}", pendingVertexShader, pendingPixelShader);
#endif
        TryLinkShaderPair(pendingVertexShader, pendingPixelShader);
        readyToLink.store(false);
      }
    }

    // Copy job
    {
      std::lock_guard<std::mutex> lock(copyQueueMutex);
      if (!copyQueue.empty()) {
        IssueCopy(copyQueue.front());
        copyQueue.pop();
      }
    }

    // Clear the display
    if (XeMain::xenos)
      Clear();

    if (XeMain::xenos && XeMain::xenos->RenderingTo2DFramebuffer() && !focusLost) {
      fbPointer = ramPointer->GetPointerToAddress(XeMain::xenos->GetSurface());

      {
        MICROPROFILE_SCOPEI("[Xe::Render]", "Deswizle", MP_AUTO);
        const u32 *ui_fbPointer = reinterpret_cast<const u32*>(fbPointer);
        pixelSSBO->UpdateBuffer(0, pitch, ui_fbPointer);
      }

      computeShaderProgram->Bind();
      pixelSSBO->Bind();

      if (XeMain::xenos) {
        computeShaderProgram->SetUniformInt("internalWidth", XeMain::xenos->GetWidth());
        computeShaderProgram->SetUniformInt("internalHeight", XeMain::xenos->GetHeight());
      }
      computeShaderProgram->SetUniformInt("resWidth", width);
      computeShaderProgram->SetUniformInt("resHeight", height);
      OnCompute();

      {
        MICROPROFILE_SCOPEI("[Xe::Render]", "BindTexture", MP_AUTO);
        renderShaderPrograms->Bind();
        backbuffer->Bind();
        OnBind();
        backbuffer->Unbind();
        renderShaderPrograms->Unbind();
      }
    }

    // Frame sync
    if (XeMain::xenos && !XeMain::xenos->RenderingTo2DFramebuffer()) {
      std::vector<DrawJob> currentFrameJobs;
      {
        std::lock_guard<std::mutex> lock(drawQueueMutex);
        while (!drawQueue.empty()) {
          currentFrameJobs.push_back(std::move(drawQueue.front()));
          drawQueue.pop();
        }
      }

      static int emptyDrawCount = 0;
      if (!currentFrameJobs.empty()) {
        emptyDrawCount = 0;
        previousJobs = std::move(currentFrameJobs);
      } else if (previousJobs.empty()) {
        if (++emptyDrawCount > 5) {
          LOG_WARNING(Xenos, "Draw queue was empty too many frames in a row");
        }
        if (XeMain::xenos && !XeMain::xenos->RenderingTo2DFramebuffer()) {
          // If there's no previous frame to reuse, skip rendering
          continue;
        }
      }
      std::unique_lock<std::mutex> lock(frameReadyMutex);
      frameReadyCondVar.wait_for(lock, 10s, [this] { return frameReady || !XeRunning; });
      frameReady = false;

      for (const auto &drawJob : previousJobs) {
        u64 combinedHash = (static_cast<u64>(drawJob.shaderVS) << 32) | drawJob.shaderPS;
        auto shaderIt = linkedShaderPrograms.find(combinedHash);
        if (shaderIt != linkedShaderPrograms.end() && shaderIt->second.program) {
          shaderIt->second.program->Bind();
          if (drawJob.indexed) {
            DrawIndexed(shaderIt->second, drawJob.params, drawJob.params.indexBufferInfo);
          } else {
            Draw(shaderIt->second, drawJob.params);
          }
        }
      }
    }

    if (waiting) {
      waiting = false;
      if (waitTime >= 0x100) {
        // Wait
        std::this_thread::sleep_for(std::chrono::milliseconds(waitTime / 0x100));
      } else {
        // Yield
        std::this_thread::yield();
      }
    }

    // Render the GUI
    if (Config::rendering.enableGui && gui.get() && !focusLost) {
      MICROPROFILE_SCOPEI("[Xe::Render::GUI]", "Render", MP_AUTO);
      gui->Render(backbuffer.get());
    }

    // Swap
    MICROPROFILE_SCOPEI("[Xe::Render]", "Swap", MP_AUTO);
    swapCount.fetch_add(1);
    OnSwap(mainWindow);
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
    } else if (specificPPU <= 3 && specificPPU > 0) {
      gui.get()->ppcDebuggerActive[specificPPU - 1] = true;
    }
  }
}

} // namespace Render
#endif
