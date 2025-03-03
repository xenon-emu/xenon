// Copyright 2025 Xenon Emulator Project

#include "OpenGL.h"

#include "Core/Xe_Main.h"
#include "Base/Config.h"
#include "Render/Renderer.h"

bool RGH2{};
bool storedPreviousInitSkips{};
int initSkip1{}, initSkip2{};
void Render::OpenGLGUI::InitBackend(SDL_Window* window, void* context) {
  ImGuiIO &io = ImGui::GetIO();
  const char* glsl_version = "#version 430";

  ImGui_ImplSDL3_InitForOpenGL(window, context);
  ImGui_ImplOpenGL3_Init(glsl_version);
  // It might not be a bad idea to take the Xbox 360 font and convert it to TTF
  robotRegular14 = io.Fonts->AddFontFromFileTTF("fonts/Roboto/Roboto-Regular.ttf", 14.f);
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

void Render::OpenGLGUI::OnSwap() {
  //TODO(Vali0004): Make Windows into callbacks, so we can create a window from a different thread.
  Window("Debug", [this] {
    TabBar("##main", [this] {
      TabItem("Codeflow", [this] {
        Button("Halt", [this] {
          std::thread([this] {
            Xe_Main->getCPU()->Halt();
          }).detach();
        });       
        Button("Step", [this] {
          std::thread([this] {
            Xe_Main->getCPU()->Step();
          }).detach();
        });       
        Button("Continue", [this] {
          std::thread([this] {
            Xe_Main->getCPU()->Continue();
          }).detach();
        });
        Toggle("RGH2 Init Skip", &RGH2, [] {
          if (!storedPreviousInitSkips || !RGH2) {
            initSkip1 = Config::SKIP_HW_INIT_1;
            initSkip2 = Config::SKIP_HW_INIT_2;
            storedPreviousInitSkips = true;
          }
          Config::SKIP_HW_INIT_1 = RGH2 ? initSkip1 : 0x3003DC0;
          Config::SKIP_HW_INIT_2 = RGH2 ? initSkip2 : 0x3003E54;
        });
      });
      TabItem("Settings", [this] {
        TabBar("##settings", [this] {
          TabItem("General", [this] {
            static int logLevel = static_cast<int>(Config::getCurrentLogLevel());
            Toggle("Exit on window close", &Config::shouldQuitOnWindowClosure);
            Toggle("Advanced log", &Config::islogAdvanced);
          });
          TabItem("Graphics", [this] {
            Toggle("Enabled", &Config::gpuRenderThreadEnabled);
            Toggle("Fullscreen", &Config::isFullscreen, [this] {
              Xe_Main->renderer->fullscreen = Config::isFullscreen;
              SDL_SetWindowFullscreen(mainWindow, Xe_Main->renderer->fullscreen);
            });
            Toggle("VSync", &Config::vsyncEnabled, [this] {
              Xe_Main->renderer->VSYNC = Config::vsyncEnabled;
              SDL_GL_SetSwapInterval(Xe_Main->renderer->VSYNC ? 1 : 0);
            });
          });
        });
      });
    });
  }, { 800.f, 500.f }, ImGuiWindowFlags_NoCollapse, nullptr, { 1750.f, 10.f });
} 

void Render::OpenGLGUI::EndSwap() {
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}