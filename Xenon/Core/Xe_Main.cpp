// Copyright 2025 Xenon Emulator Project

#include "Core/Xe_Main.h"

XeMain::XeMain() {
  userDirectory = Base::FS::GetUserPath(Base::FS::PathType::UserDir);
  Config::loadConfig(userDirectory / "xenon_config.toml");
  Base::Log::Initialize();
  Base::Log::Start();
  auto logLevel = Config::getCurrentLogLevel();
  logFilter = std::make_unique<STRIP_UNIQUE(logFilter)>(logLevel);
  Base::Log::SetGlobalFilter(*logFilter);
  pciBridge = std::make_unique<STRIP_UNIQUE(pciBridge)>();
  createSMCState();
  createPCIDevices();
  addPCIDevices();
  getFuses();
  renderer = std::make_unique<STRIP_UNIQUE(renderer)>(ram.get());
  xenos = std::make_unique<STRIP_UNIQUE(xenos)>(ram.get());
  createHostBridge();
  createRootBus();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::oneBlPath(), cpuFuses);
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
}
XeMain::~XeMain() {
  // Save config incase it was modified
  Config::saveConfig(userDirectory / "xenon_config.toml");

  // Delete the log filter
  logFilter.reset();

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

  // Delete the renderer
  renderer.reset();

  // Delete the XGPU and XCPU
  xenos.reset();
  xenonCPU.reset();
}

void XeMain::start() {
  if (!xenonCPU.get()) {
    renderHalt = true;
    LOG_CRITICAL(Xenon, "Failed to initialize Xenon's CPU!");
    SYSTEM_PAUSE();
    renderHalt = false;
    return;
  }
  if (!Config::loadElfs()) {
    // CPU Start routine and entry point.
    xenonCPU->Start(0x20000000100);
  } else {
    xenonCPU->LoadElf(Config::kernelPath());
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
  sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", Config::nandPath(), SFCX_DEV_SIZE, pciBridge.get());
  xma = std::make_unique<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
  odd = std::make_unique<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram.get());
  hdd = std::make_unique<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get());
  smcCore = std::make_unique<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get(), smcCoreState.get());
  nandDevice = std::make_unique<STRIP_UNIQUE(nandDevice)>("NAND", Config::nandPath(), NAND_START_ADDR, NAND_END_ADDR, true);
}

void XeMain::createSMCState() {
  // Initialize several settings from the struct.
  smcCoreState = std::make_unique<STRIP_UNIQUE(smcCoreState)>();
  smcCoreState->currentCOMPort = Config::COMPort().data();
  smcCoreState->uartBackup = Config::useBackupUART();
  smcCoreState->currAVPackType =
    (Xe::PCIDev::SMC::SMC_AVPACK_TYPE)Config::smcCurrentAvPack();
  smcCoreState->currPowerOnReas =
    (Xe::PCIDev::SMC::SMC_PWR_REAS)Config::smcPowerOnType();
  smcCoreState->currTrayState = Xe::PCIDev::SMC::SMC_TRAY_STATE::SMC_TRAY_CLOSED;
}

void XeMain::getFuses() {
  std::ifstream file(Config::fusesPath());
  if (!file.is_open()) {
    cpuFuses.fuseLine00 = { 0x9999999999999999 };
    return;
  }
  std::vector<std::string> fusesets;
  std::string fuseset;
  while (std::getline(file, fuseset)) {
    if (size_t pos = fuseset.find(": "); pos != std::string::npos) {
      fuseset = fuseset.substr(pos + 2);
    }
    fusesets.push_back(fuseset);
  }
  // Got some fuses, let's print them!
  u64* fuses = reinterpret_cast<u64*>(&cpuFuses);
  LOG_INFO(System, "Current FuseSet:");
  for (int i = 0; i < 12; i++) {
    fuseset = fusesets[i];
    fuses[i] = strtoull(fuseset.c_str(), nullptr, 16);
    LOG_INFO(System, " * FuseSet {:02}: 0x{}", i, fuseset.c_str());
  }
}
