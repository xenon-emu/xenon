// Copyright 2025 Xenon Emulator Project

#include "Xe_Main.h"

XeMain::XeMain() {
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
  renderer = std::make_unique<STRIP_UNIQUE(renderer)>(ram.get());
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
  nandDevice.reset();
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
}

void XeMain::saveConfig() {
  Config::saveConfig(rootDirectory / "config.toml");
}
void XeMain::loadConfig() {
  Config::loadConfig(rootDirectory / "config.toml");
}


void XeMain::start() {
  if (!xenonCPU.get()) {
#ifndef NO_GFX
    renderHalt = true;
#endif
    LOG_CRITICAL(Xenon, "Failed to initialize Xenon's CPU!");
    SYSTEM_PAUSE();
#ifndef NO_GFX
    renderHalt = false;
#endif
    return;
  }
  // If we have already created PPU0 and we already have RAM, then reset it
  if (xenonCPU && xenonCPU->GetPPU(0) && ram) {
    ram->Reset();
  }
  if (Config::xcpu.elfLoader) {
    // Load the elf
    xenonCPU->LoadElf(Config::filepaths.elfBinary);
  } else {
    // CPU Start routine and entry point.
    xenonCPU->Start(0x20000000100);
  }
}

void XeMain::addPCIDevices() {
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
  hostBridge = std::make_unique<STRIP_UNIQUE(hostBridge)>();

  hostBridge->RegisterXGPU(xenos.get());
  hostBridge->RegisterPCIBridge(pciBridge.get());
}

void XeMain::createRootBus() {
  rootBus = std::make_unique<STRIP_UNIQUE(rootBus)>();

  rootBus->AddHostBridge(hostBridge.get());
  rootBus->AddDevice(nandDevice.get());
  rootBus->AddDevice(ram.get());
}

void XeMain::createPCIDevices() {
  ethernet = std::make_unique<STRIP_UNIQUE(ethernet)>("ETHERNET", ETHERNET_DEV_SIZE);
  audioController = std::make_unique<STRIP_UNIQUE(audioController)>("AUDIOCTRLR", AUDIO_CTRLR_DEV_SIZE);
  ohci0 = std::make_unique<STRIP_UNIQUE(ohci0)>("OHCI0", OHCI0_DEV_SIZE);
  ohci1 = std::make_unique<STRIP_UNIQUE(ohci1)>("OHCI1", OHCI1_DEV_SIZE);
  ehci0 = std::make_unique<STRIP_UNIQUE(ehci0)>("EHCI0", EHCI0_DEV_SIZE);
  ehci1 = std::make_unique<STRIP_UNIQUE(ehci1)>("EHCI1", EHCI1_DEV_SIZE);

  ram = std::make_unique<STRIP_UNIQUE(ram)>("RAM", RAM_START_ADDR, RAM_START_ADDR + RAM_SIZE, false);
  sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", Config::filepaths.nand, SFCX_DEV_SIZE, pciBridge.get());
  xma = std::make_unique<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
  odd = std::make_unique<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram.get());
  hdd = std::make_unique<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get());
  smcCore = std::make_unique<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get(), smcCoreState.get());
  nandDevice = std::make_unique<STRIP_UNIQUE(nandDevice)>("NAND", Config::filepaths.nand, NAND_START_ADDR, NAND_END_ADDR, true);
}

void XeMain::createSMCState() {
  // Initialize several settings from the struct.
  smcCoreState = std::make_unique<STRIP_UNIQUE(smcCoreState)>();
  smcCoreState->currentCOMPort = Config::smc.COMPort().data();
  smcCoreState->uartBackup = Config::smc.useBackupUart;
  smcCoreState->currAVPackType =
    (Xe::PCIDev::SMC::SMC_AVPACK_TYPE)Config::smc.avPackType;
  smcCoreState->currPowerOnReas =
    (Xe::PCIDev::SMC::SMC_PWR_REAS)Config::smc.powerOnReason;
  smcCoreState->currTrayState = Xe::PCIDev::SMC::SMC_TRAY_STATE::SMC_TRAY_CLOSED;
}
