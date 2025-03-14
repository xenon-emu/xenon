// Copyright 2025 Xenon Emulator Project

#include "Core/Xe_Main.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

#include "GUI.h"

// Helper functions for text/string formatting with the gui
#define TextFmt(x, ...) Text(fmt::format(x, __VA_ARGS__))
#define CustomBase(x, fmt, ...) TextFmt(x ": " fmt, __VA_ARGS__)
#define Custom(x, fmt, ...) CustomBase(#x, fmt, __VA_ARGS__)
#define HexBase(x, ...) CustomBase(x, "0x{:X}", __VA_ARGS__)
#define Hex(c, x) HexBase(#x, c.x)
#define BFHex(c, x) HexBase(#x, u32(c.x))
#define U8Hex(c, x) HexBase(#x, static_cast<u32>(c.x))
#define HexArr(a, i) HexBase("[{}]", i, a[i])
#define Dec(c, x) Custom(x, "{}", c.x)
#define U8Dec(c, x) Custom(x, "{}", static_cast<u32>(c.x))
#define Bool(c, x) Custom(x, "{}", c.x ? "true" : "false")

void Render::GUI::Init(SDL_Window* window, void* context) {
  // Set our mainWindow handle
  mainWindow = window;

  // Check ImGui version
  IMGUI_CHECKVERSION();
  // Create ImGui Context
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  // We don't want to create a ini because it stores positions.
  // Because we initialize with a 1280x720 window, then resize to whatever,
  // this will break the window positions, causing them to render off screen
  const std::string iniPath = Config::imguiIniPath();
  io.IniFilename = iniPath.compare("none") ? iniPath.data() : nullptr;
  // Enable ImGui Keyboard Navigation
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Enable ImGui Gamepad Navigation
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  if (viewports) {
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
  ImGuiIO& io = ImGui::GetIO();
  // It might not be a bad idea to take the Xbox 360 font and convert it to TTF
  const std::filesystem::path fontsPath{ Base::FS::GetUserPath(Base::FS::PathType::FontDir) };
  const std::string robotoRegular = (fontsPath / "Roboto-Regular.ttf").string();
  robotRegular14 = io.Fonts->AddFontFromFileTTF(robotoRegular.c_str(), 14.f);
  defaultFont13 = io.Fonts->AddFontDefault();
  if (Config::SKIP_HW_INIT_1 == 0x3003DC0 && Config::SKIP_HW_INIT_2 == 0x3003E54) {
    storedPreviousInitSkips = true; // If we already have RGH2, ignore
    RGH2 = true;
  }
}

void Render::GUI::Shutdown() {
  ShutdownBackend();
  ImGui::DestroyContext();
}

//TODO(Vali0004): Make Windows into callbacks, so we can create a window from a different thread.
void Render::GUI::Window(const std::string& title, std::function<void()> callback, const ImVec2& size, ImGuiWindowFlags flags, bool* conditon, const ImVec2& position, ImGuiCond cond) {
  ImGui::SetNextWindowPos(position, cond);
  ImGui::SetNextWindowSize(size, cond);

  if (ImGui::Begin(title.c_str(), conditon, flags)) {
    if (callback) {
      callback();
    }
  }
  ImGui::End();
}

void Render::GUI::Child(const std::string& title, std::function<void()> callback, const ImVec2& size, ImGuiChildFlags flags, ImGuiWindowFlags windowFlags) {
  if (ImGui::BeginChild(title.c_str(), size, flags, windowFlags)) {
    if (callback) {
      callback();
    }
  }
  ImGui::EndChild();
}

void Render::GUI::Node(const std::string& title, std::function<void()> callback, ImGuiTreeNodeFlags flags) {
  if (ImGui::TreeNodeEx(title.c_str(), flags)) {
    if (callback) {
      callback();
    }
    ImGui::TreePop();
  }
}

void Render::GUI::Text(const std::string& label) {
  ImGui::TextUnformatted(label.c_str());
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

void Render::GUI::Menu(const std::string& title, std::function<void()> callback) {
  if (ImGui::BeginMenu(title.c_str())) {
    if (callback) {
      callback();
    }
    ImGui::EndMenu();
  }
}

void Render::GUI::MenuItem(const std::string& title, std::function<void()> callback, bool enabled, bool selected, const std::string& shortcut) {
  if (ImGui::MenuItem(title.c_str(), shortcut.c_str(), selected, enabled)) {
    if (callback) {
      callback();
    }
  }
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
    if (!ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None))
      return;
    ImGui::TextUnformatted(contents.c_str());
    ImGui::EndTooltip();
  }
}

void PPUThread(Render::GUI *gui, PPU_STATE *PPUState, u32 threadID) {
  gui->Node(fmt::format("[{}]", threadID), [&] {
    PPU_THREAD_REGISTERS &ppuRegisters = PPUState->ppuThread[threadID];
    gui->Node("SPR", [&] {
      PPU_THREAD_SPRS &SPR = ppuRegisters.SPR;
      gui->Node("XER", [&] {
        XERegister &XER = SPR.XER;
        gui->Hex(XER, XER_Hex);
        gui->BFHex(XER, ByteCount);
        gui->BFHex(XER, R0);
        gui->BFHex(XER, CA);
        gui->BFHex(XER, OV);
        gui->BFHex(XER, SO);
      });
      gui->Hex(SPR, LR);
      gui->Hex(SPR, CTR);
      gui->Hex(SPR, CFAR);
      gui->Hex(SPR, VRSAVE);
      gui->Hex(SPR, DSISR);
      gui->Hex(SPR, DAR);
      gui->Dec(SPR, DEC);
      gui->Hex(SPR, SRR0);
      gui->Hex(SPR, SRR1);
      gui->Hex(SPR, ACCR);
      gui->Hex(SPR, SPRG0);
      gui->Hex(SPR, SPRG1);
      gui->Hex(SPR, SPRG2);
      gui->Hex(SPR, SPRG3);
      gui->Hex(SPR, HSPRG0);
      gui->Hex(SPR, HSPRG1);
      gui->Hex(SPR, HSRR0);
      gui->Hex(SPR, HSRR1);
      gui->Hex(SPR, TSRL);
      gui->Hex(SPR, TSSR);
      gui->Hex(SPR, PPE_TLB_Index_Hint);
      gui->Hex(SPR, DABR);
      gui->Hex(SPR, DABRX);
      gui->Node("MSR", [&] {
        MSRegister &MSR = SPR.MSR;
        gui->BFHex(MSR, LE);
        gui->BFHex(MSR, RI);
        gui->BFHex(MSR, PMM);
        gui->BFHex(MSR, DR);
        gui->BFHex(MSR, IR);
        gui->BFHex(MSR, FE1);
        gui->BFHex(MSR, BE);
        gui->BFHex(MSR, SE);
        gui->BFHex(MSR, FE0);
        gui->BFHex(MSR, ME);
        gui->BFHex(MSR, FP);
        gui->BFHex(MSR, PR);
        gui->BFHex(MSR, EE);
        gui->BFHex(MSR, ILE);
        gui->BFHex(MSR, VXU);
        gui->BFHex(MSR, HV);
        gui->BFHex(MSR, TA);
        gui->BFHex(MSR, SF);
      });
      gui->Hex(SPR, PIR);
    });
    gui->Hex(ppuRegisters, CIA);
    gui->Hex(ppuRegisters, NIA);
    gui->Node("CI", [&] {
      PPCOpcode &CI = ppuRegisters.CI;
      gui->Hex(CI, opcode);
      gui->BFHex(CI, main);
      gui->BFHex(CI, sh64);
      gui->BFHex(CI, mbe64);
      gui->BFHex(CI, vuimm);
      gui->BFHex(CI, vs);
      gui->BFHex(CI, vsh);
      gui->BFHex(CI, oe);
      gui->BFHex(CI, spr);
      gui->BFHex(CI, vc);
      gui->BFHex(CI, vb);
      gui->BFHex(CI, va);
      gui->BFHex(CI, vd);
      gui->BFHex(CI, lk);
      gui->BFHex(CI, aa);
      gui->BFHex(CI, rb);
      gui->BFHex(CI, ra);
      gui->BFHex(CI, rd);
      gui->BFHex(CI, uimm16);
      gui->BFHex(CI, l11);
      gui->BFHex(CI, rs);
      gui->BFHex(CI, simm16);
      gui->BFHex(CI, ds);
      gui->BFHex(CI, vsimm);
      gui->BFHex(CI, ll);
      gui->BFHex(CI, li);
      gui->BFHex(CI, lev);
      gui->BFHex(CI, i);
      gui->BFHex(CI, crfs);
      gui->BFHex(CI, l10);
      gui->BFHex(CI, crfd);
      gui->BFHex(CI, crbb);
      gui->BFHex(CI, crba);
      gui->BFHex(CI, crbd);
      gui->BFHex(CI, rc);
      gui->BFHex(CI, me32);
      gui->BFHex(CI, mb32);
      gui->BFHex(CI, sh32);
      gui->BFHex(CI, bi);
      gui->BFHex(CI, bo);
      gui->BFHex(CI, bh);
      gui->BFHex(CI, frc);
      gui->BFHex(CI, frb);
      gui->BFHex(CI, fra);
      gui->BFHex(CI, frd);
      gui->BFHex(CI, crm);
      gui->BFHex(CI, frs);
      gui->BFHex(CI, flm);
      gui->BFHex(CI, l6);
      gui->BFHex(CI, l15);
      gui->BFHex(CI, bt14);
      gui->BFHex(CI, bt24);
    });
    gui->Bool(ppuRegisters, iFetch);
    gui->Node("GPRs", [&] {
      for (u64 i = 0; i != 32; ++i) {
        gui->HexArr(ppuRegisters.GPR, i);
      }
    });
    gui->Node("FPRs", [&] {
      for (u64 i = 0; i != 32; ++i) {
        FPRegister &FPR = ppuRegisters.FPR[i];
        gui->Node(fmt::format("[{}]", i), [&] {
          gui->Custom(valueAsDouble, "{}", FPR.valueAsDouble);
          gui->Custom(valueAsU64, "0x{:X}",FPR.valueAsU64);
        });
      }
    });
    gui->Node("CR", [&] {
      CRegister &CR = ppuRegisters.CR;
      gui->Hex(CR, CR_Hex);
      gui->BFHex(CR, CR0);
      gui->BFHex(CR, CR1);
      gui->BFHex(CR, CR2);
      gui->BFHex(CR, CR3);
      gui->BFHex(CR, CR4);
      gui->BFHex(CR, CR5);
      gui->BFHex(CR, CR6);
      gui->BFHex(CR, CR7);
    });
    gui->Node("FPSCR", [&] {
      FPSCRegister &FPSCR = ppuRegisters.FPSCR;
      gui->Hex(FPSCR, FPSCR_Hex);
      gui->BFHex(FPSCR, RN);
      gui->BFHex(FPSCR, NI);
      gui->BFHex(FPSCR, XE);
      gui->BFHex(FPSCR, ZE);
      gui->BFHex(FPSCR, UE);
      gui->BFHex(FPSCR, OE);
      gui->BFHex(FPSCR, VE);
      gui->BFHex(FPSCR, VXCVI);
      gui->BFHex(FPSCR, VXSQRT);
      gui->BFHex(FPSCR, VXSOFT);
      gui->BFHex(FPSCR, R0);
      gui->BFHex(FPSCR, FPRF);
      gui->BFHex(FPSCR, FI);
      gui->BFHex(FPSCR, FR);
      gui->BFHex(FPSCR, VXVC);
      gui->BFHex(FPSCR, VXIMZ);
      gui->BFHex(FPSCR, VXZDZ);
      gui->BFHex(FPSCR, VXIDI);
      gui->BFHex(FPSCR, VXISI);
      gui->BFHex(FPSCR, VXSNAN);
      gui->BFHex(FPSCR, XX);
      gui->BFHex(FPSCR, ZX);
      gui->BFHex(FPSCR, UX);
      gui->BFHex(FPSCR, OX);
      gui->BFHex(FPSCR, VX);
      gui->BFHex(FPSCR, FEX);
      gui->BFHex(FPSCR, FX);
    });
    gui->Node("SLBs", [&] {
      for (u64 i = 0; i != 64; ++i) {
        SLBEntry &SLB = ppuRegisters.SLB[i];
        gui->Node(fmt::format("[{}]", i), [&] {
          gui->U8Hex(SLB, V);
          gui->U8Hex(SLB, LP);
          gui->U8Hex(SLB, C);
          gui->U8Hex(SLB, L);
          gui->U8Hex(SLB, N);
          gui->U8Hex(SLB, Kp);
          gui->U8Hex(SLB, Ks);
          gui->Hex(SLB, VSID);
          gui->Hex(SLB, ESID);
          gui->Hex(SLB, vsidReg);
          gui->Hex(SLB, esidReg);
        });
      }
    });
    gui->Hex(ppuRegisters, exceptReg);
    gui->Bool(ppuRegisters, exceptionTaken);
    gui->Hex(ppuRegisters, exceptEA);
    gui->Hex(ppuRegisters, exceptTrapType);
    gui->Bool(ppuRegisters, exceptHVSysCall);
    gui->Hex(ppuRegisters, intEA);
    gui->Hex(ppuRegisters, lastWriteAddress);
    gui->Hex(ppuRegisters, lastRegValue);
    gui->Node("ppuRes", [&] {
        PPU_RES &ppuRes = *(ppuRegisters.ppuRes.get());
        gui->U8Hex(ppuRes, ppuID);
        gui->Bool(ppuRes, V);
        gui->Hex(ppuRes, resAddr);
    });
  });
}

void RenderInstr(Render::GUI *gui, u32 addr, u32 instr) {
  std::string instrName = PPCInterpreter::ppcDecoder.decodeName(instr);
  u32 b0 = static_cast<u8>((instr >> 24) & 0xFF);
  u32 b1 = static_cast<u8>((instr >> 16) & 0xFF);
  u32 b2 = static_cast<u8>((instr >> 8) & 0xFF);
  u32 b3 = static_cast<u8>((instr >> 0) & 0xFF);
  gui->TextFmt("{:08X} {:02X} {:02X} {:02X} {:02X}                             {}", addr, b0, b1, b2, b3, instrName);
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
    u32 nextInstr = PPCInterpreter::MMURead32(ppuState, curThread.NIA, true);
    RenderInstr(gui, curThread.CIA, curInstr);
    RenderInstr(gui, curThread.NIA, nextInstr);
    gui->Node("ppuThread", [&] {
      PPUThread(gui, PPUStatePtr, 0);
      PPUThread(gui, PPUStatePtr, 1);
    });
    gui->U8Dec(PPUState, currentThread);
    gui->Node("SPR", [&] {
      PPU_STATE_SPRS &SPR = PPUState.SPR;
      gui->Hex(SPR, SDR1);
      gui->Hex(SPR, CTRL);
      gui->Hex(SPR, TB);
      gui->Node("PVR", [&] {
        PVRegister &PVR = SPR.PVR;
        gui->Hex(PVR, PVR_Hex);
        gui->U8Hex(PVR, Revision);
        gui->U8Hex(PVR, Version);
      });
      gui->Hex(SPR, HDEC);
      gui->Hex(SPR, RMOR);
      gui->Hex(SPR, HRMOR);
      gui->Hex(SPR, LPCR);
      gui->Hex(SPR, LPIDR);
      gui->Hex(SPR, TSCR);
      gui->Hex(SPR, TTR);
      gui->Hex(SPR, PPE_TLB_Index);
      gui->Hex(SPR, PPE_TLB_VPN);
      gui->Hex(SPR, PPE_TLB_RPN);
      gui->Hex(SPR, PPE_TLB_RMT);
      gui->Hex(SPR, HID0);
      gui->Hex(SPR, HID1);
      gui->Hex(SPR, HID4);
      gui->Hex(SPR, HID6);
    });
    gui->Node("TLB", [&] {
      TLB_Reg &TLB = PPUState.TLB;
      gui->Node("tlbSet0", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet0[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            gui->Bool(TLBEntry, V);
            gui->U8Hex(TLBEntry, p);
            gui->Hex(TLBEntry, RPN);
            gui->Hex(TLBEntry, VPN);
            gui->Bool(TLBEntry, L);
            gui->Bool(TLBEntry, LP);
          });
        }
      });
      gui->Node("tlbSet1", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet1[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            gui->Bool(TLBEntry, V);
            gui->U8Hex(TLBEntry, p);
            gui->Hex(TLBEntry, RPN);
            gui->Hex(TLBEntry, VPN);
            gui->Bool(TLBEntry, L);
            gui->Bool(TLBEntry, LP);
          });
        }
      });
      gui->Node("tlbSet2", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet2[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            gui->Bool(TLBEntry, V);
            gui->U8Hex(TLBEntry, p);
            gui->Hex(TLBEntry, RPN);
            gui->Hex(TLBEntry, VPN);
            gui->Bool(TLBEntry, L);
            gui->Bool(TLBEntry, LP);
          });
        }
      });
      gui->Node("tlbSet3", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet3[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            gui->Bool(TLBEntry, V);
            gui->U8Hex(TLBEntry, p);
            gui->Hex(TLBEntry, RPN);
            gui->Hex(TLBEntry, VPN);
            gui->Bool(TLBEntry, L);
            gui->Bool(TLBEntry, LP);
          });
        }
      });
    });
    gui->Bool(PPUState, traslationInProgress);
    gui->Custom(ppuName, "{}", PPUState.ppuName);
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
        } else {
          gui->MenuItem("Continue", [&gui] {
            Xe_Main->getCPU()->Continue();
          });
          gui->MenuItem("Step (F10)", [&gui] {
            Xe_Main->getCPU()->Step();
          });
        }
      });
    });
    if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
      Xe_Main->getCPU()->Step();
    }
    Xenon *CPU = Xe_Main->getCPU();
    PPC_PPU(gui, CPU->GetPPU(0));
    PPC_PPU(gui, CPU->GetPPU(1));
    PPC_PPU(gui, CPU->GetPPU(2));
  }, { window->Size.x - 27.5f, window->Size.y - 74.f }, ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_MenuBar);
  ImGui::PopStyleVar(4);
}

void GeneralSettings(Render::GUI *gui) {
  static int logLevel = static_cast<int>(Config::getCurrentLogLevel());
  gui->Toggle("Exit on window close", &Config::shouldQuitOnWindowClosure);
  gui->Toggle("Advanced log", &Config::islogAdvanced);
}

void GraphicsSettings(Render::GUI *gui) {
  gui->Toggle("Enabled", &Config::gpuRenderThreadEnabled);
  gui->Toggle("Fullscreen", &Config::isFullscreen, [&] {
    Xe_Main->renderer->fullscreen = Config::isFullscreen;
    SDL_SetWindowFullscreen(gui->mainWindow, Xe_Main->renderer->fullscreen);
  });
  gui->Toggle("VSync", &Config::vsyncEnabled, [&] {
    Xe_Main->renderer->VSYNC = Config::vsyncEnabled;
    SDL_GL_SetSwapInterval(Xe_Main->renderer->VSYNC ? 1 : 0);
  });
}

void CodeflowSettings(Render::GUI *gui) {
  gui->Toggle("RGH2 Init Skip", &RGH2, [] {
    if (!storedPreviousInitSkips && !RGH2) {
      initSkip1 = Config::SKIP_HW_INIT_1;
      initSkip2 = Config::SKIP_HW_INIT_2;
      storedPreviousInitSkips = true;
    }
    Config::SKIP_HW_INIT_1 = RGH2 ? 0x3003DC0 : initSkip1;
    Config::SKIP_HW_INIT_2 = RGH2 ? 0x3003E54 : initSkip2;
  });
  gui->Toggle("Load Elf", &Config::elfLoader);
}

void PathSettings(Render::GUI *gui) {
  Config::fusesTxtPath = gui->InputText("Fuses", Config::fusesTxtPath);
  Config::oneBlBinPath = gui->InputText("1bl", Config::oneBlBinPath);
  Config::nandBinPath = gui->InputText("NAND", Config::nandBinPath);
  Config::elfBinaryPath = gui->InputText("ELF Binary", Config::elfBinaryPath);
  Config::oddDiscImagePath = gui->InputText("ODD Image File (iso)", Config::oddDiscImagePath);
}

void ImGuiSettings(Render::GUI *gui) {
  gui->Toggle("Style Editor", &gui->styleEditor);
  gui->Toggle("Demo", &gui->demoWindow);
  gui->Toggle("Viewports", &gui->viewports, [&] {
    ImGuiIO& io = ImGui::GetIO();
    if (gui->viewports)
      io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    else
      io.ConfigFlags &= ~(ImGuiConfigFlags_ViewportsEnable);
  });
  gui->Tooltip("Allows ImGui windows to be 'detached' from the main window. Useful for debugging");
}

void Render::GUI::OnSwap(Texture *texture) {
  if (ppcDebuggerActive && !ppcDebuggerAttached) {
    Window("PPC Debugger", [this] {
      PPCDebugger(this);
    }, { 1200.f, 700.f }, ImGuiWindowFlags_NoCollapse, &ppcDebuggerActive, { 500.f, 100.f });
  }
  if (Config::imguiDebug()) {
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
        TabItem("Settings", [&] {
          TabBar("##settings", [&] {
            TabItem("General", [&] {
              Button("Force exit", [] {
                Xe_Main->shutdown();
              });
              GeneralSettings(this);
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
          });
        });
      });
    }, { 1200.f, 700.f }, ImGuiWindowFlags_NoCollapse, &Config::imguiDebugWindow, { 1000.f, 400.f });
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
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }
}
