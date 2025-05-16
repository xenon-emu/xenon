// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Base/Path_util.h"
#include "microprofile.h"
#include "microprofile_html.h"

#include "Base/Config.h"
#include "Base/Logging/Backend.h"
#include "Base/Logging/Log.h"
#include "Base/Version.h"

#include "Core/NAND/NAND.h"
#include "Core/RAM/RAM.h"
#include "Core/RootBus/HostBridge/HostBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/AUDIOCTRLLR/AudioController.h"
#include "Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI0.h"
#include "Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI1.h"
#include "Core/RootBus/HostBridge/PCIBridge/ETHERNET/Ethernet.h"
#include "Core/RootBus/HostBridge/PCIBridge/HDD/HDD.h"
#include "Core/RootBus/HostBridge/PCIBridge/ODD/ODD.h"
#include "Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI0.h"
#include "Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI1.h"
#include "Core/RootBus/HostBridge/PCIBridge/PCIBridge.h"
#include "Core/RootBus/HostBridge/PCIBridge/SFCX/SFCX.h"
#include "Core/RootBus/HostBridge/PCIBridge/SMC/SMC.h"
#include "Core/RootBus/HostBridge/PCIBridge/XMA/XMA.h"
#include "Core/RootBus/RootBus.h"
#include "Core/XCPU/Xenon.h"
#include "Core/XGPU/XGPU.h"

#include "Render/OGLRenderer.h"
#include "Render/DummyRenderer.h"

// Global thread state
inline std::atomic<bool> XeRunning{ true };
class XeMain {
public:
  XeMain();
  ~XeMain();

  void start();

  void shutdownCPU();

  void reboot(Xe::PCIDev::SMC_PWR_REASON type);

  void reloadFiles();

  void saveConfig();
  void loadConfig();

  SDL_Window* createWindow();

  void addPCIDevices();
  void createHostBridge();
  void createPCIDevices();
  void createRootBus();
  void createSMCState();

  void shutdown() {
    XeRunning = false;
  }

  Xenon* getCPU() {
    return xenonCPU.get();
  }
  // Window
  SDL_Window *mainWindow = nullptr;
private:
  // Main objects
  //  Base path
  std::filesystem::path rootDirectory = {};
  //  Log level
  std::unique_ptr<Base::Log::Filter> logFilter{};

  // Main Emulator objects
  std::unique_ptr<RootBus> rootBus{}; // RootBus Object
  std::unique_ptr<HostBridge> hostBridge{}; // HostBridge Object
  std::unique_ptr<PCIBridge> pciBridge{}; // PCIBridge Object

public:
#ifndef NO_GFX
  // Render thread
  std::unique_ptr<Render::Renderer> renderer{};
#endif
  // CPU started flag
  bool CPUStarted = false;

  // PCI Devices
  //  SMC
  std::unique_ptr<Xe::PCIDev::SMC_CORE_STATE> smcCoreState{}; // SMCCore State for setting diffrent SMC settings.
  std::unique_ptr<Xe::PCIDev::SMC> smcCore{}; // SMCCore Object
  //  Ethernet
  std::unique_ptr<Xe::PCIDev::ETHERNET> ethernet{};
  //  Audio
  std::unique_ptr<Xe::PCIDev::AUDIOCTRLR> audioController{};
  //  OHCI
  std::unique_ptr<Xe::PCIDev::OHCI0> ohci0{};
  std::unique_ptr<Xe::PCIDev::OHCI1> ohci1{};
  //  EHCI
  std::unique_ptr<Xe::PCIDev::EHCI0> ehci0{};
  std::unique_ptr<Xe::PCIDev::EHCI1> ehci1{};
  //  Secure Flash Controller for Xbox Device object
  std::unique_ptr<Xe::PCIDev::SFCX> sfcx{};
  //  XMA
  std::unique_ptr<Xe::PCIDev::XMA> xma{};
  //  ODD (CD-ROM Drive)
  std::unique_ptr<Xe::PCIDev::ODD> odd{};
  //  HDD
  std::unique_ptr<Xe::PCIDev::HDD> hdd{};
  //  NAND
  std::unique_ptr<NAND> nand{};
  //  Random Access Memory (All console RAM, excluding Reserved memory which is mainly PCI Devices)
  std::unique_ptr<RAM> ram{};

  // Console Handles
  //  Xenon CPU
  std::unique_ptr<Xenon> xenonCPU{};
  //  Xenos GPU
  std::unique_ptr<Xe::Xenos::XGPU> xenos{};
};

// Global pointer
inline std::unique_ptr<XeMain> Xe_Main{};

// Global shutdown handler
extern s32 globalShutdownHandler();
