// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Base/Types.h"
#include "Core/RAM/RAM.h"
#include "Core/XGPU/CommandProcessor.h"
#include "Core/RootBus/HostBridge/PCIe.h"

/*
 *	XGPU.h Basic Xenos implementation.
 */

namespace Xe {
namespace Xenos {

#define XGPU_DEVICE_SIZE 0x10000
#define XE_FB_BASE 0x1E000000

struct XenosState {
  std::unique_ptr<u8[]> Regs;
};

// ARGB (Console is BGRA)
#define COLOR(r, g, b, a) ((a) << 24 | (r) << 16 | (g) << 8 | (b) << 0)
#define TILE(x) ((x + 31) >> 5) << 5
class XGPU {
public:
  XGPU(RAM *ram);
  ~XGPU();

  // Memory Read/Write methods.
  bool Read(u64 readAddress, u8 *data, u64 size);
  bool Write(u64 writeAddress, const u8 *data, u64 size);
  bool MemSet(u64 writeAddress, s32 data, u64 size);

  void ConfigRead(u64 readAddress, u8 *data, u64 size);
  void ConfigWrite(u64 writeAddress, const u8 *data, u64 size);

  bool isAddressMappedInBAR(u32 address);

  // Dump framebuffer from RAM
  void DumpFB(const std::filesystem::path &path, int pitch);

private:
  // Mutex handle
  std::recursive_mutex mutex{};
  // XGPU Config Space Data at address 0xD0010000.
  GENRAL_PCI_DEVICE_CONFIG_SPACE xgpuConfigSpace{};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux.
  u32 pciDevSizes[6]{};

  // RAM Pointer
  RAM *ramPtr{};

  // GPU State
  XenosState xenosState{};

  Xe::XGPU::CommandProcessor commandProcessor;
};
} // namespace Xenos
} // namespace Xe
