// Copyright 2025 Xenon Emulator Project

#pragma once

#include <cstring>

#include "Types.h"

struct deviceInfo {
  const char *deviceName = ""; // Device Name
  u64 startAddr = 0; // Start Address
  u64 endAddr = 0; // End Address
  bool socDevice = false; // SOC Device
};

class SystemDevice {
public:
  SystemDevice(const char *deviceName, u64 startAddress, u64 endAddress,
               bool isSOCDevice)
  {
    info.deviceName = deviceName;
    info.startAddr = startAddress;
    info.endAddr = endAddress;
    info.socDevice = isSOCDevice;
  }

  virtual void Read(u64 readAddress, u64 *data, u8 byteCount) {}
  virtual void Write(u64 writeAddress, u64 data, u8 byteCount) {}

  const char *GetDeviceName() { return info.deviceName; }
  u64 GetStartAddress() { return info.startAddr; }
  u64 GetEndAddress() { return info.endAddr; }
  bool IsSOCDevice() { return info.socDevice; }

private:
  deviceInfo info{};
};
