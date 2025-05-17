// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Core/RAM/RAM.h"
#include "Core/XGPU/EDRAM.h"
#include "Core/XGPU/XGPUConfig.h"
#include "Core/XGPU/XenosRegisters.h"
#include "Core/XGPU/CommandProcessor.h"
#include "Core/RootBus/HostBridge/PCIe.h"

/*
 *	XGPU.h Basic Xenos implementation.
 */

namespace Render { class Renderer; }

namespace Xe {
namespace Xenos {

#define XGPU_DEVICE_SIZE 0x10000

class XGPU {
public:
  XGPU(Render::Renderer *renderer, RAM *ram, PCIBridge *pciBridge);
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
  bool RenderingTo2DFramebuffer() {
    return xenosState->rbbmControl != 0xFFFF0000 && false;
  }
private:
  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus = nullptr;
  // Mutex handle
  std::recursive_mutex mutex = {};
  // XGPU Config Space Data at address 0xD0010000.
  GENRAL_PCI_DEVICE_CONFIG_SPACE xgpuConfigSpace = {};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux.
  u32 pciDevSizes[6] = {};

  // RAM Pointer
  RAM *ramPtr = nullptr;

  // Render handle
  Render::Renderer *render = nullptr;

  // GPU State
  std::unique_ptr<Xe::XGPU::XenosState> xenosState = {};

  // EDRAM
  std::unique_ptr<Xe::XGPU::EDRAM> edram = {};

  // Command Processor
  std::unique_ptr<Xe::XGPU::CommandProcessor> commandProcessor = {};
};
} // namespace Xenos
} // namespace Xe
