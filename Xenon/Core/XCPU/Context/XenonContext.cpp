/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include <random>

#include "Base/Logging/Log.h"
#include "Core/XCPU/Context/XenonContext.h"
#include "Core/XCPU/Context/PostBus/PostBus.h"

namespace Xe::XCPU {

bool XenonContext::HandleSOCRead(u64 readAddr, u8 *data, size_t byteCount) {
  // Get target block
  if (readAddr >= XE_SOCSECENG_BLOCK_START && readAddr < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
    // Security Engine
    return HandleSecEngRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCSECOTP_BLOCK_START && readAddr < XE_SOCSECOTP_BLOCK_START + XE_SOCSECOTP_BLOCK_SIZE) {
    // Secure OTP
    return HandleSecOTPRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCSECRNG_BLOCK_START && readAddr < XE_SOCSECRNG_BLOCK_START + XE_SOCSECRNG_BLOCK_SIZE) {
    // Secure RNG
    return HandleSecRNGRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCCBI_BLOCK_START && readAddr < XE_SOCCBI_BLOCK_START + XE_SOCCBI_BLOCK_SIZE) {
    // CBI
    return HandleCBIRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCINTS_BLOCK_START && readAddr < XE_SOCINTS_BLOCK_START + XE_SOCINTS_BLOCK_SIZE) {
    // INT
    return HandleINTRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCPMW_BLOCK_START && readAddr < XE_SOCPMW_BLOCK_START + XE_SOCPMW_BLOCK_SIZE) {
    // PMW
    return HandlePMWRead(readAddr, data, byteCount);
  } else if (readAddr >= XE_SOCPRV_BLOCK_START && readAddr < XE_SOCPRV_BLOCK_START + XE_SOCPRV_BLOCK_SIZE) {
    // Pervasive Logic
    return HandlePRVRead(readAddr, data, byteCount);
  }
  return false;
}

bool XenonContext::HandleSOCWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  // Get target block
  if (writeAddr >= XE_SOCSECENG_BLOCK_START && writeAddr < XE_SOCSECENG_BLOCK_START + XE_SOCSECENG_BLOCK_SIZE) {
    // Security Engine
    return HandleSecEngWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCSECOTP_BLOCK_START && writeAddr < XE_SOCSECOTP_BLOCK_START + XE_SOCSECOTP_BLOCK_SIZE) {
    // Secure OTP
    return HandleSecOTPWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCSECRNG_BLOCK_START && writeAddr < XE_SOCSECRNG_BLOCK_START + XE_SOCSECRNG_BLOCK_SIZE) {
    // Secure RNG
    return HandleSecRNGWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCCBI_BLOCK_START && writeAddr < XE_SOCCBI_BLOCK_START + XE_SOCCBI_BLOCK_SIZE) {
    // CBI
    return HandleCBIWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCINTS_BLOCK_START && writeAddr < XE_SOCINTS_BLOCK_START + XE_SOCINTS_BLOCK_SIZE) {
    // INT
    return HandleINTWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCPMW_BLOCK_START && writeAddr < XE_SOCPMW_BLOCK_START + XE_SOCPMW_BLOCK_SIZE) {
    // PMW
    return HandlePMWWrite(writeAddr, data, byteCount);
  } else if (writeAddr >= XE_SOCPRV_BLOCK_START && writeAddr < XE_SOCPRV_BLOCK_START + XE_SOCPRV_BLOCK_SIZE) {
    // Pervasive Logic
    return HandlePRVWrite(writeAddr, data, byteCount);
  }
  return false;
}

// Security Engine Read
bool XenonContext::HandleSecEngRead(u64 readAddr, u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);

  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCSECENG_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8*>(socSecEngBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  LOG_TRACE(Xenon, "SoC SecEng Read at address 0x{:X}, data 0x{:X}.", readAddr, dataOut);
  return true;
}

// Security Engine Write
bool XenonContext::HandleSecEngWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);

  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCSECENG_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socSecEngBlock.get()) + offset, &dataIn, byteCount);

  LOG_TRACE(Xenon, "SoC SecEng Write at address 0x{:X}, data 0x{:X}.", writeAddr, dataIn);
  return true;
}

// Secure OTP Read
bool XenonContext::HandleSecOTPRead(u64 readAddr, u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCSECOTP_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8*>(socSecOTPBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  LOG_TRACE(Xenon, "SoC SecOTP Read at address 0x{:X}, data 0x{:X}.", readAddr, dataOut);
  return true;
}

// Secure OTP Write
bool XenonContext::HandleSecOTPWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  LOG_ERROR(Xenon, "SoC SecOTP Write at address 0x{:X}.", writeAddr);
  return false;
}

// Secure RNG Read
bool XenonContext::HandleSecRNGRead(u64 readAddr, u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
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

  LOG_TRACE(Xenon, "SoC SecRNG Read at address 0x{:X}, data 0x{:X}.", readAddr, dataOut);
  return true;
}

// Secure RNG Write
bool XenonContext::HandleSecRNGWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCSECRNG_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socSecRNGBlock.get()) + offset, &dataIn, byteCount);

  LOG_TRACE(Xenon, "SoC SecRNG Write at address 0x{:X}, data 0x{:X}.", writeAddr, dataIn);
  return true;
}

// CBI Read
bool XenonContext::HandleCBIRead(u64 readAddr, u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  LOG_ERROR(Xenon, "SoC CBI Read at address 0x{:X}.", readAddr);
  return false;
}

// CBI Write
bool XenonContext::HandleCBIWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  LOG_ERROR(Xenon, "SoC CBI Write at address 0x{:X}.", writeAddr);
  return false;
}

// INT Read
bool XenonContext::HandleINTRead(u64 readAddr, u8 *data, size_t byteCount) {
  iic.Read(readAddr, data, byteCount);
  return true;
}

// INT Write
bool XenonContext::HandleINTWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  iic.Write(writeAddr, data, byteCount);
  return true;
}

// PMW Read
bool XenonContext::HandlePMWRead(u64 readAddr, u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCPMW_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8 *>(socPMWBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);
  LOG_WARNING(Xenon, "SoC PWM Read at address {:#x}, data {:#x}", readAddr, dataOut);
  return true;
}

// PMW Write
bool XenonContext::HandlePMWWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCPMW_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socPMWBlock.get()) + offset, &dataIn, byteCount);

  LOG_WARNING(Xenon, "SoC PMW Write at address 0x{:X}, data 0x{:X}.", writeAddr, dataIn);
  return true;
}

// Pervasive logic Read
bool XenonContext::HandlePRVRead(u64 readAddr, u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  u64 dataOut = 0;
  u16 offset = readAddr - XE_SOCPRV_BLOCK_START;
  memcpy(&dataOut, reinterpret_cast<u8*>(socPRVBlock.get()) + offset, byteCount);
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, byteCount);

  LOG_TRACE(Xenon, "SoC PRV Read at address 0x{:X}, data 0x{:X}.", readAddr, dataOut);
  return true;
}
// Pervasive logic Write
bool XenonContext::HandlePRVWrite(u64 writeAddr, const u8 *data, size_t byteCount) {
  std::lock_guard lock(mutex);
  u64 dataIn = 0;
  memcpy(&dataIn, data, byteCount);
  dataIn = byteswap_be<u64>(dataIn);
  u16 offset = writeAddr - XE_SOCPRV_BLOCK_START;

  memcpy(reinterpret_cast<u8*>(socPRVBlock.get()) + offset, &dataIn, byteCount);

  // Apply operations on know registers
  switch (writeAddr) {
  case 0x61010ULL:
    // POST Output
    Xe::XCPU::POSTBUS::POST(byteswap_be<u64>(dataIn));
    break;
  case 0x611A0ULL:
    if (socPRVBlock.get()->TimebaseControl.AsBITS.TimebaseEnable) {
      // TimeBase Enabled
      timeBaseActive = true;
    } else {
      // TimeBase Disabled
      timeBaseActive = false;
    }
    // TimeBase Control
    LOG_WARNING(Xenon, "SoC PRV: TimeBase Control being set 0x{:X}, enabled: {}, divider: 0x{:X}.",
      dataIn, timeBaseActive, socPRVBlock.get()->TimebaseControl.AsBITS.TimebaseDivider);
    break;
  case 0x61188ULL:
    // CPU VID Register
    LOG_WARNING(Xenon, "SoC PRV: New VID value being set: 0x{:X}", dataIn);
    break;
  default:
    break;
  }

  LOG_TRACE(Xenon, "SoC PRV Write at address 0x{:X}, data 0x{:X}.", writeAddr, dataIn);
  return true;
}
}