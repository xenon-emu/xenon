// Copyright 2025 Xenon Emulator Project

#include "GUI.h"

#ifndef NO_GFX
#include "Core/Xe_Main.h"
#include "Base/Exit.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

#ifdef _WIN32
#include <shellapi.h>
#endif

// Helper functions for text/string formatting with the gui
#define TextFmt(g, x, ...) g->Text(fmt::format(x, __VA_ARGS__))
#define TextCopyFmt(g, x, v, ...) g->TextCopy(x, fmt::format(v, __VA_ARGS__))
#define CustomBase(g, x, fmt, ...) TextFmt(g, x ": " fmt, __VA_ARGS__)
#define CopyCustomBase(g, x, fmt, ...) TextCopyFmt(g, x, fmt, __VA_ARGS__)
#define Custom(g, x, fmt, ...) CustomBase(g, #x, fmt, __VA_ARGS__)
#define CopyCustom(g, x, fmt, ...) CustomBase(g, #x, fmt, __VA_ARGS__)
#define HexBase(g, x, ...) CopyCustomBase(g, x, "0x{:X}", __VA_ARGS__)
#define Hex(g, c, x) HexBase(g, #x, c.x)
#define HexPtr(g, c, x) HexBase(g, #x, c->x)
#define BFHex(g, c, x) HexBase(g, #x, u32(c.x));
#define BFHexPtr(g, c, x) HexBase(g, #x, u32(c->x));
#define U8Hex(g, c, x) HexBase(g, #x, static_cast<u32>(c.x))
#define U8HexPtr(g, c, x) HexBase(g, #x, static_cast<u32>(c->x))
#define HexArr(g, a, i) HexBase(g, fmt::format("[{}]", i), a[i])
#define Dec(g, c, x) CopyCustom(g, x, "{}", c.x)
#define DecPtr(g, c, x) CopyCustom(g, x, "{}", c->x)
#define U8Dec(g, c, x) CopyCustom(g, x, "{}", static_cast<u32>(c.x))
#define U8DecPtr(g, c, x) CopyCustom(g, x, "{}", static_cast<u32>(c->x))
#define Bool(g, c, x) CopyCustom(g, x, "{}", c.x ? "true" : "false")
#define BoolPtr(g, c, x) CopyCustom(g, x, "{}", c->x ? "true" : "false")

void Render::GUI::Init(SDL_Window* window, void *context) {
  MICROPROFILE_SCOPEI("[Xe::Render::GUI]", "Init", MP_AUTO);
  // Set our mainWindow handle
  mainWindow = window;

  // Check ImGui version
  IMGUI_CHECKVERSION();
  // Create ImGui Context
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  // We don't want to create a ini because it stores positions.
  // Because we initialize with a 1280x720 window, then resize to whatever,
  // this will break the window positions, causing them to render off screen
  const std::string iniPath = Config::imgui.configPath;
  io.IniFilename = iniPath.compare("none") ? iniPath.data() : nullptr;
  // Enable ImGui Keyboard Navigation
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Enable ImGui Gamepad Navigation
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  if (Config::imgui.viewports) {
    // Enable Viewport (Allows for no window background)
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  }
  SetStyle();
  InitBackend(context);
  PostInit();
}

bool RGH2{};
bool storedPreviousInitSkips{};
s32 initSkip1{}, initSkip2{};
void Render::GUI::PostInit() {
  ImGuiIO &io = ImGui::GetIO();
  // It might not be a bad idea to take the Xbox 360 font and convert it to TTF
  const std::filesystem::path fontsPath{ Base::FS::GetUserPath(Base::FS::PathType::FontDir) };
  const std::string robotoRegular = (fontsPath / "Roboto-Regular.ttf").string();
  robotRegular16 = io.Fonts->AddFontFromFileTTF(robotoRegular.c_str(), 16.f);
  robotRegular14 = io.Fonts->AddFontFromFileTTF(robotoRegular.c_str(), 14.f);
  robotRegular18 = io.Fonts->AddFontFromFileTTF(robotoRegular.c_str(), 18.f);
  defaultFont13 = io.Fonts->AddFontDefault();
  if (Config::xcpu.HW_INIT_SKIP_1 == 0x3003DC0 && Config::xcpu.HW_INIT_SKIP_2 == 0x3003E54) {
    storedPreviousInitSkips = true; // If we already have RGH2, ignore
    RGH2 = true;
  }
}

void Render::GUI::Shutdown() {
  ShutdownBackend();
  ImGui::DestroyContext();
}

//TODO(Vali0004): Make Windows into callbacks, so we can create a window from a different thread.
void Render::GUI::Window(const std::string &title, std::function<void()> callback, const ImVec2 &size, ImGuiWindowFlags flags, bool *conditon, const ImVec2 &position, ImGuiCond cond) {
  ImGui::SetNextWindowPos(position, cond);
  ImGui::SetNextWindowSize(size, cond);

  if (ImGui::Begin(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
  }
  ImGui::End();
}

void Render::GUI::SimpleWindow(const std::string &title, std::function<void()> callback, bool *conditon, ImGuiWindowFlags flags) {
  if (ImGui::Begin(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
  }
  ImGui::End();
}

void Render::GUI::Child(const std::string &title, std::function<void()> callback, const ImVec2 &size, ImGuiChildFlags flags, ImGuiWindowFlags windowFlags) {
  if (ImGui::BeginChild(title.c_str(), size, flags, windowFlags)) {
    if (callback) {
      callback();
    }
  }
  ImGui::EndChild();
}

void Render::GUI::Node(const std::string &title, std::function<void()> callback, ImGuiTreeNodeFlags flags) {
  if (ImGui::TreeNodeEx(title.c_str(), flags)) {
    if (callback) {
      callback();
    }
    ImGui::TreePop();
  }
}

void Render::GUI::CollapsingHeader(const std::string &title, std::function<void()> callback, ImGuiTreeNodeFlags flags) {
  if (ImGui::CollapsingHeader(title.c_str(), flags)) {
    if (callback) {
      callback();
    }
  }
}

void Render::GUI::Separator() {
  ImGui::Separator();
}

void Render::GUI::IDGroup(const std::string &id, std::function<void()> callback) {
  ImGui::PushID(id.c_str());
  if (callback) {
    callback();
  }
  ImGui::PopID();
}

void Render::GUI::Group(const std::string &label, std::function<void()> callback) {
  ImGui::BeginGroup();
  if (label.empty())
    Text(label);
  if (callback) {
    callback();
  }
  ImGui::EndGroup();
}

void Render::GUI::IDGroup(s32 id, std::function<void()> callback) {
  ImGui::PushID(id);
  if (callback) {
    callback();
  }
  ImGui::PopID();
}

void Render::GUI::Text(const std::string &label) {
  ImGui::TextUnformatted(label.c_str());
}

namespace ImGui {
  // Copies behaviour from ImGui::TextLink, then removes the line and color
  bool TextButton(const char* label) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    const ImGuiID id = window->GetID(label);
    const char* label_end = FindRenderedTextEnd(label);

    ImVec2 pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    ImVec2 size = CalcTextSize(label, label_end, true);
    ImRect bb(pos, pos + size);
    ItemSize(size, 0.0f);
    if (!ItemAdd(bb, id))
      return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    RenderNavCursor(bb, id);

    if (hovered)
      SetMouseCursor(ImGuiMouseCursor_Hand);

    RenderText(bb.Min, label, label_end);
    return pressed;
  }
}

void Render::GUI::TextCopy(const std::string &label, const std::string &value) {
  std::string flabel = label + ": " + value;
  if (ImGui::TextButton(flabel.data())) {
    LOG_INFO(Debug, "{}", flabel.data());
  }
  if (ImGui::BeginPopupContextItem()) {
    MenuItem("Copy '" + flabel + "'", [&] {
      SDL_SetClipboardText(flabel.data());
    });
    MenuItem("Copy '" + value + "'", [&] {
      SDL_SetClipboardText(value.data());
    });
    ImGui::EndPopup();
  }
}

void Render::GUI::TextCopySimple(const std::string &value) {
  u64 hashtagPos = value.find('#');
  std::string valueSimple = hashtagPos != std::string::npos ? value.substr(0, hashtagPos) : value;
  std::string hashTag = hashtagPos != std::string::npos ? value.substr(hashtagPos + 2) : value;

  if (ImGui::TextButton(value.data())) {
    LOG_INFO(Debug, "{}", valueSimple.data());
  }
  if (ImGui::BeginPopupContextItem()) {
    MenuItem("Copy '" + valueSimple + "'##" + hashTag, [&] {
      SDL_SetClipboardText(valueSimple.data());
    });
    ImGui::EndPopup();
  }
}

void Render::GUI::TextCopySplit(const std::string &value, const std::string &copyValue) {
  u64 hashtagPos = value.find('#');
  u64 hashtagCopyPos = copyValue.find('#');
  bool hasHashtag = hashtagPos != std::string::npos;
  bool copyHasHashtag = hashtagCopyPos != std::string::npos;

  std::string valueSimple = hasHashtag ? value.substr(0, hashtagPos) : value;
  std::string hashTag = hasHashtag ? value.substr(hashtagPos+2) : value;

  std::string copyValueSimple = copyHasHashtag ? copyValue.substr(0, hashtagCopyPos) : copyValue;
  std::string copyHashTag = copyHasHashtag ? copyValue.substr(hashtagCopyPos+2) : copyValue;

  if (ImGui::TextButton(value.data())) {
    LOG_INFO(Debug, "{}", valueSimple);
  }
  if (ImGui::BeginPopupContextItem()) {
    MenuItem("Copy '" + valueSimple + "'##" + hashTag, [&] {
      SDL_SetClipboardText(value.data());
    });
    MenuItem("Copy '" + copyValueSimple + "'##" + copyHashTag, [&] {
      SDL_SetClipboardText(copyValueSimple.data());
    });
    ImGui::EndPopup();
  }
}

void Render::GUI::SameLine(f32 xOffset, f32 spacing) {
  ImGui::SameLine(xOffset, spacing);
}

void Render::GUI::MenuBar(std::function<void()> callback) {
  if (ImGui::BeginMenuBar()) {
    if (callback) {
      callback();
    }
    ImGui::EndMenuBar();
  }
}

void Render::GUI::Menu(const std::string &title, std::function<void()> callback) {
  if (ImGui::BeginMenu(title.c_str())) {
    if (callback) {
      callback();
    }
    ImGui::EndMenu();
  }
}

void Render::GUI::MenuItem(const std::string &title, std::function<void()> callback, bool enabled, bool selected, const std::string &shortcut) {
  if (ImGui::MenuItem(title.c_str(), shortcut.c_str(), selected, enabled)) {
    if (callback) {
      callback();
    }
  }
}

void Render::GUI::TabBar(const std::string &title, std::function<void()> callback, ImGuiTabBarFlags flags) {
  if (ImGui::BeginTabBar(title.c_str(), flags)) {
    if (callback) {
      callback();
    }
    ImGui::EndTabBar();
  }
}

void Render::GUI::TabItem(const std::string &title, std::function<void()> callback, bool *conditon, ImGuiTabItemFlags flags) {
  if (ImGui::BeginTabItem(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
    ImGui::EndTabItem();
  }
}

void Render::GUI::TabItemButton(const std::string &title, std::function<void()> callback, ImGuiTabItemFlags flags) {
  if (ImGui::TabItemButton(title.c_str(), flags)) {
    if (callback) {
      callback();
    }
  }
}

bool Render::GUI::Button(const std::string &label, std::function<void()> callback, const ImVec2 &size) {
  if (ImGui::Button(label.c_str(), size)) {
    if (callback) {
      callback();
    }
    return true;
  }
  return false;
}

bool Render::GUI::Toggle(const std::string &label, bool* conditon, std::function<void()> callback) {
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

std::string Render::GUI::InputText(const std::string &title, std::string initValue, size_t maxCharacters,
  const std::string &textHint, ImGuiInputTextFlags flags, ImVec2 size)
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

template <typename T>
  requires std::is_integral_v<T>
void Render::GUI::InputInt(const std::string &label, T *value, T step, T stepFast, const char *format) {
  ImGuiDataType_ dataType;
  if constexpr (std::is_same_v<T, u64>) {
    dataType = ImGuiDataType_U64;
  } else if constexpr (std::is_same_v<T, s64>) {
    dataType = ImGuiDataType_S64;
  } else if constexpr (std::is_same_v<T, u32>) {
    dataType = ImGuiDataType_U32;
  } else if constexpr (std::is_same_v<T, s32>) {
    dataType = ImGuiDataType_S32;
  } else if constexpr (std::is_same_v<T, u16>) {
    dataType = ImGuiDataType_U16;
  } else if constexpr (std::is_same_v<T, s16>) {
    dataType = ImGuiDataType_S16;
  } else if constexpr (std::is_same_v<T, u8>) {
    dataType = ImGuiDataType_U8;
  } else if constexpr (std::is_same_v<T, s8>) {
    dataType = ImGuiDataType_S8;
  }
  ImGui::InputScalar(label.c_str(), dataType, (void*)value, (void*)(step > 0 ? &step : nullptr), (void*)(stepFast > 0 ? &stepFast : nullptr), format);
}

void Render::GUI::Tooltip(const std::string &contents, ImGuiHoveredFlags delay) {
  if (delay != ImGuiHoveredFlags_DelayNone)
    delay |= ImGuiHoveredFlags_NoSharedDelay;

  if (ImGui::IsItemHovered(delay)) {
    if (!ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None))
      return;
    ImGui::TextUnformatted(contents.c_str());
    ImGui::EndTooltip();
  }
}

void RenderInstructions(Render::GUI *gui, PPU_STATE *state, ePPUThread thr, u64 numInstructions) {
  PPU_THREAD_REGISTERS &thread = state->ppuThread[thr];
  f32 maxLineWidth = ImGui::GetContentRegionAvail().x;
  for (u64 i = 0; i != (numInstructions * 2) + 1; ++i) {
    u32 instr = 0;
    u64 addr = (thread.CIA - (4 * numInstructions + 1)) + (4 * i);
    thread.instrFetch = true;
    instr = PPCInterpreter::MMURead32(state, addr, thr);
    if (thread.exceptReg & PPU_EX_INSSTOR || thread.exceptReg & PPU_EX_INSTSEGM) {
      break;
    }
    thread.instrFetch = false;
    const std::string instrName = PPCInterpreter::PPCInterpreter_getFullName(instr);
#ifdef __LITTLE_ENDIAN__
    const u32 b0 = static_cast<u8>((instr >> 24) & 0xFF);
    const u32 b1 = static_cast<u8>((instr >> 16) & 0xFF);
    const u32 b2 = static_cast<u8>((instr >> 8) & 0xFF);
    const u32 b3 = static_cast<u8>((instr >> 0) & 0xFF);
#else
    const u32 b0 = static_cast<u8>((instr >> 0) & 0xFF);
    const u32 b1 = static_cast<u8>((instr >> 8) & 0xFF);
    const u32 b2 = static_cast<u8>((instr >> 16) & 0xFF);
    const u32 b3 = static_cast<u8>((instr >> 24) & 0xFF);
#endif
    gui->TextCopySimple(fmt::format("{}{:08X}", addr == thread.CIA ? "[*] " : "", addr)); gui->SameLine(0.f, 2.f);
    gui->TextCopySplit(fmt::format("{:02X}##{}", b0, addr), fmt::format("{:08X}", instr)); gui->SameLine(0.f, 2.f);
    gui->TextCopySimple(fmt::format("{:02X}##{}", b1, addr + 1)); gui->SameLine(0.f, 2.f);
    gui->TextCopySimple(fmt::format("{:02X}##{}", b2, addr + 2)); gui->SameLine(0.f, 2.f);
    gui->TextCopySimple(fmt::format("{:02X}##{}", b3, addr + 3)); gui->SameLine(0.f, maxLineWidth > 800.f ? 270.f : 120.f);
    gui->TextCopySimple(fmt::format("{}##{}", instrName, addr));
  }
}

void PPUThreadDiassembly(Render::GUI *gui, PPU_STATE *state, ePPUThread thr) {
  gui->SimpleWindow(fmt::format("Diassembly [{}:{}]", state->ppuName, static_cast<u8>(thr)), [&] {
    RenderInstructions(gui, state, thr, 16);
  });
}

void PPUThreadRegisters(Render::GUI *gui, PPU_STATE *state, ePPUThread thr) {
  gui->SimpleWindow(fmt::format("Registers [{}:{}]", state->ppuName, static_cast<u8>(thr)), [&] {
    PPU_THREAD_REGISTERS &ppuRegisters = state->ppuThread[thr];
    gui->Node("GPRs", [&] {
      for (u64 i = 0; i < 32; ++i) {
        HexArr(gui, ppuRegisters.GPR, i);
      }
    });
    gui->Node("FPRs", [&] {
      for (u64 i = 0; i < 32; ++i) {
        FPRegister &FPR = ppuRegisters.FPR[i];
        gui->IDGroup(i, [&] {
          TextFmt(gui, "FPR[{}]", i);
          Custom(gui, valueAsDouble, "{}", FPR.valueAsDouble);
          Custom(gui, valueAsU64, "0x{:X}", FPR.valueAsU64);
        });
      }
    });
    gui->Node("SPRs", [&] {
      PPU_THREAD_SPRS &SPR = ppuRegisters.SPR;
      gui->Node("MSRs", [&] {
        MSRegister &MSR = SPR.MSR;
        BFHex(gui, MSR, LE);
        BFHex(gui, MSR, RI);
        BFHex(gui, MSR, PMM);
        BFHex(gui, MSR, DR);
        BFHex(gui, MSR, IR);
        BFHex(gui, MSR, FE1);
        BFHex(gui, MSR, BE);
        BFHex(gui, MSR, SE);
        BFHex(gui, MSR, FE0);
        BFHex(gui, MSR, ME);
        BFHex(gui, MSR, FP);
        BFHex(gui, MSR, PR);
        BFHex(gui, MSR, EE);
        BFHex(gui, MSR, ILE);
        BFHex(gui, MSR, VXU);
        BFHex(gui, MSR, HV);
        BFHex(gui, MSR, TA);
        BFHex(gui, MSR, SF);
      }, ImGuiTreeNodeFlags_DefaultOpen);
      gui->Node("XER", [&] {
        XERegister& XER = SPR.XER;
        Hex(gui, XER, XER_Hex);
        BFHex(gui, XER, ByteCount);
        BFHex(gui, XER, R0);
        BFHex(gui, XER, CA);
        BFHex(gui, XER, OV);
        BFHex(gui, XER, SO);
      });
      Hex(gui, SPR, LR);
      Hex(gui, SPR, CTR);
      Hex(gui, SPR, CFAR);
      Hex(gui, SPR, VRSAVE);
      Hex(gui, SPR, DSISR);
      Hex(gui, SPR, DAR);
      Dec(gui, SPR, DEC);
      Hex(gui, SPR, SRR0);
      Hex(gui, SPR, SRR1);
      Hex(gui, SPR, ACCR);
      Hex(gui, SPR, SPRG0);
      Hex(gui, SPR, SPRG1);
      Hex(gui, SPR, SPRG2);
      Hex(gui, SPR, SPRG3);
      Hex(gui, SPR, HSPRG0);
      Hex(gui, SPR, HSPRG1);
      Hex(gui, SPR, HSRR0);
      Hex(gui, SPR, HSRR1);
      Hex(gui, SPR, TSRL);
      Hex(gui, SPR, TSSR);
      Hex(gui, SPR, PPE_TLB_Index_Hint);
      Hex(gui, SPR, DABR);
      Hex(gui, SPR, DABRX);
      Hex(gui, SPR, PIR);
    });
    gui->Node("SLBs", [&] {
      for (u64 i = 0; i < 64; ++i) {
        SLBEntry &SLB = ppuRegisters.SLB[i];
        gui->Node(fmt::format("[{}]", i), [&] {
          U8Hex(gui, SLB, V);
          U8Hex(gui, SLB, LP);
          U8Hex(gui, SLB, C);
          U8Hex(gui, SLB, L);
          U8Hex(gui, SLB, N);
          U8Hex(gui, SLB, Kp);
          U8Hex(gui, SLB, Ks);
          Hex(gui, SLB, VSID);
          Hex(gui, SLB, ESID);
          Hex(gui, SLB, vsidReg);
          Hex(gui, SLB, esidReg);
        });
      }
    });
    gui->Node("GPR:CR", [&] {
      CRegister &CR = ppuRegisters.CR;
      Hex(gui, CR, CR_Hex);
      BFHex(gui, CR, CR0);
      BFHex(gui, CR, CR1);
      BFHex(gui, CR, CR2);
      BFHex(gui, CR, CR3);
      BFHex(gui, CR, CR4);
      BFHex(gui, CR, CR5);
      BFHex(gui, CR, CR6);
      BFHex(gui, CR, CR7);
    });
    gui->Node("Op:CI", [&] {
      PPCOpcode &CI = ppuRegisters.CI;
      Hex(gui, CI, opcode);
      BFHex(gui, CI, main);
      BFHex(gui, CI, sh64);
      BFHex(gui, CI, mbe64);
      BFHex(gui, CI, vuimm);
      BFHex(gui, CI, vs);
      BFHex(gui, CI, vsh);
      BFHex(gui, CI, oe);
      BFHex(gui, CI, spr);
      BFHex(gui, CI, vc);
      BFHex(gui, CI, vb);
      BFHex(gui, CI, va);
      BFHex(gui, CI, vd);
      BFHex(gui, CI, lk);
      BFHex(gui, CI, aa);
      BFHex(gui, CI, rb);
      BFHex(gui, CI, ra);
      BFHex(gui, CI, rd);
      BFHex(gui, CI, uimm16);
      BFHex(gui, CI, l11);
      BFHex(gui, CI, rs);
      BFHex(gui, CI, simm16);
      BFHex(gui, CI, ds);
      BFHex(gui, CI, vsimm);
      BFHex(gui, CI, ll);
      BFHex(gui, CI, li);
      BFHex(gui, CI, lev);
      BFHex(gui, CI, i);
      BFHex(gui, CI, crfs);
      BFHex(gui, CI, l10);
      BFHex(gui, CI, crfd);
      BFHex(gui, CI, crbb);
      BFHex(gui, CI, crba);
      BFHex(gui, CI, crbd);
      BFHex(gui, CI, rc);
      BFHex(gui, CI, me32);
      BFHex(gui, CI, mb32);
      BFHex(gui, CI, sh32);
      BFHex(gui, CI, bi);
      BFHex(gui, CI, bo);
      BFHex(gui, CI, bh);
      BFHex(gui, CI, frc);
      BFHex(gui, CI, frb);
      BFHex(gui, CI, fra);
      BFHex(gui, CI, frd);
      BFHex(gui, CI, crm);
      BFHex(gui, CI, frs);
      BFHex(gui, CI, flm);
      BFHex(gui, CI, l6);
      BFHex(gui, CI, l15);
      BFHex(gui, CI, bt14);
      BFHex(gui, CI, bt24);
    });
    gui->Node("FPSCR", [&] {
      FPSCRegister& FPSCR = ppuRegisters.FPSCR;
      Hex(gui, FPSCR, FPSCR_Hex);
      BFHex(gui, FPSCR, RN);
      BFHex(gui, FPSCR, NI);
      BFHex(gui, FPSCR, XE);
      BFHex(gui, FPSCR, ZE);
      BFHex(gui, FPSCR, UE);
      BFHex(gui, FPSCR, OE);
      BFHex(gui, FPSCR, VE);
      BFHex(gui, FPSCR, VXCVI);
      BFHex(gui, FPSCR, VXSQRT);
      BFHex(gui, FPSCR, VXSOFT);
      BFHex(gui, FPSCR, R0);
      BFHex(gui, FPSCR, C);
      BFHex(gui, FPSCR, FG);
      BFHex(gui, FPSCR, FL);
      BFHex(gui, FPSCR, FE);
      BFHex(gui, FPSCR, FU);
      BFHex(gui, FPSCR, FI);
      BFHex(gui, FPSCR, FR);
      BFHex(gui, FPSCR, VXVC);
      BFHex(gui, FPSCR, VXIMZ);
      BFHex(gui, FPSCR, VXZDZ);
      BFHex(gui, FPSCR, VXIDI);
      BFHex(gui, FPSCR, VXISI);
      BFHex(gui, FPSCR, VXSNAN);
      BFHex(gui, FPSCR, XX);
      BFHex(gui, FPSCR, ZX);
      BFHex(gui, FPSCR, UX);
      BFHex(gui, FPSCR, OX);
      BFHex(gui, FPSCR, VX);
      BFHex(gui, FPSCR, FEX);
      BFHex(gui, FPSCR, FX);
    });
    gui->Node("PPU:Reserve", [&] {
      PPU_RES *ppuRes = ppuRegisters.ppuRes.get();
      U8HexPtr(gui, ppuRes, ppuID);
      BoolPtr(gui, ppuRes, valid);
      HexPtr(gui, ppuRes, reservedAddr);
    });
    Hex(gui, ppuRegisters, CIA);
    Hex(gui, ppuRegisters, NIA);
    Bool(gui, ppuRegisters, instrFetch);
    Hex(gui, ppuRegisters, exceptReg);
    Bool(gui, ppuRegisters, exceptionTaken);
    Hex(gui, ppuRegisters, exceptEA);
    Hex(gui, ppuRegisters, exceptTrapType);
    Bool(gui, ppuRegisters, exceptHVSysCall);
    Hex(gui, ppuRegisters, intEA);
    Hex(gui, ppuRegisters, lastWriteAddress);
    Hex(gui, ppuRegisters, lastRegValue);
  });
}

bool rebuildThreadDS[6]{};
bool builtWithDisassembly[6]{};
void PPUThreadDockSpace(Render::GUI *gui, PPU_STATE *state, ePPUThread thr) {
  gui->SimpleWindow(fmt::format("{} [{}]", static_cast<u8>(thr), state->ppuName), [&] {
    std::string id = fmt::format("{}:{}_DS", state->ppuName, static_cast<u8>(thr));
    PPU_THREAD_REGISTERS &thread = state->ppuThread[thr];
    ImGuiID dsId = ImGui::GetID(id.c_str());
    if (!ImGui::DockBuilderGetNode(dsId) || (rebuildThreadDS[thread.SPR.PIR] && !builtWithDisassembly[thread.SPR.PIR])) {
      ImGui::DockBuilderRemoveNode(dsId);
      ImGui::DockBuilderAddNode(dsId, ImGuiDockNodeFlags_DockSpace);
      if (thread.CIA != 0) {
        builtWithDisassembly[thread.SPR.PIR] = true;
        ImGuiID left, right;
        left = ImGui::DockBuilderSplitNode(dsId, ImGuiDir_Left, 1.f, nullptr, &right);
        std::string disassemblyId = fmt::format("Diassembly [{}:{}]", state->ppuName, static_cast<u8>(thr));
        std::string registersId = fmt::format("Registers [{}:{}]", state->ppuName, static_cast<u8>(thr));
        ImGui::DockBuilderDockWindow(disassemblyId.c_str(), left);
        ImGui::DockBuilderDockWindow(registersId.c_str(), right);
      } else {
        std::string registersId = fmt::format("Registers [{}:{}]", state->ppuName, static_cast<u8>(thr));
        ImGui::DockBuilderDockWindow(registersId.c_str(), dsId);
      }

      ImGui::DockBuilderFinish(dsId);
      rebuildThreadDS[thread.SPR.PIR] = false;
    }
    ImGui::DockSpace(dsId);
    
    if (thread.CIA != 0) {
      rebuildThreadDS[thread.SPR.PIR] = true;
      PPUThreadDiassembly(gui, state, thr);
    }
    PPUThreadRegisters(gui, state, thr);
  });
}

void PPURegisters(Render::GUI *gui, PPU_STATE *state) {
  gui->SimpleWindow(fmt::format("Registers [{}]", state->ppuID), [&] {
    Xenon *CPU = Xe_Main->getCPU();
    gui->Node("SPR", [&] {
      PPU_STATE_SPRS &SPR = state->SPR;
      Hex(gui, SPR, SDR1);
      Hex(gui, SPR, CTRL);
      Hex(gui, SPR, TB);
      gui->Node("PVR", [&] {
        PVRegister &PVR = SPR.PVR;
        Hex(gui, PVR, PVR_Hex);
        U8Hex(gui, PVR, Revision);
        U8Hex(gui, PVR, Version);
      }, ImGuiTreeNodeFlags_DefaultOpen);
      Hex(gui, SPR, HDEC);
      Hex(gui, SPR, RMOR);
      Hex(gui, SPR, HRMOR);
      Hex(gui, SPR, LPCR);
      Hex(gui, SPR, LPIDR);
      Hex(gui, SPR, TSCR);
      Hex(gui, SPR, TTR);
      Hex(gui, SPR, PPE_TLB_Index);
      Hex(gui, SPR, PPE_TLB_VPN);
      Hex(gui, SPR, PPE_TLB_RPN);
      Hex(gui, SPR, PPE_TLB_RMT);
      Hex(gui, SPR, HID0);
      Hex(gui, SPR, HID1);
      Hex(gui, SPR, HID4);
      Hex(gui, SPR, HID6);
    });
    gui->Node("TLB", [&] {
      TLB_Reg &TLB = state->TLB;
      gui->Node("tlbSet0", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry &TLBEntry = TLB.tlbSet0[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(gui, TLBEntry, V);
            Hex(gui, TLBEntry, pte0);
            Hex(gui, TLBEntry, pte1);
          });
        }
      });
      gui->Node("tlbSet1", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry &TLBEntry = TLB.tlbSet1[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(gui, TLBEntry, V);
            Hex(gui, TLBEntry, pte0);
            Hex(gui, TLBEntry, pte1);
          });
        }
      });
      gui->Node("tlbSet2", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry &TLBEntry = TLB.tlbSet2[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(gui, TLBEntry, V);
            Hex(gui, TLBEntry, pte0);
            Hex(gui, TLBEntry, pte1);
          });
        }
      });
      gui->Node("tlbSet3", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry &TLBEntry = TLB.tlbSet3[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(gui, TLBEntry, V);
            Hex(gui, TLBEntry, pte0);
            Hex(gui, TLBEntry, pte1);
          });
        }
      });
    });
    Custom(gui, ppuName, "{}", state->ppuName);
    U8DecPtr(gui, state, currentThread);
    BoolPtr(gui, state, translationInProgress);
  });
}

void PPUDockSpace(Render::GUI *gui, PPU *PPU) {
  if (!PPU)
    return;

  PPU_STATE *state = PPU->GetPPUState();
  if (!state)
    return;

  gui->SimpleWindow(state->ppuName, [&] {
    gui->MenuBar([&] {
      bool halted = PPU->IsHalted();
      gui->MenuItem(halted ? "Continue" : "Pause", [&PPU, halted] {
        if (halted)
          PPU->Continue();
        else
          PPU->Halt();
      });
      if (PPU->IsHaltedByGuest()) {
        gui->MenuItem("Continue From Exception Handler", [&PPU] {
          PPU->ContinueFromException();
        });
      }
    });
    std::string id = fmt::format("{}_DS", state->ppuName);
    ImGuiID dsId = ImGui::GetID(id.c_str());
    if (!ImGui::DockBuilderGetNode(dsId)) {
      ImGui::DockBuilderRemoveNode(dsId);
      ImGui::DockBuilderAddNode(dsId, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dsId, ImGui::GetMainViewport()->Size);
      ImGuiID top, bottom;
      ImGui::DockBuilderSplitNode(dsId, ImGuiDir_Up, 0.f, &top, &bottom);

      std::string registersId = fmt::format("Registers [{}]", state->ppuID);
      std::string thread0Id = fmt::format("{} [{}]", 0, state->ppuName);
      std::string thread1Id = fmt::format("{} [{}]", 1, state->ppuName);
      ImGui::DockBuilderDockWindow(registersId.c_str(), top);
      ImGui::DockBuilderDockWindow(thread0Id.c_str(), bottom);
      ImGui::DockBuilderDockWindow(thread1Id.c_str(), bottom);

      ImGui::DockBuilderFinish(dsId);
    }
    ImGui::DockSpace(dsId);

    PPURegisters(gui, state);
    for (u8 i = 0; i != 2; ++i) {
      PPUThreadDockSpace(gui, state, static_cast<ePPUThread>(i));
    }
  }, &gui->ppcDebuggerActive[state->ppuID], ImGuiWindowFlags_MenuBar);
}

bool rebuildDock = false;
u8 activeCountOnBuild = 0;
void DebuggerDockSpace(Render::GUI *gui) {
  u8 activeCount = 0;
  for (u8 i = 0; i != 3; ++i) {
    if (gui->ppcDebuggerActive[i])
      ++activeCount;
  }
  if (activeCountOnBuild != activeCount && activeCount)
    rebuildDock = true;

  if (!activeCount) {
    return;
  }

  ImGuiID dsId = ImGui::GetID("DebuggerDS");
  if (!ImGui::DockBuilderGetNode(dsId) || rebuildDock) {
    activeCountOnBuild = activeCount;
    ImGui::DockBuilderRemoveNode(dsId);
    ImGui::DockBuilderAddNode(dsId, ImGuiDockNodeFlags_DockSpace);
    if (activeCount == 3) {
      ImGuiID center = dsId;
      ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 1.f, nullptr, &center);
      ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 1.f, nullptr, &center);

      ImGui::DockBuilderDockWindow("PPU0", left);
      ImGui::DockBuilderDockWindow("PPU1", center);
      ImGui::DockBuilderDockWindow("PPU2", right);
    } else {
      if (activeCount == 2) {
        ImGuiID left, right;
        left = ImGui::DockBuilderSplitNode(dsId, ImGuiDir_Left, 1.f, nullptr, &right);

        if (gui->ppcDebuggerActive[0]) {
          ImGui::DockBuilderDockWindow("PPU0", left);
          if (gui->ppcDebuggerActive[1])
            ImGui::DockBuilderDockWindow("PPU1", right);
          else if (gui->ppcDebuggerActive[2])
            ImGui::DockBuilderDockWindow("PPU2", right);
        } else if (gui->ppcDebuggerActive[1]) {
          ImGui::DockBuilderDockWindow("PPU1", left);
          if (gui->ppcDebuggerActive[2])
            ImGui::DockBuilderDockWindow("PPU2", right);
        }
      } else if (activeCount == 1) {
        if (gui->ppcDebuggerActive[0])
          ImGui::DockBuilderDockWindow("PPU0", dsId);
        else if (gui->ppcDebuggerActive[1])
          ImGui::DockBuilderDockWindow("PPU1", dsId);
        else if (gui->ppcDebuggerActive[2])
          ImGui::DockBuilderDockWindow("PPU2", dsId);
      }
    }
    ImGui::DockBuilderFinish(dsId);
    rebuildDock = false;
  }
  ImGui::DockSpace(dsId);

  if (Xe_Main.get()) {
    Xenon *CPU = Xe_Main->getCPU();
    for (u8 ppuID = 0; ppuID != 3; ++ppuID) {
      if (CPU && gui->ppcDebuggerActive[ppuID]) {
        PPU *PPU = CPU->GetPPU(ppuID);
        PPUDockSpace(gui, PPU);
      }
    }
  }
}

void LogSettings(Render::GUI *gui) {
  static s32 logLevel = static_cast<s32>(Config::log.currentLevel);
  gui->Toggle("Advanced", &Config::log.advanced);
  gui->Tooltip("Enables more advanced logging ");
#ifdef DEBUG_BUILD
  gui->Toggle("Debug Only", &Config::log.debugOnly);
  gui->Tooltip("Enables heavy logging for Debug purposes. Do not enable, causes extreme preformance loss");
#endif
}

void GraphicsSettings(Render::GUI *gui) {
  gui->Toggle("Enable", &Config::rendering.enable);
  gui->Tooltip("Enable GPU Rendering thread (Disabling this will kill rendering on next startup)");
  gui->Toggle("Enable GUI", &Config::rendering.enableGui);
  gui->Tooltip("Whether to create the GUI handle");
  gui->Toggle("Fullscreen", &Config::rendering.isFullscreen, [&] {
    Xe_Main->renderer->fullscreen = Config::rendering.isFullscreen;
    SDL_SetWindowFullscreen(gui->mainWindow, Xe_Main->renderer->fullscreen);
  });
  gui->Toggle("VSync", &Config::rendering.vsync, [&] {
    Xe_Main->renderer->VSYNC = Config::rendering.vsync;
    SDL_GL_SetSwapInterval(Xe_Main->renderer->VSYNC ? 1 : 0);
  });
  gui->Toggle("Exit on window close", &Config::rendering.quitOnWindowClosure);
}

void XCPUSettings(Render::GUI *gui) {
  if (Xe_Main->CPUStarted) {
    gui->Button("Shutdown", [] {
      Xe_Main->shutdownCPU();
    });
  } else {
    gui->Button("Start", [] {
      Xe_Main->start();
    });
  }
  gui->Button("Reboot", [] {
    Xe_Main->reboot(Xe_Main->smcCoreState->currPowerOnReason);
  });
  gui->Toggle("Load Elf", &Config::xcpu.elfLoader);
  gui->Toggle("RGH2 Init Skip (Corona Only)", &RGH2, [] {
    if (!storedPreviousInitSkips && !RGH2) {
      initSkip1 = Config::xcpu.HW_INIT_SKIP_1;
      initSkip2 = Config::xcpu.HW_INIT_SKIP_2;
      storedPreviousInitSkips = true;
    }
    Config::xcpu.HW_INIT_SKIP_1 = RGH2 ? 0x3003DC0 : initSkip1;
    Config::xcpu.HW_INIT_SKIP_2 = RGH2 ? 0x3003E54 : initSkip2;
  });
}

void SMCSettings(Render::GUI *gui) {
  Config::smc.uartSystem = gui->InputText("UART System", Config::smc.uartSystem);
#ifdef _WIN32
  gui->InputInt("vCOM Port", &Config::smc.comPort);
  gui->Tooltip("Note: a Virtual COM drier is needed, please use a different UART system if you do not have one");
#endif
  Config::smc.socketIp = gui->InputText("Socket IP", Config::smc.socketIp);
  gui->Tooltip("Decides which IP the UART netcat/socat implementation listens for");
  gui->InputInt("Socket Port", &Config::smc.socketPort);
  gui->Tooltip("Decides which port the UART netcat/socat implementation listens for");
  gui->InputInt("Power On Reason", &Config::smc.powerOnReason);
  gui->Tooltip("17 is Power Button, 18 is Eject Button");
}

void PathSettings(Render::GUI *gui) {
  Config::filepaths.fuses = gui->InputText("Fuses", Config::filepaths.fuses);
  Config::filepaths.oneBl = gui->InputText("1bl", Config::filepaths.oneBl);
  Config::filepaths.nand = gui->InputText("NAND", Config::filepaths.nand);
  Config::filepaths.elfBinary = gui->InputText("ELF Binary", Config::filepaths.elfBinary);
  Config::filepaths.oddImage = gui->InputText("ODD Image File (iso)", Config::filepaths.oddImage);
  gui->Button("Reload files", [] {
    Xe_Main->reloadFiles();
  });
  gui->Tooltip("Warning: It is *highly* recommended you shutdown the CPU before reloading files");
}

void ImGuiSettings(Render::GUI *gui) {
  gui->Toggle("Style Editor", &gui->styleEditor);
  gui->Toggle("Demo", &gui->demoWindow);
  gui->Toggle("Viewports", &Config::imgui.viewports, [&] {
    ImGuiIO &io = ImGui::GetIO();
    if (Config::imgui.viewports)
      io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    else
      io.ConfigFlags &= ~(ImGuiConfigFlags_ViewportsEnable);
  });
  gui->Tooltip("Allows ImGui windows to be 'detached' from the main window. Useful for debugging");
  Config::imgui.configPath = gui->InputText("Config path", Config::imgui.configPath);
  gui->Tooltip("Where imgui.ini is present (none is disabled)");
}

void ConfigSettings(Render::GUI *gui) {
  gui->Button("Save", [] {
    Xe_Main->saveConfig();
  });
  gui->Button("Load", [] {
    Xe_Main->loadConfig();
  });
}

void Render::GUI::OnSwap(Texture *texture) {
  if (ppcDebuggerDetached) {
    Window("PPC Debugger", [this] {
      TabBar("##debugger", [&] {
        if (!ppcDebuggerDetached) {
          DebuggerDockSpace(this);
        }
        for (u8 i = 0; i != 3; ++i) {
          TabItemButton("PPU" + std::to_string(i), [&] {
            rebuildDock = true;
            ppcDebuggerActive[i] ^= true;
          });
          ImGui::SameLine();
        }
        TabItemButton("All", [&] {
          rebuildDock = true;
          for (bool& a : ppcDebuggerActive)
            a ^= true;
        });
        Xenon *CPU = Xe_Main->getCPU();
        bool halted = CPU->IsHalted();
        TabItemButton(halted ? "Continue" : "Pause", [&] {
          if (halted)
            CPU->Continue();
          else
            CPU->Halt();
        });
      });
    }, { 1200.f, 700.f }, ImGuiWindowFlags_None, &ppcDebuggerDetached, { 500.f, 100.f });
  }
  if (Config::imgui.debugWindow) {
    Window("Debug", [&] {
      TabBar("##main", [&] {
        TabItem("Debug", [&] {
          TabBar("##debug", [&] {
            if (!ppcDebuggerDetached) {
              DebuggerDockSpace(this);
            }
            for (u8 i = 0; i != 3; ++i) {
              TabItemButton("PPU" + std::to_string(i), [&] {
                rebuildDock = true;
                ppcDebuggerActive[i] ^= true;
              });
              ImGui::SameLine();
            }
            TabItemButton("All", [&] {
              rebuildDock = true;
              for (bool& a : ppcDebuggerActive)
                a ^= true;
            });
            if (Xe_Main.get()) {
              Xenon *CPU = Xe_Main->getCPU();
              if (CPU) {
                bool halted = CPU->IsHalted();
                TabItemButton(halted ? "Continue" : "Pause", [&] {
                  if (halted)
                    CPU->Continue();
                  else
                    CPU->Halt();
                });
              }
            }
          });
        });
#if defined(MICROPROFILE_ENABLED) && MICROPROFILE_ENABLED
        TabItem("Profiler", [&] {
#ifdef MICROPROFILE_WEBSERVER
          Button("Open", [&] {
            std::string url = fmt::format("http://127.0.0.1:{}/", MicroProfileWebServerPort());
#ifdef _WIN32
            ShellExecuteA(nullptr, "open", url.data(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__linux__)
            std::string command = "xdg-open " + url;
            s32 result = std::system(command.c_str());
#endif
          });
#else
          //TODO: Implement some system to display it in ImGui
#endif
        });
#endif
        TabItem("Dump", [&] {
          Button("Dump FB", [&] {
            const auto UserDir = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
            Xe_Main->xenos->DumpFB(UserDir / "fbmem.bin", Xe_Main->renderer->pitch);
          });
          Button("Dump Memory", [&] {
            const auto UserDir = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
            const auto& path = UserDir / "memory.bin";
            std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!f) {
              LOG_ERROR(Xenon, "Failed to open {} for writing", path.filename().string());
            }
            else {
              RAM *ramPtr = Xe_Main->ram.get();
              f.write(reinterpret_cast<const char*>(ramPtr->getPointerToAddress(0)), RAM_SIZE);
              LOG_INFO(Xenon, "RAM dumped to '{}' (size: 0x{:08X})", path.string(), RAM_SIZE);
            }
            f.close();
          });
        });
        TabItem("Settings", [&] {
          TabBar("##settings", [&] {
            TabItem("CPU", [&] {
              XCPUSettings(this);
            });
            TabItem("SMC", [&] {
              SMCSettings(this);
            });
            TabItem("General", [&] {
              Button("Exit", [] {
                Xe_Main->shutdown();
              });
              Tooltip("Cleanly exits the process");
              Button("Soft exit", [] {
                s32 exitCode = Base::exit(0);
                LOG_INFO(Xenon, "Exited with code '{}'", exitCode);
              });
              Tooltip("Uses 'exit(0);' instead of properly shutting down");
              Button("Force exit", [] {
                s32 exitCode = Base::fexit(0);
                LOG_INFO(Xenon, "Exited with code '{}'", exitCode);
              });
              Tooltip("Forcefully closes the process using TerminateProcess and _exit");
            });
            TabItem("Log", [&] {
              LogSettings(this);
            });
            TabItem("Paths", [&] {
              PathSettings(this);
            });
            TabItem("Graphics", [&] {
              GraphicsSettings(this);
            });
            TabItem("ImGui", [&] {
              ImGuiSettings(this);
            });
            TabItem("Config", [&] {
              ConfigSettings(this);
            });
          });
        });
      });
    }, { 1200.f, 700.f }, ImGuiWindowFlags_None, &Config::imgui.debugWindow, { 1000.f, 400.f });
  }
}

void Render::GUI::Render(Texture* texture) {
  BeginSwap();
  ImGui::NewFrame();
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
  if (styleEditor) {
    Window("Style Editor", [] {
      ImGui::ShowStyleEditor();
    }, { 1000.f, 900.f }, ImGuiWindowFlags_NoCollapse, &styleEditor, { 600.f, 60.f });
  }
  if (demoWindow) {
    ImGui::ShowDemoWindow(&demoWindow);
  }
  OnSwap(texture);
  ImGui::EndFrame();
  ImGui::Render();
  EndSwap();
}
void Render::GUI::SetStyle() {
  ImGuiIO &io = ImGui::GetIO();
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;
  // Colors
  colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
  colors[ImGuiCol_TextDisabled] = ImColor(255, 230, 49, 255);
  colors[ImGuiCol_WindowBg] = ImColor(15, 15, 15, 248);
  colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0, 0);
  colors[ImGuiCol_PopupBg] = ImColor(20, 20, 20, 240);
  colors[ImGuiCol_Border] = ImColor(255, 255, 255, 200);
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
  colors[ImGuiCol_Header] = ImColor(110, 210, 50, 79);
  colors[ImGuiCol_HeaderHovered] = ImColor(109, 232, 0, 94);
  colors[ImGuiCol_HeaderActive] = ImColor(108, 232, 0, 130);
  colors[ImGuiCol_Separator] = ImColor(97, 97, 97, 127);
  colors[ImGuiCol_SeparatorHovered] = ImColor(117, 117, 117, 127);
  colors[ImGuiCol_SeparatorActive] = ImColor(117, 117, 117, 163);
  colors[ImGuiCol_ResizeGrip] = ImColor(0, 0, 0, 0);
  colors[ImGuiCol_ResizeGripHovered] = ImColor(108, 232, 0, 255);
  colors[ImGuiCol_ResizeGripActive] = ImColor(111, 210, 50, 255);

  colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
  colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.8f);
  colors[ImGuiCol_TabSelected] = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.6f);
  colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_HeaderActive];
  colors[ImGuiCol_TabDimmed] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
  colors[ImGuiCol_TabDimmedSelected] = ImLerp(colors[ImGuiCol_TabSelected], colors[ImGuiCol_TitleBg], 0.4f);
  colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(135, 135, 221, 0);
  colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_Header] * ImColor(255, 255, 255, 178);

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
  style.WindowRounding = 5.f;
  style.WindowBorderSize = 1.f;
  style.WindowMinSize = { 200.f, 200.f };
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
  style.TabRounding = 4.f;
  style.TabBorderSize = 1.f;
  style.TabBarBorderSize = 0.5f;
  style.TabBarOverlineSize = 0.f;
  style.ButtonTextAlign = { 0.5f, 0.5f };
  style.DisplaySafeAreaPadding = { 3.f, 22.f };
  style.MouseCursorScale = 0.7f;
  // Change some style vars for Viewports
  if (io.ConfigFlags  &ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }
}
#endif
