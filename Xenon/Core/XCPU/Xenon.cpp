// Copyright 2025 Xenon Emulator Project

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
      xenonContext.fuseSet.fuseLine00 = { 0x9999999999999999 };
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
      u64 *fuses = reinterpret_cast<u64*>(&xenonContext.fuseSet);
      LOG_INFO(System, "Current FuseSet:");
      for (int i = 0; i < 12; i++) {
        fuseset = fusesets[i];
        fuses[i] = strtoull(fuseset.c_str(), nullptr, 16);
        LOG_INFO(System, " * FuseSet {:02}: 0x{}", i, fuseset.c_str());
      }
    }
  }

  // Load 1BL from path.
  std::ifstream file(blPath, std::ios_base::in | std::ios_base::binary);
  if (!file.is_open()) {
    LOG_CRITICAL(Xenon, "Unable to open file: {} for reading. Check your file path. System Stopped!", blPath);
    SYSTEM_PAUSE();
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
      LOG_ERROR(Base_Filesystem, "Exception trying to get file size. Exception: {}",
        ex.what());
      return;
    }
    if (fileSize == XE_SROM_SIZE) {
      file.read(reinterpret_cast<char*>(xenonContext.SROM), XE_SROM_SIZE);
      LOG_INFO(Xenon, "1BL Loaded.");
    }
  }
  file.close();

  // Asign Interpreter global variables
  PPCInterpreter::intXCPUContext = &xenonContext;
  PPCInterpreter::sysBus = mainBus;
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
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, resetVector, XE_PVR, 0); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, resetVector, XE_PVR, 2); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, resetVector, XE_PVR, 4); // Threads 4-5
  // Start execution on the main thread
  ppu0->StartExecution();
  // Get our CPI based on the first PPU, then share it across all PPUs
  ppu1->SetCPI(ppu0->GetCPI());
  ppu2->SetCPI(ppu0->GetCPI());
  // Start execution on the other threads
  ppu1->StartExecution();
  ppu2->StartExecution();
}

u32 Xenon::RunCPITests(u64 resetVector) {
  // Create PPU element
  ppu0.reset();
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, resetVector, XE_PVR, 0); // Threads 0-1
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
  ppu0 = std::make_unique<STRIP_UNIQUE(ppu0)>(&xenonContext, mainBus, 0, XE_PVR, 0); // Threads 0-1
  ppu1 = std::make_unique<STRIP_UNIQUE(ppu1)>(&xenonContext, mainBus, 0, XE_PVR, 2); // Threads 2-3
  ppu2 = std::make_unique<STRIP_UNIQUE(ppu2)>(&xenonContext, mainBus, 0, XE_PVR, 4); // Threads 4-5
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

void Xenon::Halt(u64 haltOn) {
  if (ppu0.get())
    ppu0->Halt(haltOn);
  if (ppu1.get())
    ppu1->Halt(haltOn);
  if (ppu2.get())
    ppu2->Halt(haltOn);
}

void Xenon::Continue() {
  if (ppu0.get())
    ppu0->Continue();
  if (ppu1.get())
    ppu1->Continue();
  if (ppu2.get())
    ppu2->Continue();
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

bool Xenon::HandleSOCRead(u64 readAddr, u8* data, size_t byteCount) {
  // Get target Block.

  if (readAddr >= XE_SOCSECENG_BLOCK_START && readAddr 
    < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
    // Security Engine.
    return HandleSecEngRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCSECOTP_BLOCK_START && readAddr 
    < XE_SOCSECOTP_BLOCK_START + XE_SOCSECOTP_BLOCK_SIZE) {
    // Secure OTP.
    return HandleSecOTPRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCSECRNG_BLOCK_START && readAddr 
    < XE_SOCSECRNG_BLOCK_START + XE_SOCSECRNG_BLOCK_SIZE) {
    // Secure RNG.
    return HandleSecRNGRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCCBI_BLOCK_START && readAddr
    < XE_SOCCBI_BLOCK_START + XE_SOCCBI_BLOCK_SIZE) {
    // CBI.
    return HandleCBIRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCPMW_BLOCK_START && readAddr
    < XE_SOCPMW_BLOCK_START + XE_SOCPMW_BLOCK_SIZE) {
    // PMW.
    return HandlePMWRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCPRV_BLOCK_START && readAddr
    < XE_SOCPRV_BLOCK_START + XE_SOCPRV_BLOCK_SIZE) {
    // Pervasive Logic.
    return HandlePRVRead(readAddr, data, byteCount);
  } else {
    // Not an SOC address or unimplemented block.
    LOG_ERROR(Xenon, "SOC Read to unimplemented block at address {:#x}", readAddr);
  }
  return false;
}

bool Xenon::HandleSOCWrite(u64 writeAddr, u8* data, size_t byteCount) {
  // Get target Block.

  if (writeAddr >= XE_SOCSECENG_BLOCK_START && writeAddr
    < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
    // Security Engine.
    return HandleSecEngWrite(writeAddr, data, byteCount);
  }
  else if (writeAddr >= XE_SOCSECOTP_BLOCK_START && writeAddr
    < XE_SOCSECOTP_BLOCK_START + XE_SOCSECOTP_BLOCK_SIZE) {
    // Secure OTP.
    return HandleSecOTPWrite(writeAddr, data, byteCount);
  }
  else if (writeAddr >= XE_SOCSECRNG_BLOCK_START && writeAddr
    < XE_SOCSECRNG_BLOCK_START + XE_SOCSECRNG_BLOCK_SIZE) {
    // Secure RNG.
    return HandleSecRNGWrite(writeAddr, data, byteCount);
  }
  else if (writeAddr >= XE_SOCCBI_BLOCK_START && writeAddr
    < XE_SOCCBI_BLOCK_START + XE_SOCCBI_BLOCK_SIZE) {
    // CBI.
    return HandleCBIWrite(writeAddr, data, byteCount);
  }
  else if (writeAddr >= XE_SOCPMW_BLOCK_START && writeAddr
    < XE_SOCPMW_BLOCK_START + XE_SOCPMW_BLOCK_SIZE) {
    // PMW.
    return HandlePMWWrite(writeAddr, data, byteCount);
  }
  else if (writeAddr >= XE_SOCPRV_BLOCK_START && writeAddr
    < XE_SOCPRV_BLOCK_START + XE_SOCPRV_BLOCK_SIZE) {
    // Pervasive Logic.
    return HandlePRVWrite(writeAddr, data, byteCount);
  }
  else {
    // Not an SOC address or unimplemented block.
    LOG_ERROR(Xenon, "SOC Write to unimplemented block at address {:#x}", writeAddr);
  }
  return false;
}

// Security Engine Read.
bool Xenon::HandleSecEngRead(u64 readAddr, u8* data, size_t byteCount) {
  return false;
}
// Security Engine Write.
bool Xenon::HandleSecEngWrite(u64 writeAddr, u8* data, size_t byteCount) {
  return false;
}

// Secure OTP Read.
bool Xenon::HandleSecOTPRead(u64 readAddr, u8* data, size_t byteCount) {
  return false;
}
// Secure OTP Write.
bool Xenon::HandleSecOTPWrite(u64 writeAddr, u8* data, size_t byteCount) {
  return false;
}

// Secure RNG Read.
bool Xenon::HandleSecRNGRead(u64 readAddr, u8* data, size_t byteCount)
{
  return false;
}
// Secure RNG Write.
bool Xenon::HandleSecRNGWrite(u64 writeAddr, u8* data, size_t byteCount)
{
  return false;
}

// CBI Read.
bool Xenon::HandleCBIRead(u64 readAddr, u8* data, size_t byteCount)
{
  return false;
}
// CBI Write.
bool Xenon::HandleCBIWrite(u64 writeAddr, u8* data, size_t byteCount)
{
  return false;
}

// PMW Read.
bool Xenon::HandlePMWRead(u64 readAddr, u8* data, size_t byteCount)
{
  return false;
}
// PMW Write.
bool Xenon::HandlePMWWrite(u64 writeAddr, u8* data, size_t byteCount)
{
  return false;
}

// Pervasive logic Read.
bool Xenon::HandlePRVRead(u64 readAddr, u8* data, size_t byteCount)
{
  return false;
}
// Pervasive logic Write.
bool Xenon::HandlePRVWrite(u64 writeAddr, u8* data, size_t byteCount)
{
  return false;
}
