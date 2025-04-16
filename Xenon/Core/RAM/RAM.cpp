// Copyright 2025 Xenon Emulator Project

#include "RAM.h"
#include "Base/Logging/Log.h"

/***Sets the destination, value (205) and size (RAMData)***/
RAM::RAM(const std::string &deviceName, u64 startAddress, u64 endAddress, bool isSOCDevice) :
  SystemDevice(deviceName, startAddress, endAddress, isSOCDevice) {
  RAMData = std::make_unique<STRIP_UNIQUE_ARR(RAMData)>(RAM_SIZE);
  if (!RAMData.get()) {
    LOG_CRITICAL(System, "RAM failed to allocate! This is really bad!");
    SYSTEM_PAUSE();
  }
  else {
    memset(RAMData.get(), 0xCD, RAM_SIZE);
  }
}
RAM::~RAM() {
  RAMData.reset();
}

/*****************Responsible for RAM reading*****************/
void RAM::Reset() {
  if (!RAMData.get()) {
    RAMData = std::make_unique<STRIP_UNIQUE_ARR(RAMData)>(RAM_SIZE);
    memset(RAMData.get(), 0xCD, RAM_SIZE);
  }
  else {
    memset(RAMData.get(), 0xCD, RAM_SIZE);
  }
}

/*****************Responsible for RAM reading*****************/
void RAM::Read(u64 readAddress, u8 *data, u64 size) {
  const u64 offset = static_cast<u32>(readAddress - RAM_START_ADDR);
  if (false)
    LOG_INFO(Xenon, "Reading {:#08x} bytes from {:#08x}", size, readAddress);
  memcpy(data, RAMData.get() + offset, size);
}

/******************Responsible for RAM writing*****************/
void RAM::Write(u64 writeAddress, const u8 *data, u64 size) {
  const u32 offset = static_cast<u32>(writeAddress - RAM_START_ADDR);
  if (false)
    LOG_INFO(Xenon, "Writing {:#08x} bytes to {:#08x}", size, writeAddress);
  memcpy(RAMData.get() + offset, data, size);
}

/******************Responsible for RAM writing*****************/
void RAM::MemSet(u64 writeAddress, s32 data, u64 size) {
  const u32 offset = static_cast<u32>(writeAddress - RAM_START_ADDR);
  if (false)
    LOG_INFO(Xenon, "Setting {:#08x} to {:#02x} for {:#08x} bytes", writeAddress, data, size);
  memset(RAMData.get() + offset, data, size);
}

u8 *RAM::getPointerToAddress(u32 address) {
  const u64 offset = static_cast<u32>(address - RAM_START_ADDR);
  return RAMData.get() + offset;
}
