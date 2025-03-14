// Copyright 2025 Xenon Emulator Project

#pragma once

#include <string>
#include <functional>
#include <SDL3/SDL.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include "Base/Assert.h"
#include "Render/Abstractions/Texture.h"

namespace Render {

inline constexpr bool ContainsHex(const std::string_view &str) {
  if (str.find("0x") != std::string_view::npos) {
    return true;
  }
  for (auto &c : str) {
    char uC = static_cast<char>(toupper(c)) - 65;
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

  void Window(const std::string &title, std::function<void()> callback = {}, const ImVec2 &size = {}, ImGuiWindowFlags flags = 0, bool *conditon = nullptr, const ImVec2 &position = {}, ImGuiCond cond = ImGuiCond_Once);
  void Child(const std::string &title, std::function<void()> callback = {}, const ImVec2 &size = {}, ImGuiChildFlags flags = 0, ImGuiWindowFlags windowFlags = 0);
  void Node(const std::string &title, std::function<void()> callback = {}, ImGuiTreeNodeFlags flags = 0);
  void Text(const std::string &label);
  void SameLine(float xOffset = 0.f, float spacing = -1.f);
  void MenuBar(std::function<void()> callback = {});
  void MenuItem(const std::string &title, std::function<void()> callback = {}, bool enabled = true, bool selected = false, const std::string &shortcut = {});
  void Menu(const std::string &title, std::function<void()> callback = {});
  void TabBar(const std::string &title, std::function<void()> callback = {}, ImGuiTabBarFlags flags = 0);
  void TabItem(const std::string &title, std::function<void()> callback = {}, bool *conditon = nullptr, ImGuiTabItemFlags flags = 0);
  bool Button(const std::string &label, std::function<void()> callback = {}, const ImVec2 &size = {});
  bool Toggle(const std::string &label, bool *conditon = nullptr, std::function<void()> callback = {});
  std::string InputText(const std::string &title, std::string initValue = {}, size_t maxCharacters = 256,
    const std::string &textHint = {}, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None, ImVec2 size = {});
  void Tooltip(const std::string &contents, ImGuiHoveredFlags delay = ImGuiHoveredFlags_DelayNone);

  void Render(Texture *texture);
  void SetStyle();

  ImFont *defaultFont13 = nullptr;
  ImFont *robotRegular14 = nullptr;
  SDL_Window *mainWindow = nullptr;
  bool styleEditor = false;
  bool demoWindow = false;
  bool viewports = false;
  bool ppcDebuggerActive = false;
  bool ppcDebuggerAttached = true;
};

} // namespace Render
