// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <string>
#include <functional>

#include "Render/Abstractions/Texture.h"
#include "Base/Assert.h"

#ifndef NO_GFX
#include <SDL3/SDL.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

namespace Render {

inline constexpr bool ContainsHex(const std::string_view &str) {
  if (str.find("0x") != std::string_view::npos) {
    return true;
  }
  for (auto &c : str) {
    const char uC = static_cast<char>(toupper(c)) - 65;
    if (uC >= 0 && uC <= 5) {
      return true;
    }
  }
  return false;
}

#define INPUT_TEXT_MULTILINE "##multiline##"
class GUI {
public:
  void Init(SDL_Window *window, void *context);
  void PostInit();
  void Shutdown();

  virtual ~GUI() = default;
  virtual void InitBackend(void *context) = 0;
  virtual void ShutdownBackend() = 0;
  virtual void BeginSwap() = 0;
  virtual void OnSwap(Texture *texture);
  virtual void EndSwap() = 0;

  bool BeginWindow(const std::string &title, const ImVec2 &size = {}, ImGuiWindowFlags flags = 0, bool *conditon = nullptr, const ImVec2 &position = {}, ImGuiCond cond = ImGuiCond_Once);
  bool BeginSimpleWindow(const std::string &title, bool *conditon = nullptr, ImGuiWindowFlags flags = 0);
  void EndWindow();
  bool BeginChild(const std::string &title, const ImVec2 &size = {}, ImGuiChildFlags flags = 0, ImGuiWindowFlags windowFlags = 0);
  void EndChild();
  bool BeginNode(const std::string &title, ImGuiTreeNodeFlags flags = 0);
  void EndNode();
  bool CollapsingHeader(const std::string &title, ImGuiTreeNodeFlags flags = 0);
  void IDGroup(const std::string &id, std::function<void()> callback = {});
  void IDGroup(s32 id, std::function<void()> callback = {});
  void Group(const std::string &label, std::function<void()> callback = {});
  void Separator();
  void Text(const std::string &label);
  void TextCopy(const std::string &label, const std::string &value);
  void TextCopySplit(const std::string &value, const std::string &copyValue);
  void TextCopySimple(const std::string &value);
  void SameLine(f32 xOffset = 0.f, f32 spacing = -1.f);
  bool BeginMenuBar();
  void EndMenuBar();
  bool MenuItem(const std::string &title, bool enabled = true, bool selected = false, const std::string &shortcut = {});
  bool BeginMenu(const std::string &title);
  void EndMenu();
  bool BeginTabBar(const std::string &title, ImGuiTabBarFlags flags = 0);
  void EndTabBar();
  bool BeginTabItem(const std::string &title, bool *conditon = nullptr, ImGuiTabItemFlags flags = 0);
  void EndTabItem();
  bool TabItemButton(const std::string &title, ImGuiTabItemFlags flags = 0);
  bool Button(const std::string &label, const ImVec2 &size = {});
  bool Toggle(const std::string &label, bool *conditon = nullptr);
  std::string InputText(const std::string &title, std::string initValue = {}, size_t maxCharacters = 256,
    const std::string &textHint = {}, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None, ImVec2 size = {});
  template <typename T>
    requires std::is_integral_v<T>
  void InputInt(const std::string &label, T *value, T step = 1, T stepFast = 100, const char *format = "%d");
  void Tooltip(const std::string &contents, ImGuiHoveredFlags delay = ImGuiHoveredFlags_DelayNone);

  void Render(Texture *texture);
  void SetStyle();

  ImFont *robotRegular14 = nullptr;
  ImFont *robotRegular16 = nullptr;
  ImFont *robotRegular18 = nullptr;
  SDL_Window *mainWindow = nullptr;
  bool styleEditor = false;
  bool demoWindow = false;
  u32 stepAmount = 1;
  bool ppcDebuggerActive[3]{};
  bool ppcDebuggerDetached = false;
};

} // namespace Render

#endif
