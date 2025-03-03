// Copyright 2025 Xenon Emulator Project

#include "GUI.h"

void Render::GUI::Init(SDL_Window* window, void* context) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  // We don't want to create a ini because it stores positions.
  // Because we initialize with a 1280x720 window, then resize to whatever,
  // this will break the window positions, causing them to render off screen
  io.IniFilename = NULL;

  SetStyle();
  InitBackend(window, context);
}
void Render::GUI::Shutdown() {
  ShutdownBackend();
  ImGui::DestroyContext();
}

void Render::GUI::Window(const std::string& title, std::function<void()> callback, const ImVec2& size, ImGuiWindowFlags flags, bool* conditon, const ImVec2& position) {
  ImGui::SetNextWindowPos(position, ImGuiCond_Once);
  ImGui::SetNextWindowSize(size, ImGuiCond_Once);

  if (ImGui::Begin(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
  }
  ImGui::End();
}

void Render::GUI::TabBar(const std::string& title, std::function<void()> callback, ImGuiTabBarFlags flags) {
  if (ImGui::BeginTabBar(title.c_str(), flags)) {
    if (callback) {
      callback();
    }
    ImGui::EndTabBar();
  }
} 

void Render::GUI::TabItem(const std::string& title, std::function<void()> callback, bool* conditon, ImGuiTabItemFlags flags) {
  if (ImGui::BeginTabItem(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
    ImGui::EndTabItem();
  }
}

bool Render::GUI::Button(const std::string& label, std::function<void()> callback, const ImVec2& size) {
  if (ImGui::Button(label.c_str(), size)) {
    if (callback) {
      callback();
    }
    return true;
  }
  return false;
}

bool Render::GUI::Toggle(const std::string& label, bool* conditon, std::function<void()> callback) {
  bool dummy{};
  if (!conditon) {
    conditon = &dummy;
  }
  if (ImGui::Checkbox(label.c_str(), conditon)) {
    if (callback) {
      callback();
    }
    return true;
  }
  return false;
}

std::string Render::GUI::InputText(const std::string& title, std::string initValue, size_t maxCharacters,
  const std::string& textHint, ImGuiInputTextFlags flags, ImVec2 size)
{
  std::vector<char> buf(maxCharacters, '\0');
  if (buf[0] == '\0' && !initValue.empty()) {
    memcpy(buf.data(), initValue.data(), initValue.size());
  }

  if (textHint.empty()) {
    ImGui::InputText(title.c_str(), buf.data(), maxCharacters, flags);
  }
  else if (textHint.compare(INPUT_TEXT_MULTILINE)) {
    ImGui::InputTextWithHint(title.c_str(), textHint.c_str(), buf.data(), maxCharacters, flags);
  }
  else {
    ImGui::InputTextMultiline(title.c_str(), buf.data(), maxCharacters, size, flags);
  }

  return buf.data();
} 

void Render::GUI::Tooltip(const std::string& contents, ImGuiHoveredFlags delay) {
  if (delay != ImGuiHoveredFlags_DelayNone)
    delay |= ImGuiHoveredFlags_NoSharedDelay;

  if (ImGui::IsItemHovered(delay)) {
    ImGui::SetTooltip(contents.c_str());
  }
}
 

void Render::GUI::Render() {
  BeginSwap();
  ImGui::NewFrame();
  if (styleEditor) {
    Window("Style Editor", [] {
      ImGui::ShowStyleEditor();
    }, { 1000.f, 900.f }, ImGuiWindowFlags_NoCollapse, nullptr, { 600.f, 60.f });
  }
  if (demoWindow) {
    ImGui::ShowDemoWindow(&demoWindow);
  }
  OnSwap();
  ImGui::EndFrame();
  ImGui::Render();
  EndSwap();
}
void Render::GUI::SetStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;
  // Colors
  colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
  colors[ImGuiCol_TextDisabled] = ImColor(255, 230, 49, 255);
  colors[ImGuiCol_WindowBg] = ImColor(15, 15, 15, 248);
  colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0, 0);
  colors[ImGuiCol_PopupBg] = ImColor(20, 20, 20, 240);
  colors[ImGuiCol_Border] = ImColor(76, 76, 76, 127);
  colors[ImGuiCol_BorderShadow] = ImColor(0, 0, 0, 0);
  colors[ImGuiCol_FrameBg] = ImColor(10, 10, 10, 138);
  colors[ImGuiCol_FrameBgHovered] = ImColor(10, 10, 10, 199);
  colors[ImGuiCol_FrameBgActive] = ImColor(71, 69, 69, 138);
  colors[ImGuiCol_TitleBg] = ImColor(111, 210, 50, 255);
  colors[ImGuiCol_TitleBgActive] = ImColor(108, 232, 0, 255);
  colors[ImGuiCol_TitleBgCollapsed] = ImColor(41, 41, 41, 191);
  colors[ImGuiCol_MenuBarBg] = ImColor(36, 36, 36, 255);
  colors[ImGuiCol_ScrollbarBg] = ImColor(5, 5, 5, 135);
  colors[ImGuiCol_ScrollbarGrab] = ImColor(79, 79, 79, 255);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(104, 104, 104, 255);
  colors[ImGuiCol_ScrollbarGrabActive] = ImColor(130, 130, 130, 255);
  colors[ImGuiCol_CheckMark] = ImColor(255, 255, 255, 255);
  colors[ImGuiCol_SliderGrab] = ImColor(87, 87, 87, 255);
  colors[ImGuiCol_SliderGrabActive] = ImColor(99, 97, 97, 255);
  colors[ImGuiCol_Button] = ImColor(108, 232, 0, 255);
  colors[ImGuiCol_ButtonHovered] = ImColor(110, 210, 50, 208);
  colors[ImGuiCol_ButtonActive] = ImColor(110, 210, 50, 240);
  colors[ImGuiCol_Header] = ImColor(94, 94, 94, 79);
  colors[ImGuiCol_HeaderHovered] = ImColor(97, 97, 97, 94);
  colors[ImGuiCol_HeaderActive] = ImColor(94, 94, 94, 130);
  colors[ImGuiCol_Separator] = ImColor(97, 97, 97, 127);
  colors[ImGuiCol_SeparatorHovered] = ImColor(117, 117, 117, 127);
  colors[ImGuiCol_SeparatorActive] = ImColor(117, 117, 117, 163);
  colors[ImGuiCol_ResizeGrip] = ImColor(0, 0, 0, 0);
  colors[ImGuiCol_ResizeGripHovered] = ImColor(108, 232, 0, 255);
  colors[ImGuiCol_ResizeGripActive] = ImColor(111, 210, 50, 255);
  colors[ImGuiCol_Tab] = ImColor(110, 210, 50, 208);
  colors[ImGuiCol_TabHovered] = ImColor(109, 232, 0, 240);
  colors[ImGuiCol_TabSelected] = ImColor(108, 232, 0, 255);
  colors[ImGuiCol_TabSelectedOverline] = ImColor(0, 0, 0, 0);
  colors[ImGuiCol_PlotLines] = ImColor(155, 155, 155, 255);
  colors[ImGuiCol_PlotLinesHovered] = ImColor(255, 110, 89, 255);
  colors[ImGuiCol_PlotHistogram] = ImColor(229, 179, 0, 255);
  colors[ImGuiCol_PlotHistogramHovered] = ImColor(255, 153, 0, 255);
  colors[ImGuiCol_TextSelectedBg] = ImColor(66, 150, 250, 89);
  colors[ImGuiCol_DragDropTarget] = ImColor(255, 255, 0, 230);
  colors[ImGuiCol_NavHighlight] = ImColor(66, 150, 250, 255);
  colors[ImGuiCol_NavWindowingHighlight] = ImColor(255, 255, 255, 179);
  colors[ImGuiCol_NavWindowingDimBg] = ImColor(204, 204, 204, 51);
  colors[ImGuiCol_ModalWindowDimBg] = ImColor(204, 204, 204, 89);

  // Style config
  style.Alpha = 1.f;
  style.DisabledAlpha = 0.95f;
  style.WindowPadding = { 10.f, 10.f };
  style.WindowRounding = 0.f;
  style.WindowBorderSize = 1.f;
  style.WindowMinSize = { 1.f, 1.f };
  style.WindowTitleAlign = { 0.f, 0.5f };
  style.WindowMenuButtonPosition = ImGuiDir_Left;
  style.ChildRounding = 6.f;
  style.ChildBorderSize = 0.f;
  style.PopupRounding = 0.f;
  style.PopupBorderSize = 1.f;
  style.FramePadding = { 8.f, 4.f };
  style.FrameRounding = 4.f;
  style.FrameBorderSize = 1.f;
  style.ItemSpacing = { 10.f, 8.f };
  style.ItemInnerSpacing = { 6.f, 6.f };
  style.TouchExtraPadding = { 0.f, 0.f };
  style.IndentSpacing = 21.f;
  style.ScrollbarSize = 15.f;
  style.ScrollbarRounding = 0.f;
  style.GrabMinSize = 8.f;
  style.GrabRounding = 3.f;
  style.TabRounding = 5.f;
  style.TabBorderSize = 1.f;
  style.TabBarBorderSize = 0.5f;
  style.TabBarOverlineSize = 0.f;
  style.ButtonTextAlign = { 0.5f, 0.5f };
  style.DisplaySafeAreaPadding = { 3.f, 22.f };
  style.MouseCursorScale = 0.7f;
}