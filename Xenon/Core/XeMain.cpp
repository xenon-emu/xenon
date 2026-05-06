/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "XeMain.h"

#include "Render/Backends/Vulkan/VulkanRenderer.h"

void XeMain::Create() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "Create", MP_AUTO);
  Base::Log::Initialize();
  Base::Log::Start();

  Base::FS::DumpPaths();
  LOG_INFO(System, "Starting...");

  // Load config
  LOG_INFO(Xenon, "Loading Config...");
  LoadConfig();

  // Create bridges
  CreateBusTrees();

#ifndef NO_GFX
  // Create and start renderer
  switch (Base::JoaatStringHash(Config::rendering.backend)) {
  case "OpenGL"_jLower:
    renderer = std::make_unique<Render::OGLRenderer>();
    renderer->Start(ram);
    break;
  case "Vulkan"_jLower:
    renderer = std::make_unique<Render::VulkanRenderer>();
    renderer->Start(ram);
    break;
  case "Dummy"_jLower:
    renderer = std::make_unique<Render::DummyRenderer>();
    renderer->Start(ram);
    break;
  default:
    LOG_ERROR(Render, "Invalid renderer backend: {}", Config::rendering.backend);
    break;
  }
#endif

  // Create CPU
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus, Config::filepaths.oneBl, Config::filepaths.fuses, ram);
  if (auto guestBridge = pciBridge.lock())
    guestBridge->RegisterIIC(xenonCPU->GetIICPointer());

  // Create XGPU
  auto xenosPtr = std::make_unique<Xe::Xenos::XGPU>(
#ifndef NO_GFX
    renderer.get(),
#else
    nullptr,
#endif
    ram, pciBridge
  );
  if (auto bridge = hostBridge.lock()) {
    xenos = bridge->RegisterXGPU(std::move(xenosPtr));
  }
}

void XeMain::Shutdown() {
  // Set as already shutdown
  if (Base::gShutdownStarted.exchange(true)) {
    return;
  }

  // Set all states to false
  XePaused = false;
  XeRunning.store(false, std::memory_order_release);

  // Save config
  SaveConfig();

  // Shutdown the XCPU
  if (CPUStarted) {
    xenonCPU.reset();
    CPUStarted = false;
  }

#ifndef NO_GFX
  // Stop rendering after the CPU is stopped
  if (renderer) {
    renderer->Shutdown();
    renderer.reset();
  }
#endif

  // Shutdown the RootBus, it owns the HostBus, which contains all PCI devices.
  if (rootBus) {
    rootBus.reset();
  }

  // Stop the logger
  Base::Log::Stop();
  // Wait a bit for the logger to flush
  std::this_thread::sleep_for(200ms);

#if AUTO_FLIP
  MicroProfileStopAutoFlip();
#endif
  MicroProfileShutdown();

  Base::gShutdownFinished = true;
}

void XeMain::SaveConfig() {
  if (configPath.empty()) {
    configPath = (Base::FS::GetPath(Base::FS::PathType::UserConfigDir)) / "config.toml";
  }

  Config::SaveConfig(configPath);
}

void XeMain::LoadConfig() {
  if (configPath.empty()) {
    configPath = (Base::FS::GetPath(Base::FS::PathType::UserConfigDir)) / "config.toml";
  }

  LOG_INFO(Xenon, "Loading config...");
  Config::LoadConfig(configPath);

  // Set the RAM Size
  ramSizeStr = Config::xcpu.ramSize;
  ramSize = RAM::ParseRamSize(ramSizeStr);

  // Set the global log filter
  //  We do this because it's a config option
  logFilter = std::make_unique<STRIP_UNIQUE(logFilter)>(Config::log.currentLevel);
  Base::Log::SetGlobalFilter(*logFilter);
}

void XeMain::StartCPU() {
  LOG_INFO(Xenon, "Starting CPU...");
  if (!xenonCPU.get()) {
    LOG_CRITICAL(Xenon, "Failed to initialize Xenon's CPU!");
    Base::SystemPause();
    return;
  }

  // TODO: Add a path back that checks for NAND validity
  //  Lifecycle management doesn't allow us to check here properly,
  //  and the ram can be destroyed, we should probably check here.
  //  same with the NAND/SFCX
  if (Config::xcpu.elfLoader) {
    // Load the elf
    xenonCPU->LoadElf(Config::filepaths.elfBinary);
  } else {
    // CPU Start routine and entry point.
    xenonCPU->Start(0x20000000100);
  }

  // Set CPU as ready
  CPUStarted = true;
}

void XeMain::ShutdownCPU() {
  if (!CPUStarted) {
    return;
  }

  if (auto bridge = pciBridge.lock()) {
    // Set the CPU to 'Resetting' mode before killing the handle
    xenonCPU->Reset();
    // Reset RAM
    if (auto ramPtr = ram.lock()) {
      ramPtr->Reset();
    }

    // Reset the CPU
    xenonCPU.reset();
    xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus, Config::filepaths.oneBl, Config::filepaths.fuses, ram);

    // Ensure the IIC pointer in the PCI bridge is correct
    bridge->RegisterIIC(xenonCPU->GetIICPointer());
  }

  // Set the CPU as inactive
  CPUStarted = false;
}

void XeMain::Reboot(u32 type) {
  // Shutdown the CPU
  if (CPUStarted) {
    ShutdownCPU();
  }

  // Set the power-on type
  if (auto smc = smcCore.lock()) {
    smc->SetPowerOnReason(static_cast<Xe::PCIDev::SMC_PWR_REASON>(type));
  }

  // Setup CPU
  StartCPU();
}

void XeMain::ReloadFiles() {
  if (!xenonCPU)
    return;

  xenonCPU->Halt();

  if (auto bridge = pciBridge.lock()) {
    // Reset the SFCX
    auto sfcxPtr = std::make_unique<Xe::PCIDev::SFCX>(SFCX_DEV_SIZE, Config::filepaths.nand, pciBridge, ram);
    bridge->ResetPCIDevice(std::move(sfcxPtr));

    // Reset the NAND
    auto nand = std::make_unique<NAND>(sfcx);
    rootBus->ResetDevice(std::move(nand));

    if (!CPUStarted) {
      // Reset the CPU again to reload 1bl and fuses
      xenonCPU.reset();
      xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus, Config::filepaths.oneBl, Config::filepaths.fuses, ram);

      // Ensure the IIC pointer in the PCI bridge is correct
      bridge->RegisterIIC(xenonCPU->GetIICPointer());
    }
  }

  xenonCPU->Continue();
}

void XeMain::CreateBusTrees() {
  LOG_INFO(Xenon, "Creating bus paths...");
  // Create root
  rootBus = std::make_unique<STRIP_UNIQUE(rootBus)>();
  LOG_INFO(RootBus, "Creating tree root...");

  // Create the busses (host and guest)
  LOG_INFO(RootBus, "Creating host PCI bus...");
  hostBridge = rootBus->AddHostBridge(std::make_unique<HostBridge>(ramSize));
  LOG_INFO(RootBus, "Creating guest PCI bus...");
  if (auto bridge = hostBridge.lock()) {
    pciBridge = bridge->RegisterPCIBridge(std::make_unique<PCIBridge>());
  } else {
    LOG_CRITICAL(Xenon, "The host PCI bridge is missing! Unable to continue...");
    Base::SystemPause();
    return;
  }

  // Create bus devices
  CreateBusDevices();
}

void XeMain::CreateBusDevices() {
  LOG_INFO(RootBus, "Creating devices...");

  // RAM - Random Access Memory (All console RAM, excluding Reserved memory which is mainly PCI Devices)
  rootBus->AddDevice(std::make_unique<RAM>(RAM_START_ADDR, ramSize, false));
  ram = rootBus->GetDevice<STRIP_WEAK(ram)>("RAM"_j);

  if (auto bridge = pciBridge.lock()) {
    // OHCI
    auto ohci0 = std::make_unique<Xe::PCIDev::OHCI0>(OHCI_DEV_SIZE);
    bridge->AddPCIDevice(std::move(ohci0));
    auto ohci1 = std::make_unique<Xe::PCIDev::OHCI1>(OHCI_DEV_SIZE);
    bridge->AddPCIDevice(std::move(ohci1));

    // EHCI
    auto ehci0 = std::make_unique<Xe::PCIDev::EHCI0>(EHCI_DEV_SIZE);
    bridge->AddPCIDevice(std::move(ehci0));
    auto ehci1 = std::make_unique<Xe::PCIDev::EHCI1>(EHCI_DEV_SIZE);
    bridge->AddPCIDevice(std::move(ehci1));

    // Audio
    auto audioController = std::make_unique<Xe::PCIDev::AUDIOCTRLR>(AUDIO_CTRLR_DEV_SIZE);
    bridge->AddPCIDevice(std::move(audioController));

    // Ethernet
    auto ethernet = std::make_unique<Xe::PCIDev::ETHERNET>(ETHERNET_DEV_SIZE, pciBridge, ram);
    bridge->AddPCIDevice(std::move(ethernet));

    // Secure Flash Controller for Xbox Device object
    auto sfcxPtr = std::make_unique<Xe::PCIDev::SFCX>(SFCX_DEV_SIZE, Config::filepaths.nand, pciBridge, ram);
    bridge->AddPCIDevice(std::move(sfcxPtr));
    sfcx = bridge->GetDevice<STRIP_WEAK(sfcx)>("SFCX"_j);

    // NAND
    rootBus->AddDevice(std::make_unique<NAND>(sfcx));

    // XMA
    auto xma = std::make_unique<Xe::PCIDev::XMA>(XMA_DEV_SIZE);
    bridge->AddPCIDevice(std::move(xma));

    // ODD (CD-ROM Drive)
    auto odd = std::make_unique<Xe::PCIDev::ODD>(ODD_DEV_SIZE, pciBridge, ram);
    bridge->AddPCIDevice(std::move(odd));

    // HDD
    auto hdd = std::make_unique<Xe::PCIDev::HDD>(HDD_DEV_SIZE, pciBridge, ram);
    bridge->AddPCIDevice(std::move(hdd));

    // SMC
    auto smcCorePtr = std::make_unique<Xe::PCIDev::SMC>(SMC_DEV_SIZE, pciBridge);
    bridge->AddPCIDevice(std::move(smcCorePtr));
    smcCore = bridge->GetDevice<STRIP_WEAK(smcCore)>("SMC"_j);
  } else {
    LOG_CRITICAL(Xenon, "The Guest PCI bridge is missing! Unable to continue..");
    Base::SystemPause();
    return;
  }
}

Xe::XCPU::XenonCPU *XeMain::GetCPU() {
  return xenonCPU.get();
}