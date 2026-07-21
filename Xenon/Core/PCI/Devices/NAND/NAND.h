/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/SystemDevice.h"

#include "Core/PCI/Devices/SFCX/SFCX.h"

class NAND : public SystemDevice {
public:
  NAND(std::weak_ptr<Xe::PCIDev::SFCX> sfcx);
  ~NAND();

  void Read(u64 address, u8 *data, u64 size) override;
  void Write(u64 address, const u8 *data, u64 size) override;
  void MemSet(u64 address, s32 data, u64 size) override;
private:
  std::weak_ptr<Xe::PCIDev::SFCX> sfcxDevice = {};
};