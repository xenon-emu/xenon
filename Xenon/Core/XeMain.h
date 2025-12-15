/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Logging/Backend.h"
#include "Base/Logging/Log.h"
#include "Base/Config.h"
#include "Base/PathUtil.h"

#include "Core/PCI/Bridge/HostBridge.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/Devices/NAND/NAND.h"
#include "Core/RAM/RAM.h"
#include "Core/PCI/Devices/AUDIOCTRLLR/AudioController.h"
#include "Core/PCI/Devices/EHCI/EHCI0.h"
#include "Core/PCI/Devices/EHCI/EHCI1.h"
#include "Core/PCI/Devices/ETHERNET/Ethernet.h"
#include "Core/PCI/Devices/HDD/HDD.h"
#include "Core/PCI/Devices/ODD/ODD.h"
#include "Core/PCI/Devices/OHCI/OHCI0.h"
#include "Core/PCI/Devices/OHCI/OHCI1.h"
#include "Core/PCI/Devices/SFCX/SFCX.h"
#include "Core/PCI/Devices/SMC/SMC.h"
#include "Core/PCI/Devices/XMA/XMA.h"
#include "Core/RootBus/RootBus.h"
#include "Core/XCPU/XenonCPU.h"
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

extern void CreateBridges();
extern void CreatePCIDevices(RAM *ram);
extern void CreateRootBus();

extern Xe::XCPU::XenonCPU *GetCPU();

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
inline std::unique_ptr<Xe::XCPU::XenonCPU> xenonCPU{};
//  Xenos GPU
inline std::shared_ptr<Xe::Xenos::XGPU> xenos{};

} // namespace XeMain

// Global shutdown handler
extern s32 globalShutdownHandler();
