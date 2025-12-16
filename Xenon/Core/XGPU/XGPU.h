/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

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
#include "Core/PCI/PCIe.h"

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

  bool IsAddressMappedInBAR(u32 address);

  // Dump framebuffer from RAM
  void DumpFB(const std::filesystem::path &path, s32 pitch);

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
    return !xenosState->framebufferDisable;
  }

  // GPU State
  std::unique_ptr<Xe::XGPU::XenosState> xenosState = {};
private:
  // PCI Bridge pointer. Used for Interrupts.
  PCIBridge *parentBus = nullptr;
  // Mutex handle
  Base::FutexRecursiveMutex mutex = {};
  // XGPU Config Space Data at address 0xD0010000.
  GENRAL_PCI_DEVICE_CONFIG_SPACE xgpuConfigSpace = {};
  // PCI Device Size, using when determining PCI device size of each BAR in Linux.
  u32 pciDevSizes[6] = {};

  // RAM Pointer
  RAM *ramPtr = nullptr;

  // Render handle
  Render::Renderer *render = nullptr;

  // EDRAM
  std::unique_ptr<Xe::XGPU::EDRAM> edram = {};

  // Command Processor
  std::unique_ptr<Xe::XGPU::CommandProcessor> commandProcessor = {};

  // Vertical Sync Worker thread
  std::thread xeVSyncWorkerThread;

  // VSYNC Worker thread running
  volatile bool xeVsyncWorkerThreadRunning = true;

  // Vertical Sync Worker Thread Loop
  // Should fire an interrupt to a given CPU every time a vertical sync event happens.
  // Normally this is the spped of the display's refresh rate.
  void xeVSyncWorkerThreadLoop();
};
} // namespace Xenos
} // namespace Xe
