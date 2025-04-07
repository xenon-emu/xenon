// Copyright 2025 Xenon Emulator Project

#include "GUI.h"

#ifndef NO_GFX
#include "Core/Xe_Main.h"
#include "Base/Exit.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

// Helper functions for text/string formatting with the gui
#define TextFmt(g, x, ...) g->Text(fmt::format(x, __VA_ARGS__))
#define TextCopyFmt(g, x, v, ...) g->TextCopy(x, fmt::format(v, __VA_ARGS__))
#define CustomBase(g, x, fmt, ...) TextFmt(g, x ": " fmt, __VA_ARGS__)
#define CopyCustomBase(g, x, fmt, ...) TextCopyFmt(g, x, fmt, __VA_ARGS__)
#define Custom(g, x, fmt, ...) CustomBase(g, #x, fmt, __VA_ARGS__)
#define CopyCustom(g, x, fmt, ...) CustomBase(g, #x, fmt, __VA_ARGS__)
#define HexBase(g, x, ...) CopyCustomBase(g, x, "0x{:X}", __VA_ARGS__)
#define Hex(g, c, x) HexBase(g, #x, c.x)
#define BFHex(g, c, x) HexBase(g, #x, u32(c.x));
#define U8Hex(g, c, x) HexBase(g, #x, static_cast<u32>(c.x))
#define HexArr(g, a, i) HexBase(g, fmt::format("[{}]", i), a[i])
#define Dec(g, c, x) CopyCustom(g, x, "{}", c.x)
#define U8Dec(g, c, x) CopyCustom(g, x, "{}", static_cast<u32>(c.x))
#define Bool(g, c, x) CopyCustom(g, x, "{}", c.x ? "true" : "false")

void Render::GUI::Init(SDL_Window* window, void* context) {
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
int initSkip1{}, initSkip2{};
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
void Render::GUI::Window(const std::string &title, std::function<void()> callback, const ImVec2 &size, ImGuiWindowFlags flags, bool* conditon, const ImVec2 &position, ImGuiCond cond) {
  ImGui::SetNextWindowPos(position, cond);
  ImGui::SetNextWindowSize(size, cond);

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

void Render::GUI::SameLine(float xOffset, float spacing) {
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

void Render::GUI::TabItem(const std::string &title, std::function<void()> callback, bool* conditon, ImGuiTabItemFlags flags) {
  if (ImGui::BeginTabItem(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
    ImGui::EndTabItem();
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

void Render::GUI::InputInt(const std::string &label, u32 *value, u32 step, u32 stepFast) {
  ImGui::InputScalar(label.c_str(), ImGuiDataType_U32, (void*)value, (void*)(step > 0 ? &step : NULL), (void*)(stepFast > 0 ? &stepFast : NULL), "%d");
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

void PPUThread(Render::GUI *gui, PPU_STATE *PPUState, ePPUThread threadID) {
  gui->Node(fmt::format("[{}]", static_cast<u8>(threadID)), [&] {
    PPU_THREAD_REGISTERS &ppuRegisters = PPUState->ppuThread[threadID];
    gui->Node("SPR", [&] {
      PPU_THREAD_SPRS &SPR = ppuRegisters.SPR;
      gui->Node("XER", [&] {
        XERegister &XER = SPR.XER;
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
      gui->Node("MSR", [&] {
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
      });
      Hex(gui, SPR, PIR);
    });
    Hex(gui, ppuRegisters, CIA);
    Hex(gui, ppuRegisters, NIA);
    gui->Node("CI", [&] {
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
    Bool(gui, ppuRegisters, iFetch);
    gui->Node("GPRs", [&] {
      for (u64 i = 0; i != 32; ++i) {
        HexArr(gui, ppuRegisters.GPR, i);
      }
    });
    gui->Node("FPRs", [&] {
      for (u64 i = 0; i != 32; ++i) {
        FPRegister &FPR = ppuRegisters.FPR[i];
        gui->Node(fmt::format("[{}]", i), [&] {
          Custom(gui, valueAsDouble, "{}", FPR.valueAsDouble);
          Custom(gui, valueAsU64, "0x{:X}",FPR.valueAsU64);
        });
      }
    });
    gui->Node("CR", [&] {
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
    gui->Node("FPSCR", [&] {
      FPSCRegister &FPSCR = ppuRegisters.FPSCR;
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
      BFHex(gui, FPSCR, FPRF);
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
    gui->Node("SLBs", [&] {
      for (u64 i = 0; i != 64; ++i) {
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
    Hex(gui, ppuRegisters, exceptReg);
    Bool(gui, ppuRegisters, exceptionTaken);
    Hex(gui, ppuRegisters, exceptEA);
    Hex(gui, ppuRegisters, exceptTrapType);
    Bool(gui, ppuRegisters, exceptHVSysCall);
    Hex(gui, ppuRegisters, intEA);
    Hex(gui, ppuRegisters, lastWriteAddress);
    Hex(gui, ppuRegisters, lastRegValue);
    gui->Node("ppuRes", [&] {
        PPU_RES &ppuRes = *(ppuRegisters.ppuRes.get());
        U8Hex(gui, ppuRes, ppuID);
        Bool(gui, ppuRes, V);
        Hex(gui, ppuRes, resAddr);
    });
  });
}

void RenderInstr(Render::GUI *gui, u32 addr, u32 instr) {
  const std::string instrName = PPCInterpreter::ppcDecoder.decodeName(instr);
  const u32 b0 = static_cast<u8>((instr >> 24)  &0xFF);
  const u32 b1 = static_cast<u8>((instr >> 16)  &0xFF);
  const u32 b2 = static_cast<u8>((instr >> 8)  &0xFF);
  const u32 b3 = static_cast<u8>((instr >> 0)  &0xFF);
  TextFmt(gui, "{:08X} {:02X} {:02X} {:02X} {:02X}                             {}", addr, b0, b1, b2, b3, instrName);
}

void PPC_PPU(Render::GUI *gui, PPU *PPU) {
  if (!PPU) {
    return;
  }
  PPU_STATE *PPUStatePtr = PPU->GetPPUState();
  #define ppuState PPUStatePtr
  PPU_STATE &PPUState = *PPUStatePtr;
  gui->Node(PPUState.ppuName, [&] {
    u32 curInstr = _instr.opcode;
    u32 nextInstr = _nextinstr.opcode;
    RenderInstr(gui, curThread.CIA, curInstr);
    RenderInstr(gui, curThread.NIA, nextInstr);
    gui->Node("ppuThread", [&] {
      PPUThread(gui, PPUStatePtr, ePPUThread_Zero);
      PPUThread(gui, PPUStatePtr, ePPUThread_One);
    });
    U8Dec(gui, PPUState, currentThread);
    gui->Node("SPR", [&] {
      PPU_STATE_SPRS &SPR = PPUState.SPR;
      Hex(gui, SPR, SDR1);
      Hex(gui, SPR, CTRL);
      Hex(gui, SPR, TB);
      gui->Node("PVR", [&] {
        PVRegister &PVR = SPR.PVR;
        Hex(gui, PVR, PVR_Hex);
        U8Hex(gui, PVR, Revision);
        U8Hex(gui, PVR, Version);
      });
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
      TLB_Reg &TLB = PPUState.TLB;
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
    Bool(gui, PPUState, translationInProgress);
    Custom(gui, ppuName, "{}", PPUState.ppuName);
  }, ImGuiTreeNodeFlags_DefaultOpen);
}
void PPCDebugger(Render::GUI *gui) {
  ImGuiWindow *window = ImGui::GetCurrentWindow();
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2.f, 8.f });
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 8.f, 4.f });
  gui->Child("##instrs", [&] {
    gui->MenuBar([&gui] {
      gui->Menu("Window", [&gui] {  
        if (!gui->ppcDebuggerActive) {
          gui->MenuItem("Enable", [&] {
            gui->ppcDebuggerActive = true;
          });
        } else {
          gui->MenuItem(gui->ppcDebuggerAttached ? "Detach" : "Attach", [&] {
            gui->ppcDebuggerAttached ^= true;
          });
          gui->MenuItem("Disable", [&] {
            gui->ppcDebuggerActive = false;
          });
        }
      });
      gui->Menu("Debug", [&gui] {
        if (!Xe_Main->getCPU()->IsHalted()) {
          gui->MenuItem("Pause", [&gui] {
            Xe_Main->getCPU()->Halt();
          });
          gui->MenuItem("Exit (Soft)", [] {
            Xe_Main->shutdown();
          });
          gui->MenuItem("Exit (Force)", [] {
            exit(0);
          });
        } else {
          gui->MenuItem("Continue", [&gui] {
            Xe_Main->getCPU()->Continue();
          });
          gui->InputInt("Amount", &gui->stepAmount);
          gui->MenuItem("Step (F10)", [&gui] {
            Xe_Main->getCPU()->Step(gui->stepAmount);
          });
        }
      });
    });
    if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
      Xe_Main->getCPU()->Step(gui->stepAmount);
    }
    Xenon *CPU = Xe_Main->getCPU();
    PPC_PPU(gui, CPU->GetPPU(0));
    PPC_PPU(gui, CPU->GetPPU(1));
    PPC_PPU(gui, CPU->GetPPU(2));
  }, { window->Size.x - 27.5f, window->Size.y - 74.f }, ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_MenuBar);
  ImGui::PopStyleVar(4);
}

void LogSettings(Render::GUI *gui) {
  static int logLevel = static_cast<int>(Config::log.currentLevel);
  gui->Toggle("Advanced", &Config::log.advanced);
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

void CodeflowSettings(Render::GUI *gui) {
  gui->Toggle("RGH2 Init Skip", &RGH2, [] {
    if (!storedPreviousInitSkips && !RGH2) {
      initSkip1 = Config::xcpu.HW_INIT_SKIP_1;
      initSkip2 = Config::xcpu.HW_INIT_SKIP_2;
      storedPreviousInitSkips = true;
    }
    Config::xcpu.HW_INIT_SKIP_1 = RGH2 ? 0x3003DC0 : initSkip1;
    Config::xcpu.HW_INIT_SKIP_2 = RGH2 ? 0x3003E54 : initSkip2;
  });
  gui->Toggle("Load Elf", &Config::xcpu.elfLoader);
}

void PathSettings(Render::GUI *gui) {
  Config::filepaths.fuses = gui->InputText("Fuses", Config::filepaths.fuses);
  Config::filepaths.oneBl = gui->InputText("1bl", Config::filepaths.oneBl);
  Config::filepaths.nand = gui->InputText("NAND", Config::filepaths.nand);
  Config::filepaths.elfBinary = gui->InputText("ELF Binary", Config::filepaths.elfBinary);
  Config::filepaths.oddImage = gui->InputText("ODD Image File (iso)", Config::filepaths.oddImage);
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
  if (ppcDebuggerActive && !ppcDebuggerAttached) {
    Window("PPC Debugger", [this] {
      PPCDebugger(this);
    }, { 1200.f, 700.f }, ImGuiWindowFlags_None, &ppcDebuggerActive, { 500.f, 100.f });
  }
  if (Config::imgui.debugWindow) {
    Window("Debug", [&] {
      TabBar("##main", [&] {
        TabItem("Debug", [&] {
          if (!ppcDebuggerActive) {
            Button("Start", [&] {
              ppcDebuggerActive = true;
            });
          }
          if (ppcDebuggerActive && ppcDebuggerAttached) {
            PPCDebugger(this);
          }
        });
        TabItem("Dump", [&] {
          Button("Dump FB", [&] {
            const auto UserDir = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
            Xe_Main->xenos->DumpFB(UserDir / "fbmem.bin", Xe_Main->renderer->pitch);
          });
        });
        // This whole section is broken
        /*TabItem("CPU", [&] {
          Button("Re-start execution", [&] {
            //Xe_Main->xenonCPU->Halt();
            Xe_Main->start();
          });
          Button("Re-run CPI Test", [&] {
            Xe_Main->xenonCPU->Halt();
            PPU *PPU = Xe_Main->xenonCPU->GetPPU(0);
            PPU->CalculateCPI();
            LOG_INFO(Xenon, "PPU0: {} clocks per instruction", PPU->GetCPI());
            Xe_Main->xenonCPU->Continue();
          });
        });*/
        TabItem("Settings", [&] {
          TabBar("##settings", [&] {
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
            TabItem("Codeflow", [&] {
              CodeflowSettings(this);
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
