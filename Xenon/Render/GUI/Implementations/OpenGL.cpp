// Copyright 2025 Xenon Emulator Project

#include "OpenGL.h"

#include "Core/Xe_Main.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"
#include "Base/Config.h"
#include "Render/Renderer.h"
#include "Render/Implementations/OGLTexture.h"

#define TextFmt(x, ...) Text(fmt::format(x, __VA_ARGS__))

bool RGH2{};
bool storedPreviousInitSkips{};
int initSkip1{}, initSkip2{};
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

#define CustomBase(x, fmt, ...) gui->TextFmt(x ": " fmt, __VA_ARGS__)
#define Custom(x, fmt, ...) CustomBase(#x, fmt, __VA_ARGS__)
#define HexBase(x, ...) CustomBase(x, "0x{:X}", __VA_ARGS__)
#define Hex(c, x) HexBase(#x, c.x)
#define BFHex(c, x) HexBase(#x, u32(c.x))
#define U8Hex(c, x) HexBase(#x, static_cast<u32>(c.x))
#define HexArr(a, i) HexBase("[{}]", i, a[i])
#define Dec(c, x) Custom(x, "{}", c.x)
#define U8Dec(c, x) Custom(x, "{}", static_cast<u32>(c.x))
#define Bool(c, x) Custom(x, "{}", c.x ? "true" : "false")
void PPUThread(Render::OpenGLGUI *gui, PPU_STATE *PPUState, u32 threadID) {
  gui->Node(fmt::format("[{}]", threadID), [&] {
    PPU_THREAD_REGISTERS &ppuRegisters = PPUState->ppuThread[threadID];
    gui->Node("SPR", [&] {
      PPU_THREAD_SPRS &SPR = ppuRegisters.SPR;
      gui->Node("XER", [&] {
        XERegister &XER = SPR.XER;
        Hex(XER, XER_Hex);
        BFHex(XER, ByteCount);
        BFHex(XER, R0);
        BFHex(XER, CA);
        BFHex(XER, OV);
        BFHex(XER, SO);
      });
      Hex(SPR, LR);
      Hex(SPR, CTR);
      Hex(SPR, CFAR);
      Hex(SPR, VRSAVE);
      Hex(SPR, DSISR);
      Hex(SPR, DAR);
      Dec(SPR, DEC);
      Hex(SPR, SRR0);
      Hex(SPR, SRR1);
      Hex(SPR, ACCR);
      Hex(SPR, SPRG0);
      Hex(SPR, SPRG1);
      Hex(SPR, SPRG2);
      Hex(SPR, SPRG3);
      Hex(SPR, HSPRG0);
      Hex(SPR, HSPRG1);
      Hex(SPR, HSRR0);
      Hex(SPR, HSRR1);
      Hex(SPR, TSRL);
      Hex(SPR, TSSR);
      Hex(SPR, PPE_TLB_Index_Hint);
      Hex(SPR, DABR);
      Hex(SPR, DABRX);
      gui->Node("MSR", [&] {
        MSRegister &MSR = SPR.MSR;
        BFHex(MSR, LE);
        BFHex(MSR, RI);
        BFHex(MSR, PMM);
        BFHex(MSR, DR);
        BFHex(MSR, IR);
        BFHex(MSR, FE1);
        BFHex(MSR, BE);
        BFHex(MSR, SE);
        BFHex(MSR, FE0);
        BFHex(MSR, ME);
        BFHex(MSR, FP);
        BFHex(MSR, PR);
        BFHex(MSR, EE);
        BFHex(MSR, ILE);
        BFHex(MSR, VXU);
        BFHex(MSR, HV);
        BFHex(MSR, TA);
        BFHex(MSR, SF);
      });
      Hex(SPR, PIR);
    });
    Hex(ppuRegisters, CIA);
    Hex(ppuRegisters, NIA);
    gui->Node("CI", [&] {
      PPCOpcode &CI = ppuRegisters.CI;
      Hex(CI, opcode);
      BFHex(CI, main);
      BFHex(CI, sh64);
      BFHex(CI, mbe64);
      BFHex(CI, vuimm);
      BFHex(CI, vs);
      BFHex(CI, vsh);
      BFHex(CI, oe);
      BFHex(CI, spr);
      BFHex(CI, vc);
      BFHex(CI, vb);
      BFHex(CI, va);
      BFHex(CI, vd);
      BFHex(CI, lk);
      BFHex(CI, aa);
      BFHex(CI, rb);
      BFHex(CI, ra);
      BFHex(CI, rd);
      BFHex(CI, uimm16);
      BFHex(CI, l11);
      BFHex(CI, rs);
      BFHex(CI, simm16);
      BFHex(CI, ds);
      BFHex(CI, vsimm);
      BFHex(CI, ll);
      BFHex(CI, li);
      BFHex(CI, lev);
      BFHex(CI, i);
      BFHex(CI, crfs);
      BFHex(CI, l10);
      BFHex(CI, crfd);
      BFHex(CI, crbb);
      BFHex(CI, crba);
      BFHex(CI, crbd);
      BFHex(CI, rc);
      BFHex(CI, me32);
      BFHex(CI, mb32);
      BFHex(CI, sh32);
      BFHex(CI, bi);
      BFHex(CI, bo);
      BFHex(CI, bh);
      BFHex(CI, frc);
      BFHex(CI, frb);
      BFHex(CI, fra);
      BFHex(CI, frd);
      BFHex(CI, crm);
      BFHex(CI, frs);
      BFHex(CI, flm);
      BFHex(CI, l6);
      BFHex(CI, l15);
      BFHex(CI, bt14);
      BFHex(CI, bt24);
    });
    Bool(ppuRegisters, iFetch);
    gui->Node("GPRs", [&] {
      for (u64 i = 0; i != 32; ++i) {
        HexArr(ppuRegisters.GPR, i);
      }
    });
    gui->Node("FPRs", [&] {
      for (u64 i = 0; i != 32; ++i) {
        FPRegister &FPR = ppuRegisters.FPR[i];
        gui->Node(fmt::format("[{}]", i), [&] {
          Custom(valueAsDouble, "{}", FPR.valueAsDouble);
          Custom(valueAsU64, "0x{:X}",FPR.valueAsU64);
        });
      }
    });
    gui->Node("CR", [&] {
      CRegister &CR = ppuRegisters.CR;
      Hex(CR, CR_Hex);
      BFHex(CR, CR0);
      BFHex(CR, CR1);
      BFHex(CR, CR2);
      BFHex(CR, CR3);
      BFHex(CR, CR4);
      BFHex(CR, CR5);
      BFHex(CR, CR6);
      BFHex(CR, CR7);
    });
    gui->Node("FPSCR", [&] {
      FPSCRegister &FPSCR = ppuRegisters.FPSCR;
      Hex(FPSCR, FPSCR_Hex);
      BFHex(FPSCR, RN);
      BFHex(FPSCR, NI);
      BFHex(FPSCR, XE);
      BFHex(FPSCR, ZE);
      BFHex(FPSCR, UE);
      BFHex(FPSCR, OE);
      BFHex(FPSCR, VE);
      BFHex(FPSCR, VXCVI);
      BFHex(FPSCR, VXSQRT);
      BFHex(FPSCR, VXSOFT);
      BFHex(FPSCR, R0);
      BFHex(FPSCR, FPRF);
      BFHex(FPSCR, FI);
      BFHex(FPSCR, FR);
      BFHex(FPSCR, VXVC);
      BFHex(FPSCR, VXIMZ);
      BFHex(FPSCR, VXZDZ);
      BFHex(FPSCR, VXIDI);
      BFHex(FPSCR, VXISI);
      BFHex(FPSCR, VXSNAN);
      BFHex(FPSCR, XX);
      BFHex(FPSCR, ZX);
      BFHex(FPSCR, UX);
      BFHex(FPSCR, OX);
      BFHex(FPSCR, VX);
      BFHex(FPSCR, FEX);
      BFHex(FPSCR, FX);
    });
    gui->Node("SLBs", [&] {
      for (u64 i = 0; i != 64; ++i) {
        SLBEntry &SLB = ppuRegisters.SLB[i];
        gui->Node(fmt::format("[{}]", i), [&] {
          U8Hex(SLB, V);
          U8Hex(SLB, LP);
          U8Hex(SLB, C);
          U8Hex(SLB, L);
          U8Hex(SLB, N);
          U8Hex(SLB, Kp);
          U8Hex(SLB, Ks);
          Hex(SLB, VSID);
          Hex(SLB, ESID);
          Hex(SLB, vsidReg);
          Hex(SLB, esidReg);
        });
      }
    });
    Hex(ppuRegisters, exceptReg);
    Bool(ppuRegisters, exceptionTaken);
    Hex(ppuRegisters, exceptEA);
    Hex(ppuRegisters, exceptTrapType);
    Bool(ppuRegisters, exceptHVSysCall);
    Hex(ppuRegisters, intEA);
    Hex(ppuRegisters, lastWriteAddress);
    Hex(ppuRegisters, lastRegValue);
    gui->Node("ppuRes", [&] {
        PPU_RES &ppuRes = *(ppuRegisters.ppuRes.get());
        U8Hex(ppuRes, ppuID);
        Bool(ppuRes, V);
        Hex(ppuRes, resAddr);
    });
  });
}

void RenderInstr(Render::OpenGLGUI *gui, u32 addr, u32 instr) {
  std::string instrName = PPCInterpreter::ppcDecoder.decodeName(instr);
  u32 b0 = static_cast<u8>((instr >> 24) & 0xFF);
  u32 b1 = static_cast<u8>((instr >> 16) & 0xFF);
  u32 b2 = static_cast<u8>((instr >> 8) & 0xFF);
  u32 b3 = static_cast<u8>((instr >> 0) & 0xFF);
  gui->TextFmt("{:08X} {:02X} {:02X} {:02X} {:02X}                             {}", addr, b0, b1, b2, b3, instrName);
}

void PPC_PPU(Render::OpenGLGUI *gui, PPU *PPU) {
  if (!PPU) {
    return;
  }
  PPU_STATE *PPUStatePtr = PPU->GetPPUState();
  #define ppuState PPUStatePtr
  PPU_STATE &PPUState = *PPUStatePtr;
  gui->Node(PPUState.ppuName, [&] {
    u32 curInstr = _instr.opcode;
    u32 nextInstr = PPCInterpreter::MMURead32(ppuState, curThread.NIA);
    RenderInstr(gui, curThread.CIA, curInstr);
    RenderInstr(gui, curThread.NIA, nextInstr);
    gui->Node("ppuThread", [&] {
      PPUThread(gui, PPUStatePtr, 0);
      PPUThread(gui, PPUStatePtr, 1);
    });
    U8Dec(PPUState, currentThread);
    gui->Node("SPR", [&] {
      PPU_STATE_SPRS &SPR = PPUState.SPR;
      Hex(SPR, SDR1);
      Hex(SPR, CTRL);
      Hex(SPR, TB);
      gui->Node("PVR", [&] {
        PVRegister &PVR = SPR.PVR;
        Hex(PVR, PVR_Hex);
        U8Hex(PVR, Revision);
        U8Hex(PVR, Version);
      });
      Hex(SPR, HDEC);
      Hex(SPR, RMOR);
      Hex(SPR, HRMOR);
      Hex(SPR, LPCR);
      Hex(SPR, LPIDR);
      Hex(SPR, TSCR);
      Hex(SPR, TTR);
      Hex(SPR, PPE_TLB_Index);
      Hex(SPR, PPE_TLB_VPN);
      Hex(SPR, PPE_TLB_RPN);
      Hex(SPR, PPE_TLB_RMT);
      Hex(SPR, HID0);
      Hex(SPR, HID1);
      Hex(SPR, HID4);
      Hex(SPR, HID6);
    });
    gui->Node("TLB", [&] {
      TLB_Reg &TLB = PPUState.TLB;
      gui->Node("tlbSet0", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet0[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(TLBEntry, V);
            U8Hex(TLBEntry, p);
            Hex(TLBEntry, RPN);
            Hex(TLBEntry, VPN);
            Bool(TLBEntry, L);
            Bool(TLBEntry, LP);
          });
        }
      });
      gui->Node("tlbSet1", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet1[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(TLBEntry, V);
            U8Hex(TLBEntry, p);
            Hex(TLBEntry, RPN);
            Hex(TLBEntry, VPN);
            Bool(TLBEntry, L);
            Bool(TLBEntry, LP);
          });
        }
      });
      gui->Node("tlbSet2", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet2[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(TLBEntry, V);
            U8Hex(TLBEntry, p);
            Hex(TLBEntry, RPN);
            Hex(TLBEntry, VPN);
            Bool(TLBEntry, L);
            Bool(TLBEntry, LP);
          });
        }
      });
      gui->Node("tlbSet3", [&] {
        for (u64 i = 0; i != 256; ++i) {
          TLBEntry& TLBEntry = TLB.tlbSet3[i];
          gui->Node(fmt::format("[{}]", i), [&] {
            Bool(TLBEntry, V);
            U8Hex(TLBEntry, p);
            Hex(TLBEntry, RPN);
            Hex(TLBEntry, VPN);
            Bool(TLBEntry, L);
            Bool(TLBEntry, LP);
          });
        }
      });
    });
    Bool(PPUState, traslationInProgress);
    Custom(ppuName, "{}", PPUState.ppuName);
  }, ImGuiTreeNodeFlags_DefaultOpen);
}
void PPCDebugger(Render::OpenGLGUI *gui) {
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

void GeneralSettings(Render::OpenGLGUI *gui) {
  static int logLevel = static_cast<int>(Config::getCurrentLogLevel());
  gui->Toggle("Exit on window close", &Config::shouldQuitOnWindowClosure);
  gui->Toggle("Advanced log", &Config::islogAdvanced);
}

void GraphicsSettings(Render::OpenGLGUI *gui) {
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

void CodeflowSettings(Render::OpenGLGUI *gui) {
  gui->Toggle("RGH2 Init Skip", &RGH2, [] {
    if (!storedPreviousInitSkips || !RGH2) {
      initSkip1 = Config::SKIP_HW_INIT_1;
      initSkip2 = Config::SKIP_HW_INIT_2;
      storedPreviousInitSkips = true;
    }
    Config::SKIP_HW_INIT_1 = RGH2 ? initSkip1 : 0x3003DC0;
    Config::SKIP_HW_INIT_2 = RGH2 ? initSkip2 : 0x3003E54;
  });
}

void ImGuiSettings(Render::OpenGLGUI *gui) {
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

void Render::OpenGLGUI::OnSwap(Texture *texture) {
  if (ppcDebuggerActive && !ppcDebuggerAttached) {
    Window("PPC Debugger", [this] {
      PPCDebugger(this);
    }, { 1200.f, 700.f }, ImGuiWindowFlags_NoCollapse, &ppcDebuggerActive, { 500.f, 100.f });
  }
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
            GeneralSettings(this);
          });
          TabItem("Codeflow", [&] {
            CodeflowSettings(this);
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
  }, { 1200.f, 700.f }, ImGuiWindowFlags_NoCollapse, nullptr, { 0.f, 0.f });
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