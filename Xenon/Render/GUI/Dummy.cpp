// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Dummy.h"

#ifndef NO_GFX
void Render::DummyGUI::InitBackend(void *context) {
  LOG_INFO(Render, "DummyGUI::InitBackend");
  if (!ImGui_ImplSDL3_InitForOther(mainWindow)) {
    LOG_ERROR(System, "Failed to initialize ImGui's SDL3 implementation");
  }
}

void Render::DummyGUI::ShutdownBackend() {
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->SetTexID(0);
  ImGui_ImplSDL3_Shutdown();
  LOG_INFO(Render, "DummyGUI::ShutdownBackend");
}

void Render::DummyGUI::BeginSwap() {
  ImGuiIO &io = ImGui::GetIO();
  unsigned char *pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  io.Fonts->SetTexID(1);
  ImGui_ImplSDL3_NewFrame();
  LOG_INFO(Render, "DummyGUI::BeginSwap");
}

void Render::DummyGUI::EndSwap() {
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
  LOG_INFO(Render, "DummyGUI::EndSwap");
}
#endif
