/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Bridge/BridgeHeader.h"
#include "Core/PCI/Bridge/NorthbridgeRegisters.h"
#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/PCIe.h"
#include "Core/XGPU/XenosGPU.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>

// The northbridge as the CPU sees it past RAM.
//
// The config window is 0xD0000000-0xD0FFFFFF, addressed as bus << 20 | dev << 15 | fn << 12 | reg:
//   bus 0 dev 0  PCI-PCI bridge to the southbridge (type 1 header, its secondary bus is bus 1)
//   bus 0 dev 1  host bridge / northbridge, BAR0 0xE0000000, 32 MiB register file
//   bus 0 dev 2  GPU + memory controllers, BAR0 0xEC800000, 128 KiB (0xE4000000 during HWINIT)
// They're all single function and anything else on bus 0 master-aborts. Other buses go to the PCI-PCI bridge.
//
// Memory is decoded in this order: the northbridge registers (host bridge BAR0), the GPU's BAR0, then whatever the
// PCI-PCI bridge claims (its interrupt controller and the BARs behind it, the NAND window included). The caller
// master-aborts the rest.
class HostBridge {
public:
  explicit HostBridge(u64 ramSize);
  ~HostBridge();

  // The GPU. While registered it is device 2: BAR0, config space, the BIU's memory size and self-refresh
  // request. nullptr unregisters it.
  void RegisterXenosGPU(std::shared_ptr<Xe::Xenos::XenosGPU> gpu);

  // The PCI-PCI bridge to the southbridge (bus 0 device 0). The northbridge uses it to route the IOC's interrupts.
  void RegisterPCIBridge(std::shared_ptr<PCIBridge> bridge);

  // Platform reset (reboot or power off). The GPU stops and goes back to power-on, every southbridge function is
  // reset, and the northbridge registers and the host bridge's own header return to their reset
  // values. Call it with the CPU halted, before guest memory is cleared.
  void Reset();

  // Memory space (configuration window included). False when nothing decodes the address.
  bool Read(u64 readAddress, u8 *data, u64 size);
  bool Write(u64 writeAddress, const u8 *data, u64 size);
  bool MemSet(u64 writeAddress, s32 data, u64 size);

private:
  enum class eTarget {
    None,
    Config,
    Northbridge,
    XenosGPU,
    PCIBridge
  };
  // Which target claims address. 'offset' is set for the northbridge.
  eTarget Decode(u32 address, u32 &offset);

  // Configuration cycles. False = master abort (reads return all ones).
  bool ConfigRead(u32 address, u8 *data, u64 size);
  bool ConfigWrite(u32 address, const u8 *data, u64 size);
  void OwnConfigWrite(u32 reg, const u8 *data, u64 size);
  // The header as the reset leaves it.
  void ResetConfig();

  // Guards the host bridge's config space.
  std::mutex mutex{};

  // Own header (bus 0 device 1). BAR0 is mirrored in northbridgeBase for the decoder.
  Xe::PCI::BridgeHeader header;
  std::atomic<u32> northbridgeBase{0};

  // The GPU, when registered.
  std::shared_ptr<Xe::Xenos::XenosGPU> xenosGPU{};

  // The registered PCI-PCI bridge.
  std::shared_ptr<PCIBridge> pciBridge{};

  // BAR0: the northbridge register file.
  Xe::PCI::NorthbridgeRegisters northbridge;
};
