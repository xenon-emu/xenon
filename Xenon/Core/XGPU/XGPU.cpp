// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "XGPU.h"

#include "XGPUConfig.h"
#include "XenosRegisters.h"

#include "Core/XeMain.h"

#include "Base/Config.h"
#include "Base/PathUtil.h"
#include "Base/Logging/Log.h"

//#ifdef _DEBUG
#define XE_DEBUG
//#endif

Xe::Xenos::XGPU::XGPU(Render::Renderer *renderer, RAM *ram, PCIBridge *pciBridge) :
  render(renderer),
  ramPtr(ram), parentBus(pciBridge) {
  edram = std::make_unique<STRIP_UNIQUE(edram)>();

  xenosState = std::make_unique<STRIP_UNIQUE(xenosState)>(ramPtr, edram.get(), nullptr);

  memset(&xgpuConfigSpace.data, 0xF, sizeof(GENRAL_PCI_DEVICE_CONFIG_SPACE));

  // Located at config address 0xD0010000
  memcpy(xgpuConfigSpace.data, xgpuConfigMap, sizeof(xgpuConfigSpace.data));

  u32 consoleRevison;
  PCI_CONFIG_HDR_REG0 &revision = xgpuConfigSpace.configSpaceHeader.reg0;
  PCI_CONFIG_HDR_REG2 &gpuRevision = xgpuConfigSpace.configSpaceHeader.reg2;
  revision.vendorID = 0x1414;
  switch (Config::highlyExperimental.consoleRevison) {
  case Config::eConsoleRevision::Zephyr: {
    // gpuRevision.revID = ... ;
    // revision.deviceID = ... ;
  } break;
  case Config::eConsoleRevision::Falcon: {
    gpuRevision.revID = 0x10;
    revision.deviceID = 0x5821;
  } break;
  case Config::eConsoleRevision::Jasper: {
    gpuRevision.revID = 0x11;
    revision.deviceID = 0x5831;
  } break;
  case Config::eConsoleRevision::Trinity: {
    gpuRevision.revID = 0x00;
    revision.deviceID = 0x5841;
  } break;
  case Config::eConsoleRevision::Corona4GB:
  case Config::eConsoleRevision::Corona: {
    gpuRevision.revID = 0x01;
    revision.deviceID = 0x5841;
  } break;
  case Config::eConsoleRevision::Winchester: {
    gpuRevision.revID = 0x01;
    revision.deviceID = 0x5851;
  } break;
  }
  LOG_INFO(Xenos, "Xenos DeviceID: 0x{:X}", revision.deviceID);
  LOG_INFO(Xenos, "Xenos RevID: 0x{:X}", gpuRevision.revID);

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x20000; // BAR0

  // Set Clocks speeds.

  // TODO: Fix for Valhalla (Winchester)
  // TODO: Fix for Slims
  switch (Config::highlyExperimental.consoleRevison) {
  case Config::eConsoleRevision::Zephyr:
  case Config::eConsoleRevision::Falcon:
  case Config::eConsoleRevision::Jasper: {
    xenosState->WriteRegister(XeRegister::SPLL_CNTL_REG, 0x09000000);
    xenosState->WriteRegister(XeRegister::RPLL_CNTL_REG, 0x11000C00);
    xenosState->WriteRegister(XeRegister::FPLL_CNTL_REG, 0x1A000001);
    xenosState->WriteRegister(XeRegister::MPLL_CNTL_REG, 0x19100000);
  } break;
  case Config::eConsoleRevision::Trinity: {
    xenosState->WriteRegister(XeRegister::SPLL_CNTL_REG, 0x09000000);
    xenosState->WriteRegister(XeRegister::RPLL_CNTL_REG, 0x11000C00);
    xenosState->WriteRegister(XeRegister::FPLL_CNTL_REG, 0x1A000001);
    xenosState->WriteRegister(XeRegister::MPLL_CNTL_REG, 0x19100000);
    xenosState->WriteRegister(XeRegister::MDLL_CNTL1_REG, 0x19100000);
  } break;
  case Config::eConsoleRevision::Corona4GB:
  case Config::eConsoleRevision::Corona: {
    xenosState->WriteRegister(XeRegister::SPLL_CNTL_REG, 0x09000000);
    xenosState->WriteRegister(XeRegister::RPLL_CNTL_REG, 0x11000C00);
    xenosState->WriteRegister(XeRegister::FPLL_CNTL_REG, 0x1A000001);
    xenosState->WriteRegister(XeRegister::MPLL_CNTL_REG, 0x19100000);
    xenosState->WriteRegister(XeRegister::MDLL_CNTL1_REG, 0x19100000);
  } break;
  case Config::eConsoleRevision::Winchester: {
    xenosState->WriteRegister(XeRegister::SPLL_CNTL_REG, 0x09000000);
    xenosState->WriteRegister(XeRegister::RPLL_CNTL_REG, 0x11000C00);
    xenosState->WriteRegister(XeRegister::FPLL_CNTL_REG, 0x1A000001);
    xenosState->WriteRegister(XeRegister::MPLL_CNTL_REG, 0x19100000);
    xenosState->WriteRegister(XeRegister::MDLL_CNTL1_REG, 0x19100000);
  } break;
  }

  commandProcessor = std::make_unique<STRIP_UNIQUE(commandProcessor)>(ramPtr, xenosState.get(), render, parentBus);
  xenosState->commandProcessor = commandProcessor.get(); // CP expects xenosState, xenosState expects CP, this fixes it.
}

Xe::Xenos::XGPU::~XGPU() {
  commandProcessor.reset();
  xenosState.reset();
  edram.reset();
}

bool Xe::Xenos::XGPU::Read(u64 readAddress, u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  if (IsAddressMappedInBAR(static_cast<u32>(readAddress))) {
    THROW(size > 4);
    const u32 regIndex = (readAddress & 0xFFFFF) / 4;
    const XeRegister reg = static_cast<XeRegister>(regIndex);
    u32 value = xenosState->ReadRegister(reg, size);
    memcpy(data, &value, size);
#ifdef XE_DEBUG
    if (reg != XeRegister::D1MODE_VBLANK_STATUS)
      LOG_DEBUG(Xenos, "Read from {} (0x{:X}), index: 0x{:X}, value: 0x{:X}, size: 0x{:X}", Xe::XGPU::GetRegisterNameById(regIndex), readAddress, regIndex, value, size);
#endif
    return true;
  }

  return false;
}

bool Xe::Xenos::XGPU::Write(u64 writeAddress, const u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  if (IsAddressMappedInBAR(static_cast<u32>(writeAddress))) {
    THROW(size > 4);
    const u32 regIndex = (writeAddress & 0xFFFFF) / 4;
    const XeRegister reg = static_cast<XeRegister>(regIndex);
    u32 value = 0;
    memcpy(&value, data, size);
    xenosState->WriteRegister(reg, value);
#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "Write to {} (addr: 0x{:X}), index 0x{:X}, data = 0x{:X}", Xe::XGPU::GetRegisterNameById(regIndex), writeAddress, regIndex, value);
#endif
    return true;
  }

  return false;
}

bool Xe::Xenos::XGPU::MemSet(u64 writeAddress, s32 data, u64 size) {
  std::lock_guard lck(mutex);
  if (IsAddressMappedInBAR(static_cast<u32>(writeAddress))) {
    const u32 regIndex = (writeAddress & 0xFFFFF) / 4;

#ifdef XE_DEBUG
    LOG_TRACE(Xenos, "Write to {} (addr: 0x{:X}), index 0x{:X}, data = 0x{:X}", Xe::XGPU::GetRegisterNameById(regIndex), writeAddress, regIndex, data);
#endif
    const XeRegister reg = static_cast<XeRegister>(regIndex);

    memset(xenosState->GetRegisterPointer(reg), data, size);
    return true;
  }

  return false;
}

void Xe::Xenos::XGPU::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  memcpy(data, &xgpuConfigSpace.data[readAddress & 0xFF], size);
}

void Xe::Xenos::XGPU::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  // Check if we're being scanned
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          tmp &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
      tmp = 0; // Register not implemented
    }
  }

  memcpy(&xgpuConfigSpace.data[writeAddress & 0xFF], &tmp, size);
}

bool Xe::Xenos::XGPU::IsAddressMappedInBAR(u32 address) {
  #define ADDRESS_BOUNDS_CHECK(a, b) (address >= a && address <= (a + b))
  if (ADDRESS_BOUNDS_CHECK(xgpuConfigSpace.configSpaceHeader.BAR0, XGPU_DEVICE_SIZE) ||
    ADDRESS_BOUNDS_CHECK(xgpuConfigSpace.configSpaceHeader.BAR1, XGPU_DEVICE_SIZE) ||
    ADDRESS_BOUNDS_CHECK(xgpuConfigSpace.configSpaceHeader.BAR2, XGPU_DEVICE_SIZE) ||
    ADDRESS_BOUNDS_CHECK(xgpuConfigSpace.configSpaceHeader.BAR3, XGPU_DEVICE_SIZE) ||
    ADDRESS_BOUNDS_CHECK(xgpuConfigSpace.configSpaceHeader.BAR4, XGPU_DEVICE_SIZE) ||
    ADDRESS_BOUNDS_CHECK(xgpuConfigSpace.configSpaceHeader.BAR5, XGPU_DEVICE_SIZE))
  {
    return true;
  }

  return false;
}

void Xe::Xenos::XGPU::DumpFB(const std::filesystem::path &path, int pitch) {
  std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!f) {
    LOG_ERROR(Xenos, "Failed to open {} for writing", path.filename().string());
  }
  else {
    f.write(reinterpret_cast<const char*>(ramPtr->getPointerToAddress(xenosState->fbSurfaceAddress)), pitch);
    LOG_INFO(Xenos, "Framebuffer dumped to '{}'", path.string());
  }
  f.close();
}
