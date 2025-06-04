// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "Core/RAM/RAM.h"

#include "EDRAM.h"
#include "ShaderConstants.h"
#include "Xenos.h"

enum class XeRegister;

struct XeShaderFloatConsts {
  f32 values[256 * 4 * 2] = {};
};

struct XeShaderBoolConsts {
  u32 values[8 * 4] = {};
};

namespace Xe::XGPU {

#define XE_FB_BASE 0x1E000000

class CommandProcessor;

class XenosState {
public:
  XenosState() = default;
  XenosState(RAM *ram, EDRAM *edramPtr, CommandProcessor *commandProcessorPtr);

  ~XenosState();

  void WriteRawRegister(u32 addr, u32 value);

  u32 ReadRawRegister(u32 addr, u32 size = sizeof(u32));

  void WriteRegister(XeRegister reg, u32 value) {
    std::lock_guard lck(mutex);
    // Write value
    return WriteRawRegister(static_cast<u32>(reg) * 4, value);
  }

  u32 ReadRegister(XeRegister reg, u32 size = sizeof(u32)) {
    std::lock_guard lck(mutex);
    // Read value
    return ReadRawRegister(static_cast<u32>(reg) * 4, size);
  }

  u8* GetRegisterPointer(XeRegister reg) {
    std::lock_guard lck(mutex);
    return &Regs[static_cast<u32>(reg) * 4];
  }

  bool RegisterDirty(XeRegister reg) {
    std::lock_guard lck(mutex);
    u64 index = static_cast<u32>(reg) * 4;
    const u64 mask = 1ull << (index % BitCount);
    return (RegMask[index / BitCount] & mask) != 0;
  }

  void ClearDirtyState() {
    std::lock_guard lck(mutex);
    memset(RegMask, 0, sizeof(RegMask));
  }

  void SetDirtyState() {
    std::lock_guard lck(mutex);
    memset(RegMask, 0xFF, sizeof(RegMask));
  }

  u64 GetDirtyBlock(const u32 firstIndex) {
    std::lock_guard lck(mutex);
    return RegMask[firstIndex / BitCount];
  }

  // Mutex
  std::recursive_mutex mutex{};

  // RAM Pointer
  RAM *ramPtr = nullptr;

  // CP Pointer
  CommandProcessor *commandProcessor = nullptr;

  // Primary surface
  u32 fbSurfaceAddress = XE_FB_BASE;
  bool framebufferDisable = false;

  // Config
  u32 configControl = 0x00; //0x10000000 - Set to always be warm boot, but we never setup our states so we should always cold boot

  // Scratch
  u32 scratchMask = 0;
  u32 scratchAddr = 0;
  u32 scratch[8] = {};

  u32 waitUntil = 0;

  // RBBM
  u32 rbbmControl = 0;
  u32 rbbmDebug = 0xF0000;
  u32 rbbmStatus = 0;
  u32 rbbmSoftReset = 0;

  // RB
  RB_SURFACE_INFO_REG surfaceInfo = {};
  u32 colorInfo = 0;
  u32 depthInfo = 0;
  u32 color1Info = 0;
  u32 color2Info = 0;
  u32 color3Info = 0;
  u32 blendRed = 0;
  u32 blendGreen = 0;
  u32 blendBlue = 0;
  u32 blendAlpha = 0;
  u32 stencilReferenceMask = 0;
  u32 depthControl = 0;
  u32 blendControl0 = 0;
  u32 tileControl = 0;
  RB_MODECONTROL_REG modeControl = {};
  u32 blendControl1 = 0;
  u32 blendControl2 = 0;
  u32 blendControl3 = 0;
  RB_COPY_CONTROL_REG copyControl = {};
  u32 copyDestBase = 0;
  RB_COPY_DEST_PITCH_REG copyDestPitch = {};
  RB_COPY_DEST_INFO_REG copyDestInfo = {};
  u32 depthClear = 0;
  u32 clearColor = 0;
  u32 clearColorLo = 0;
  u32 copyFunction = 0;
  u32 copyReference = 0;
  u32 copyMask = 0;

  // VGT
  u32 maxVertexIndex = 0;
  u32 minVertexIndex = 0;
  u32 indexOffset = 0;
  u32 multiPrimitiveIndexBufferResetIndex = 0;
  u32 currentBinIdMin = 0;

  VertexFetchData vertexData{};
  VGT_DRAW_INITIATOR_REG vgtDrawInitiator = {};
  u32 vgtDMABase = 0;
  VGT_DMA_SIZE_REG vgtDMASize = {};

  // PA
  u32 viewportControl = 0;
  u32 windowOffset = 0;
  u32 windowScissorTl = 0;
  u32 windowScissorBr = 0;
  u32 viewportXOffset = 0;
  u32 viewportYOffset = 0;
  u32 viewportZOffset = 0;
  u32 viewportXScale = 0;
  u32 viewportYScale = 0;
  u32 viewportZScale = 0;

  // SQ
  u32 programCntl = 0;

  // D1CRTC
  u32 crtcControl = 0; // Checks if the RTC is active

  // D1MODE
  u32 modeViewportSize = 0x050002D0;
  u32 vCounter = 720;
  u32 vblankStatus = 0x1000;
  u32 vblankVlineStatus = 1;

  // MH
  u32 mhStatus = 0;

  // Coherency
  u32 coherencySizeHost = 0;
  u32 coherencyBaseHost = 0;
  COHER_STATUS_HOST_REG coherencyStatusHost = {};

  // EDRAM
  EDRAM *edram = nullptr;
  u32 edramTiming = 0;
  u32 edramInfo = 0;

  // DC
  u32 dcLutAutofill = 0x2000000;

  // XDVO
  u32 xdvoEnable = 1;
  u32 xdvoBitDepthControl = 0;
  u32 xdvoClockInv = 0;
  u32 xdvoControl = 0;
  u32 xdvoCrcEnable = 1;
  u32 xdvoCrcControl = 0;
  u32 xdvoCrcMaskSignalRGB = 0;
  u32 xdvoCrcMaskSignalControl = 0;
  u32 xdvoCrcSignalRGB = 0;
  u32 xdvoCrcSignalControl = 0;
  u32 xdvoStrengthControl = 0;
  u32 xdvoDataStrengthControl = 0;
  u32 xdvoForceOutputControl = 1;
  u32 xdvoRegisterIndex = 0;
  u32 xdvoRegisterData = 0;

  // Internal rendering width/height
  u32 internalWidth = 1280;
  u32 internalHeight = 720;

  // Internal buffers
  XeShaderFloatConsts floatConsts = {};
  XeShaderBoolConsts boolConsts = {};

  // Registers
  std::unique_ptr<u8[]> Regs;
  static constexpr u32 NumRegs = 0x5004;
  static constexpr u32 BitCount = sizeof(u64) * 8;
  static constexpr u32 BlockCount = (NumRegs + BitCount - 1) / BitCount;
  u64 RegMask[BlockCount] = {};
};

} // namespace Xe::XGPU
