// Copyright 2025 Xenon Emulator Project

#include "Xenon.h"

#include "Base/Logging/Log.h"

Xenon::Xenon(RootBus *inBus, const std::string blPath, eFuses inFuseSet) {
  // First, Initialize system bus.
  mainBus = inBus;

  // Allocate and zero the SROM
  xenonContext.SRAM.resize(XE_SRAM_SIZE);
  memset(xenonContext.SRAM.data(), 0, XE_SRAM_SIZE);
  // Allocate and zero the SRAM
  xenonContext.SROM.resize(XE_SROM_SIZE);
  memset(xenonContext.SROM.data(), 0, XE_SROM_SIZE);
  // Allocate and zero the Security Engine Data
  xenonContext.secEngData.resize(XE_SECENG_SIZE);
  memset(xenonContext.secEngData.data(), 0, XE_SECENG_SIZE);

  // Set Security Engine context to 0.
  memset(&xenonContext.secEngBlock, 0, sizeof(SOCSECENG_BLOCK));

  // Populate FuseSet.
  xenonContext.fuseSet = inFuseSet;

  // Load 1BL from path.
  std::ifstream file(blPath, std::ios_base::in | std::ios_base::binary);
  if (!file.is_open()) {
    LOG_CRITICAL(Xenon, "Unable to open file: {} for reading. Check your file path. System Stopped!", blPath);
    SYSTEM_PAUSE();
  } else {
    size_t fileSize = std::filesystem::file_size(blPath);
    if (fileSize == XE_SROM_SIZE) {
      file.read(reinterpret_cast<char*>(xenonContext.SROM.data()), XE_SROM_SIZE);
      LOG_INFO(Xenon, "1BL Loaded.");
    }
  }
}

Xenon::~Xenon() {
  ppu0.reset();
  ppu1.reset();
  ppu2.reset();
  xenonContext.SROM.clear();
  xenonContext.SRAM.clear();
  xenonContext.secEngData.clear();
}

void Xenon::Start(u64 resetVector) {
  // Start execution on every thread.
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, XE_PVR, 0, "PPU0"); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, XE_PVR, 2, "PPU1"); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, XE_PVR, 4, "PPU2"); // Threads 4-5
}

void Xenon::Halt() {
  if (ppu0.get()) ppu0->Halt();
  if (ppu1.get()) ppu1->Halt();
  if (ppu2.get()) ppu2->Halt();
}
void Xenon::Continue() {
  if (ppu0.get()) ppu0->Continue();
  if (ppu1.get()) ppu1->Continue();
  if (ppu2.get()) ppu2->Continue();
}
void Xenon::Step(int amount) {
  if (ppu0.get()) ppu0->Step(amount);
  if (ppu1.get()) ppu1->Step(amount);
  if (ppu2.get()) ppu2->Step(amount);
}
bool Xenon::IsHalted() {
  bool halted = false;
  if (ppu0.get()) halted = ppu0->IsHalted();
  if (ppu1.get()) halted = ppu1->IsHalted();
  if (ppu2.get()) halted = ppu2->IsHalted();
  return halted;
}
