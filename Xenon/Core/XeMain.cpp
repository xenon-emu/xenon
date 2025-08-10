// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "XeMain.h"

void XeMain::Create() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "Create", MP_AUTO);
  Base::Log::Initialize();
  Base::Log::Start();
  LOG_INFO(System, "Starting Xenon.");
  rootDirectory = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
  LoadConfig();
  Base::Log::Filter logFilter{ Config::log.currentLevel };
  Base::Log::SetGlobalFilter(logFilter);
  CreatePCIDevices();
#ifndef NO_GFX
  switch (Base::JoaatStringHash(Config::rendering.backend, false)) {
  case "OpenGL"_j:
    renderer = std::make_unique<Render::OGLRenderer>(ram.get());
    renderer->Start();
    break;
  case "Dummy"_j:
    renderer = std::make_unique<Render::DummyRenderer>(ram.get());
    renderer->Start();
    break;
  default:
    LOG_ERROR(Render, "Invalid renderer backend: {}", Config::rendering.backend);
    break;
  }
  xenos = std::make_unique<STRIP_UNIQUE(xenos)>(renderer.get(), ram.get(), pciBridge.get());
#else
  xenos = std::make_unique<STRIP_UNIQUE(xenos)>(nullptr, ram.get(), pciBridge.get());
#endif
  CreateHostBridge();
  CreateRootBus();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
}

void XeMain::Shutdown() {
  // Check if we've shutdown, no need to do it twice
  if (XeShutdonSignaled) {
    return;
  }
  // Set as already shutdown
  XeShutdonSignaled = true;

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
  MICROPROFILE_SCOPEI("[Xe::Main]", "SaveConfig", MP_AUTO);
  Config::saveConfig(rootDirectory / "config.toml");
}
void XeMain::LoadConfig() {
  LOG_INFO(Xenon, "Loading Config...");
  MICROPROFILE_SCOPEI("[Xe::Main]", "LoadConfig", MP_AUTO);
  Config::loadConfig(rootDirectory / "config.toml");
}

void XeMain::StartCPU() {
  LOG_INFO(Xenon, "Starting CPU...");
  if (!xenonCPU.get()) {
    LOG_CRITICAL(Xenon, "Failed to initialize Xenon's CPU!");
    Base::SystemPause();
    return;
  }
  CPUStarted = true;
  if (Config::xcpu.elfLoader) {
    // Load the elf
    MICROPROFILE_SCOPEI("[Xe::Main]", "LoadElf", MP_AUTO);
    xenonCPU->LoadElf(Config::filepaths.elfBinary);
  } else {
    MICROPROFILE_SCOPEI("[Xe::Main]", "Start", MP_AUTO);
    // CPU Start routine and entry point.
    xenonCPU->Start(0x20000000100);
  }
}

void XeMain::ShutdownCPU() {
  if (!CPUStarted) {
    return;
  }
  MICROPROFILE_SCOPEI("[Xe::Main]", "ShutdownCPU", MP_AUTO);
  // Set the CPU to 'Resetting' mode before killing the handle
  xenonCPU->Reset();
  // Reset RAM
  ram->Reset();
#ifndef NO_GFX
  // Reinit RAM handles for rendering (should be valid, mainly safety)
  renderer->ramPointer = ram.get();
  renderer->fbPointer = ram->GetPointerToAddress(XE_FB_BASE);
#endif
  // Reset the CPU
  xenonCPU.reset();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
  // Ensure the IIC pointer in the PCI bridge is correct
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  // Set the CPU as inactive
  CPUStarted = false;
}

void XeMain::Reboot(u32 type) {
  MICROPROFILE_SCOPEI("[Xe::Main]", "Reboot", MP_AUTO);
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
  MICROPROFILE_SCOPEI("[Xe::Main]", "ReloadFiles", MP_AUTO);
  if (!GetCPU())
    return;
  GetCPU()->Halt();
  // Reset the SFCX
  u32 cpi = xenonCPU->GetCPI();
  sfcx = std::make_shared<STRIP_UNIQUE(sfcx)>("SFCX", SFCX_DEV_SIZE, Config::filepaths.nand, cpi, pciBridge.get(), ram.get());
  sfcx->Start();
  pciBridge->ResetPCIDevice(sfcx);
  // Reset the NAND
  nand = std::make_shared<STRIP_UNIQUE(nand)>("NAND", sfcx.get(), true);
  rootBus->ResetDevice(nand);
  if (!CPUStarted) {
    // Reset the CPU again to reload 1bl and fuses
    xenonCPU.reset();
    xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
    // Ensure the IIC pointer in the PCI bridge is correct
    pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  }
  GetCPU()->Continue();
}

void XeMain::CreateHostBridge() {
  MICROPROFILE_SCOPEI("[Xe::Main::PCI]", "CreateHostBridge", MP_AUTO);
  LOG_INFO(Xenon, "Creating Host Bridge...");
  u64 ramSize = 512_MiB;
  if (!ram.get()) {
    LOG_ERROR(Xenon, "Unable to get RAM size! Defaulting to 512MiB");
  } else {
    ramSize = ram->GetSize();
  }
  hostBridge = std::make_shared<STRIP_UNIQUE(hostBridge)>(ramSize);

  // Transfers ownership of xenos & pciBridge
  hostBridge->RegisterXGPU(xenos);
  hostBridge->RegisterPCIBridge(pciBridge);
}

void XeMain::CreateRootBus() {
  MICROPROFILE_SCOPEI("[Xe::Main::PCI]", "CreateRootBus", MP_AUTO);
  LOG_INFO(Xenon, "Creating Root Bus...");
  rootBus = std::make_shared<STRIP_UNIQUE(rootBus)>();

  rootBus->AddHostBridge(hostBridge);
  rootBus->AddDevice(nand);
  rootBus->AddDevice(ram);
}

void XeMain::CreatePCIDevices() {
  pciBridge = std::make_shared<STRIP_UNIQUE(pciBridge)>();
  LOG_INFO(Xenon, "Creating PCI Devices...");
  MICROPROFILE_SCOPEI("[Xe::Main::PCI]", "CreateDevices", MP_AUTO);
  {
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "RAM", MP_AUTO);
      ram = std::make_shared<STRIP_UNIQUE(ram)>("RAM", RAM_START_ADDR, Config::xcpu.ramSize, false);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "OHCI", MP_AUTO);
      ohci0 = std::make_shared<STRIP_UNIQUE(ohci0)>("OHCI0", OHCI_DEV_SIZE);
      pciBridge->AddPCIDevice(ohci0);
      ohci1 = std::make_shared<STRIP_UNIQUE(ohci1)>("OHCI1", OHCI_DEV_SIZE);
      pciBridge->AddPCIDevice(ohci1);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "EHCI", MP_AUTO);
      ehci0 = std::make_shared<STRIP_UNIQUE(ehci0)>("EHCI0", EHCI_DEV_SIZE);
      pciBridge->AddPCIDevice(ehci0);
      ehci1 = std::make_shared<STRIP_UNIQUE(ehci1)>("EHCI1", EHCI_DEV_SIZE);
      pciBridge->AddPCIDevice(ehci1);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "AudioController", MP_AUTO);
      audioController = std::make_shared<STRIP_UNIQUE(audioController)>("AUDIOCTRLR", AUDIO_CTRLR_DEV_SIZE);
      pciBridge->AddPCIDevice(audioController);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "Ethernet", MP_AUTO);
      ethernet = std::make_shared<STRIP_UNIQUE(ethernet)>("ETHERNET", ETHERNET_DEV_SIZE, pciBridge.get(), ram.get());
      pciBridge->AddPCIDevice(ethernet);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "SFCX", MP_AUTO);
      sfcx = std::make_shared<STRIP_UNIQUE(sfcx)>("SFCX", SFCX_DEV_SIZE, Config::filepaths.nand, 0, pciBridge.get(), ram.get());
      pciBridge->AddPCIDevice(sfcx);
      nand = std::make_shared<STRIP_UNIQUE(nand)>("NAND", sfcx.get(), true);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "XMA", MP_AUTO);
      xma = std::make_shared<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
      pciBridge->AddPCIDevice(xma);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "ODD", MP_AUTO);
      odd = std::make_shared<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram.get());
      pciBridge->AddPCIDevice(odd);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "HDD", MP_AUTO);
      hdd = std::make_shared<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get(), ram.get());
      pciBridge->AddPCIDevice(hdd);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "SMC", MP_AUTO);
      smcCore = std::make_shared<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get());
      pciBridge->AddPCIDevice(smcCore);
    }
    sfcx->Start();
  }
}

Xenon *XeMain::GetCPU() {
  return xenonCPU.get();
}