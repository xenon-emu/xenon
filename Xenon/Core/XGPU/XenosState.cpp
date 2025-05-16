// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Config.h"

#include "CommandProcessor.h"
#include "XenosState.h"
#include "XenosRegisters.h"

Xe::XGPU::XenosState::XenosState(RAM *ram, EDRAM *edramPtr, CommandProcessor *commandProcessorPtr) :
  ramPtr(ram), edram(edramPtr),
  commandProcessor(commandProcessorPtr),
#ifndef TOOL
  internalWidth(Config::xgpu.internal.width),
  internalHeight(Config::xgpu.internal.height)
#else
  internalWidth(1280),
  internalHeight(720)
#endif
{
  Regs = std::make_unique<STRIP_UNIQUE_ARR(Regs)>(0xFFFFF);
  memset(Regs.get(), 0, 0xFFFFF);
  memset(RegMask, 0, sizeof(RegMask));
}

Xe::XGPU::XenosState::~XenosState() {
  Regs.reset();
}

u32 Xe::XGPU::XenosState::ReadRawRegister(u32 addr, u32 size) {
  // Set a lock
  std::lock_guard lck(mutex);
  // Define register values
  u32 regIndex = addr / 4;
  XeRegister reg = static_cast<XeRegister>(regIndex);
  // Read value
  u32 tmp = 0;
  memcpy(&tmp, &Regs[addr], sizeof(tmp));
  // Swap value
  u32 value = byteswap_be(tmp);
  // Switch for properly return the requested amount of data
  switch (size) {
  case 2:
    value >>= 16;
    break;
  case 1:
    value >>= 24;
    break;
  default:
    break;
  }
  switch (reg) {
  // VdpHasWarmBooted expects this to be 0x10, otherwise, it waits until the GPU has intialised
  case XeRegister::CONFIG_CNTL:
    value = configControl;
    break;
  case XeRegister::RBBM_CNTL:
    value = rbbmControl;
    break;
  case XeRegister::RBBM_SOFT_RESET:
    value = rbbmSoftReset;
    if (rbbmSoftReset == 0) {
      // Reset RBBM
      if ((rbbmStatus & 0x600)) {
        rbbmStatus &= ~(0x600);
      }
      if ((rbbmStatus & 0x400)) {
        rbbmStatus &= ~(0x400);
      }
      // GPU is partly reset, set status to 0x80 (just instantly reset for now)
      rbbmStatus |= 0x80;
      // Set as warm boot
      configControl = 0x10000000;
    }
    break;
  case XeRegister::CP_ME_RAM_DATA:
    value = commandProcessor->CPReadMicrocodeData(Xe::XGPU::eCPMicrocodeType::uCodeTypeME);
    break;
    // Gets past VdInitializeEngines+0x58
  case XeRegister::RBBM_DEBUG:
    value = rbbmDebug;
    break;
  case XeRegister::CP_PFP_UCODE_DATA:
    value = commandProcessor->CPReadMicrocodeData(Xe::XGPU::eCPMicrocodeType::uCodeTypePFP);
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
    const u32 scratchRegIndex = regIndex - static_cast<u32>(XeRegister::SCRATCH_REG0);
    value = scratch[scratchRegIndex];
  } break;
  case XeRegister::RBBM_STATUS:
    if (!(rbbmStatus & 0x400)) {
      rbbmStatus |= 0x400;
    }
    if (!(rbbmStatus & 0x600)) {
      rbbmStatus |= 0x600;
    }
    value = rbbmStatus;
    break;
  case XeRegister::MH_STATUS:
    if (!(mhStatus & 0x2000000)) {
      mhStatus |= 0x2000000;
    }
    value = mhStatus;
    break;
  case XeRegister::COHER_STATUS_HOST:
    if ((coherencyStatusHost & 0x80000000ul)) {
      coherencyStatusHost &= ~0x80000000ul;
      LOG_DEBUG(Xenos, "[Xe] Flushing 0x{:X} with a size of 0x{:X}", coherencyBaseHost, coherencySizeHost);
      ClearDirtyState();
    }
    value = coherencyStatusHost;
    break;
  case XeRegister::COHER_SIZE_HOST:
    value = coherencySizeHost;
    break;
  case XeRegister::COHER_BASE_HOST:
    value = coherencyBaseHost;
    break;
  case XeRegister::RB_EDRAM_TIMING:
    value = edramTiming;
    break;
  case XeRegister::RB_EDRAM_INFO:
    value = edramInfo;
    break;
  case XeRegister::D1CRTC_CONTROL:
    value = crtcControl;
    break;
  case XeRegister::DC_LUT_AUTOFILL:
    if (dcLutAutofill == 0x1) {
      dcLutAutofill = 0x2000000;
    }
    value = dcLutAutofill;
    break;
  case XeRegister::D1MODE_V_COUNTER:
    value = vCounter;
    break;
  case XeRegister::D1MODE_VBLANK_VLINE_STATUS:
    value = vblankVlineStatus;
    break;
  case XeRegister::D1MODE_VIEWPORT_SIZE:
    value = modeViewportSize;
    break;
  case XeRegister::XDVO_ENABLE:
    value = xdvoEnable;
    break;
  case XeRegister::XDVO_BIT_DEPTH_CONTROL:
    value = xdvoBitDepthControl;
    break;
  case XeRegister::XDVO_CLOCK_INV:
    value = xdvoClockInv;
    break;
  case XeRegister::XDVO_CONTROL:
    value = xdvoControl;
    break;
  case XeRegister::XDVO_CRC_EN:
    value = xdvoCrcEnable;
    break;
  case XeRegister::XDVO_CRC_CNTL:
    value = xdvoCrcControl;
    break;
  case XeRegister::XDVO_CRC_MASK_SIG_RGB:
    value = xdvoCrcMaskSignalRGB;
    break;
  case XeRegister::XDVO_CRC_MASK_SIG_CNTL:
    value = xdvoCrcMaskSignalControl;
    break;
  case XeRegister::XDVO_CRC_SIG_RGB:
    value = xdvoCrcSignalRGB;
    break;
  case XeRegister::XDVO_CRC_SIG_CNTL:
    value = xdvoCrcSignalControl;
    break;
  case XeRegister::XDVO_STRENGTH_CONTROL:
    value = xdvoStrengthControl;
    break;
  case XeRegister::XDVO_DATA_STRENGTH_CONTROL:
    value = xdvoDataStrengthControl;
    break;
  case XeRegister::XDVO_FORCE_OUTPUT_CNTL:
    value = xdvoForceOutputControl;
    break;
  case XeRegister::XDVO_REGISTER_INDEX:
    value = xdvoRegisterIndex;
    break;
  case XeRegister::XDVO_REGISTER_DATA:
    value = xdvoRegisterData;
    break;
  case XeRegister::RB_SURFACE_INFO:
    value = surfaceInfo;
    break;
  case XeRegister::RB_COLOR_INFO:
    value = colorInfo;
    break;
  case XeRegister::RB_DEPTH_INFO:
    value = depthInfo;
    break;
  case XeRegister::RB_COLOR1_INFO:
    value = color1Info;
    break;
  case XeRegister::RB_COLOR2_INFO:
    value = color2Info;
    break;
  case XeRegister::RB_COLOR3_INFO:
    value = color3Info;
    break;
  case XeRegister::RB_BLEND_RED:
    value = blendRed;
    break;
  case XeRegister::RB_BLEND_GREEN:
    value = blendGreen;
    break;
  case XeRegister::RB_BLEND_BLUE:
    value = blendBlue;
    break;
  case XeRegister::RB_BLEND_ALPHA:
    value = blendAlpha;
    break;
  case XeRegister::PA_CL_VTE_CNTL:
    value = viewportControl;
    break;
  case XeRegister::PA_SC_WINDOW_OFFSET:
    value = windowOffset;
    break;
  case XeRegister::PA_SC_WINDOW_SCISSOR_TL:
    value = windowScissorTl;
    break;
  case XeRegister::PA_SC_WINDOW_SCISSOR_BR:
    value = windowScissorBr;
    break;
  case XeRegister::PA_CL_VPORT_XOFFSET:
    value = viewportXOffset;
    break;
  case XeRegister::PA_CL_VPORT_YOFFSET:
    value = viewportYOffset;
    break;
  case XeRegister::PA_CL_VPORT_ZOFFSET:
    value = viewportZOffset;
    break;
  case XeRegister::PA_CL_VPORT_XSCALE:
    value = viewportXScale;
    break;
  case XeRegister::PA_CL_VPORT_YSCALE:
    value = viewportYScale;
    break;
  case XeRegister::PA_CL_VPORT_ZSCALE:
    value = viewportZScale;
    break;
  case XeRegister::RB_STENCILREFMASK:
    value = stencilReferenceMask;
    break;
  case XeRegister::RB_DEPTHCONTROL:
    value = depthControl;
    break;
  case XeRegister::RB_BLENDCONTROL0:
    value = blendControl0;
    break;
  case XeRegister::RB_TILECONTROL:
    value = tileControl;
    break;
  case XeRegister::RB_MODECONTROL:
    value = modeControl;
    break;
  case XeRegister::RB_BLENDCONTROL1:
    value = blendControl1;
    break;
  case XeRegister::RB_BLENDCONTROL2:
    value = blendControl2;
    break;
  case XeRegister::RB_BLENDCONTROL3:
    value = blendControl3;
    break;
  case XeRegister::RB_COPY_CONTROL:
    value = copyControl;
    break;
  case XeRegister::RB_COPY_DEST_BASE:
    value = copyDestBase;
    break;
  case XeRegister::RB_COPY_DEST_PITCH:
    value = copyDestPitch;
    break;
  case XeRegister::RB_COPY_DEST_INFO:
    value = copyDestInfo;
    break;
  case XeRegister::RB_DEPTH_CLEAR:
    value = depthClear;
    break;
  case XeRegister::RB_COLOR_CLEAR:
    value = clearColor;
    break;
  case XeRegister::RB_COLOR_CLEAR_LO:
    value = clearColorLo;
    break;
  case XeRegister::RB_COPY_FUNC:
    value = copyFunction;
    break;
  case XeRegister::RB_COPY_REF:
    value = copyReference;
    break;
  case XeRegister::RB_COPY_MASK:
    value = copyMask;
    break;
  case XeRegister::RB_SIDEBAND_BUSY:
    // Checks if the EDRAM is currently busy doing work.
    value = edram->isEdramBusy() ? 1 : 0;
    break;
  case XeRegister::RB_SIDEBAND_DATA:
    value = edram->ReadReg();
    break;
  default:
    break;
  }
  return value;
}

void Xe::XGPU::XenosState::WriteRawRegister(u32 addr, u32 value) {
  // Set a lock
  std::lock_guard lck(mutex);
  // Define register values
  u32 regIndex = addr / 4;
  XeRegister reg = static_cast<XeRegister>(regIndex);
  // Swap value
  u32 tmp = value;
  value = byteswap_be(value);
  bool useSwapped = true;
  switch (reg) {
  // VdpHasWarmBooted expects this to be 0x10, otherwise, it waits until the GPU has intialised
  case XeRegister::CONFIG_CNTL:
    configControl = value;
    break;
  case XeRegister::RBBM_CNTL:
    useSwapped = false;
    rbbmControl = tmp;
    break;
  case XeRegister::RBBM_SOFT_RESET:
    rbbmSoftReset = value;
    break;
  case XeRegister::CP_RB_BASE:
    commandProcessor->CPUpdateRBBase(value);
    break;
  case XeRegister::CP_RB_CNTL:
    commandProcessor->CPUpdateRBSize(value);
    break;
  case XeRegister::CP_RB_WPTR:
    commandProcessor->CPUpdateRBWritePointer(value);
    break;
  case XeRegister::SCRATCH_UMSK:
    scratchMask = value;
    break;
  case XeRegister::SCRATCH_ADDR:
    scratchAddr = value;
    break;
  case XeRegister::CP_ME_RAM_WADDR:
    // Software is writing CP Microcode Engine uCode write address.
    commandProcessor->CPSetMEMicrocodeWriteAddress(value);
    break;
  case XeRegister::CP_ME_RAM_RADDR:
    // Software is writing CP Microcode Engine uCode read address.
    commandProcessor->CPSetMEMicrocodeReadAddress(value);
    break;
  case XeRegister::CP_ME_RAM_DATA:
    // Software is writing CP Microcode Engine uCode data.
    commandProcessor->CPWriteMicrocodeData(Xe::XGPU::eCPMicrocodeType::uCodeTypeME, value);
    break;
  case XeRegister::RBBM_DEBUG:
    rbbmDebug = value;
    break;
  case XeRegister::CP_PFP_UCODE_ADDR:
    // Software is writing CP PFP uCode data address.
    commandProcessor->CPSetPFPMicrocodeAddress(value);
    break;
  case XeRegister::CP_PFP_UCODE_DATA:
    // Software is writing CP PFP uCode data.
    commandProcessor->CPWriteMicrocodeData(Xe::XGPU::eCPMicrocodeType::uCodeTypePFP, value);
    break;
  case XeRegister::SCRATCH_REG0:
  case XeRegister::SCRATCH_REG1:
  case XeRegister::SCRATCH_REG2:
  case XeRegister::SCRATCH_REG3:
  case XeRegister::SCRATCH_REG4:
  case XeRegister::SCRATCH_REG5:
  case XeRegister::SCRATCH_REG6:
  case XeRegister::SCRATCH_REG7: {
    useSwapped = false;
    // Write the initial value
    const u32 scratchRegIndex = regIndex - static_cast<u32>(XeRegister::SCRATCH_REG0);
    scratch[scratchRegIndex] = tmp;
    // Check if writing is enabled
    if ((1 << scratchRegIndex) & scratchMask) {
      // Writeback
      const u32 memAddr = scratchAddr + (scratchRegIndex * 4);
      LOG_DEBUG(Xenos, "[CP] Scratch {} was accessed, writing back to 0x{:X} with 0x{:X}", scratchRegIndex, memAddr, tmp);
      u8 *memPtr = ramPtr->getPointerToAddress(memAddr);
      memcpy(memPtr, &scratch[scratchRegIndex], sizeof(scratch[scratchRegIndex]));
    }
  } break;
  case XeRegister::MH_STATUS:
    mhStatus = value;
    if (!(mhStatus & 0x2000000)) {
      mhStatus |= 0x2000000;
    }
    value = mhStatus;
    break;
  case XeRegister::COHER_STATUS_HOST:
    coherencyStatusHost = value;
    if (!(coherencyStatusHost & 0x80000000ul)) {
      coherencyStatusHost |= 0x80000000ul;
    }
    value = coherencyStatusHost;
    break;
  case XeRegister::COHER_SIZE_HOST:
    coherencySizeHost = value;
    break;
  case XeRegister::COHER_BASE_HOST:
    coherencyBaseHost = value;
    break;
  case XeRegister::RB_EDRAM_TIMING:
    edramTiming = value;
    if ((edramTiming & 0x60000) < 0x400) {
      edramTiming |= 0x60000;
    }
    value = edramTiming;
    break;
  case XeRegister::RB_EDRAM_INFO:
    edramInfo = value;
    break;
  case XeRegister::D1CRTC_CONTROL:
    crtcControl = value;
    break;
  case XeRegister::D1GRPH_PRIMARY_SURFACE_ADDRESS:
    fbSurfaceAddress = value;
    break;
  case XeRegister::D1GRPH_X_END:
    internalWidth = value;
    break;
  case XeRegister::D1GRPH_Y_END:
    internalHeight = value;
    break;
  case XeRegister::DC_LUT_AUTOFILL:
    dcLutAutofill = value;
    break;
  case XeRegister::D1MODE_V_COUNTER:
    vCounter = value;
    break;
  case XeRegister::D1MODE_VBLANK_VLINE_STATUS:
    vblankVlineStatus = value;
    break;
  case XeRegister::D1MODE_VIEWPORT_SIZE:
    modeViewportSize = value;
    break;
  case XeRegister::XDVO_ENABLE:
    xdvoEnable = value;
    break;
  case XeRegister::XDVO_BIT_DEPTH_CONTROL:
    xdvoBitDepthControl = value;
    break;
  case XeRegister::XDVO_CLOCK_INV:
    xdvoClockInv = value;
    break;
  case XeRegister::XDVO_CONTROL:
    xdvoControl = value;
    break;
  case XeRegister::XDVO_CRC_EN:
    xdvoCrcEnable = value;
    break;
  case XeRegister::XDVO_CRC_CNTL:
    xdvoCrcControl = value;
    break;
  case XeRegister::XDVO_CRC_MASK_SIG_RGB:
    xdvoCrcMaskSignalRGB = value;
    break;
  case XeRegister::XDVO_CRC_MASK_SIG_CNTL:
    xdvoCrcMaskSignalControl = value;
    break;
  case XeRegister::XDVO_CRC_SIG_RGB:
    xdvoCrcSignalRGB = value;
    break;
  case XeRegister::XDVO_CRC_SIG_CNTL:
    xdvoCrcSignalControl = value;
    break;
  case XeRegister::XDVO_STRENGTH_CONTROL:
    xdvoStrengthControl = value;
    break;
  case XeRegister::XDVO_DATA_STRENGTH_CONTROL:
    xdvoDataStrengthControl = value;
    break;
  case XeRegister::XDVO_FORCE_OUTPUT_CNTL:
    xdvoForceOutputControl = value;
    break;
  case XeRegister::XDVO_REGISTER_INDEX:
    xdvoRegisterIndex = value;
    break;
  case XeRegister::XDVO_REGISTER_DATA:
    xdvoRegisterData = value;
    break;
  case XeRegister::RB_SURFACE_INFO:
    surfaceInfo = value;
    break;
  case XeRegister::RB_COLOR_INFO:
    colorInfo = value;
    break;
  case XeRegister::RB_DEPTH_INFO:
    depthInfo = value;
    break;
  case XeRegister::RB_COLOR1_INFO:
    color1Info = value;
    break;
  case XeRegister::RB_COLOR2_INFO:
    color2Info = value;
    break;
  case XeRegister::RB_COLOR3_INFO:
    color3Info = value;
    break;
  case XeRegister::RB_BLEND_RED:
    blendRed = value;
    break;
  case XeRegister::RB_BLEND_GREEN:
    blendGreen = value;
    break;
  case XeRegister::RB_BLEND_BLUE:
    blendBlue = value;
    break;
  case XeRegister::RB_BLEND_ALPHA:
    blendAlpha = value;
    break;
  case XeRegister::PA_SC_WINDOW_OFFSET:
    windowOffset = value;
    break;
  case XeRegister::PA_SC_WINDOW_SCISSOR_TL:
    windowScissorTl = value;
    break;
  case XeRegister::PA_SC_WINDOW_SCISSOR_BR:
    windowScissorBr = value;
    break;
  case XeRegister::PA_CL_VPORT_XOFFSET:
    viewportXOffset = value;
    break;
  case XeRegister::PA_CL_VPORT_YOFFSET:
    viewportYOffset = value;
    break;
  case XeRegister::PA_CL_VPORT_ZOFFSET:
    viewportZOffset = value;
    break;
  case XeRegister::PA_CL_VPORT_XSCALE:
    viewportXScale = value;
    break;
  case XeRegister::PA_CL_VPORT_YSCALE:
    viewportYScale = value;
    break;
  case XeRegister::PA_CL_VPORT_ZSCALE:
    viewportZScale = value;
    break;
  case XeRegister::RB_STENCILREFMASK:
    stencilReferenceMask = value;
    break;
  case XeRegister::RB_DEPTHCONTROL:
    depthControl = value;
    break;
  case XeRegister::RB_BLENDCONTROL0:
    blendControl0 = value;
    break;
  case XeRegister::RB_TILECONTROL:
    tileControl = value;
    break;
  case XeRegister::PA_CL_VTE_CNTL:
    viewportControl = value;
    break;
  case XeRegister::RB_MODECONTROL:
    modeControl = value;
    break;
  case XeRegister::RB_BLENDCONTROL1:
    blendControl1 = value;
    break;
  case XeRegister::RB_BLENDCONTROL2:
    blendControl2 = value;
    break;
  case XeRegister::RB_BLENDCONTROL3:
    blendControl3 = value;
    break;
  case XeRegister::RB_COPY_CONTROL:
    copyControl = value;
    break;
  case XeRegister::RB_COPY_DEST_BASE:
    copyDestBase = value;
    break;
  case XeRegister::RB_COPY_DEST_PITCH:
    copyDestPitch = value;
    break;
  case XeRegister::RB_COPY_DEST_INFO:
    copyDestInfo = value;
    break;
  case XeRegister::RB_DEPTH_CLEAR:
    depthClear = value;
    break;
  case XeRegister::RB_COLOR_CLEAR:
    clearColor = value;
    break;
  case XeRegister::RB_COLOR_CLEAR_LO:
    clearColorLo = value;
    break;
  case XeRegister::RB_COPY_FUNC:
    copyFunction = value;
    break;
  case XeRegister::RB_COPY_REF:
    copyReference = value;
    break;
  case XeRegister::RB_COPY_MASK:
    copyMask = value;
    break;
  case XeRegister::RB_SIDEBAND_RD_ADDR:
    // Software is writing the address (index) of the edram reg it wants to write.
    edram->SetRWRegIndex(Xe::XGPU::eRegIndexType::readIndex, value);
    break;
  case XeRegister::RB_SIDEBAND_WR_ADDR:
    // Software is writing the address (index) of the edram reg it wants to read.
    edram->SetRWRegIndex(Xe::XGPU::eRegIndexType::writeIndex, value);
    break;
  case XeRegister::RB_SIDEBAND_DATA:
    useSwapped = false;
    // Software is writing the data of the edram reg previously specified.
    edram->WriteReg(tmp); // NOTE: We want data to not be byteswapped.
    break;
  default:
    // Do nothing here, just continue to write
    break;
  }
  // Write to register array
  if (useSwapped) {
    memcpy(&Regs[addr], &value, sizeof(value));
  } else {
    memcpy(&Regs[addr], &tmp, sizeof(tmp));
  }
  // Set dirty state
  const u64 mask = 1ull << ((addr * 4) % BitCount);
  RegMask[addr / BitCount] |= mask;
}
