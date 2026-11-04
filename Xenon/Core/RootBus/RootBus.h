/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/SystemDevice.h"
#include "Core/PCI/Bridge/HostBridge.h"

#include <memory>
#include <mutex>
#include <unordered_set>

// The CPU's view of the bus past the SoC. Addresses are 32-bit bus addresses: the MMU strips the region bits and
// handles the on-die SoC blocks (the 0x200_0000_0000 region) itself. What no block claims there is a bus address too:
// real mode with the reset HRMOR addresses everything through that region, the NAND flash window included.
//
//   0x00000000 - RAM size   DRAM (512 MiB retail)
//   everything else         the northbridge: config window, NB registers, GPU, the PCI-PCI bridge (and the NAND
//                           window behind it)
//
// An address nobody decodes is a master abort: reads return all ones, writes are dropped, and the first one in each
// 4 KiB page is logged.
class RootBus {
public:
  RootBus();
  ~RootBus();

  void AddHostBridge(std::shared_ptr<HostBridge> newHostBridge);

  // System memory is the only device that attaches here, the rest of the bus hangs off the host bridge.
  void AddDevice(std::shared_ptr<SystemDevice> device);

  // soc is true when the access came through the SoC region. It still reaches the bus (the flash window, the PCI
  // bridge) but never RAM, because the on-die blocks sit at the same low addresses RAM does. The 1BL reads memory
  // mapped NAND flash at early boot using an soc address, thus we keep this behavior.
  bool Read(u64 readAddress, u8 *data, u64 size, bool soc = false);
  bool Write(u64 writeAddress, const u8 *data, u64 size, bool soc = false);
  bool MemSet(u64 writeAddress, s32 data, u64 size, bool soc = false);

private:
  // Master aborts are logged once per 4 KiB page.
  void NoteMasterAbort(u64 address, bool write);

  std::shared_ptr<HostBridge> hostBridge{};
  std::shared_ptr<SystemDevice> ram{};
  u64 ramSize = 0;

  std::mutex abortMutex;
  std::unordered_set<u64> reportedAbortPages;
};
