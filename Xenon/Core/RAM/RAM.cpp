/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "RAM.h"

#include "Base/Hash.h"
#include "Base/Logging/Log.h"

// Contstructor
RAM::RAM(const std::string& deviceName, u64 startAddress, std::string size, bool isSOCDevice)
    : SystemDevice(deviceName, startAddress, startAddress + 0x20000000, isSOCDevice) {

  // Parse RAM size.
  char firstChar = '\0';
  for (auto& c : size) {
    if (!std::isdigit(c)) {
      firstChar = c;
      break;
    }
  }
  if (firstChar == '\0') {
    LOG_INFO(Xenon, "RAM Size: {}", size);
    ramSize = std::strtoull(size.c_str(), nullptr, 10);
  } else {
    u64 charPos = size.find_first_of(firstChar);
    if (charPos != -1) {
      std::string number = size.substr(0, charPos);
      std::string unit = size.substr(charPos);
      f64 ramSizeF = std::stod(number.c_str());
      f64 unitF = 0;
      // 536870912 = 0x20_000_000
      // 1073741824 = 0x40_000_000
      switch (Base::JoaatStringHash(unit)) {
        case "B"_jLower: unitF = 1; break;
        case "KB"_jLower:
          if (ramSizeF < 536870.912) {
            ramSizeF = 536870.912;
            LOG_ERROR(System, "Invalid RAM size '{}'! Defaulting to 536870.912KB", ramSizeF);
          }
          unitF = static_cast<f64>(1_KB);
          break;
        case "KiB"_jLower:
          if (ramSizeF < 524288) {
            ramSizeF = 524288;
            LOG_ERROR(System, "Invalid RAM size '{}'! Defaulting to 524288KiB", ramSizeF);
          }
          unitF = static_cast<f64>(1_KiB);
          break;
        case "MB"_jLower:
          if (ramSizeF < 536.870912) {
            ramSizeF = 536.870912;
            LOG_ERROR(System, "Invalid RAM size '{}'! Defaulting to 536.870912MB", ramSizeF);
          }
          unitF = static_cast<f64>(1_MB);
          break;
        case "MiB"_jLower:
          if (ramSizeF < 512) {
            ramSizeF = 512;
            LOG_ERROR(System, "Invalid RAM size '{}'! Defaulting to 512MiB", ramSizeF);
          }
          unitF = static_cast<f64>(1_MiB);
          break;
        case "GB"_jLower:
          if (ramSizeF < 1.073741824) {
            ramSizeF = 1.073741824;
            LOG_ERROR(System, "Invalid RAM size '{}'! Defaulting to 1.073741824GB", ramSizeF);
          }
          unitF = static_cast<f64>(1_GB);
          break;
        case "GiB"_jLower:
          if (ramSizeF < 1) {
            ramSizeF = 1;
            LOG_ERROR(System, "Invalid RAM size '{}'! Defaulting to 1GiB", ramSizeF);
          }
          unitF = static_cast<f64>(1_GiB);
          break;
        default:
          ramSizeF = 512;
          unitF = static_cast<f64>(1_MiB);
          LOG_ERROR(System, "Invalid RAM Unit '{}'! Defaulting to 512MiB", unit);
          break;
      }
      ramSize = static_cast<u64>(std::round(ramSizeF * unitF));
      LOG_INFO(Xenon, "RAM Size: {}", size);
    }
  }

  // Update the end address of the RAM device based on the new size.
  UpdateEndAddress(GetStartAddress() + ramSize);

  // Allocate and intialize RAM.
  ramData = std::make_unique<STRIP_UNIQUE_ARR(ramData)>(ramSize);
  if (!ramData.get()) {
    LOG_CRITICAL(System, "RAM failed to allocate! This is really bad!");
    Base::SystemPause();
  } else {
    ramBasePtr = ramData.get();
    memset(ramBasePtr, 0xCD, ramSize);
  }
}

// Destructor
RAM::~RAM() { ramData.reset(); }

// Resets the RAM to default state.
// NOTE: Real hardware preserves RAM contents across reboots.
void RAM::Reset() {
  if (!ramData.get()) { ramData = std::make_unique<STRIP_UNIQUE_ARR(ramData)>(ramSize); }
  ramBasePtr = ramData.get();
  memset(ramBasePtr, 0xCD, ramSize);
}

// Resizes the RAM to a new size.
void RAM::Resize(u64 size) {
  ramSize = size;
  if (!ramData.get()) { ramData = std::make_unique<STRIP_UNIQUE_ARR(ramData)>(ramSize); }
  ramBasePtr = ramData.get();
}

// MMIO Read.
void RAM::Read(u64 readAddress, u8* data, u64 size) {
  const u64 offset = static_cast<u32>(readAddress - RAM_START_ADDR);
  memcpy(data, ramBasePtr + offset, size);
  if (false) LOG_TRACE(Xenon, "Reading {:#08x} bytes from {:#08x}", size, readAddress);
}

// MMIO Write.
void RAM::Write(u64 writeAddress, const u8* data, u64 size) {
  const u32 offset = static_cast<u32>(writeAddress - RAM_START_ADDR);
  memcpy(ramBasePtr + offset, data, size);
  if (false) LOG_TRACE(Xenon, "Writing {:#08x} bytes to {:#08x}", size, writeAddress);

  // Notify observers about the physical write.
  if (writeNotifyCallback) { writeNotifyCallback(offset, static_cast<u32>(size)); }
}

// MMIO MemSet.
void RAM::MemSet(u64 writeAddress, s32 data, u64 size) {
  const u32 offset = static_cast<u32>(writeAddress - RAM_START_ADDR);
  memset(ramBasePtr + offset, data, size);
  if (false) LOG_TRACE(Xenon, "Setting {:#08x} to {:#02x} for {:#08x} bytes", writeAddress, data, size);

  // Notify observers about the physical write.
  if (writeNotifyCallback) { writeNotifyCallback(offset, static_cast<u32>(size)); }
}

// Returns a pointer for a specific offset from RAM Base or nullptr for OOB.
u8* RAM::GetPointerToAddress(u32 address) {
  const u64 offset = static_cast<u32>(address - RAM_START_ADDR);
  if (offset > ramSize) {
    LOG_CRITICAL(System, "[RAM]: GetPointerToAddress out of bounds access at address {:#x}!", address);
    return nullptr;
  }
  return ramBasePtr + offset;
}
