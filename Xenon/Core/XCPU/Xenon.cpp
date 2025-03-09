// Copyright 2025 Xenon Emulator Project

#include "Xenon.h"

#include "Base/Logging/Log.h"

Xenon::Xenon(RootBus *inBus, const std::string blPath, eFuses inFuseSet) {
  // First, Initialize system bus.
  mainBus = inBus;

  // Set SROM to 0.
  memset(xenonContext.SROM, 0, XE_SROM_SIZE);

  // Set Security Engine context to 0.
  memset(&xenonContext.secEngBlock, 0, sizeof(SOCSECENG_BLOCK));
  memset(xenonContext.secEngData, 0, XE_SECENG_SIZE);

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
      file.read(reinterpret_cast<char*>(xenonContext.SROM), XE_SROM_SIZE);
      LOG_INFO(Xenon, "1BL Loaded.");
    }
  }
}

Xenon::~Xenon() {
  ppu0.reset();
  ppu1.reset();
  ppu2.reset();
  delete[] xenonContext.SRAM;
  delete[] xenonContext.SROM;
  delete[] xenonContext.secEngData;
}

void Xenon::Start(u64 resetVector) {
  // Start execution on every thread.
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, XE_PVR, 0, "PPU0"); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, XE_PVR, 2, "PPU1"); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, XE_PVR, 4, "PPU2"); // Threads 4-5
  // Halt the CPU to ensure no opcodes are ran
  Halt();
  // Get our CPI based on the first PPU, then share it across all PPUs
  ppu1->SetCPI(ppu0->GetCPI());
  ppu2->SetCPI(ppu0->GetCPI());
  // Continue after halting
  Continue();
}

void Xenon::Halt() {
  ppu0->Halt();
  ppu1->Halt();
  ppu2->Halt();
}
void Xenon::Continue() {
  ppu0->Continue();
  ppu1->Continue();
  ppu2->Continue();
}
void Xenon::Step(int amount) {
  ppu0->Step(amount);
  ppu1->Step(amount);
  ppu2->Step(amount);
}
bool Xenon::IsHalted() {
  bool halted = ppu0->IsHalted();
  halted = ppu1->IsHalted();
  halted = ppu2->IsHalted();
  return halted;
}
