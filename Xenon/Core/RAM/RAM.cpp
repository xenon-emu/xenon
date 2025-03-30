// Copyright 2025 Xenon Emulator Project

#include "RAM.h"
#include "Base/Logging/Log.h"

/***Sets the destination, value (205) and size (RAMData)***/
RAM::RAM(const char *deviceName, u64 startAddress, u64 endAddress, bool isSOCDevice) :
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
void RAM::Read(u64 readAddress, u8 *data, u8 byteCount) {
  const u64 offset = static_cast<u32>(readAddress - RAM_START_ADDR);
  if (false)
    LOG_INFO(Xenon, "Reading from address {:#08x} in RAM: byteCount={:#02x}", readAddress, static_cast<u32>(byteCount));
  memcpy(data, RAMData.get() + offset, byteCount);
}

/******************Responsible for RAM writing*****************/
void RAM::Write(u64 writeAddress, u8 *data, u8 byteCount) {
  const u32 offset = static_cast<u32>(writeAddress - RAM_START_ADDR);
  if (false)
    LOG_INFO(Xenon, "Writing to address {:#08x} in RAM: data={:#08x},byteCount={:#02x}", writeAddress, reinterpret_cast<u64>(data), static_cast<u32>(byteCount));
  memcpy(RAMData.get() + offset, data, byteCount);
}

u8 *RAM::getPointerToAddress(u32 address) {
  const u64 offset = static_cast<u32>(address - RAM_START_ADDR);
  return RAMData.get() + offset;
}
