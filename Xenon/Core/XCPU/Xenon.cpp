// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Xenon.h"

#include "Base/Logging/Log.h"
#include "Interpreter/PPCInterpreter.h"

Xenon::Xenon(RootBus *inBus, const std::string blPath, const std::string fusesPath) {
  // First, Initialize system bus.
  mainBus = inBus;

  // Set SROM to 0.
  memset(xenonContext.SROM, 0, XE_SROM_SIZE);

  // Populate FuseSet
  {
    std::ifstream file(fusesPath);
    if (!file.is_open()) {
      xenonContext.socSecOTPBlock.get()->sec->AsULONGLONG = { 0x9999999999999999 };
    } else {
      std::vector<std::string> fusesets;
      std::string fuseset;
      while (std::getline(file, fuseset)) {
        if (size_t pos = fuseset.find(": "); pos != std::string::npos) {
          fuseset = fuseset.substr(pos + 2);
        }
        fusesets.push_back(fuseset);
      }
      // Got some fuses, let's print them!
      LOG_INFO(System, "Current FuseSet:");
      for (int i = 0; i < 12; i++) {
        fuseset = fusesets[i];
        LOG_INFO(System, " * FuseSet {:02}: 0x{}", i, fuseset.c_str());
      }

      xenonContext.socSecOTPBlock.get()->sec[0].AsULONGLONG = strtoull(fusesets[0].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->ConsoleType[0] = strtoull(fusesets[1].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->ConsoleSequence[0] = strtoull(fusesets[2].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->UniqueId1[0] = strtoull(fusesets[3].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->UniqueId2[0] = strtoull(fusesets[4].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->UniqueId3[0] = strtoull(fusesets[5].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->UniqueId4[0] = strtoull(fusesets[6].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->UpdateSequence[0] = strtoull(fusesets[7].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->EepromKey1[0] = strtoull(fusesets[8].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->EepromKey2[0] = strtoull(fusesets[9].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->EepromHash1[0] = strtoull(fusesets[10].c_str(), nullptr, 16);
      xenonContext.socSecOTPBlock.get()->EepromHash2[0] = strtoull(fusesets[11].c_str(), nullptr, 16);
    }
  }

  // Load 1BL from path.
  std::ifstream file(blPath, std::ios_base::in | std::ios_base::binary);
  if (!file.is_open()) {
    LOG_CRITICAL(Xenon, "Unable to open file: {} for reading. Check your file path. System Stopped!", blPath);
    SystemPause();
  } else {
    u64 fileSize = 0;
    // fs::file_size can cause a exception if it is not a valid file
    try {
      std::error_code ec;
      fileSize = std::filesystem::file_size(blPath, ec);
      if (fileSize == -1 || !fileSize) {
        fileSize = 0;
        LOG_ERROR(Base_Filesystem, "Failed to retrieve the file size of {} (Error: {})", blPath, ec.message());
      }
    }
    catch (const std::exception& ex) {
      LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Reason: {}", ex.what());
      return;
    }
    if (fileSize == XE_SROM_SIZE) {
      file.read(reinterpret_cast<char*>(xenonContext.SROM), XE_SROM_SIZE);
      LOG_INFO(Xenon, "1BL Loaded.");
    }
  }
  file.close();

  // Asign Interpreter global variables
  PPCInterpreter::CPUContext = &xenonContext;
  PPCInterpreter::sysBus = mainBus;

  // Setup SOC blocks.
  xenonContext.socPRVBlock.get()->PowerOnResetStatus.AsBITS.SecureMode = 1; // CB Checks this.
  xenonContext.socPRVBlock.get()->PowerManagementControl.AsULONGLONG = 0x382C00000000B001ULL; // Power Management Control.
}

Xenon::~Xenon() {
  ppu0.reset();
  ppu1.reset();
  ppu2.reset();
  delete[] xenonContext.SRAM;
  delete[] xenonContext.SROM;
}

void Xenon::Start(u64 resetVector) {
  // If we already have active objects, halt cpu and kill threads
  if (ppu0.get()) {
    Halt();
    ppu0.reset();
    ppu1.reset();
    ppu2.reset();
  }
  // Create PPU elements
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, resetVector, 0); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, resetVector, 2); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, resetVector, 4); // Threads 4-5
  // Start execution on the main thread
  ppu0->StartExecution();
  sharedCPI = ppu0->GetCPI();
  // Get our CPI based on the first PPU, then share it across all PPUs
  ppu1->SetCPI(GetCPI());
  ppu2->SetCPI(GetCPI());
  // Start execution on the other threads
  ppu1->StartExecution();
  ppu2->StartExecution();
}

u32 Xenon::RunCPITests(u64 resetVector) {
  // Create PPU element
  ppu0.reset();
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, resetVector, 0); // Threads 0-1
  // Start execution on the main thread
  ppu0->StartExecution();
  // Get CPU and reset PPU
  u32 cpi = ppu0->GetCPI();
  ppu0.reset();
  return cpi;
}

void Xenon::LoadElf(const std::string path) {
  ppu0.reset();
  ppu1.reset();
  ppu2.reset();
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, 0, 0); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, 0, 2); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, 0, 4); // Threads 4-5
  std::filesystem::path filePath{ path };
  std::ifstream file{ filePath, std::ios_base::in | std::ios_base::binary };
  u64 fileSize = 0;
  // fs::file_size can cause a exception if it is not a valid file
  try {
    std::error_code ec;
    fileSize = std::filesystem::file_size(filePath, ec);
    if (fileSize == -1 || !fileSize) {
      fileSize = 0;
      LOG_ERROR(Base_Filesystem, "Failed to retrieve the file size of {} (Error: {})", filePath.string(), ec.message());
    }
  }
  catch (const std::exception& ex) {
    LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Exception: {}",
      ex.what());
    return;
  }
  std::unique_ptr<u8[]> elfBinary = std::make_unique<u8[]>(fileSize);
  file.read(reinterpret_cast<char*>(elfBinary.get()), fileSize);
  file.close();
  ppu0->loadElfImage(elfBinary.get(), fileSize);
  // Start execution on the main thread
  ppu0->StartExecution(false);
  // Get our CPI based on the first PPU, then share it across all PPUs
  ppu1->SetCPI(ppu0->GetCPI());
  ppu2->SetCPI(ppu0->GetCPI());
  // Start execution on the other threads
  ppu1->StartExecution(false);
  ppu2->StartExecution(false);
}

void Xenon::Reset() {
  if (ppu0.get())
    ppu0->Reset();
  std::this_thread::sleep_for(200ms);
  if (ppu1.get())
    ppu1->Reset();
  std::this_thread::sleep_for(200ms);
  if (ppu2.get())
    ppu2->Reset();
  std::this_thread::sleep_for(200ms);
}

void Xenon::Halt(u64 haltOn, bool requestedByGuest, u8 ppuId, ePPUThread threadId) {
  if (ppu0.get())
    ppu0->Halt(haltOn, requestedByGuest, ppuId, threadId);
  if (ppu1.get())
    ppu1->Halt(haltOn, requestedByGuest, ppuId, threadId);
  if (ppu2.get())
    ppu2->Halt(haltOn, requestedByGuest, ppuId, threadId);
}

void Xenon::Continue() {
  if (ppu0.get())
    ppu0->Continue();
  if (ppu1.get())
    ppu1->Continue();
  if (ppu2.get())
    ppu2->Continue();
}

void Xenon::ContinueFromException() {
  if (ppu0.get())
    ppu0->ContinueFromException();
  if (ppu1.get())
    ppu1->ContinueFromException();
  if (ppu2.get())
    ppu2->ContinueFromException();
}

void Xenon::Step(int amount) {
  if (ppu0.get())
    ppu0->Step(amount);
  if (ppu1.get())
    ppu1->Step(amount);
  if (ppu2.get())
    ppu2->Step(amount);
}

bool Xenon::IsHalted() {
  if (ppu0.get() && ppu0->IsHalted()) {
    return true;
  }
  if (ppu1.get() && ppu1->IsHalted()) {
    return true;
  }
  if (ppu2.get() && ppu2->IsHalted()) {
    return true;
  }
  return false;
}

bool Xenon::IsHaltedByGuest() {
  if (ppu0.get() && ppu0->IsHaltedByGuest()) {
    return true;
  }
  if (ppu1.get() && ppu1->IsHaltedByGuest()) {
    return true;
  }
  if (ppu2.get() && ppu2->IsHaltedByGuest()) {
    return true;
  }
  return false;
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
