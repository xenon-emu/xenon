// Copyright 2025 Xenon Emulator Project

#pragma once

#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Core/RAM/RAM.h"
#include "Core/XGPU/CommandProcessor.h"
#include "Core/XGPU/XGPUConfig.h"
#include "Core/XGPU/XenosRegisters.h"
#include "Core/RootBus/HostBridge/PCIe.h"

/*
 *	XGPU.h Basic Xenos implementation.
 */

namespace Xe {
namespace Xenos {

#define XGPU_DEVICE_SIZE 0x10000

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

  u32 GetSurface() {
    return xenosState->fbSurfaceAddress;
  }
  u32 GetWidth() {
    return xenosState->internalWidth;
  }
  u32 GetHeight() {
    return xenosState->internalHeight;
  }
private:
  // Mutex handle
  std::recursive_mutex mutex = {};
  // XGPU Config Space Data at address 0xD0010000.
  GENRAL_PCI_DEVICE_CONFIG_SPACE xgpuConfigSpace = {};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux.
  u32 pciDevSizes[6] = {};

  // RAM Pointer
  RAM *ramPtr = nullptr;

  // GPU State
  std::unique_ptr<Xe::XGPU::XenosState> xenosState = {};

  // Command Processor
  std::unique_ptr<Xe::XGPU::CommandProcessor> commandProcessor = {};
};
} // namespace Xenos
} // namespace Xe
