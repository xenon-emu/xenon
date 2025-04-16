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
  CPUStarted = true;
  if (Config::xcpu.elfLoader) {
    // Load the elf
    xenonCPU->LoadElf(Config::filepaths.elfBinary);
  } else {
    // CPU Start routine and entry point.
    xenonCPU->Start(0x20000000100);
  }
}

void XeMain::shutdownCPU() {
  // Halt rendering (prevent debugger from segfaulting)
#ifndef NO_GFX
  renderHalt = true;
#endif
  // Halt the CPU
  xenonCPU->Halt();
  // Reset RAM
  ram->Reset();
  // Ensure GPU registers are reset
  xenos.reset();
  xenos = std::make_unique<STRIP_UNIQUE(xenos)>(ram.get());
  // Re-register the XGPU to the host bridge
  hostBridge->RegisterXGPU(xenos.get());
#ifndef NO_GFX
  // Reinit RAM handles for rendering (should be valid, mainly safety)
  renderer->ramPointer = ram.get();
  renderer->fbPointer = renderer->ramPointer->getPointerToAddress(XE_FB_BASE);
#endif
  // Reset the CPU
  xenonCPU.reset();
  xenonCPU = std::make_unique<STRIP_UNIQUE(xenonCPU)>(rootBus.get(), Config::filepaths.oneBl, Config::filepaths.fuses);
  // Ensure the IIC pointer in the PCI bridge is correct
  pciBridge->RegisterIIC(xenonCPU->GetIICPointer());
  // Continue rendering
#ifndef NO_GFX
  renderHalt = false;
#endif
  // Set the CPU as inactive
  CPUStarted = false;
}

void XeMain::reboot(Xe::PCIDev::SMC::SMC_PWR_REASON type) {
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
  getCPU()->Halt();
  // Reset the SFCX
  sfcx.reset();
  sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", Config::filepaths.nand, SFCX_DEV_SIZE, pciBridge.get(), ram.get());
  pciBridge->resetPCIDevice(sfcx.get());
  // Reset the NAND
  nandDevice.reset();
  nandDevice = std::make_unique<STRIP_UNIQUE(nandDevice)>("NAND", sfcx.get(), NAND_START_ADDR, NAND_END_ADDR, true);
  rootBus->ResetDevice(nandDevice.get());
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
  sfcx = std::make_unique<STRIP_UNIQUE(sfcx)>("SFCX", Config::filepaths.nand, SFCX_DEV_SIZE, pciBridge.get(), ram.get());
  xma = std::make_unique<STRIP_UNIQUE(xma)>("XMA", XMA_DEV_SIZE);
  odd = std::make_unique<STRIP_UNIQUE(odd)>("CDROM", ODD_DEV_SIZE, pciBridge.get(), ram.get());
  hdd = std::make_unique<STRIP_UNIQUE(hdd)>("HDD", HDD_DEV_SIZE, pciBridge.get());
  smcCore = std::make_unique<STRIP_UNIQUE(smcCore)>("SMC", SMC_DEV_SIZE, pciBridge.get(), smcCoreState.get());
  nandDevice = std::make_unique<STRIP_UNIQUE(nandDevice)>("NAND", sfcx.get(), NAND_START_ADDR, NAND_END_ADDR, true);
}

void XeMain::createSMCState() {
  // Initialize several settings from the struct.
  smcCoreState = std::make_unique<STRIP_UNIQUE(smcCoreState)>();
  smcCoreState->currentUARTSytem = Config::smc.uartSystem;
#ifdef _WIN32
  smcCoreState->currentCOMPort = Config::smc.COMPort().data();
#endif
  smcCoreState->socketIp = Config::smc.socketIp;
  smcCoreState->socketPort = Config::smc.socketPort;
  smcCoreState->currAVPackType =
    (Xe::PCIDev::SMC::SMC_AVPACK_TYPE)Config::smc.avPackType;
  smcCoreState->currPowerOnReason =
    (Xe::PCIDev::SMC::SMC_PWR_REASON)Config::smc.powerOnReason;
  smcCoreState->currTrayState = Xe::PCIDev::SMC::SMC_TRAY_STATE::SMC_TRAY_CLOSED;
}
