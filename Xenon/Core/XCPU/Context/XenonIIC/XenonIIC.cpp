/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/Context/XenonIIC/XenonIIC.h"

#include "Base/Thread.h"

// Debug output enable.
// #define IIC_DEBUG

#ifndef IIC_DEBUG
  #define DEBUGP(x, ...)
#else
  #define DEBUGP(x, ...) LOG_DEBUG(Xenon_IIC, x, ##__VA_ARGS__);
#endif

// Constructor
Xe::XCPU::XenonIIC::XenonIIC() { socINTBlock = std::make_unique<STRIP_UNIQUE(socINTBlock)>(); }

// Destructor
Xe::XCPU::XenonIIC::~XenonIIC() {
  // Stop the interrupt evaluation thread and wait for it to finish before tearing down state.
  signalThreadActive.store(false, std::memory_order_release);
  signalCV.notify_all();
  if (signalThread.joinable()) { signalThread.join(); }
  socINTBlock.reset();
}

// Write routine
void Xe::XCPU::XenonIIC::Write(u64 writeAddress, const u8* data, u64 size) {
  // Offset to our structure
  u32 offset = static_cast<u32>(writeAddress & 0x7FFF);

  // Data is in BE format, byteswap it.
  u64 dataIn = 0;
  memcpy(&dataIn, data, size);
  dataIn = byteswap_be<u64>(dataIn);

  // Write down the data to socINTBlock under lock
  {
    std::lock_guard lock(iicMutex);
    memcpy(reinterpret_cast<u8*>(socINTBlock.get()) + offset, &dataIn, size);
  }

#ifdef IIC_DEBUG
  // Print all accesses
  const auto access = getSOCINTAccess(offset);
  DEBUGP("[IIC]: Write to {}, size {:#x}, inData {:#x}", access.c_str(), size, dataIn);
#endif // IIC_DEBUG

  //
  // Process state changes
  //

  // Processor Blocks
  if (offset < ProcessorBlocksEnd) {
    const u32 threadID = offset / ProcessorBlockSize;    // 0..5
    const u32 blockOffset = offset % ProcessorBlockSize; // 0..0xFFF

    switch (blockOffset) {
      case 0x0000: break; // LogicalIdentification
      case 0x0008:        // InterruptTaskPriority
        // Lowering the task priority can make a previously-masked interrupt deliverable (and raising
        // it can retract the interrupt).
        markDirty(threadID);
        break;
      case 0x0010: // IpiGeneration
        // Interrupt packet received, generate appropriate interrupt to target threads.
        {
          const u8 interruptType = static_cast<u8>(dataIn & 0xFF);
          const u8 cpusToInterrupt = static_cast<u8>((dataIn >> 16) & 0xFF);
          generateInterrupt(interruptType, cpusToInterrupt);
        }
        break;
      case 0x0020: break; // InterruptCapture
      case 0x0028: break; // InterruptAssertion
      case 0x0030: break; // InterruptInService
      case 0x0038: break; // InterruptTriggerMode
      case 0x0050: break; // InterruptAcknowledge
      case 0x0058: break; // InterruptAcknowledgeAutoUpdate
      case 0x0060:        // EndOfInterrupt
        // Erase the highest-priority ACK'd interrupt.
        { removeFirstACKdInterrupt(threadID); }
        break;
      case 0x0068: // EndOfInterruptAutoUpdate
        // Erase the highest-priority ACK'd interrupt and update the interrupt priority.
        {
          removeFirstACKdInterrupt(threadID);
          // Update task priority.
          {
            std::lock_guard lock(iicMutex);
            socINTBlock->ProcessorBlock[threadID].InterruptTaskPriority.AsULONGLONG = dataIn & 0xFF;
          }
          // Mark dirty after both the EOI and the priority change.
          markDirty(threadID);
        }
        break;
      case 0x0070: break; // SpuriousVector
      case 0x00F0: break; // ThreadReset
      default: break;
    }

  } else {
    // Other misc 'global' registers
    switch (offset) {
      case 0x6000: break; // MiscellaneousInterruptGeneration0
      case 0x6010: break; // MiscellaneousInterruptGeneration1
      case 0x6020: break; // MiscellaneousInterruptGeneration2
      case 0x6030: break; // MiscellaneousInterruptGeneration3
      case 0x6040: break; // MiscellaneousInterruptGeneration4
      case 0x6070: break; // EndOfInterruptBaseAddress
      case 0x6FF0: break; // InterruptRecoverableError
      case 0x7000: break; // InterruptRecoverableErrorOrMask
      case 0x7010: break; // InterruptRecoverableErrorAndMask
      case 0x7020: break; // InterruptDebugConfiguration
      case 0x7030: break; // InterruptPerformanceMeasurementCounter
      case 0x7080: break; // EndOfInterruptGeneration
      default: break;
    }
  }
}

// Read routine
void Xe::XCPU::XenonIIC::Read(u64 readAddress, u8* data, u64 size) {
  // Offset to our structure.
  u32 offset = static_cast<u32>(readAddress & 0x7FFF);
  // Read the data from our structure.
  u64 dataOut = 0;

  {
    std::lock_guard lock(iicMutex);
    memcpy(&dataOut, reinterpret_cast<u8*>(socINTBlock.get()) + offset, size);
  }

  //
  // Process state changes
  //

  // Processor Blocks
  if (offset < ProcessorBlocksEnd) {
    const u32 threadID = offset / ProcessorBlockSize;    // 0..5
    const u32 blockOffset = offset % ProcessorBlockSize; // 0..0xFFF

    switch (blockOffset) {
      case 0x0000: break; // LogicalIdentification
      case 0x0008: break; // InterruptTaskPriority
      case 0x0010: break; // IpiGeneration
      case 0x0020: break; // InterruptCapture
      case 0x0028: break; // InterruptAssertion
      case 0x0030: break; // InterruptInService
      case 0x0038: break; // InterruptTriggerMode
      case 0x0050:        // InterruptAcknowledge
        // Send out the highest priority pending interrupt for this thread and mark it as ACK'd.
        dataOut = acknowledgeInterrupt(threadID);
        // Update our state
        {
          std::lock_guard lock(iicMutex);
          socINTBlock->ProcessorBlock[threadID].InterruptAcknowledge.AsULONGLONG = dataOut;
        }
        break;
      case 0x0058: break; // InterruptAcknowledgeAutoUpdate
      case 0x0060: break; // EndOfInterrupt
      case 0x0068: break; // EndOfInterruptAutoUpdate
      case 0x0070: break; // SpuriousVector
      case 0x00F0: break; // ThreadReset
      default: break;
    }
  } else {
    // Other misc 'global' registers
    switch (offset) {
      case 0x6000: break; // MiscellaneousInterruptGeneration0
      case 0x6010: break; // MiscellaneousInterruptGeneration1
      case 0x6020:        // MiscellaneousInterruptGeneration2
        // Workaround for PowerMode setting where HV code checks for this specific bit changing.
        {
          std::lock_guard lock(iicMutex);
          if (socINTBlock->MiscellaneousInterruptGeneration2.AsBITS.InterruptState) {
            dataOut |= 0x200;
            socINTBlock->MiscellaneousInterruptGeneration2.AsBITS.InterruptState = 0;
          } else {
            socINTBlock->MiscellaneousInterruptGeneration2.AsBITS.InterruptState = 1;
          }
        }
        break;
      case 0x6030: break; // MiscellaneousInterruptGeneration3
      case 0x6040: break; // MiscellaneousInterruptGeneration4
      case 0x6070: break; // EndOfInterruptBaseAddress
      case 0x6FF0: break; // InterruptRecoverableError
      case 0x7000: break; // InterruptRecoverableErrorOrMask
      case 0x7010: break; // InterruptRecoverableErrorAndMask
      case 0x7020: break; // InterruptDebugConfiguration
      case 0x7030: break; // InterruptPerformanceMeasurementCounter
      case 0x7080: break; // EndOfInterruptGeneration
      default: break;
    }
  }

#ifdef IIC_DEBUG
  // Print all accesses
  const auto access = getSOCINTAccess(offset);
  DEBUGP("[IIC]: Read to {}, size {:#x}, returning -> {:#x}", access.c_str(), size, dataOut);
#endif // IIC_DEBUG

  // Copy the data out
  // Data is in BE format, byteswap it
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, size);
}

//
// Helper Routines
//

// Registers the external interrupt callback and starts the evaluation thread.
void Xe::XCPU::XenonIIC::RegisterSignalCallback(InterruptSignalCallback cb) {
  {
    std::lock_guard lock(signalMutex);
    signalCallback = std::move(cb);
  }

  // Start the evaluation thread.
  if (!signalThreadActive.exchange(true, std::memory_order_acq_rel)) {
    signalThread = std::thread(&XenonIIC::SignalThread, this);
  }
}

// Detaches the signal callback.
void Xe::XCPU::XenonIIC::ClearSignalCallback() {
  std::lock_guard lock(signalMutex);
  signalCallback = {};
}

// Marks a thread's interrupt queue dirty and wakes the evaluation thread.
void Xe::XCPU::XenonIIC::markDirty(u8 threadID) {
  if (threadID >= 6) { return; }
  {
    std::lock_guard lock(signalMutex);
    dirtyThreads |= (1u << threadID);
  }

  signalCV.notify_one();
}

// Re-evaluates deliverability for a thread and calls the callback based on it.
void Xe::XCPU::XenonIIC::recomputeAndSignal(u8 threadID) {
  if (threadID >= 6) { return; }
  // Check if line should be asserted.
  const bool asserted = hasPendingInterrupts(threadID);
  InterruptSignalCallback cb;
  {
    std::lock_guard lock(signalMutex);
    cb = signalCallback;
  }
  // Issue call.
  if (cb) { cb(threadID, asserted); }
}

// Interrupt evaluation thread. Sleeps on signalCV until a mutation marks a thread dirty, then recomputes and pushes
// each dirty thread's interrupt line status to the PPU.
void Xe::XCPU::XenonIIC::SignalThread() {
  Base::SetCurrentThreadName("[Xe] IIC Signal Thread");

  std::unique_lock<std::mutex> lock(signalMutex);
  while (signalThreadActive.load(std::memory_order_acquire)) {
    signalCV.wait(lock, [this] { return !signalThreadActive.load(std::memory_order_acquire) || dirtyThreads != 0; });

    if (!signalThreadActive.load(std::memory_order_acquire)) { break; }

    // Drain the dirty set under the lock, then evaluate without holding it.
    u32 dirty = dirtyThreads;
    dirtyThreads = 0;
    lock.unlock();

    while (dirty) {
      const u8 threadID = static_cast<u8>(std::countr_zero(dirty));
      dirty &= dirty - 1;
      recomputeAndSignal(threadID);
    }

    lock.lock();
  }

  DEBUGP("[IIC]: Signal thread exiting.");
}

// Generates an interrupt of the specified type to the specified CPUs.
void Xe::XCPU::XenonIIC::generateInterrupt(u8 interruptType, u8 cpusToInterrupt) {
  MICROPROFILE_SCOPEI("[Xe::IIC]", "GenInterrupt", MP_AUTO);

  DEBUGP("[IIC]: Generating interrupt {} for threads with mask {:#x}",
         getIntName(static_cast<eXeIntVectors>(interruptType)).c_str(), cpusToInterrupt);

  const u32 bit = vectorToBit(interruptType);

  for (u8 threadID = 0; threadID < 6; threadID++) {
    u8 cpuMask;
    {
      std::lock_guard lock(iicMutex);
      cpuMask = socINTBlock->ProcessorBlock[threadID].LogicalIdentification.AsBITS.LogicalId;
    }
    if ((cpusToInterrupt & cpuMask)) {
      // Atomically set the pending bit for this interrupt vector.
      interruptState[threadID].pendingMask.fetch_or(bit, std::memory_order_release);
      // Mark the thread dirty. The evaluation thread recomputes the line level and pushes it to the PPU.
      markDirty(threadID);
    }
  }
}

// Cancels a pending interrupt that has not being ACK'd yet.
void Xe::XCPU::XenonIIC::cancelInterrupt(u8 interruptType, u8 cpusToInterrupt) { return; }

// Returns true if there are pending interrupts for the given thread that are deliverable.
bool Xe::XCPU::XenonIIC::hasPendingInterrupts(u8 threadID, bool ignorePendingACKd) {
  // Check for valid thread ID
  if (threadID >= 6) { return false; }

  // Load pending mask (interrupts not yet ACK'd).
  const u32 pending = interruptState[threadID].pendingMask.load(std::memory_order_acquire);
  if (!pending) { return false; }

  // Current task priority for this thread
  u8 currentPriority;
  {
    std::lock_guard lock(iicMutex);
    currentPriority = static_cast<u8>(socINTBlock->ProcessorBlock[threadID].InterruptTaskPriority.AsULONGLONG & 0xFF);
  }

  // Build a mask of all vectors above the current priority threshold.
  // Vectors > currentPriority are deliverable. vectorToBit gives 1 << (vec >> 2).
  // All bits at positions > (currentPriority >> 2) represent deliverable vectors.
  const u32 priorityBit = currentPriority >> 2;
  // Mask of all bits strictly above the priority threshold
  const u32 deliverableMask = (priorityBit < 31) ? ~((1u << (priorityBit + 1)) - 1) : 0u;

  return (pending & deliverableMask) != 0;
}

// Removes the highest-priority ACK'd interrupt (EOI) for a given thread.
void Xe::XCPU::XenonIIC::removeFirstACKdInterrupt(u8 threadID) {
  // Bounds check
  if (threadID >= 6) { return; }

  auto& state = interruptState[threadID];
  u32 acked = state.acknowledgedMask.load(std::memory_order_acquire);
  if (!acked) {
    DEBUGP("[IIC]: EOI on thread {} with no ACK'd interrupts", threadID);
    return;
  }

  // Find the highest-priority (highest bit position) ACK'd interrupt using CAS loop.
  // The highest set bit corresponds to the highest-priority vector on Xenon's IIC.
  while (acked) {
    const u32 highestBit = 1u << (31 - std::countl_zero(acked));
    // Atomically clear this bit from acknowledgedMask
    if (state.acknowledgedMask.compare_exchange_weak(acked, acked & ~highestBit, std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
      DEBUGP("[IIC]: Removed ACK'd interrupt with bit {:#x} from thread {}", highestBit, threadID);
      return;
    }
    // CAS failed, acked was reloaded - retry
  }

  DEBUGP("[IIC]: EOI on thread {} found no ACK'd interrupts to remove", threadID);
}

// Acknowledges the highest-priority deliverable interrupt for a given thread.
// Returns the interrupt vector, or prioNONE if no deliverable interrupt exists.
u8 Xe::XCPU::XenonIIC::acknowledgeInterrupt(u8 threadID) {
  // Bounds check
  if (threadID >= 6) { return prioNONE; }

  auto& state = interruptState[threadID];

  // Current task priority for this thread
  u8 currentPriority;
  {
    std::lock_guard lock(iicMutex);
    currentPriority = static_cast<u8>(socINTBlock->ProcessorBlock[threadID].InterruptTaskPriority.AsULONGLONG & 0xFF);
  }

  // Build mask of deliverable vectors (those with vector > currentPriority).
  // Higher vector number = higher priority on Xenon's IIC.
  const u32 priorityBit = currentPriority >> 2;
  const u32 deliverableMask = (priorityBit < 31) ? ~((1u << (priorityBit + 1)) - 1) : 0u;

  // CAS loop to atomically find and acknowledge the highest-priority deliverable interrupt.
  // Highest priority = highest vector number = highest set bit in the deliverable portion.
  u32 pending = state.pendingMask.load(std::memory_order_acquire);
  while (true) {
    const u32 eligible = pending & deliverableMask;
    if (!eligible) { return prioNONE; }

    // Isolate the highest set bit (highest priority deliverable interrupt).
    const u32 highestBit = 1u << (31 - std::countl_zero(eligible));

    // Atomically move this interrupt from pending to acknowledged
    if (state.pendingMask.compare_exchange_weak(pending, pending & ~highestBit, std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
      // Set the acknowledged bit
      state.acknowledgedMask.fetch_or(highestBit, std::memory_order_release);
      // Convert bit position back to vector: bit position * 4
      const u8 vector = static_cast<u8>(std::countr_zero(highestBit) << 2);
      return vector;
    }
    // CAS failed, pending was reloaded - retry
  }
}

// Returns the name of the register being accessed based on the offset and what block it belongs to.
std::string Xe::XCPU::XenonIIC::getSOCINTAccess(u32 offset) {
  std::ostringstream ss;

  if (offset < ProcessorBlocksEnd) {
    const u32 pid = offset / ProcessorBlockSize;   // 0..5
    const u32 inner = offset % ProcessorBlockSize; // 0..0xFFF
    ss << "ProcessorBlock[" << pid << "].";

    switch (inner) {
      case 0x0000: ss << "LogicalIdentification"; return ss.str();
      case 0x0008: ss << "InterruptTaskPriority"; return ss.str();
      case 0x0010: ss << "IpiGeneration"; return ss.str();
      case 0x0018: ss << "Reserved1"; return ss.str();
      case 0x0020: ss << "InterruptCapture"; return ss.str();
      case 0x0028: ss << "InterruptAssertion"; return ss.str();
      case 0x0030: ss << "InterruptInService"; return ss.str();
      case 0x0038: ss << "InterruptTriggerMode"; return ss.str();
      case 0x0050: ss << "InterruptAcknowledge"; return ss.str();
      case 0x0058: ss << "InterruptAcknowledgeAutoUpdate"; return ss.str();
      case 0x0060: ss << "EndOfInterrupt"; return ss.str();
      case 0x0068: ss << "EndOfInterruptAutoUpdate"; return ss.str();
      case 0x0070: ss << "SpuriousVector"; return ss.str();
      case 0x00F0: ss << "ThreadReset"; return ss.str();
      default: break;
    }

    if (inner >= 0x0040 && inner < 0x0050) {
      const u32 idx = (inner - 0x0040) / 8;
      ss << "Reserved2[" << idx << "]";
      return ss.str();
    }
    if (inner >= 0x0078 && inner < 0x00F0) {
      const u32 idx = (inner - 0x0078) / 8;
      ss << "Reserved3[" << idx << "]";
      return ss.str();
    }
    if (inner >= 0x00F8 && inner < 0x1000) {
      const u32 idx = (inner - 0x00F8) / 8;
      ss << "Reserved4[" << idx << "]";
      return ss.str();
    }

    ss << "Unknown(0x" << std::hex << inner << ")";
    return ss.str();
  }

  switch (offset) {
    case 0x6000: return "MiscellaneousInterruptGeneration0";
    case 0x6008: return "Reserved1";
    case 0x6010: return "MiscellaneousInterruptGeneration1";
    case 0x6018: return "Reserved2";
    case 0x6020: return "MiscellaneousInterruptGeneration2";
    case 0x6028: return "Reserved3";
    case 0x6030: return "MiscellaneousInterruptGeneration3";
    case 0x6038: return "Reserved4";
    case 0x6040: return "MiscellaneousInterruptGeneration4";
    case 0x6070: return "EndOfInterruptBaseAddress";
    case 0x6FF0: return "InterruptRecoverableError";
    case 0x6FF8: return "Reserved7";
    case 0x7000: return "InterruptRecoverableErrorOrMask";
    case 0x7008: return "Reserved8";
    case 0x7010: return "InterruptRecoverableErrorAndMask";
    case 0x7018: return "Reserved9";
    case 0x7020: return "InterruptDebugConfiguration";
    case 0x7028: return "Reserved10";
    case 0x7030: return "InterruptPerformanceMeasurementCounter";
    case 0x7080: return "EndOfInterruptGeneration";
    default: break;
  }

  if (offset >= 0x6048 && offset < 0x6070) {
    const u32 idx = (offset - 0x6048) / 8; // Reserved5[5]
    std::ostringstream os;
    os << "Reserved5[" << idx << "]";
    return os.str();
  }
  if (offset >= 0x6078 && offset < 0x6FF0) {
    const u32 idx = (offset - 0x6078) / 8; // Reserved6[495]
    std::ostringstream os;
    os << "Reserved6[" << idx << "]";
    return os.str();
  }
  if (offset >= 0x7038 && offset < 0x7080) {
    const u32 idx = (offset - 0x7038) / 8; // Reserved11[9]
    std::ostringstream os;
    os << "Reserved11[" << idx << "]";
    return os.str();
  }
  if (offset >= 0x7088 && offset < 0x8000) {
    const u32 idx = (offset - 0x7088) / 8; // Reserved12[495]
    std::ostringstream os;
    os << "Reserved12[" << idx << "]";
    return os.str();
  }

  std::ostringstream os;
  os << "Unknown(0x" << std::hex << offset << ")";
  return os.str();
}

struct IRQ_NAME {
  u8 interruptType = Xe::XCPU::eXeIntVectors::prioNONE;
  std::string_view interruptName = {};
};

static constexpr IRQ_NAME interruptMap[]{{0x08, "Inter Processor Interrupt 4"},
                                         {0x10, "Inter Processor Interrupt 3"},
                                         {0x14, "System Management Mode Interrupt"},
                                         {0x18, "Secure Flash Controller for Xbox Interrupt"},
                                         {0x20, "SATA Hard Drive Disk Interrupt"},
                                         {0x24, "SATA Optical Disk Drive Interrupt"},
                                         {0x2C, "OHCI USB Controller 0 Interrupt"},
                                         {0x30, "EHCI USB Controller 0 Interrupt"},
                                         {0x34, "OHCI USB Controller 1 Interrupt"},
                                         {0x38, "EHCI USB Controller 1 Interrupt"},
                                         {0x40, "Xbox Media Audio Interrupt"},
                                         {0x44, "Audio Controller Interrupt"},
                                         {0x4C, "Ethernet Controller Interrupt"},
                                         {0x54, "Xbox Procedural Synthesis Interrupt"},
                                         {0x58, "Xenos Graphics Engine Interrupt"},
                                         {0x60, "Profiler Interrupt"},
                                         {0x64, "BUS Interface Unit Interrupt"},
                                         {0x68, "I/O Controller Interrupt"},
                                         {0x6C, "Front Side Bus Interrupt"},
                                         {0x70, "Inter Processor Interrupt 2"},
                                         {0x74, "Clock Interrupt"},
                                         {0x78, "Inter Processor Interrupt 1"},
                                         {0x7C, "No Interrupt"}};

// Returns the name of the interrupt based on its type
std::string Xe::XCPU::XenonIIC::getIntName(eXeIntVectors interruptType) {
  for (auto& interrupt : interruptMap) {
    if (interrupt.interruptType == interruptType) { return interrupt.interruptName.data(); }
  }

  // No match. Should not happen although Linux and the Xbox kernel both set the priority to 0/2 sometimes.
  return "Unknown Interrupt";
}
