// Copyright 2025 Xenon Emulator Project

#include "OpenGL.h"

#include "Core/Xe_Main.h"
#include "Base/Config.h"
#include "Render/Renderer.h"
#include "Render/Implementations/OGLTexture.h"

// Does not currently work
//#define TEST_IMGUI_RENDER

bool RGH2{};
bool storedPreviousInitSkips{};
int initSkip1{}, initSkip2{};
void Render::OpenGLGUI::InitBackend(SDL_Window *window, void *context) {
  ImGuiIO &io = ImGui::GetIO();
  const char* glsl_version = "#version 430";

  ImGui_ImplSDL3_InitForOpenGL(window, context);
  ImGui_ImplOpenGL3_Init(glsl_version);
  // It might not be a bad idea to take the Xbox 360 font and convert it to TTF
  std::filesystem::path fontsPath{ Base::FS::GetUserPath(Base::FS::PathType::FontDir) };
  std::string robotoRegular = (fontsPath / "Roboto-Regular.ttf").string();
  robotRegular14 = io.Fonts->AddFontFromFileTTF(robotoRegular.c_str(), 14.f);
  defaultFont13 = io.Fonts->AddFontDefault();
  if (Config::SKIP_HW_INIT_1 == 0x3003DC0 && Config::SKIP_HW_INIT_2 == 0x3003E54) {
    RGH2 = true;
  }
}

void Render::OpenGLGUI::ShutdownBackend() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
}

void Render::OpenGLGUI::BeginSwap() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
}

void PPCDebugger(Render::OpenGLGUI *gui) {
  ImGuiWindow *window = ImGui::GetCurrentWindow();
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2.f, 8.f });
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 8.f, 4.f });
  gui->Child("##instrs", [&] {
    gui->MenuBar([&gui] {
      if (!Xe_Main->getCPU()->IsHalted()) {
        gui->MenuItem("Pause", [&gui] {
          Xe_Main->getCPU()->Halt();
        });
      }
      else {
        gui->MenuItem("Continue", [&gui] {
          Xe_Main->getCPU()->Continue();
        });
        gui->MenuItem("Step", [&gui] {
          Xe_Main->getCPU()->Step();
        });
      }
    });
    gui->Text("This is all of the instructions!");
  }, { window->Size.x - 264.f, window->Size.y - 46.f }, ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_MenuBar);
  ImGui::SameLine();
  gui->Child("##regs", [&] {
    gui->Text("This is all of the register context!");
  }, { 245.f, window->Size.y - 46.f }, ImGuiChildFlags_FrameStyle);
  ImGui::PopStyleVar(4);
}

void GeneralSettings(Render::OpenGLGUI *gui) {
  static int logLevel = static_cast<int>(Config::getCurrentLogLevel());
  gui->Toggle("Exit on window close", &Config::shouldQuitOnWindowClosure);
  gui->Toggle("Advanced log", &Config::islogAdvanced);
}

void GraphicsSettings(Render::OpenGLGUI *gui) {
  gui->Toggle("Enabled", &Config::gpuRenderThreadEnabled);
  gui->Toggle("Fullscreen", &Config::isFullscreen, [&] {
    Xe_Main->renderer->fullscreen = Config::isFullscreen;
    SDL_SetWindowFullscreen(gui->mainWindow, Xe_Main->renderer->fullscreen);
  });
  gui->Toggle("VSync", &Config::vsyncEnabled, [&] {
    Xe_Main->renderer->VSYNC = Config::vsyncEnabled;
    SDL_GL_SetSwapInterval(Xe_Main->renderer->VSYNC ? 1 : 0);
  });
} 

void CodeflowSettings(Render::OpenGLGUI *gui) {
  gui->Toggle("RGH2 Init Skip", &RGH2, [] {
    if (!storedPreviousInitSkips || !RGH2) {
      initSkip1 = Config::SKIP_HW_INIT_1;
      initSkip2 = Config::SKIP_HW_INIT_2;
      storedPreviousInitSkips = true;
    }
    Config::SKIP_HW_INIT_1 = RGH2 ? initSkip1 : 0x3003DC0;
    Config::SKIP_HW_INIT_2 = RGH2 ? initSkip2 : 0x3003E54;
  });
}  

u32 texture_id{};
u32 width{}, height{};
#ifdef TEST_IMGUI_RENDER
void AddToDrawList(ImDrawList* drawList) {
  drawList->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    // Use the compute shader
    glUseProgram(Xe_Main->renderer->shaderProgram);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, Xe_Main->renderer->pixelBuffer);
    glUniform1i(glGetUniformLocation(Xe_Main->renderer->shaderProgram, "internalWidth"), Xe_Main->renderer->internalWidth);
    glUniform1i(glGetUniformLocation(Xe_Main->renderer->shaderProgram, "internalHeight"), Xe_Main->renderer->internalHeight);
    glUniform1i(glGetUniformLocation(Xe_Main->renderer->shaderProgram, "resWidth"), width);
    glUniform1i(glGetUniformLocation(Xe_Main->renderer->shaderProgram, "resHeight"),height);
    glDispatchCompute(width / 16, height / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // Draw the texture
    glUseProgram(Xe_Main->renderer->renderShaderProgram);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glBindVertexArray(Xe_Main->renderer->dummyVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 3);

    // Cleanup state after drawing
    glBindVertexArray(0);
    glUseProgram(0);
  }, nullptr);
}
#endif
void ImGuiSettings(Render::OpenGLGUI *gui) {
  gui->Toggle("Style Editor", &gui->styleEditor);
  gui->Toggle("Demo", &gui->demoWindow);
#ifdef TEST_IMGUI_RENDER
  gui->Toggle("Render Xenon in ImGui", &Xe_Main->renderer->imguiRender);
  gui->Tooltip("Renders Xenon into a viewport.\nNote: There may be some performance loss");
#endif
  gui->Toggle("Viewports", &gui->viewports, [&] {
    ImGuiIO& io = ImGui::GetIO();
    if (gui->viewports)
      io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    else
      io.ConfigFlags &= ~(ImGuiConfigFlags_ViewportsEnable);
  });
  gui->Tooltip("Allows ImGui windows to be 'detached' from the main window. Useful for debugging");
}

void Render::OpenGLGUI::OnSwap(Texture *texture) {
  if (Xe_Main->renderer->imguiRender) {
    OGLTexture* ogl_texture = reinterpret_cast<OGLTexture*>(texture);
    u32* ptexture_id = reinterpret_cast<u32*>(ogl_texture->GetTexture());
    if (texture_id != *ptexture_id) {
      texture_id = *ptexture_id;
    }
    Window("##main", [&] {
      MenuBar([&] {
        Menu("Debug", [&] {
          Button("Start", [&] {
            ppcDebuggerActive = true;
          });
          if (ppcDebuggerActive) {
            Button("Detach from window", [&] {
              ppcDebuggerAttached = false;
            });
            if (!Xe_Main->getCPU()->IsHalted()) {
              MenuItem("Pause", [&] {
                Xe_Main->getCPU()->Continue();
              });
            }
            else {
              MenuItem("Continue", [&] {
                Xe_Main->getCPU()->Continue();
              });
            }
            MenuItem("Step", [&] {
              Xe_Main->getCPU()->Step();
            });
          }
        });
        Menu("Settings", [&] {
          Menu("General", [&] {
            GeneralSettings(this);
          });
          Menu("Codeflow", [&] {
            CodeflowSettings(this);
          });
          Menu("Graphics", [&] {
            GraphicsSettings(this);
          });
          Menu("ImGui", [&] {
            ImGuiSettings(this);
          });
        });
      });                          
#ifdef TEST_IMGUI_RENDER
      // Render it in ImGui
      Child("##backbuffer", [&] {
        width = TILE(1280);
        height = TILE(720);
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* drawList = window->DrawList;
        AddToDrawList(drawList);
      }, { 1280.f, 720.f });
#endif
    }, 
    { static_cast<float>(Xe_Main->renderer->width), static_cast<float>(Xe_Main->renderer->height) },
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar, nullptr, {}, ImGuiCond_Always
    );
  }
  if (!Xe_Main->renderer->imguiRender || (Xe_Main->renderer->imguiRender && !ppcDebuggerAttached)) {
    Window("PPC Debugger", [this] {
      PPCDebugger(this);
    }, { 1200.f, 700.f }, ImGuiWindowFlags_NoCollapse, nullptr, { 500.f, 100.f });
  }  
  if (!Xe_Main->renderer->imguiRender) {
    Window("Debug", [&] {
      TabBar("##main", [&] {
        TabItem("Settings", [&] {
          TabBar("##settings", [&] {
            TabItem("General", [&] {
              GeneralSettings(this);
            });
            TabItem("Codeflow", [&] {
              CodeflowSettings(this);
            });
            TabItem("Graphics", [&] {
              GraphicsSettings(this);
            });
            TabItem("ImGui", [&] {
              ImGuiSettings(this);
            });
          });
        });
      });
    }, { 800.f, 500.f }, ImGuiWindowFlags_NoCollapse, nullptr, { 1750.f, 10.f });
  }
} 

void Render::OpenGLGUI::EndSwap() {
  ImGuiIO& io = ImGui::GetIO();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    SDL_Window* backupCurrentWindow = SDL_GL_GetCurrentWindow();
    SDL_GLContext backupCurrentContext = SDL_GL_GetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    SDL_GL_MakeCurrent(backupCurrentWindow, backupCurrentContext);
  }
}