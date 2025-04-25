#include "Core/XGPU/CommandProcessor.h"

namespace Xe::XGPU {
  
CommandProcessor::CommandProcessor(RAM *ramPtr) : ram(ramPtr) {
  cpWorkerThread = std::thread(&CommandProcessor::cpWorkerThreadLoop);
}

CommandProcessor::~CommandProcessor() {
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

  cpRingBufferSize = static_cast<size_t>(uint32_t(1) << (newSize + 3));
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Size: {:#x}", cpRingBufferSize.load());
}

void CommandProcessor::cpWorkerThreadLoop() {
  while (cpWorkerThreadRunning) {
    if (cpRingBufferBasePtr == nullptr) {
      // Stall until we're told otherwise
      std::this_thread::sleep_for(100ns);
    }
    // Run commands

  }
}

} // namespace Xe::XGPU