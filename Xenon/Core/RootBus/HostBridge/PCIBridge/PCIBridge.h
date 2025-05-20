// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <cstring>
#include <unordered_map>

#include "PCIDevice.h"

#include "Core/RootBus/HostBridge/PCIe.h"
#include "Core/XCPU/IIC/IIC.h"

/*  Dev type          Config Address    BAR
    PCI Host Bridge     D0008000      E0000000
    PCI-PCI Bridge      D0000000      EA000000
    Display Controller  D0010000      EC800000

*/

#define PCI_BRIDGE_BASE_ADDRESS 0xEA000000
#define PCI_BRIDGE_BASE_SIZE 0xFFF
#define PCI_BRIDGE_BASE_END_ADDRESS  PCI_BRIDGE_BASE_ADDRESS + PCI_BRIDGE_BASE_SIZE

#define PCI_BRIDGE_CONFIG_SPACE_ADDRESS_BASE 0xD0000000
#define PCI_BRIDGE_CONFIG_SPACE_SIZE 0x1000000 // 16 Mb

#define PCI_BRIDGE_SIZE 0x10000

// IRQs for PCI -> CPU

#define PRIO_IPI_4 0x08
#define PRIO_IPI_3 0x10
#define PRIO_SMM 0x14
#define PRIO_SFCX 0x18
#define PRIO_SATA_HDD 0x20
#define PRIO_SATA_ODD 0x24
#define PRIO_OHCI_0 0x2C
#define PRIO_EHCI_0 0x30
#define PRIO_OHCI_1 0x34
#define PRIO_EHCI_1 0x38
#define PRIO_XMA 0x40
#define PRIO_AUDIO 0x44
#define PRIO_ENET 0x4C
#define PRIO_XPS 0x54
#define PRIO_GRAPHICS 0x58
#define PRIO_PROFILER 0x60
#define PRIO_BIU 0x64
#define PRIO_IOC 0x68
#define PRIO_FSB 0x6C
#define PRIO_IPI_2 0x70
#define PRIO_CLOCK 0x74
#define PRIO_IPI_1 0x78
#define PRIO_NONE 0x7C

struct PRIO_REG {
  u32 hexData;
  bool intEnabled;
  bool latched;
  u8 targetCPU;
  u8 cpuIRQ;
};

struct PCI_BRIDGE_STATE {
  u32 REG_EA000000;
  u32 REG_EA000004;
  u32 REG_EA00000C;
  PRIO_REG PRIO_REG_CLCK;
  PRIO_REG PRIO_REG_ODD;
  PRIO_REG PRIO_REG_HDD;
  PRIO_REG PRIO_REG_SMM;
  PRIO_REG PRIO_REG_OHCI0;
  PRIO_REG PRIO_REG_OHCI1;
  PRIO_REG PRIO_REG_EHCI0;
  PRIO_REG PRIO_REG_EHCI1;
  PRIO_REG PRIO_REG_XMA;
  PRIO_REG PRIO_REG_AUDIO;
  PRIO_REG PRIO_REG_ENET;
  PRIO_REG PRIO_REG_GRAPHICS;
  PRIO_REG PRIO_REG_SFCX;
};

class PCIBridge {
public:
  PCIBridge();
  ~PCIBridge();

  // Checks wheter the current address belongs to the PCI bridge via
  // the BAR's
  bool IsAddressMappedinBAR(u32 address);

  void AddPCIDevice(std::shared_ptr<PCIDevice> device);

  void ResetPCIDevice(std::shared_ptr<PCIDevice> device);

  bool Read(u64 readAddress, u8 *data, u64 size);
  bool Write(u64 writeAddress, const u8 *data, u64 size);
  bool MemSet(u64 writeAddress, s32 data, u64 size);

  void ConfigRead(u64 readAddress, u8 *data, u64 size);
  void ConfigWrite(u64 writeAddress, const u8 *data, u64 size);

  void RegisterIIC(Xe::XCPU::IIC::XenonIIC *xenonIICPtr);

  bool RouteInterrupt(u8 prio);
  void CancelInterrupt(u8 prio);

private:
  // IIC Pointer used for interrupts
  Xe::XCPU::IIC::XenonIIC *xenonIIC;

  // Connected device pointers
  std::unordered_map<std::string, std::shared_ptr<PCIDevice>> connectedPCIDevices;

  // Current bridge config
  PCI_PCI_BRIDGE_CONFIG_SPACE pciBridgeConfig = {};
  PCI_BRIDGE_STATE pciBridgeState = {};
  u8 pciBridgeConfigSpace[256];
};
