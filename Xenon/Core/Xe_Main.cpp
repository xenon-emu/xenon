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
  createPCIDevices();
  addPCIDevices();
  getFuses();
  renderer = std::make_shared<STRIP_UNIQUE(renderer)>(ram.get());
  xenos = std::make_shared<STRIP_UNIQUE(xenos)>(ram.get());
  createHostBridge();
  createRootBus();
  xenonCPU = std::make_shared<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::oneBlPath(), cpuFuses);
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
}
XeMain::~XeMain() {
  // Save config incase it was modified
  Config::saveConfig(userDirectory / "xenon_config.toml");

  // The CPU needs to be killed first, to ensure execution is finished
  xenonCPU.reset();

  // Delete the GPU first, as we likely would have exited from it.
  xenos.reset();

  // Delete all objects
  logFilter.reset();

  rootBus.reset();
  hostBridge.reset();
  pciBridge.reset();

  smcCoreState.reset();
  smcCore.reset();
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

  renderer.reset();
}

void XeMain::start() {
  // CPU Start routine and entry point.
  xenonCPU->Start(0x20000000100);
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
  createSMCState();
  ethernet = std::make_unique<STRIP_UNIQUE(ethernet)>("ETHERNET", ETHERNET_DEV_SIZE);
  audioController = std::make_unique<STRIP_UNIQUE(audioController)>("AUDIOCTRLR", AUDIO_CTRLR_DEV_SIZE);
  ohci0 = std::make_unique<STRIP_UNIQUE(ohci0)>("OHCI0", OHCI0_DEV_SIZE);
  ohci1 = std::make_unique<STRIP_UNIQUE(ohci1)>("OHCI1", OHCI1_DEV_SIZE);
  ehci0 = std::make_unique<STRIP_UNIQUE(ehci0)>("EHCI0", EHCI0_DEV_SIZE);
  ehci1 = std::make_unique<STRIP_UNIQUE(ehci1)>("EHCI1", EHCI1_DEV_SIZE);

  ram = std::make_shared<STRIP_UNIQUE(ram)>("RAM", RAM_START_ADDR, RAM_START_ADDR + RAM_SIZE, false);
  sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", Config::nandPath(), SFCX_DEV_SIZE, pciBridge.get());
  xma = std::make_unique<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
  odd = std::make_shared<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram.get());
  hdd = std::make_shared<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get());
  smcCore = std::make_unique<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get(), smcCoreState.get());
  nandDevice = std::make_unique<STRIP_UNIQUE(nandDevice)>("NAND", Config::nandPath(), NAND_START_ADDR, NAND_END_ADDR, true);
}

void XeMain::createSMCState() {
  // Initialize several settings from the struct.
  smcCoreState = std::make_shared<STRIP_UNIQUE(smcCoreState)>();
  smcCoreState->currentCOMPort = Config::COMPort()->data();
  smcCoreState->currAVPackType =
    (Xe::PCIDev::SMC::SMC_AVPACK_TYPE)Config::smcCurrentAvPack();
  smcCoreState->currPowerOnReas =
    (Xe::PCIDev::SMC::SMC_PWR_REAS)Config::smcPowerOnType();
  smcCoreState->currTrayState = Xe::PCIDev::SMC::SMC_TRAY_STATE::SMC_TRAY_CLOSE;
}

void XeMain::getFuses() {
  std::ifstream file(Config::fusesPath());
  if (!file.is_open())
  {
    cpuFuses.fuseLine00 = { 0x9999999999999999 };
    return;
  }
  std::vector<std::string> fusesets;
  std::string fuseset;
  while (std::getline(file, fuseset))
  {
    if (size_t pos = fuseset.find(": "); pos != std::string::npos)
    {
      fuseset = fuseset.substr(pos + 2);
    }
    fusesets.push_back(fuseset);
  }
  // Got some fuses, let's print them!
  u64* fuses = reinterpret_cast<u64*>(&cpuFuses);
  LOG_INFO(System, "Current FuseSet:");
  for (int i = 0; i < 12; i++)
  {
    fuseset = fusesets[i];
    fuses[i] = strtoull(fuseset.c_str(), 0, 16);
    LOG_INFO(System, " * FuseSet {:02}: 0x{}", i, fuseset.c_str());
  }
}