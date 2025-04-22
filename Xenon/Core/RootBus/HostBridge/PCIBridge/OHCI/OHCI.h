// Copyright 2025 Xenon Emulator Project

#pragma once

#include "Core/RootBus/HostBridge/PCIBridge/PCIDevice.h"

namespace Xe {
namespace PCIDev {

class OHCI : public PCIDevice {
public:
  OHCI(const std::string &deviceName, u64 size, int instance, unsigned int ports);
  void Read(u64 readAddress, u8 *data, u64 size) override;
  void Write(u64 writeAddress, const u8 *data, u64 size) override;
  void MemSet(u64 writeAddress, s32 data, u64 size) override;
  void ConfigRead(u64 readAddress, u8* data, u64 size) override;
  void ConfigWrite(u64 writeAddress, const u8* data, u64 size) override;
private:
  int instance;
  unsigned int ports;

  u32 HcRevision;         //  0
  u32 HcControl;          //  4
  u32 HcCommandStatus;    //  8
  u32 HcInterruptStatus;  //  c
  u32 HcInterruptEnable;  // 10
  u32 HcHCCA;             // 18
  u32 HcPeriodCurrentED;  // 1c
  u32 HcControlHeadED;    // 20
  u32 HcBulkHeadED;       // 28
  u32 HcFmInterval;       // 34
  u32 HcPeriodicStart;    // 40
  u32 HcRhDescriptorA;    // 48
  u32 HcRhDescriptorB;    // 4c
  u32 HcRhStatus;         // 50
};

};
};
