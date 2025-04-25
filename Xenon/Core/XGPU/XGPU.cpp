// Copyright 2025 Xenon Emulator Project

#include "XGPU.h"

#include "XGPUConfig.h"
#include "XenosRegisters.h"

#include "Core/Xe_Main.h"

#include "Base/Config.h"
#include "Base/Path_util.h"
#include "Base/Logging/Log.h"

#ifdef _DEBUG
#define XE_DEBUG
#endif

Xe::Xenos::XGPU::XGPU(RAM *ram) :
  ramPtr(ram),
  internalWidth(Config::xgpu.internal.width),
  internalHeight(Config::xgpu.internal.height)
{
  memset(&xgpuConfigSpace.data, 0xF, sizeof(GENRAL_PCI_DEVICE_CONFIG_SPACE));

  // Setup config space as per dump taken from a Jasper console.
  // Located at config address 0xD0010000.
  memcpy(xgpuConfigSpace.data, xgpuConfigMap, sizeof(xgpuConfigSpace.data));

  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x20000; // BAR0

  xenosState.Regs = std::make_unique<STRIP_UNIQUE_ARR(xenosState.Regs)>(0xFFFFF);
  memset(xenosState.Regs.get(), 0, 0xFFFFF);

  // Set Clocks speeds.
  u32 reg = 0x09000000;
  memcpy(&xenosState.Regs[(u32)XeRegister::SPLL_CNTL_REG * 4], &reg, 4);
  reg = 0x11000C00;
  memcpy(&xenosState.Regs[(u32)XeRegister::RPLL_CNTL_REG * 4], &reg, 4);
  reg = 0x1A000001;
  memcpy(&xenosState.Regs[(u32)XeRegister::FPLL_CNTL_REG * 4], &reg, 4);
  reg = 0x19100000;
  memcpy(&xenosState.Regs[(u32)XeRegister::MPLL_CNTL_REG * 4], &reg, 4);

  commandProcessor = std::make_unique<STRIP_UNIQUE(commandProcessor)>(ramPtr);
}

Xe::Xenos::XGPU::~XGPU() {
  commandProcessor.reset();
  xenosState.Regs.reset();
}

bool Xe::Xenos::XGPU::Read(u64 readAddress, u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  if (isAddressMappedInBAR(static_cast<u32>(readAddress))) {
    THROW(size > 4);
    const u32 regIndex = (readAddress & 0xFFFFF) / 4;
    
#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "Read to {}, index {:#x}", GetRegisterNameById(regIndex), regIndex);
#endif

    LOG_TRACE(Xenos, "Read Addr = {:#x}, reg: {:#x}.", readAddress, regIndex);

    XeRegister reg = static_cast<XeRegister>(regIndex);

    u32 regData = 0;
    memcpy(&regData, &xenosState.Regs[regIndex * 4], 4);

    // Switch for properly return the requested amount of data
    switch (size) {
    case 2:
      regData = regData >> 16;
      break;
    case 1:
      regData = regData >> 24;
      break;
    default:
      break;
    }

    u32 value = 0;
    switch (reg) {
    case XeRegister::MH_STATUS:
      value = 0x2;
      break;
    case XeRegister::DC_LUT_AUTOFILL:
      value = 0x2;
      break;
    case XeRegister::XDVO_REGISTER_INDEX:
      value = 0x0;
      break;
    default:
      value = regData;
      memcpy(data, &value, size);
      return true;
    }

    value = byteswap_be(value);
    memcpy(data, &value, size);

    return true;
  }

  return false;
}

bool Xe::Xenos::XGPU::Write(u64 writeAddress, const u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  if (isAddressMappedInBAR(static_cast<u32>(writeAddress))) {
    THROW(size > 4);
    const u32 regIndex = (writeAddress & 0xFFFFF) / 4;

    u32 tmp = 0;
    memcpy(&tmp, data, size);
    tmp = byteswap_be<u32>(tmp);

#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "Write to {} (addr: {:#x}), index {:#x}, data = {:#x}", GetRegisterNameById(regIndex), writeAddress, regIndex, tmp);
#endif

    const XeRegister reg = static_cast<XeRegister>(regIndex);

    switch (reg) {
    case XeRegister::D1GRPH_PRIMARY_SURFACE_ADDRESS:
      fbSurfaceAddress = tmp;
      break;
    case XeRegister::D1GRPH_X_END:
      internalWidth = tmp;
      break;
    case XeRegister::D1GRPH_Y_END:
      internalHeight = tmp;
      break;
    case XeRegister::CP_RB_CNTL:
      commandProcessor->CPUpdateRBSize(tmp);
      break;
    case XeRegister::CP_RB_BASE:
      commandProcessor->CPUpdateRBBase(tmp);
      break;
    }

    memcpy(&xenosState.Regs[regIndex * 4], data, size);
    return true;
  }

  return false;
}

bool Xe::Xenos::XGPU::MemSet(u64 writeAddress, s32 data, u64 size) {
  std::lock_guard lck(mutex);
  if (isAddressMappedInBAR(static_cast<u32>(writeAddress))) {
    const u32 regIndex = (writeAddress & 0xFFFFF) / 4;

#ifdef XE_DEBUG
    LOG_TRACE(Xenos, "Write to {} (addr: {:#x}), index {:#x}, data = {:#x}", GetRegisterNameById(regIndex), writeAddress, regIndex,
      byteswap_be<u32>(data));
#endif

    memset(&xenosState.Regs[regIndex * 4], data, size);
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

bool Xe::Xenos::XGPU::isAddressMappedInBAR(u32 address) {
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
    f.write(reinterpret_cast<const char*>(ramPtr->getPointerToAddress(fbSurfaceAddress)), pitch);
    LOG_INFO(Xenos, "Framebuffer dumped to '{}'", path.string());
  }
  f.close();
}
