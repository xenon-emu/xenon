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
  file.close();
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
  // Create PPU elements
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, resetVector, XE_PVR, 0, "PPU0"); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, resetVector, XE_PVR, 2, "PPU1"); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, resetVector, XE_PVR, 4, "PPU2"); // Threads 4-5
  // Start execution on the main thread
  ppu0->StartExecution();
  // Get our CPI based on the first PPU, then share it across all PPUs
  ppu1->SetCPI(ppu0->GetCPI());
  ppu2->SetCPI(ppu0->GetCPI());
  // Start execution on the other threads
  ppu1->StartExecution();
  ppu2->StartExecution();
}

void Xenon::LoadElf(const std::string path) {
  // TODO(Vali0004): Fix multi-threading for ELF loading
  ppu0.reset();
  ppu1.reset();
  ppu2.reset();
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, 0, XE_PVR, 0, "PPU0"); // Threads 0-1
  std::filesystem::path filePath{ path };
  std::ifstream file{ filePath, std::ios_base::in | std::ios_base::binary };
  size_t fileSize = std::filesystem::file_size(filePath);
  std::unique_ptr<u8[]> elfBinary = std::make_unique<u8[]>(fileSize);
  file.read(reinterpret_cast<char*>(elfBinary.get()), fileSize);
  file.close();
  ppu0->loadElfImage(elfBinary.get(), fileSize);
  ppu0->StartExecution(false);
}

void Xenon::Halt(u64 haltOn) {
  if (ppu0.get()) ppu0->Halt(haltOn);
  if (ppu1.get()) ppu1->Halt(haltOn);
  if (ppu2.get()) ppu2->Halt(haltOn);
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

PPU *Xenon::GetPPU(u8 ppuID) {
  switch (ppuID) {
  case 0:
    return ppu0.get();
  case 1:
    return ppu1.get();
  case 2:
    return ppu2.get();
  }

  return nullptr;
}
