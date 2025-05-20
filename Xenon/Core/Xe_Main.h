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
namespace XeMain {

extern void Create();
extern void Shutdown();

extern void Start();

extern void ShutdownCPU();

extern void Reboot(Xe::PCIDev::SMC_PWR_REASON type);

extern void ReloadFiles();

extern void SaveConfig();
extern void LoadConfig();

#ifndef NO_GFX
extern SDL_Window* CreateSDLWindow();
#endif

extern void AddPCIDevices();
extern void CreateHostBridge();
extern void CreatePCIDevices();
extern void CreateRootBus();
extern void CreateSMCState();

#ifndef NO_GFX
// Window
inline SDL_Window *mainWindow = nullptr;
#endif

// Main objects
//  Base path
inline std::filesystem::path rootDirectory = {};
//  Log level
inline std::unique_ptr<Base::Log::Filter> logFilter{};

// Main Emulator objects
inline std::unique_ptr<RootBus> rootBus{}; // RootBus Object
inline std::unique_ptr<HostBridge> hostBridge{}; // HostBridge Object
inline std::unique_ptr<PCIBridge> pciBridge{}; // PCIBridge Object

#ifndef NO_GFX
// Render thread
inline std::unique_ptr<Render::Renderer> renderer{};
#endif
// CPU started flag
inline bool CPUStarted = false;

// PCI Devices
//  SMC
inline std::unique_ptr<Xe::PCIDev::SMC_CORE_STATE> smcCoreState{}; // SMCCore State for setting diffrent SMC settings.
inline std::unique_ptr<Xe::PCIDev::SMC> smcCore{}; // SMCCore Object
//  Ethernet
inline std::unique_ptr<Xe::PCIDev::ETHERNET> ethernet{};
//  Audio
inline std::unique_ptr<Xe::PCIDev::AUDIOCTRLR> audioController{};
//  OHCI
inline std::unique_ptr<Xe::PCIDev::OHCI0> ohci0{};
inline std::unique_ptr<Xe::PCIDev::OHCI1> ohci1{};
//  EHCI
inline std::unique_ptr<Xe::PCIDev::EHCI0> ehci0{};
inline std::unique_ptr<Xe::PCIDev::EHCI1> ehci1{};
//  Secure Flash Controller for Xbox Device object
inline std::unique_ptr<Xe::PCIDev::SFCX> sfcx{};
//  XMA
inline std::unique_ptr<Xe::PCIDev::XMA> xma{};
//  ODD (CD-ROM Drive)
inline std::unique_ptr<Xe::PCIDev::ODD> odd{};
//  HDD
inline std::unique_ptr<Xe::PCIDev::HDD> hdd{};
//  NAND
inline std::unique_ptr<NAND> nand{};
//  Random Access Memory (All console RAM, excluding Reserved memory which is mainly PCI Devices)
inline std::unique_ptr<RAM> ram{};

// Console Handles
//  Xenon CPU
inline std::unique_ptr<Xenon> xenonCPU{};
//  Xenos GPU
inline std::unique_ptr<Xe::Xenos::XGPU> xenos{};

inline Xenon *GetCPU() {
  return xenonCPU.get();
}

} // namespace XeMain

// Global shutdown handler
extern s32 globalShutdownHandler();
