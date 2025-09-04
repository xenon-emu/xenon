// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Vulkan.h"

#ifndef NO_GFX
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Base/Config.h"
#include "Render/Abstractions/Renderer.h"
#include "Render/Vulkan/VulkanTexture.h"

void Render::VulkanGUI::InitBackend(void *context) {
  if (!ImGui_ImplSDL3_InitForOther(mainWindow)) {
    LOG_ERROR(System, "Failed to initialize ImGui's SDL3 implementation");
  }
}

void Render::VulkanGUI::ShutdownBackend() {
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->SetTexID(0);
  ImGui_ImplSDL3_Shutdown();
}

void Render::VulkanGUI::BeginSwap() {
  ImGuiIO &io = ImGui::GetIO();
  unsigned char *pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  io.Fonts->SetTexID(1);
  ImGui_ImplSDL3_NewFrame();
}

void Render::VulkanGUI::EndSwap() {
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}
#endif
