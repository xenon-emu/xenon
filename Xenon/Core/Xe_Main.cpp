// Copyright 2025 Xenon Emulator Project

#include "Xe_Main.h"

XeMain::XeMain() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "Create", MP_AUTO);
  Base::Log::Initialize();
  Base::Log::Start();
  rootDirectory = Base::FS::GetUserPath(Base::FS::PathType::RootDir);
  loadConfig();
  logFilter = std::make_unique<STRIP_UNIQUE(logFilter)>(Config::log.currentLevel);
  Base::Log::SetGlobalFilter(*logFilter);
  pciBridge = std::make_unique<STRIP_UNIQUE(pciBridge)>();
  createSMCState();
  createPCIDevices();
  addPCIDevices();
#ifndef NO_GFX
  renderer = std::make_unique<Render::OGLRenderer>(ram.get());
#endif
  xenos = std::make_unique<STRIP_UNIQUE(xenos)>(ram.get());
  createHostBridge();
  createRootBus();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
}
XeMain::~XeMain() {
  saveConfig();
  // Delete the XGPU and XCPU
  xenos.reset();
  xenonCPU.reset();

  // Delete all PCI devices and their deps
  smcCore.reset();
  smcCoreState.reset();
  ethernet.reset();
  audioController.reset();
  ohci0.reset();
  ohci1.reset();
  ehci0.reset();
  ehci1.reset();

  sfcx.reset();
  nand.reset();
  ram.reset();
  xma.reset();
  odd.reset();
  hdd.reset();

  // Delete the bridges
  rootBus.reset();
  hostBridge.reset();
  pciBridge.reset();

#ifndef NO_GFX
  // Delete the renderer
  renderer.reset();
#endif

  // Stop the logger
  Base::Log::Stop();

  // Delete the log filter
  logFilter.reset();
#if AUTO_FLIP
  MicroProfileStopAutoFlip();
#endif
  MicroProfileShutdown();
}

void XeMain::saveConfig() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "SaveConfig", MP_AUTO);
  Config::saveConfig(rootDirectory / "config.toml");
}
void XeMain::loadConfig() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "LoadConfig", MP_AUTO);
  Config::loadConfig(rootDirectory / "config.toml");
}

void XeMain::start() {
  if (!xenonCPU.get()) {
    LOG_CRITICAL(Xenon, "Failed to initialize Xenon's CPU!");
    SYSTEM_PAUSE();
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

void XeMain::shutdownCPU() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "ShutdownCPU", MP_AUTO);
  // Set the CPU to 'Resetting' mode before killing the handle
  xenonCPU->Reset();
  // Reset RAM
  ram->Reset();
#ifndef NO_GFX
  // Reinit RAM handles for rendering (should be valid, mainly safety)
  renderer->ramPointer = ram.get();
  renderer->fbPointer = ram->getPointerToAddress(XE_FB_BASE);
#endif
  // Reset the CPU
  xenonCPU.reset();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
  // Ensure the IIC pointer in the PCI bridge is correct
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  // Set the CPU as inactive
  CPUStarted = false;
}

void XeMain::reboot(Xe::PCIDev::SMC_PWR_REASON type) {
  MICROPROFILE_SCOPEI("[Xe::Main]", "Reboot", MP_AUTO);
  // Check if the CPU is active
  if (CPUStarted) {
    // Shutdown the CPU
    shutdownCPU();
  }
  // Set poweron type
  smcCoreState->currPowerOnReason = type;
  // Setup CPU
  start();
}

void XeMain::reloadFiles() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "ReloadFiles", MP_AUTO);
  getCPU()->Halt();
  // Reset the SFCX
  sfcx.reset();
  sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", SFCX_DEV_SIZE, Config::filepaths.nand, pciBridge.get(), ram.get());
  pciBridge->resetPCIDevice(sfcx.get());
  // Reset the NAND
  nand.reset();
  nand = std::make_unique<STRIP_UNIQUE(nand)>("NAND", NAND_START_ADDR, NAND_END_ADDR, sfcx.get(), true);
  rootBus->ResetDevice(nand.get());
  if (!CPUStarted) {
    // Reset the CPU again to reload 1bl and fuses
    xenonCPU.reset();
    xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
    // Ensure the IIC pointer in the PCI bridge is correct
    pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  }
  getCPU()->Continue();
}

void XeMain::addPCIDevices() {
  MICROPROFILE_SCOPEI("[Xe::Main]", "AddPCIDevices", MP_AUTO);
  pciBridge->addPCIDevice(ohci0.get());
  pciBridge->addPCIDevice(ohci1.get());
  pciBridge->addPCIDevice(ehci0.get());
  pciBridge->addPCIDevice(ehci1.get());
  pciBridge->addPCIDevice(audioController.get());
  pciBridge->addPCIDevice(ethernet.get());
  pciBridge->addPCIDevice(sfcx.get());
  pciBridge->addPCIDevice(xma.get());
  pciBridge->addPCIDevice(odd.get());
  pciBridge->addPCIDevice(hdd.get());
  pciBridge->addPCIDevice(smcCore.get());
}

void XeMain::createHostBridge() {
  MICROPROFILE_SCOPEI("[Xe::Main::PCI]", "CreateHostBridge", MP_AUTO);
  hostBridge = std::make_unique<STRIP_UNIQUE(hostBridge)>();

  hostBridge->RegisterXGPU(xenos.get());
  hostBridge->RegisterPCIBridge(pciBridge.get());
}

void XeMain::createRootBus() {
  MICROPROFILE_SCOPEI("[Xe::Main::PCI]", "CreateRootBus", MP_AUTO);
  rootBus = std::make_unique<STRIP_UNIQUE(rootBus)>();

  rootBus->AddHostBridge(hostBridge.get());
  rootBus->AddDevice(nand.get());
  rootBus->AddDevice(ram.get());
}

void XeMain::createPCIDevices() {
  MICROPROFILE_SCOPEI("[Xe::Main::PCI]", "CreateDevices", MP_AUTO);
  {
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "Ethernet", MP_AUTO);
      ethernet = std::make_unique<STRIP_UNIQUE(ethernet)>("ETHERNET", ETHERNET_DEV_SIZE);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "AudioController", MP_AUTO);
      audioController = std::make_unique<STRIP_UNIQUE(audioController)>("AUDIOCTRLR", AUDIO_CTRLR_DEV_SIZE);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "OHCI", MP_AUTO);
      ohci0 = std::make_unique<STRIP_UNIQUE(ohci0)>("OHCI0", OHCI_DEV_SIZE);
      ohci1 = std::make_unique<STRIP_UNIQUE(ohci1)>("OHCI1", OHCI_DEV_SIZE);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "EHCI", MP_AUTO);
      ehci0 = std::make_unique<STRIP_UNIQUE(ehci0)>("EHCI0", EHCI_DEV_SIZE);
      ehci1 = std::make_unique<STRIP_UNIQUE(ehci1)>("EHCI1", EHCI_DEV_SIZE);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "RAM", MP_AUTO);
      ram = std::make_unique<STRIP_UNIQUE(ram)>("RAM", RAM_START_ADDR, RAM_START_ADDR + RAM_SIZE, false);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "SFCX", MP_AUTO);
      sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", SFCX_DEV_SIZE, Config::filepaths.nand, pciBridge.get(), ram.get());
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "XMA", MP_AUTO);
      xma = std::make_unique<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "ODD", MP_AUTO);
      odd = std::make_unique<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram.get());
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "HDD", MP_AUTO);
      hdd = std::make_unique<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get());
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "SMC", MP_AUTO);
      smcCore = std::make_unique<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get(), smcCoreState.get());
    }
    {
      MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "NAND", MP_AUTO);
      nand = std::make_unique<STRIP_UNIQUE(nand)>("NAND", NAND_START_ADDR, NAND_END_ADDR, sfcx.get(), true);
    }
  }
}

void XeMain::createSMCState() {
  MICROPROFILE_SCOPEI("[Xe::Main::PCI::Create]", "SMCState", MP_AUTO);
  // Initialize several settings from the struct.
  smcCoreState = std::make_unique<STRIP_UNIQUE(smcCoreState)>();
  smcCoreState->currentUARTSytem = Config::smc.uartSystem;
#ifdef _WIN32
  smcCoreState->currentCOMPort = Config::smc.COMPort();
#endif
  smcCoreState->socketIp = Config::smc.socketIp;
  smcCoreState->socketPort = Config::smc.socketPort;
  smcCoreState->currAVPackType = (Xe::PCIDev::SMC_AVPACK_TYPE)Config::smc.avPackType;
  smcCoreState->currPowerOnReason = (Xe::PCIDev::SMC_PWR_REASON)Config::smc.powerOnReason;
  smcCoreState->currTrayState = Xe::PCIDev::SMC_TRAY_CLOSED;
}
