/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/LifetimeGuard.h"

#include "Core/PCI/PCIe.h"

#include "Core/PCI/Bridge/PCIBridge.h"

#include "Core/XGPU/XGPU.h"

/*
  PCI Configuration Space at address 0xD0000000
  Bus0
    - Dev0  PCI-PCI Bridge    0xD0000000
    - Dev1  HostBridge        0xD0008000
*/

#define HOST_BRIDGE_SIZE 0x1FFFFFF // Maybe??

// Host Bridge Registers
//  These control interrupts,
//  and some other misc things.
struct HOSTBRIDGE_REGS {
  u32 REG_E0020000;
  u32 REG_E0020004;
};

struct BIU_REGS {
  u32 REG_E1003000;
  u32 REG_E1003100;
  u32 REG_E1003200;
  u32 REG_E1003300;
  u32 REG_E1010000;
  u32 REG_E1010010;
  u32 REG_E1010020;
  u32 REG_E1013000;
  u32 REG_E1013100;
  u32 REG_E1013200;
  u32 REG_E1013300;
  u32 REG_E1018000;
  u32 REG_E1018020;
  u32 REG_E1020000;
  u32 REG_E1020004;
  u32 REG_E1020008;
  u32 RAMSize;
  u32 REG_E1040074;
  u32 REG_E1040078;
};

class HostBridge {
public:
  HostBridge(u64 ramSize);
  ~HostBridge();

  std::weak_ptr<Xe::Xenos::XGPU> RegisterXGPU(std::unique_ptr<Xe::Xenos::XGPU> xgpu);
  std::weak_ptr<PCIBridge> RegisterPCIBridge(std::unique_ptr<PCIBridge> bridge);

  bool Read(u64 address, u8 *data, u64 size);
  bool Write(u64 address, const u8 *data, u64 size);
  bool MemSet(u64 address, s32 data, u64 size);

  // Configuration Space R/W
  bool ConfigRead(u64 address, u8 *data, u64 size);
  bool ConfigWrite(u64 address, const u8 *data, u64 size);

  std::weak_ptr<PCIBridge> GetPCIBridge() {
    return pciBridge;
  }
  std::weak_ptr<Xe::Xenos::XGPU> GetXGPU() {
    return xGPU;
  }

  // Acquire a lease guarding this bridge against concurrent teardown. See
  // Base::LifetimeGuard for the contract.
  Base::LifetimeGuard::Lease GetLease() {
    return lifetimeGuard.TryAcquire();
  }
private:
  // Pointer to the registered PCI Bridge
  std::shared_ptr<PCIBridge> pciBridge{};
  // Pointer to the registered XCGPU
  std::shared_ptr<Xe::Xenos::XGPU> xGPU{};

  // Guards against destruction racing an in-flight Read/Write/ConfigRead/
  // ConfigWrite from another thread.
  Base::LifetimeGuard lifetimeGuard{};

  // Access mutex. Only guards RegisterXGPU/RegisterPCIBridge mutation of
  // the shared_ptr members now; the dispatch entry points use the lease
  // above instead of holding this for their whole body.
  std::mutex mutex{};

  // Helpers
  bool IsAddressMappedinBAR(u32 address);

  // Host bridge context
  GENRAL_PCI_DEVICE_CONFIG_SPACE hostBridgeConfigSpace{};
  HOSTBRIDGE_REGS hostBridgeRegs{};
  BIU_REGS biuRegs{};
};
