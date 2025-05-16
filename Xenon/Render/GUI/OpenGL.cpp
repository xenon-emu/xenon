// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "OpenGL.h"

#ifndef NO_GFX
#include "Core/Xe_Main.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Base/Config.h"
#include "Render/Abstractions/Renderer.h"
#include "Render/OpenGL/OGLTexture.h"

void Render::OpenGLGUI::InitBackend(void *context) {
  if (!ImGui_ImplSDL3_InitForOpenGL(mainWindow, context)) {
    LOG_ERROR(System, "Failed to initialize ImGui's SDL3 implementation");
    SYSTEM_PAUSE();
  }
  if (!ImGui_ImplOpenGL3_Init()) {
    LOG_ERROR(System, "Failed to initialize ImGui's OpenGL implementation");
    SYSTEM_PAUSE();
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
#endif
