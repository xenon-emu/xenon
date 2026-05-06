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

extern void CreateBusDevices();
extern void CreateBusTrees();

extern Xe::XCPU::XenonCPU *GetCPU();

// Main objects
//  Config path
inline fs::path configPath = {};
//  Log Filter
inline std::unique_ptr<Base::Log::Filter> logFilter = {};
//  Root bus
inline std::shared_ptr<RootBus> rootBus{};
//  Host PCI bus
inline std::weak_ptr<HostBridge> hostBridge{};
//  Guest PCI bus
inline std::weak_ptr<PCIBridge> pciBridge{};

#ifndef NO_GFX
//  Rendering context
inline std::unique_ptr<Render::Renderer> renderer{};
#endif
//  RAM Size
inline std::string ramSizeStr = {};
inline u64 ramSize = 0;
//  CPU flag
inline bool CPUStarted = false;

// Console Handles
//  Xenon CPU
inline std::unique_ptr<Xe::XCPU::XenonCPU> xenonCPU{};
//  Xenos GPU
inline std::weak_ptr<Xe::Xenos::XGPU> xenos{};

// PCI Devices (weak references)
// SMC
inline std::weak_ptr<Xe::PCIDev::SMC> smcCore{};
// SFCX
inline std::weak_ptr<Xe::PCIDev::SFCX> sfcx{};
// RAM
inline std::weak_ptr<RAM> ram{};

} // namespace XeMain