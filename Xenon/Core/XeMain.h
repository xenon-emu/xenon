// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Base/Logging/Backend.h"
#include "Base/Logging/Log.h"
#include "Base/Config.h"
#include "Base/PathUtil.h"

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

#include "Render/Backends/OGL/OGLRenderer.h"
#include "Render/Backends/Dummy/DummyRenderer.h"

// Global thread state
namespace XeMain {

extern void Create();
extern void Shutdown();

extern void StartCPU();

extern void ShutdownCPU();

extern void Reboot(u32 type);

extern void ReloadFiles();

extern void SaveConfig();
extern void LoadConfig();

extern void CreateHostBridge();
extern void CreatePCIDevices();
extern void CreateRootBus();

extern Xenon *GetCPU();

// Main objects
//  Base path
inline std::filesystem::path rootDirectory = {};

// Main Emulator objects
inline std::shared_ptr<RootBus> rootBus{}; // RootBus Object
inline std::shared_ptr<HostBridge> hostBridge{}; // HostBridge Object
inline std::shared_ptr<PCIBridge> pciBridge{}; // PCIBridge Object

#ifndef NO_GFX
// Render thread
inline std::unique_ptr<Render::Renderer> renderer{};
#endif
// CPU started flag
inline bool CPUStarted = false;

// PCI Devices
//  SMC
inline std::shared_ptr<Xe::PCIDev::SMC> smcCore{};
//  Ethernet
inline std::shared_ptr<Xe::PCIDev::ETHERNET> ethernet{};
//  Audio
inline std::shared_ptr<Xe::PCIDev::AUDIOCTRLR> audioController{};
//  OHCI
inline std::shared_ptr<Xe::PCIDev::OHCI0> ohci0{};
inline std::shared_ptr<Xe::PCIDev::OHCI1> ohci1{};
//  EHCI
inline std::shared_ptr<Xe::PCIDev::EHCI0> ehci0{};
inline std::shared_ptr<Xe::PCIDev::EHCI1> ehci1{};
//  Secure Flash Controller for Xbox Device object
inline std::shared_ptr<Xe::PCIDev::SFCX> sfcx{};
//  XMA
inline std::shared_ptr<Xe::PCIDev::XMA> xma{};
//  ODD (CD-ROM Drive)
inline std::shared_ptr<Xe::PCIDev::ODD> odd{};
//  HDD
inline std::shared_ptr<Xe::PCIDev::HDD> hdd{};
//  NAND
inline std::shared_ptr<NAND> nand{};
//  Random Access Memory (All console RAM, excluding Reserved memory which is mainly PCI Devices)
inline std::shared_ptr<RAM> ram{};

// Console Handles
//  Xenon CPU
inline std::unique_ptr<Xenon> xenonCPU{};
//  Xenos GPU
inline std::shared_ptr<Xe::Xenos::XGPU> xenos{};

} // namespace XeMain

// Global shutdown handler
extern s32 globalShutdownHandler();
