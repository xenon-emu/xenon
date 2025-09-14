/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Base/Logging/Log.h"
#include "Core/XCPU/XenonCPU.h"
#include "Interpreter/PPCInterpreter.h"

namespace Xe::XCPU {

  XenonCPU::XenonCPU(RootBus *inBus, const std::string blPath, const std::string fusesPath, RAM *ramPtr) {
    // Initilize Xenon Context
    xenonContext = std::make_unique<STRIP_UNIQUE(xenonContext)>(inBus, ramPtr);

    // Set SROM to 0.
    memset(xenonContext->SROM.get(), 0, XE_SROM_SIZE);

    // Populate FuseSet
    {
      std::ifstream file(fusesPath);
      if (!file.is_open()) {
        xenonContext->socSecOTPBlock->sec->AsULONGLONG = { 0x9999999999999999 };
      }
      else {
        LOG_INFO(System, "Current FuseSet:");
        std::vector<std::pair<std::string, u64>> fusesets{};
        std::string fuseset;
        while (std::getline(file, fuseset)) {
          if (size_t pos = fuseset.find(": "); pos != std::string::npos) {
            fuseset = fuseset.substr(pos + 2);
          }
          u64 fuse = strtoull(fuseset.c_str(), nullptr, 16);
          LOG_INFO(System, " * FuseSet {:02}: 0x{:X}", fusesets.size(), fuse);
          fusesets.push_back(std::make_pair(fuseset, fuse));
        }

        xenonContext->socSecOTPBlock->sec[0].AsULONGLONG = fusesets[0].second;
        xenonContext->socSecOTPBlock->ConsoleType[0] = fusesets[1].second;
        xenonContext->socSecOTPBlock->ConsoleSequence[0] = fusesets[2].second;
        xenonContext->socSecOTPBlock->UniqueId1[0] = fusesets[3].second;
        xenonContext->socSecOTPBlock->UniqueId2[0] = fusesets[4].second;
        xenonContext->socSecOTPBlock->UniqueId3[0] = fusesets[5].second;
        xenonContext->socSecOTPBlock->UniqueId4[0] = fusesets[6].second;
        xenonContext->socSecOTPBlock->UpdateSequence[0] = fusesets[7].second;
        xenonContext->socSecOTPBlock->EepromKey1[0] = fusesets[8].second;
        xenonContext->socSecOTPBlock->EepromKey2[0] = fusesets[9].second;
        xenonContext->socSecOTPBlock->EepromHash1[0] = fusesets[10].second;
        xenonContext->socSecOTPBlock->EepromHash2[0] = fusesets[11].second;
      }
    }

    // Load 1BL binary if needed.
    if (!Config::xcpu.simulate1BL) {
      // Load 1BL from path.
      std::ifstream file(blPath, std::ios_base::in | std::ios_base::binary);
      if (!file.is_open()) {
        LOG_CRITICAL(Xenon, "Unable to open file: {} for reading. Check your file path. System Stopped!", blPath);
        Base::SystemPause();
      }
      else {
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
        catch (const std::exception &ex) {
          LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Reason: {}", ex.what());
          return;
        }

        if (fileSize == XE_SROM_SIZE) {
          file.read(reinterpret_cast<char *>(xenonContext->SROM.get()), XE_SROM_SIZE);
          LOG_INFO(Xenon, "1BL Loaded.");
        }
      }
      file.close();
    }

    // Asign Interpreter global CPU context
    PPCInterpreter::xenonContext = xenonContext.get();

    // Setup SOC blocks.
    xenonContext->socPRVBlock.get()->PowerOnResetStatus.AsBITS.SecureMode = 1; // CB Checks this.
    xenonContext->socPRVBlock.get()->PowerManagementControl.AsULONGLONG = 0x382C00000000B001ULL; // Power Management Control.
  }

  XenonCPU::~XenonCPU() {
    LOG_INFO(Xenon, "Shutting PPU cores down...");
    ppu0.reset();
    ppu1.reset();
    ppu2.reset();
    xenonContext.reset();
  }

  void XenonCPU::Start(u64 resetVector) {
    // If we already have active objects, halt cpu and kill threads
    if (ppu0.get()) {
      Halt();
      ppu0.reset();
      ppu1.reset();
      ppu2.reset();
    }
    // Create PPU elements
    ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(xenonContext.get(), resetVector, 0); // Threads 0-1
    ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(xenonContext.get(), resetVector, 2); // Threads 2-3
    ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(xenonContext.get(), resetVector, 4); // Threads 4-5
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

  u32 XenonCPU::RunCPITests(u64 resetVector) {
    // Create PPU element
    ppu0.reset();
    ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(xenonContext.get(), resetVector, 0); // Threads 0-1
    // Start execution on the main thread
    ppu0->StartExecution();
    // Get CPU and reset PPU
    u32 cpi = ppu0->GetCPI();
    ppu0.reset();
    return cpi;
  }

  void XenonCPU::LoadElf(const std::string path) {
    ppu0.reset();
    ppu1.reset();
    ppu2.reset();
    ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(xenonContext.get(), 0, 0); // Threads 0-1
    ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(xenonContext.get(), 0, 2); // Threads 2-3
    ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(xenonContext.get(), 0, 4); // Threads 4-5
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
    catch (const std::exception &ex) {
      LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Exception: {}",
        ex.what());
      return;
    }
    std::unique_ptr<u8[]> elfBinary = std::make_unique<u8[]>(fileSize);
    file.read(reinterpret_cast<char *>(elfBinary.get()), fileSize);
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

  void XenonCPU::Reset() {
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

  void XenonCPU::Halt(u64 haltOn, bool requestedByGuest, u8 ppuId, ePPUThreadID threadId) {
    if (ppu0.get())
      ppu0->Halt(haltOn, requestedByGuest, ppuId, threadId);
    if (ppu1.get())
      ppu1->Halt(haltOn, requestedByGuest, ppuId, threadId);
    if (ppu2.get())
      ppu2->Halt(haltOn, requestedByGuest, ppuId, threadId);
  }

  void XenonCPU::Continue() {
    if (ppu0.get())
      ppu0->Continue();
    if (ppu1.get())
      ppu1->Continue();
    if (ppu2.get())
      ppu2->Continue();
  }

  void XenonCPU::ContinueFromException() {
    if (ppu0.get())
      ppu0->ContinueFromException();
    if (ppu1.get())
      ppu1->ContinueFromException();
    if (ppu2.get())
      ppu2->ContinueFromException();
  }

  void XenonCPU::Step(int amount) {
    if (ppu0.get())
      ppu0->Step(amount);
    if (ppu1.get())
      ppu1->Step(amount);
    if (ppu2.get())
      ppu2->Step(amount);
  }

  bool XenonCPU::IsHalted() {
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

  bool XenonCPU::IsHaltedByGuest() {
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

  PPU *XenonCPU::GetPPU(u8 ppuID) {
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

} // Xe::XCPU