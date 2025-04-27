/*
* Copyright 2025 Xenon Emulator Project. All rights reserved.
*
* Most of the information in this file was gathered from
* various sources across the internet.
*
* In particular, this work is based heavily on research by
* the Xenia developers - especially Triang3l -
* whose dedication and deep understanding of the hardware
* made accurate Xenos emulation possible.
*
* Huge thanks to everyone who contributed to uncovering and
* documenting this complex system.
*/

#include "Base/Thread.h"

#include "Core/XGPU/CommandProcessor.h"

#include "Render/RenderDoc.h"

namespace Xe::XGPU {
  
CommandProcessor::CommandProcessor(RAM *ramPtr) : ram(ramPtr) {
  Render::StartCapture();
  cpWorkerThread = std::thread(&CommandProcessor::cpWorkerThreadLoop, this);
}

CommandProcessor::~CommandProcessor() {
  Render::EndCapture();
  cpWorkerThreadRunning = false;
  if (cpWorkerThread.joinable()) {
    cpWorkerThread.join();
  }
}

void CommandProcessor::CPUpdateRBBase(u32 address) {
  if (address == 0)
    return;

  cpRingBufferBasePtr = ram->getPointerToAddress(address);
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Base Address: {:#x}", address);
}

void CommandProcessor::CPUpdateRBSize(size_t newSize) {
  if ((newSize & CP_RB_CNTL_RB_BUFSZ_MASK) == 0)
    return;

  cpRingBufferSize = static_cast<size_t>(1u << (newSize + 3));
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Size: {:#x}", cpRingBufferSize.load());
}

void CommandProcessor::cpWorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Command Processor");
  while (cpWorkerThreadRunning) {
    if (cpRingBufferBasePtr == nullptr) {
      // Stall until we're told otherwise
      std::this_thread::sleep_for(100ns);
    }
  }
}

} // namespace Xe::XGPU