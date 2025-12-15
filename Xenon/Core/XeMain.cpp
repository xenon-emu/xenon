/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "XeMain.h"

#include "Render/Backends/Vulkan/VulkanRenderer.h"

void XeMain::Create() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "Create", MP_AUTO);
  Base::Log::Initialize();
  Base::Log::Start();
  LOG_INFO(System, "Starting Xenon.");
  rootDirectory = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
  LoadConfig();
  Base::Log::Filter logFilter{ Config::log.currentLevel };
  Base::Log::SetGlobalFilter(logFilter);
#ifndef NO_GFX
  switch (Base::JoaatStringHash(Config::rendering.backend)) {
  case "OpenGL"_jLower:
    renderer = std::make_unique<Render::OGLRenderer>();
    break;
  case "Vulkan"_jLower:
    renderer = std::make_unique<Render::VulkanRenderer>();
    break;
  case "Dummy"_jLower:
    renderer = std::make_unique<Render::DummyRenderer>();
    break;
  default:
    LOG_ERROR(Render, "Invalid renderer backend: {}", Config::rendering.backend);
    break;
  }
#endif

  // Create RAM
  ram = std::make_shared<STRIP_UNIQUE(ram)>("RAM", RAM_START_ADDR, Config::xcpu.ramSize, false);

  // Create bridges
  CreateBridges();

  // Start renderer
#ifndef NO_GFX
  if (renderer)
    renderer->Start(ram.get());
#endif

  // With a async backend, we want it to catch up first, so just wait a bit.
  std::this_thread::sleep_for(100ms);

  // Create PCI devices
  CreatePCIDevices(ram.get());

  // Create rootbus
  CreateRootBus();

  // Create CPU
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses, ram.get());
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());

  // Create XGPU
  xenos = std::make_unique<STRIP_UNIQUE(xenos)>(
#ifndef NO_GFX
    renderer.get(),
#else
    nullptr,
#endif
    ram.get(), pciBridge.get()
  );
  hostBridge->RegisterXGPU(xenos);
}

void XeMain::Shutdown() {
  // Check if we've shutdown, no need to do it twice
  if (XeShutdownSignaled) {
    return;
  }
  // Set as already shutdown
  XeShutdownSignaled = true;

  // Set all states to false
  XePaused = false;
  XeRunning = false;

  // Save config
  SaveConfig();

  // Shutdown the XCPU
  xenonCPU.reset();
  CPUStarted = false;

  // Shutdown the PCI bridges
  pciBridge.reset();

  // Shutdown the rootbus
  rootBus.reset();
  nand.reset();
  ram.reset();

#ifndef NO_GFX
  // Delete the renderer
  renderer->Shutdown();
  renderer.reset();
#endif

  // Stop the logger
  Base::Log::Stop();
  // Wait a bit for the logger to flush
  std::this_thread::sleep_for(200ms);
#if AUTO_FLIP
  MicroProfileStopAutoFlip();
#endif
  MicroProfileShutdown();
}

void XeMain::SaveConfig() {
  Config::saveConfig(rootDirectory / "config.toml");
}
void XeMain::LoadConfig() {
  LOG_INFO(Xenon, "Loading Config...");
  Config::loadConfig(rootDirectory / "config.toml");
}

void XeMain::StartCPU() {
  LOG_INFO(Xenon, "Starting CPU...");
  if (!xenonCPU.get()) {
    LOG_CRITICAL(Xenon, "Failed to initialize Xenon's CPU!");
    Base::SystemPause();
    return;
  } else if (!ram) {
    LOG_CRITICAL(Xenon, "No RAM, unable to start execution.");
    Base::SystemPause();
    return;
  }

  if (Config::xcpu.elfLoader) {
    // Load the elf
    xenonCPU->LoadElf(Config::filepaths.elfBinary);
  } else if (!sfcx || !nand) {
    // If we have no valid execution path, error out.
    LOG_CRITICAL(Xenon, "No NAND, unable to start execution.");
    Base::SystemPause();
    return;
  } else {
    // CPU Start routine and entry point.
    xenonCPU->Start(0x20000000100);
  }
  CPUStarted = true;
}

void XeMain::ShutdownCPU() {
  if (!CPUStarted) {
    return;
  }
  // Set the CPU to 'Resetting' mode before killing the handle
  xenonCPU->Reset();
  if (ram) {
    // Reset RAM
    ram->Reset();
#ifndef NO_GFX
    // Reinit RAM handles for rendering (should be valid, mainly safety)
    renderer->ramPointer = ram.get();
    renderer->fbPointer = ram->GetPointerToAddress(XE_FB_BASE);
#endif
  }
  // Reset the CPU
  xenonCPU.reset();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses, ram.get());
  // Ensure the IIC pointer in the PCI bridge is correct
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  // Set the CPU as inactive
  CPUStarted = false;
}

void XeMain::Reboot(u32 type) {
  // Check if the CPU is active
  if (CPUStarted) {
    // Shutdown the CPU
    ShutdownCPU();
  }
  // Set poweron type
  smcCore->SetPowerOnReason(static_cast<Xe::PCIDev::SMC_PWR_REASON>(type));
  // Setup CPU
  StartCPU();
}

void XeMain::ReloadFiles() {
  if (!GetCPU())
    return;
  GetCPU()->Halt();
  // Reset the SFCX
  sfcx = std::make_shared<STRIP_UNIQUE(sfcx)>("SFCX", SFCX_DEV_SIZE, Config::filepaths.nand, pciBridge.get(), ram.get());
  sfcx->Start();
  pciBridge->ResetPCIDevice(sfcx);
  // Reset the NAND
  nand = std::make_shared<STRIP_UNIQUE(nand)>("NAND", sfcx.get());
  rootBus->ResetDevice(nand);
  if (!CPUStarted) {
    // Reset the CPU again to reload 1bl and fuses
    xenonCPU.reset();
    xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses, ram.get());
    // Ensure the IIC pointer in the PCI bridge is correct
    pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  }
  GetCPU()->Continue();
}

void XeMain::CreateBridges() {
  LOG_INFO(Xenon, "Creating Host Bridge...");
  u64 ramSize = 512_MiB;
  if (!ram.get()) {
    LOG_ERROR(Xenon, "Unable to get RAM size! Defaulting to 512MiB");
  } else {
    ramSize = ram->GetSize();
  }
  // Create the PCI Bridge
  pciBridge = std::make_shared<STRIP_UNIQUE(pciBridge)>();
  hostBridge = std::make_shared<STRIP_UNIQUE(hostBridge)>(ramSize);

  // Transfers ownership
  hostBridge->RegisterPCIBridge(pciBridge);
}

void XeMain::CreateRootBus() {
  LOG_INFO(Xenon, "Creating Root Bus...");
  rootBus = std::make_shared<STRIP_UNIQUE(rootBus)>();

  rootBus->AddHostBridge(hostBridge);
  if (nand)
    rootBus->AddDevice(nand);
  rootBus->AddDevice(ram);
}

void XeMain::CreatePCIDevices(RAM *ram) {
  LOG_INFO(Xenon, "Creating PCI Devices...");
  ohci0 = std::make_shared<STRIP_UNIQUE(ohci0)>("OHCI0", OHCI_DEV_SIZE);
  ohci1 = std::make_shared<STRIP_UNIQUE(ohci1)>("OHCI1", OHCI_DEV_SIZE);
  pciBridge->AddPCIDevice(ohci0);
  pciBridge->AddPCIDevice(ohci1);

  ehci0 = std::make_shared<STRIP_UNIQUE(ehci0)>("EHCI0", EHCI_DEV_SIZE);
  ehci1 = std::make_shared<STRIP_UNIQUE(ehci1)>("EHCI1", EHCI_DEV_SIZE);
  pciBridge->AddPCIDevice(ehci0);
  pciBridge->AddPCIDevice(ehci1);

  audioController = std::make_shared<STRIP_UNIQUE(audioController)>("AUDIOCTRLR", AUDIO_CTRLR_DEV_SIZE);
  pciBridge->AddPCIDevice(audioController);

  ethernet = std::make_shared<STRIP_UNIQUE(ethernet)>("ETHERNET", ETHERNET_DEV_SIZE, pciBridge.get(), ram);
  pciBridge->AddPCIDevice(ethernet);

  sfcx = std::make_shared<STRIP_UNIQUE(sfcx)>("SFCX", SFCX_DEV_SIZE, Config::filepaths.nand, pciBridge.get(), ram);
  if (sfcx->hasInitialised) {
    pciBridge->AddPCIDevice(sfcx);
    nand = std::make_shared<STRIP_UNIQUE(nand)>("NAND", sfcx.get());
  }

  xma = std::make_shared<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
  pciBridge->AddPCIDevice(xma);

  odd = std::make_shared<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram);
  pciBridge->AddPCIDevice(odd);

  hdd = std::make_shared<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get(), ram);
  pciBridge->AddPCIDevice(hdd);

  smcCore = std::make_shared<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get());
  pciBridge->AddPCIDevice(smcCore);

  sfcx->Start();
}

Xe::XCPU::XenonCPU *XeMain::GetCPU() {
  return xenonCPU.get();
}