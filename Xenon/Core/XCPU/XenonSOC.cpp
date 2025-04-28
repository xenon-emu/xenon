// Copyright 2025 Xenon Emulator Project

#include <random>

#include "Base/Logging/Log.h"
#include "Core/XCPU/PPU/PowerPC.h"
#include "Core/XCPU/PostBus/PostBus.h"

bool XENON_CONTEXT::HandleSOCRead(u64 readAddr, u8* data, size_t byteCount) {
  // Get target Block.
  if (readAddr >= XE_SOCSECENG_BLOCK_START && readAddr < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
    // Security Engine.
    return HandleSecEngRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCSECOTP_BLOCK_START && readAddr < XE_SOCSECOTP_BLOCK_START + XE_SOCSECOTP_BLOCK_SIZE) {
    // Secure OTP.
    return HandleSecOTPRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCSECRNG_BLOCK_START && readAddr < XE_SOCSECRNG_BLOCK_START + XE_SOCSECRNG_BLOCK_SIZE) {
    // Secure RNG.
    return HandleSecRNGRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCCBI_BLOCK_START && readAddr < XE_SOCCBI_BLOCK_START + XE_SOCCBI_BLOCK_SIZE) {
    // CBI.
    return HandleCBIRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCPMW_BLOCK_START && readAddr < XE_SOCPMW_BLOCK_START + XE_SOCPMW_BLOCK_SIZE) {
    // PMW.
    return HandlePMWRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCPRV_BLOCK_START && readAddr < XE_SOCPRV_BLOCK_START + XE_SOCPRV_BLOCK_SIZE) {
    // Pervasive Logic.
    return HandlePRVRead(readAddr, data, byteCount);
  }

  return false;
}

bool XENON_CONTEXT::HandleSOCWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  // Get target Block.

  if (writeAddr >= XE_SOCSECENG_BLOCK_START && writeAddr < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
    // Security Engine.
    return HandleSecEngWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCSECOTP_BLOCK_START && writeAddr < XE_SOCSECOTP_BLOCK_START + XE_SOCSECOTP_BLOCK_SIZE) {
    // Secure OTP.
    return HandleSecOTPWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCSECRNG_BLOCK_START && writeAddr < XE_SOCSECRNG_BLOCK_START + XE_SOCSECRNG_BLOCK_SIZE) {
    // Secure RNG.
    return HandleSecRNGWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCCBI_BLOCK_START && writeAddr < XE_SOCCBI_BLOCK_START + XE_SOCCBI_BLOCK_SIZE) {
    // CBI.
    return HandleCBIWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCPMW_BLOCK_START && writeAddr < XE_SOCPMW_BLOCK_START + XE_SOCPMW_BLOCK_SIZE) {
    // PMW.
    return HandlePMWWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCPRV_BLOCK_START && writeAddr < XE_SOCPRV_BLOCK_START + XE_SOCPRV_BLOCK_SIZE) {
    // Pervasive Logic.
    return HandlePRVWrite(writeAddr, data, byteCount);
  }

  return false;
}

// Security Engine Read.
bool XENON_CONTEXT::HandleSecEngRead(u64 readAddr, u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCSECENG_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8*>(socSecEngBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  LOG_TRACE(Xenon, "SoC SecEng Read at address {:#x}, data {:#x}.", readAddr, dataOut);

  mutex.unlock();
  return true;
}
// Security Engine Write.
bool XENON_CONTEXT::HandleSecEngWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCSECENG_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socSecEngBlock.get()) + offset, &dataIn, byteCount);

  LOG_TRACE(Xenon, "SoC SecEng Write at address {:#x}, data {:#x}.", writeAddr, dataIn);

  mutex.unlock();
  return true;
}

// Secure OTP Read.
bool XENON_CONTEXT::HandleSecOTPRead(u64 readAddr, u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCSECOTP_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8*>(socSecOTPBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  LOG_TRACE(Xenon, "SoC SecOTP Read at address {:#x}, data {:#x}.", readAddr, dataOut);

  mutex.unlock();
  return true;
}
// Secure OTP Write.
bool XENON_CONTEXT::HandleSecOTPWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  mutex.lock();

  LOG_ERROR(Xenon, "SoC SecOTP Write at address {:#x}.", writeAddr);

  mutex.unlock();
  return false;
}

// Secure RNG Read.
bool XENON_CONTEXT::HandleSecRNGRead(u64 readAddr, u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCSECRNG_BLOCK_START;
  if (readAddr == 0x26008) {
    std::random_device randomDevice;  // Seed for the random number engine
    std::default_random_engine generator(randomDevice());
    std::uniform_int_distribution<u64> distribution(0, UINT64_MAX);

    socSecRNGBlock.get()->Fifo = distribution(generator);
  }
  memcpy(&dataOut, reinterpret_cast<u8*>(socSecRNGBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  // SEC_RNG Status:
  // Software will check FifoEmpty, if 1 (true) will loop until it becomes zero.
  // Afterwards reads the FiFo queue and reads a random number.

  LOG_TRACE(Xenon, "SoC SecRNG Read at address {:#x}, data {:#x}.", readAddr, dataOut);

  mutex.unlock();
  return true;
}
// Secure RNG Write.
bool XENON_CONTEXT::HandleSecRNGWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCSECRNG_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socSecRNGBlock.get()) + offset, &dataIn, byteCount);

  LOG_TRACE(Xenon, "SoC SecRNG Write at address {:#x}, data {:#x}.", writeAddr, dataIn);

  mutex.unlock();
  return true;
}

// CBI Read.
bool XENON_CONTEXT::HandleCBIRead(u64 readAddr, u8* data, size_t byteCount) {
  mutex.lock();

  LOG_ERROR(Xenon, "SoC CBI Read at address {:#x}.", readAddr);

  mutex.unlock();
  return false;
}
// CBI Write.
bool XENON_CONTEXT::HandleCBIWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  mutex.lock();

  LOG_ERROR(Xenon, "SoC CBI Write at address {:#x}.", writeAddr);

  mutex.unlock();
  return false;
}

// PMW Read.
bool XENON_CONTEXT::HandlePMWRead(u64 readAddr, u8* data, size_t byteCount) {
  mutex.lock();

  LOG_ERROR(Xenon, "SoC PWM Read at address {:#x}.", readAddr);

  mutex.unlock();
  return false;
}
// PMW Write.
bool XENON_CONTEXT::HandlePMWWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCPMW_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socPMWBlock.get()) + offset, &dataIn, byteCount);

  LOG_TRACE(Xenon, "SoC PMW Write at address {:#x}, data {:#x}.", writeAddr, dataIn);

  mutex.unlock();
  return true;
}

// Pervasive logic Read.
bool XENON_CONTEXT::HandlePRVRead(u64 readAddr, u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCPRV_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8*>(socPRVBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  LOG_TRACE(Xenon, "SoC PRV Read at address {:#x}, data {:#x}.", readAddr, dataOut);

  mutex.unlock();
  return true;
}
// Pervasive logic Write.
bool XENON_CONTEXT::HandlePRVWrite(u64 writeAddr, const u8* data, size_t byteCount) {
  mutex.lock();

  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCPRV_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socPRVBlock.get()) + offset, &dataIn, byteCount);

  // Apply operations on know registers.
  switch (writeAddr) {
  case 0x61010ULL:
    // POST Output.
    Xe::XCPU::POSTBUS::POST(byteswap_be<u64>(dataIn));
    break;
  case 0x611A0ULL:
    if (socPRVBlock.get()->TimebaseControl.AsBITS.TimebaseEnable) {
      // TimeBase Enabled.
      timeBaseActive = true;
    }
    else {
      // TimeBase Disabled.
      timeBaseActive = false;
    }
    // TimeBase Control.
    LOG_TRACE(Xenon, "SoC PRV: TimeBase Control being set {:#x}, enabled: {}, divider: {:#x}.",
      dataIn, timeBaseActive, socPRVBlock.get()->TimebaseControl.AsBITS.TimebaseDivider);
    break;
  case 0x61188ULL:
    // CPU VID Register.
    LOG_WARNING(Xenon, "SoC PRV: New VID value being set: {:#x}", dataIn);
    break;
  default:
    break;
  }

  LOG_TRACE(Xenon, "SoC PRV Write at address {:#x}, data {:#x}.", writeAddr, dataIn);

  mutex.unlock();
  return true;
}