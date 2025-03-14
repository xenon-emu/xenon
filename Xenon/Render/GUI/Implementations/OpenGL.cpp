// Copyright 2025 Xenon Emulator Project

#include "OpenGL.h"

#include "Core/Xe_Main.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Base/Config.h"
#include "Render/Renderer.h"
#include "Render/Implementations/OGLTexture.h"

void Render::OpenGLGUI::InitBackend(void *context) {
  ImGuiIO &io = ImGui::GetIO();

  if (!ImGui_ImplSDL3_InitForOpenGL(mainWindow, context)) {
    LOG_ERROR(System, "Failed to initialize ImGui's SDL3 implementation");
    SYSTEM_PAUSE();
  }
  if (!ImGui_ImplOpenGL3_Init()) {
    LOG_ERROR(System, "Failed to initialize ImGui's OpenGL implementation");
    SYSTEM_PAUSE();
  }
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

void Render::OpenGLGUI::EndSwap() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    SDL_Window* backupCurrentWindow = SDL_GL_GetCurrentWindow();
    SDL_GLContext backupCurrentContext = SDL_GL_GetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    SDL_GL_MakeCurrent(backupCurrentWindow, backupCurrentContext);
  }
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
