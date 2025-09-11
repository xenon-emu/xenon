// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <fstream>
#include <filesystem>
#include <vector>

#include "Base/SystemDevice.h"
#include "Core/PCI/Devices/SFCX/SFCX.h"

class NAND : public SystemDevice {
public:
  NAND(const std::string &deviceName, Xe::PCIDev::SFCX *sfcx);
  ~NAND();

  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;

private:
  Xe::PCIDev::SFCX *sfcxDevice = nullptr;
};
