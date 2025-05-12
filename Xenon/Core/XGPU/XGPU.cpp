// Copyright 2025 Xenon Emulator Project

#include "XGPU.h"

#include "XGPUConfig.h"
#include "XenosRegisters.h"

#include "Core/Xe_Main.h"

#include "Base/Config.h"
#include "Base/Path_util.h"
#include "Base/Logging/Log.h"

//#ifdef _DEBUG
#define XE_DEBUG
//#endif

Xe::Xenos::XGPU::XGPU(RAM *ram, PCIBridge *pciBridge) : ramPtr(ram), parentBus(pciBridge){
  xenosState = std::make_unique<STRIP_UNIQUE(xenosState)>();

  memset(&xgpuConfigSpace.data, 0xF, sizeof(GENRAL_PCI_DEVICE_CONFIG_SPACE));

  // Located at config address 0xD0010000.
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

  commandProcessor = std::make_unique<STRIP_UNIQUE(commandProcessor)>(ramPtr, xenosState.get(), parentBus);
}

Xe::Xenos::XGPU::~XGPU() {
  commandProcessor.reset();
  xenosState.reset();
}

bool Xe::Xenos::XGPU::Read(u64 readAddress, u8 *data, u64 size) {
  std::lock_guard lck(mutex);
  if (isAddressMappedInBAR(static_cast<u32>(readAddress))) {
    THROW(size > 4);
    const u32 regIndex = (readAddress & 0xFFFFF) / 4;
    const XeRegister reg = static_cast<XeRegister>(regIndex);

    u32 regData = xenosState->ReadRegister(reg);
    // Switch for properly return the requested amount of data
    switch (size) {
    case 2:
      regData >>= 16;
      break;
    case 1:
      regData >>= 24;
      break;
    default:
      break;
    }
    u32 value = regData;

    switch (reg) {
    // VdpHasWarmBooted expects this to be 0x10, otherwise, it waits until the GPU has intialised
    case XeRegister::CONFIG_CNTL:
      value = xenosState->configControl;
      break;
    case XeRegister::RBBM_CNTL:
      value = xenosState->rbbmControl;
      break;
    case XeRegister::RBBM_SOFT_RESET:
      value = xenosState->rbbmSoftReset;
      if (xenosState->rbbmSoftReset == 0) {
        // Reset RBBM
        if ((xenosState->rbbmStatus & 0x600)) {
          xenosState->rbbmStatus &= ~(0x600);
        }
        if ((xenosState->rbbmStatus & 0x400)) {
          xenosState->rbbmStatus &= ~(0x400);
        }
        // GPU is partly reset, set status to 0x80 (just instantly reset for now)
        xenosState->rbbmStatus |= 0x80;
        // Set as warm boot
        xenosState->configControl = 0x10000000;
      }
      break;
    // Gets past VdInitializeEngines+0x58
    case XeRegister::RBBM_DEBUG:
      value = xenosState->rbbmDebug;
      break;
    case XeRegister::RBBM_STATUS:
      if (!(xenosState->rbbmStatus & 0x400)) {
        xenosState->rbbmStatus |= 0x400;
      }
      if (!(xenosState->rbbmStatus & 0x600)) {
        xenosState->rbbmStatus |= 0x600;
      }
      value = xenosState->rbbmStatus;
      break;
    case XeRegister::MH_STATUS:
      if (!(xenosState->mhStatus & 0x2000000)) {
        xenosState->mhStatus |= 0x2000000;
      }
      value = xenosState->mhStatus;
      break;
    case XeRegister::D1CRTC_CONTROL:
      value = xenosState->crtcControl;
      break;
    case XeRegister::DC_LUT_AUTOFILL:
      if (xenosState->dcLutAutofill == 0x1) {
        xenosState->dcLutAutofill = 0x2000000;
      }
      value = xenosState->dcLutAutofill;
      break;
    case XeRegister::XDVO_ENABLE:
      value = xenosState->xdvoEnable;
      break;
    case XeRegister::XDVO_BIT_DEPTH_CONTROL:
      value = xenosState->xdvoBitDepthControl;
      break;
    case XeRegister::XDVO_CLOCK_INV:
      value = xenosState->xdvoClockInv;
      break;
    case XeRegister::XDVO_CONTROL:
      value = xenosState->xdvoControl;
      break;
    case XeRegister::XDVO_CRC_EN:
      value = xenosState->xdvoCrcEnable;
      break;
    case XeRegister::XDVO_CRC_CNTL:
      value = xenosState->xdvoCrcControl;
      break;
    case XeRegister::XDVO_CRC_MASK_SIG_RGB:
      value = xenosState->xdvoCrcMaskSignalRGB;
      break;
    case XeRegister::XDVO_CRC_MASK_SIG_CNTL:
      value = xenosState->xdvoCrcMaskSignalControl;
      break;
    case XeRegister::XDVO_CRC_SIG_RGB:
      value = xenosState->xdvoCrcSignalRGB;
      break;
    case XeRegister::XDVO_CRC_SIG_CNTL:
      value = xenosState->xdvoCrcSignalControl;
      break;
    case XeRegister::XDVO_STRENGTH_CONTROL:
      value = xenosState->xdvoStrengthControl;
      break;
    case XeRegister::XDVO_DATA_STRENGTH_CONTROL:
      value = xenosState->xdvoDataStrengthControl;
      break;
    case XeRegister::XDVO_FORCE_OUTPUT_CNTL:
      value = xenosState->xdvoForceOutputControl;
      break;
    case XeRegister::XDVO_REGISTER_INDEX:
      value = xenosState->xdvoRegisterIndex;
      break;
    case XeRegister::XDVO_REGISTER_DATA:
      value = xenosState->xdvoRegisterData;
      break;
    case XeRegister::RB_SURFACE_INFO:
      value = xenosState->surfaceInfo;
      break;
    case XeRegister::RB_COLOR_INFO:
      value = xenosState->colorInfo;
      break;
    case XeRegister::RB_DEPTH_INFO:
      value = xenosState->depthInfo;
      break;
    case XeRegister::RB_COLOR1_INFO:
      value = xenosState->color1Info;
      break;
    case XeRegister::RB_COLOR2_INFO:
      value = xenosState->color2Info;
      break;
    case XeRegister::RB_COLOR3_INFO:
      value = xenosState->color3Info;
      break;
    case XeRegister::RB_BLEND_RED:
      value = xenosState->blendRed;
      break;
    case XeRegister::RB_BLEND_GREEN:
      value = xenosState->blendGreen;
      break;
    case XeRegister::RB_BLEND_BLUE:
      value = xenosState->blendBlue;
      break;
    case XeRegister::RB_BLEND_ALPHA:
      value = xenosState->blendAlpha;
      break;
    case XeRegister::PA_CL_VTE_CNTL:
      value = xenosState->viewportControl;
      break;
    case XeRegister::PA_SC_WINDOW_OFFSET:
      value = xenosState->windowOffset;
      break;
    case XeRegister::PA_SC_WINDOW_SCISSOR_TL:
      value = xenosState->windowScissorTl;
      break;
    case XeRegister::PA_SC_WINDOW_SCISSOR_BR:
      value = xenosState->windowScissorBr;
      break;
    case XeRegister::PA_CL_VPORT_XOFFSET:
      value = xenosState->viewportXOffset;
      break;
    case XeRegister::PA_CL_VPORT_YOFFSET:
      value = xenosState->viewportYOffset;
      break;
    case XeRegister::PA_CL_VPORT_ZOFFSET:
      value = xenosState->viewportZOffset;
      break;
    case XeRegister::PA_CL_VPORT_XSCALE:
      value = xenosState->viewportXScale;
      break;
    case XeRegister::PA_CL_VPORT_YSCALE:
      value = xenosState->viewportYScale;
      break;
    case XeRegister::PA_CL_VPORT_ZSCALE:
      value = xenosState->viewportZScale;
      break;
    case XeRegister::RB_STENCILREFMASK:
      value = xenosState->stencilReferenceMask;
      break;
    case XeRegister::RB_DEPTHCONTROL:
      value = xenosState->depthControl;
      break;
    case XeRegister::RB_BLENDCONTROL0:
      value = xenosState->blendControl0;
      break;
    case XeRegister::RB_TILECONTROL:
      value = xenosState->tileControl;
      break;
    case XeRegister::RB_MODECONTROL:
      value = xenosState->modeControl;
      break;
    case XeRegister::RB_BLENDCONTROL1:
      value = xenosState->blendControl1;
      break;
    case XeRegister::RB_BLENDCONTROL2:
      value = xenosState->blendControl2;
      break;
    case XeRegister::RB_BLENDCONTROL3:
      value = xenosState->blendControl3;
      break;
    case XeRegister::RB_COPY_CONTROL:
      value = xenosState->copyControl;
      break;
    case XeRegister::RB_DEPTH_CLEAR:
      value = xenosState->depthClear;
      break;
    case XeRegister::RB_COLOR_CLEAR:
      value = xenosState->clearColor;
      break;
    case XeRegister::RB_COLOR_CLEAR_LO:
      value = xenosState->clearColorLo;
      break;
    default:
      value = regData;
      break;
    }
#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "Read from {} (0x{:X}), index 0x{:X}, value 0x{:X}", Xe::XGPU::GetRegisterNameById(regIndex), readAddress, regIndex, value);
#endif

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

    u32 tmpOld = 0;
    memcpy(&tmpOld, data, size);
    u32 tmp = byteswap_be<u32>(tmpOld);

#ifdef XE_DEBUG
    LOG_DEBUG(Xenos, "Write to {} (addr: 0x{:X}), index 0x{:X}, data = 0x{:X}", Xe::XGPU::GetRegisterNameById(regIndex), writeAddress, regIndex, tmp);
#endif

    const XeRegister reg = static_cast<XeRegister>(regIndex);

    switch (reg) {
    // VdpHasWarmBooted expects this to be 0x10, otherwise, it waits until the GPU has intialised
    case XeRegister::CONFIG_CNTL:
      xenosState->configControl = tmp;
      xenosState->WriteRegister(reg, xenosState->configControl);
      break;
    case XeRegister::RBBM_CNTL:
      xenosState->rbbmControl = byteswap_be<u32>(tmp);
      xenosState->WriteRegister(reg, xenosState->rbbmControl);
      break;
    case XeRegister::RBBM_SOFT_RESET:
      xenosState->rbbmSoftReset = tmp;
      xenosState->WriteRegister(reg, xenosState->rbbmSoftReset);
      break;
    case XeRegister::CP_RB_BASE:
      commandProcessor->CPUpdateRBBase(tmp);
      break;
    case XeRegister::CP_RB_CNTL:
      commandProcessor->CPUpdateRBSize(tmp);
      break;
    case XeRegister::CP_RB_WPTR:
      commandProcessor->CPUpdateRBWritePointer(tmp);
      break;
    case XeRegister::SCRATCH_UMSK:
      xenosState->scratchMask = tmp;
      xenosState->WriteRegister(reg, xenosState->scratchMask);
      break;
    case XeRegister::SCRATCH_ADDR:
      xenosState->scratchAddr = tmp;
      xenosState->WriteRegister(reg, xenosState->scratchAddr);
      break;
    case XeRegister::RBBM_DEBUG:
      xenosState->rbbmDebug = tmp;
      xenosState->WriteRegister(reg, xenosState->rbbmDebug);
      break;
    case XeRegister::SCRATCH_REG0:
    case XeRegister::SCRATCH_REG1:
    case XeRegister::SCRATCH_REG2:
    case XeRegister::SCRATCH_REG3:
    case XeRegister::SCRATCH_REG4:
    case XeRegister::SCRATCH_REG5:
    case XeRegister::SCRATCH_REG6:
    case XeRegister::SCRATCH_REG7: {
      // Write the initial value
      xenosState->WriteRegister(reg, tmp);
      const u32 scratchRegIndex = regIndex - static_cast<u32>(XeRegister::SCRATCH_REG0);
      // Check if writing is enabled
      if ((1 << scratchRegIndex) & xenosState->scratchMask) {
        // Writeback
        const u32 memAddr = xenosState->scratchAddr + (scratchRegIndex * 4);
        u8 *memPtr = ramPtr->getPointerToAddress(memAddr);
        memcpy(memPtr, &tmpOld, sizeof(tmpOld));
      }
    } break;
    case XeRegister::MH_STATUS:
      xenosState->mhStatus = tmp;
      if (!(xenosState->mhStatus & 0x2000000)) {
        xenosState->mhStatus |= 0x2000000;
      }
      xenosState->WriteRegister(reg, xenosState->mhStatus);
      break;
    case XeRegister::COHER_STATUS_HOST:
      tmp |= 0x80000000ul;
      xenosState->WriteRegister(reg, tmp);
      break;
    case XeRegister::D1CRTC_CONTROL:
      xenosState->crtcControl = tmp;
      xenosState->WriteRegister(reg, xenosState->crtcControl);
      break;
    case XeRegister::D1GRPH_PRIMARY_SURFACE_ADDRESS:
      xenosState->fbSurfaceAddress = tmp;
      xenosState->WriteRegister(reg, xenosState->fbSurfaceAddress);
      break;
    case XeRegister::D1GRPH_X_END:
      xenosState->internalWidth = tmp;
      xenosState->WriteRegister(reg, xenosState->internalWidth);
      break;
    case XeRegister::D1GRPH_Y_END:
      xenosState->internalHeight = tmp;
      xenosState->WriteRegister(reg, xenosState->internalHeight);
      break;
    case XeRegister::DC_LUT_AUTOFILL:
      xenosState->dcLutAutofill = tmp;
      xenosState->WriteRegister(reg, xenosState->dcLutAutofill);
      break;
    case XeRegister::XDVO_ENABLE:
      xenosState->xdvoEnable = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoEnable);
      break;
    case XeRegister::XDVO_BIT_DEPTH_CONTROL:
      xenosState->xdvoBitDepthControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoBitDepthControl);
      break;
    case XeRegister::XDVO_CLOCK_INV:
      xenosState->xdvoClockInv = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoClockInv);
      break;
    case XeRegister::XDVO_CONTROL:
      xenosState->xdvoControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoControl);
      break;
    case XeRegister::XDVO_CRC_EN:
      xenosState->xdvoCrcEnable = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoCrcEnable);
      break;
    case XeRegister::XDVO_CRC_CNTL:
      xenosState->xdvoCrcControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoCrcControl);
      break;
    case XeRegister::XDVO_CRC_MASK_SIG_RGB:
      xenosState->xdvoCrcMaskSignalRGB = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoCrcMaskSignalRGB);
      break;
    case XeRegister::XDVO_CRC_MASK_SIG_CNTL:
      xenosState->xdvoCrcMaskSignalControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoCrcMaskSignalControl);
      break;
    case XeRegister::XDVO_CRC_SIG_RGB:
      xenosState->xdvoCrcSignalRGB = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoCrcSignalRGB);
      break;
    case XeRegister::XDVO_CRC_SIG_CNTL:
      xenosState->xdvoCrcSignalControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoCrcSignalControl);
      break;
    case XeRegister::XDVO_STRENGTH_CONTROL:
      xenosState->xdvoStrengthControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoStrengthControl);
      break;
    case XeRegister::XDVO_DATA_STRENGTH_CONTROL:
      xenosState->xdvoDataStrengthControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoDataStrengthControl);
      break;
    case XeRegister::XDVO_FORCE_OUTPUT_CNTL:
      xenosState->xdvoForceOutputControl = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoForceOutputControl);
      break;
    case XeRegister::XDVO_REGISTER_INDEX:
      xenosState->xdvoRegisterIndex = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoRegisterIndex);
      break;
    case XeRegister::XDVO_REGISTER_DATA:
      xenosState->xdvoRegisterData = tmp;
      xenosState->WriteRegister(reg, xenosState->xdvoRegisterData);
      break;
    case XeRegister::RB_SURFACE_INFO:
      xenosState->surfaceInfo = tmp;
      xenosState->WriteRegister(reg, xenosState->surfaceInfo);
      break;
    case XeRegister::RB_COLOR_INFO:
      xenosState->colorInfo = tmp;
      xenosState->WriteRegister(reg, xenosState->colorInfo);
      break;
    case XeRegister::RB_DEPTH_INFO:
      xenosState->depthInfo = tmp;
      xenosState->WriteRegister(reg, xenosState->depthInfo);
      break;
    case XeRegister::RB_COLOR1_INFO:
      xenosState->color1Info = tmp;
      xenosState->WriteRegister(reg, xenosState->color1Info);
      break;
    case XeRegister::RB_COLOR2_INFO:
      xenosState->color2Info = tmp;
      xenosState->WriteRegister(reg, xenosState->color2Info);
      break;
    case XeRegister::RB_COLOR3_INFO:
      xenosState->color3Info = tmp;
      xenosState->WriteRegister(reg, xenosState->color3Info);
      break;
    case XeRegister::RB_BLEND_RED:
      xenosState->blendRed = tmp;
      xenosState->WriteRegister(reg, xenosState->blendRed);
      break;
    case XeRegister::RB_BLEND_GREEN:
      xenosState->blendGreen = tmp;
      xenosState->WriteRegister(reg, xenosState->blendGreen);
      break;
    case XeRegister::RB_BLEND_BLUE:
      xenosState->blendBlue = tmp;
      xenosState->WriteRegister(reg, xenosState->blendBlue);
      break;
    case XeRegister::RB_BLEND_ALPHA:
      xenosState->blendAlpha = tmp;
      xenosState->WriteRegister(reg, xenosState->blendAlpha);
      break;
    case XeRegister::PA_CL_VTE_CNTL:
      xenosState->viewportControl = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportControl);
      break;
    case XeRegister::PA_SC_WINDOW_OFFSET:
      xenosState->windowOffset = tmp;
      xenosState->WriteRegister(reg, xenosState->windowOffset);
      break;
    case XeRegister::PA_SC_WINDOW_SCISSOR_TL:
      xenosState->windowScissorTl = tmp;
      xenosState->WriteRegister(reg, xenosState->windowScissorTl);
      break;
    case XeRegister::PA_SC_WINDOW_SCISSOR_BR:
      xenosState->windowScissorBr = tmp;
      xenosState->WriteRegister(reg, xenosState->windowScissorBr);
      break;
    case XeRegister::PA_CL_VPORT_XOFFSET:
      xenosState->viewportXOffset = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportXOffset);
      break;
    case XeRegister::PA_CL_VPORT_YOFFSET:
      xenosState->viewportYOffset = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportYOffset);
      break;
    case XeRegister::PA_CL_VPORT_ZOFFSET:
      xenosState->viewportZOffset = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportZOffset);
      break;
    case XeRegister::PA_CL_VPORT_XSCALE:
      xenosState->viewportXScale = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportXScale);
      break;
    case XeRegister::PA_CL_VPORT_YSCALE:
      xenosState->viewportYScale = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportYScale);
      break;
    case XeRegister::PA_CL_VPORT_ZSCALE:
      xenosState->viewportZScale = tmp;
      xenosState->WriteRegister(reg, xenosState->viewportZScale);
      break;
    case XeRegister::RB_STENCILREFMASK:
      xenosState->stencilReferenceMask = tmp;
      xenosState->WriteRegister(reg, xenosState->stencilReferenceMask);
      break;
    case XeRegister::RB_DEPTHCONTROL:
      xenosState->depthControl = tmp;
      xenosState->WriteRegister(reg, xenosState->depthControl);
      break;
    case XeRegister::RB_BLENDCONTROL0:
      xenosState->blendControl0 = tmp;
      xenosState->WriteRegister(reg, xenosState->blendControl0);
      break;
    case XeRegister::RB_TILECONTROL:
      xenosState->tileControl = tmp;
      xenosState->WriteRegister(reg, xenosState->tileControl);
      break;
    case XeRegister::RB_MODECONTROL:
      xenosState->modeControl = tmp;
      xenosState->WriteRegister(reg, xenosState->modeControl);
      break;
    case XeRegister::RB_BLENDCONTROL1:
      xenosState->blendControl1 = tmp;
      xenosState->WriteRegister(reg, xenosState->blendControl1);
      break;
    case XeRegister::RB_BLENDCONTROL2:
      xenosState->blendControl2 = tmp;
      xenosState->WriteRegister(reg, xenosState->blendControl2);
      break;
    case XeRegister::RB_BLENDCONTROL3:
      xenosState->blendControl3 = tmp;
      xenosState->WriteRegister(reg, xenosState->blendControl3);
      break;
    case XeRegister::RB_COPY_CONTROL:
      xenosState->copyControl = tmp;
      xenosState->WriteRegister(reg, xenosState->copyControl);
      break;
    case XeRegister::RB_DEPTH_CLEAR:
      xenosState->depthClear = tmp;
      xenosState->WriteRegister(reg, xenosState->depthClear);
      break;
    case XeRegister::RB_COLOR_CLEAR: {
      xenosState->clearColor = tmp;
      u8 a = (xenosState->clearColor >> 24) & 0xFF;
      u8 r = (xenosState->clearColor >> 16) & 0xFF;
      u8 g = (xenosState->clearColor >> 8) & 0xFF;
      u8 b = (xenosState->clearColor >> 0) & 0xFF;
      Xe_Main->renderer->UpdateClearColor(r, g, b, a);
      xenosState->WriteRegister(reg, xenosState->clearColor);
    } break;
    case XeRegister::RB_COLOR_CLEAR_LO:
      xenosState->clearColorLo = tmp;
      xenosState->WriteRegister(reg, xenosState->clearColorLo);
      break;
    default:
      xenosState->WriteRegister(reg, tmp);
      break;
    }
    return true;
  }

  return false;
}

bool Xe::Xenos::XGPU::MemSet(u64 writeAddress, s32 data, u64 size) {
  std::lock_guard lck(mutex);
  if (isAddressMappedInBAR(static_cast<u32>(writeAddress))) {
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
    f.write(reinterpret_cast<const char*>(ramPtr->getPointerToAddress(xenosState->fbSurfaceAddress)), pitch);
    LOG_INFO(Xenos, "Framebuffer dumped to '{}'", path.string());
  }
  f.close();
}
