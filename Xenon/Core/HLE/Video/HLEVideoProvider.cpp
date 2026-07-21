/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include <bit>

#include "HLEVideoProvider.h"

#include "Base/Logging/Log.h"
#include "Core/HLE/HLEFunctionManager.h"
#include "Core/XCPU/Interpreter/PPCInterpreter.h"

namespace Xe::Core::HLE {

bool HLEVideoProvider::Setup() {
  LOG_INFO(HLE, "[Video]: Video HLE provider initialized.");
  return true;
}

void HLEVideoProvider::CollectFunctions(std::vector<sHLEFunction> &table) {
  // xboxkrnl ordinals for video functions (taken from xboxkrnl 2.0.17489 dev)
  constexpr u32 ordVdRetrainEDRAM = 617;
  constexpr u32 ordVdIsHSIOTrainingSucceeded = 454;
  constexpr u32 ordVdSwap = 603;
  constexpr u32 ordVdQueryVideoMode = 458;

  table.emplace_back(sHLEFunction{ true, ordVdRetrainEDRAM, eSystemExecutables::xboxkrnl, 0x800FC218, "VdRetrainEDRAM",
    reinterpret_cast<HLEFunctionPtr>(HLE_VdRetrainEDRAM) });
  table.emplace_back(sHLEFunction{ true, ordVdIsHSIOTrainingSucceeded, eSystemExecutables::xboxkrnl, 0x800F9128, "VdIsHSIOTrainingSucceeded",
    reinterpret_cast<HLEFunctionPtr>(HLE_VdIsHSIOTrainingSucceeded) });
  //table.emplace_back(sHLEFunction{ true, ordVdSwap, eSystemExecutables::xboxkrnl, 0x800F8E20, "VdSwap",
  //  reinterpret_cast<HLEFunctionPtr>(HLE_VdSwap) });
  //table.emplace_back(sHLEFunction{ true, ordVdQueryVideoMode, eSystemExecutables::xboxkrnl, 0x800F6E80, "VdQueryVideoMode",
  //  reinterpret_cast<HLEFunctionPtr>(HLE_VdQueryVideoMode) });

  LOG_INFO(HLE, "[Video]: Registered {} video HLE functions.", 4);
}

// VdRetrainEDRAM
// Returns 0, STATUS_SUCCESS
void HLEVideoProvider::HLE_VdRetrainEDRAM(sPPEState *ppeState) {
  sPPUThread &thread = ppeState->ppuThread[curThreadId];
  thread.GPR[3] = 0;
}

// VdIsHSIOTrainingSucceeded
// Returns 1 - HSIO training always succeeds
void HLEVideoProvider::HLE_VdIsHSIOTrainingSucceeded(sPPEState *ppeState) {
  sPPUThread &thread = ppeState->ppuThread[curThreadId];
  thread.GPR[3] = 1;
}

// VdSwap
void HLEVideoProvider::HLE_VdSwap(sPPEState *ppeState) {
  // Get current thread
  sPPUThread &thread = ppeState->ppuThread[curThreadId];

  //namespace xenos = xe::gpu::xenos;
  // Get GPU Fetch RAM pointer
  u8 *gpuFetchPtr = PPCInterpreter::MMUGetPointerFromRAM(thread.GPR[4]);
  if (gpuFetchPtr == nullptr) {
    return;
  }

  //xenos::xe_gpu_texture_fetch_t gpu_fetch;
  //xe::copy_and_swap_32_unaligned(&gpu_fetch, reinterpret_cast<uint32_t *>(gpuFetchPtr), 6);

  // Mask to GPU address space and add 4Kb page due to a design bug in the system
  //gpu_fetch.base_address = (gpu_fetch.base_address & 0x1FFFF) + 0x1;

  //u32 width = gpu_fetch.size_2d.width + 1;
  //u32 height = gpu_fetch.size_2d.height + 1;

  // Get ringbuffer pointer
  u32 *ringbufferPtr = reinterpret_cast<u32 *>(PPCInterpreter::MMUGetPointerFromRAM(thread.GPR[3]));
  if (ringbufferPtr == nullptr) {
    return;
  }

  u32 bufferOffset = 0;
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(xenos::MakePacketType0(0x4800, 6));
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.dword_0);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.dword_1);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.dword_2);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.dword_3);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.dword_4);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.dword_5);
  //
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(xenos::MakePacketType3(xenos::PM4_XE_SWAP, 4));
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(xe::gpu::xenos::kSwapSignature);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(gpu_fetch.base_address << 12);
  //
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(width);
  //ringbufferPtr[bufferOffset++] = byteswap_be<u32>(height);

  // Fill the rest of the buffer with NOP packets.
  for (uint32_t i = bufferOffset; i < 64; i++) {
    //ringbufferPtr[i] = byteswap_be<u32>(xenos::MakePacketType2());
  }

  // Advance ringbuffer pointer
  thread.GPR[3] += bufferOffset * 4;
}

// VdQueryVideoMode
// r3 = pointer to an X_VIDEO_MODE structure (0x30 / 48 bytes) to fill.
// We populate a default 1280x720 progressive 60Hz HDMI video mode.
void HLEVideoProvider::HLE_VdQueryVideoMode(sPPEState *ppeState) {
  sPPUThread &thread = ppeState->ppuThread[curThreadId];
  u64 videoModePtr = thread.GPR[3];

  if (!videoModePtr)
    return;

  u8 *ramPtr = PPCInterpreter::MMUGetPointerFromRAM(videoModePtr);
  if (ramPtr == nullptr)
    return;

  LOG_INFO(HLE, "[Video]: VdQueryVideoMode(ptr=0x{:08X}, mem ptr={:#X}).", static_cast<u32>(videoModePtr), (u64)ramPtr);

  // XVIDEO_MODE structure layout (all fields big-endian):
  // Offset 0x00: DisplayWidth       (1280)
  // Offset 0x04: DisplayHeight      (720)
  // Offset 0x08: IsInterlaced       (0 = progressive)
  // Offset 0x0C: IsWidescreen       (1 = 16:9)
  // Offset 0x10: IsHiDef            (1 = HD)
  // Offset 0x14: RefreshRate        (60)
  // Offset 0x18: VideoStandard      (1 = NTSC_M)
  // Offset 0x1C: Reserved[6]        (padding to 0x30)
  // Offset 0x2C: Unknown / flags

  XVIDEO_MODE *oldMode = reinterpret_cast<XVIDEO_MODE *>(ramPtr);
  oldMode->displayWidth = byteswap_be<u32>(1280);
  oldMode->displayHeight = byteswap_be<u32>(720);
  oldMode->isInterlaced = byteswap_be<u32>(0);
  oldMode->isWidescreen = byteswap_be<u32>(1);
  oldMode->isHiDef = byteswap_be<u32>(1);
  oldMode->refreshRate = byteswap_be<u32>(std::bit_cast<u32>(60.0f));
  oldMode->videoStandard = byteswap_be<u32>(1);
}

} // namespace Xe::Core::HLE
