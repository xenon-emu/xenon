/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/Context/XenonIIC/XenonIIC.h"

  // Debug output enable.
  //#define IIC_DEBUG

// Constructor
Xe::XCPU::XenonIIC::XenonIIC() {
  socINTBlock = std::make_unique<STRIP_UNIQUE(socINTBlock)>();
}

// Destructor
Xe::XCPU::XenonIIC::~XenonIIC() {
  socINTBlock.reset();
}

// Write routine
void Xe::XCPU::XenonIIC::Write(u64 writeAddress, const u8* data, u64 size) {
  // Set a lock
  std::lock_guard lock(iicMutex);
  // Offset to our structure
  u32 offset = static_cast<u32>(writeAddress & 0x7FFF);

  // Data is in BE format, byteswap it.
  u64 dataIn = 0;
  memcpy(&dataIn, data, size);
  dataIn = byteswap_be<u64>(dataIn);

  // Write down the data
  memcpy(reinterpret_cast<u8*>(socINTBlock.get()) + offset, &dataIn, size);

#ifdef IIC_DEBUG
  // Print all accesses
  const u32 threadID = offset / ProcessorBlockSize;
  const auto access = getSOCINTAccess(offset);
  LOG_DEBUG(Xenon_IIC, "[IIC]: Write to {}, size {:#x}, inData {:#x}", access.c_str(), size, dataIn);
#endif // IIC_DEBUG

  //
  // Process state changes
  //
  
  // Processor Blocks
  if (offset < ProcessorBlocksEnd) {
    const u32 threadID = offset / ProcessorBlockSize;     // 0..5
    const u32 blockOffset = offset % ProcessorBlockSize;  // 0..0xFFF

    switch (blockOffset) {
    case 0x0000: break; // LogicalIdentification
    case 0x0008: break; // InterruptTaskPriority
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
    case 0x0060: // EndOfInterrupt
      // Erase the first interrupt in queue that has been ACK'd
      {
        removeFirstACKdInterrupt(threadID);
      }
      break; 
    case 0x0068: // EndOfInterruptAutoUpdate
      // Erase the first interrupt in queue that has been ACK'd and update the interrupt priority.
      {
        removeFirstACKdInterrupt(threadID);
        // Update task priority.
        socINTBlock->ProcessorBlock[threadID].InterruptTaskPriority.AsULONGLONG = dataIn & 0xFF;
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
  // Set a lock
  std::lock_guard lock(iicMutex);
  // Offset to our structure
  u32 offset = static_cast<u32>(readAddress & 0x7FFF);
  // Read the data from our structure
  u64 dataOut = 0;
  memcpy(&dataOut, reinterpret_cast<u8*>(socINTBlock.get()) + offset, size);

  //
  // Process state changes
  //

  // Processor Blocks
  if (offset < ProcessorBlocksEnd) {
    const u32 threadID = offset / ProcessorBlockSize;     // 0..5
    const u32 blockOffset = offset % ProcessorBlockSize;  // 0..0xFFF

    switch (blockOffset) {
    case 0x0000: break; // LogicalIdentification
    case 0x0008: break; // InterruptTaskPriority
    case 0x0010: break; // IpiGeneration
    case 0x0020: break; // InterruptCapture
    case 0x0028: break; // InterruptAssertion
    case 0x0030: break; // InterruptInService
    case 0x0038: break; // InterruptTriggerMode
    case 0x0050: // InterruptAcknowledge
      // Send out the highest priority pending interrupt for this thread and mark it as ACK'd.
      dataOut = acknowledgeInterrupt(threadID);
      // Update our state
      socINTBlock->ProcessorBlock[threadID].InterruptAcknowledge.AsULONGLONG = dataOut;
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
    case 0x6020: // MiscellaneousInterruptGeneration2
      // Workaround for PowerMode setting where HV code checks for this specific bit changing.
      if (socINTBlock->MiscellaneousInterruptGeneration2.AsBITS.InterruptState) {
        dataOut |= 0x200;
        socINTBlock->MiscellaneousInterruptGeneration2.AsBITS.InterruptState = 0;
      }
      else { socINTBlock->MiscellaneousInterruptGeneration2.AsBITS.InterruptState = 1; }
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
  LOG_DEBUG(Xenon_IIC, "[IIC]: Read to {}, size {:#x}, returning -> {:#x}", access.c_str(), size, dataOut);
#endif // IIC_DEBUG

  // Copy the data out
  // Data is in BE format, byteswap it
  dataOut = byteswap_be<u64>(dataOut);
  memcpy(data, &dataOut, size);
}


//
// Helper Routines
//

// Generates an interrupt of the specified type to the specified CPUs.
void Xe::XCPU::XenonIIC::generateInterrupt(u8 interruptType, u8 cpusToInterrupt) {
  MICROPROFILE_SCOPEI("[Xe::IIC]", "GenInterrupt", MP_AUTO);
  
  // Set a lock
  std::lock_guard lock(iicMutex);

#ifdef IIC_DEBUG
  LOG_DEBUG(Xenon_IIC, "[IIC]: Generating interrupt {} for threads with mask {:#x}", 
    getIntName(static_cast<eXeIntVectors>(interruptType)).c_str(), cpusToInterrupt);
#endif // IIC_DEBUG

  // Create our interrupt packet
  sInterruptPacket intPacket = { interruptType, false};

  for (u8 threadID = 0; threadID < 6; threadID++) {
    u8 cpuMask = socINTBlock->ProcessorBlock[threadID].LogicalIdentification.AsBITS.LogicalId;
    if ((cpusToInterrupt & cpuMask)) {
      // Store the interrupt in the interrupt queue
      interruptState[threadID].pendingInterrupts.push(intPacket);
    }
  }
}

// Cancels a pending interrupt that has not being ACK'd yet.
void Xe::XCPU::XenonIIC::cancelInterrupt(u8 interruptType, u8 cpusToInterrupt) {
  return;
}

// Returns true if there are pending interrupts for the given thread that match the specified priority.
bool Xe::XCPU::XenonIIC::hasPendingInterrupts(u8 threadID, bool ignorePendingACKd) {
  // Check for valid thread ID
  if (threadID >= 6) {
    return false;
  }

  // Set a lock
  std::lock_guard lock(iicMutex);

  auto& pq = interruptState[threadID].pendingInterrupts;

  if (pq.empty()) {
    // No pending interrupts
    return false;
  }

  // Check for priority
  const u8 priority = static_cast<u8>(socINTBlock->ProcessorBlock[threadID].InterruptTaskPriority.AsULONGLONG & 0xFF);

  // Ignore ACK packages if any and lower priority interrupts
  bool foundAcked = false;
  bool foundMatch = false;
  u8 type = 0;
  std::vector<sInterruptPacket> buffer;
  buffer.reserve(pq.size());

  while (!pq.empty()) {
    const auto pkt = pq.top();
    pq.pop();
    buffer.emplace_back(pkt);

    if (pkt.acknowledged) { foundAcked = true; } 
    else if (pkt.interruptType > priority) { foundMatch = true; type = pkt.interruptType; }
  }

  for (const auto& pkt : buffer) { pq.push(pkt); }

  // There are ACK'd packets, cant signal an interupt.
  if (foundAcked && !ignorePendingACKd) {
    return false;
  }

  return foundMatch;
}

// Removes the first ACK'd interrupt from the pending queue for a given thread.
void Xe::XCPU::XenonIIC::removeFirstACKdInterrupt(u8 threadID) {
  // Set a lock
  std::lock_guard lock(iicMutex);

  auto& pq = interruptState[threadID].pendingInterrupts;
  if (pq.empty()) {
#ifdef IIC_DEBUG
      LOG_DEBUG(Xenon_IIC, "[IIC]: EOI on thread {} with empty queue", threadID);
#endif // IIC_DEBUG
    return;
  }

  // Rebuild the priority queue excluding the first acknowledged packet encountered in order.
  bool removed = false;
  std::vector<sInterruptPacket> buffer;
  buffer.reserve(pq.size());

  // Pop all elements; the top is highest priority due to operator < inversion.
  while (!pq.empty()) {
    const auto pkt = pq.top();
    pq.pop();
    if (!removed && pkt.acknowledged) {
      removed = true;
#ifdef IIC_DEBUG
        LOG_DEBUG(Xenon_IIC, "[IIC]: Removed ACK'd interrupt {} from thread {}",
          getIntName(static_cast<eXeIntVectors>(pkt.interruptType)).c_str(), threadID);
#endif // IIC_DEBUG
      continue; // skip this packet
    }
    buffer.emplace_back(pkt);
  }

  // Restore the remaining packets back into the priority queue.
  for (const auto& pkt : buffer) {
    pq.push(pkt);
  }

#ifdef IIC_DEBUG
  if (!removed) {
    LOG_DEBUG(Xenon_IIC, "[IIC]: EOI on thread {} found no ACK'd interrupts to remove", threadID);
  }
#endif // IIC_DEBUG
}

// Acknowledges and returns the highest priority pending interrupt for a given thread.
// NOTE: If the highest priority interrupt is the same priority as the current task priority, we return and ACK the next
// higher priority interrupt instead.
u8 Xe::XCPU::XenonIIC::acknowledgeInterrupt(u8 threadID) {
  // Set a lock
  std::lock_guard lock(iicMutex);

  u8 outInterrupt = prioNONE;

  auto& pq = interruptState[threadID].pendingInterrupts;

  if (!pq.empty()) {
    // Current task priority for this thread
    const u8 currentPriority = static_cast<u8>(socINTBlock->ProcessorBlock[threadID].InterruptTaskPriority.AsULONGLONG & 0xFF);

    // Get the highest priority interrupt
    const auto topPacket = pq.top();
    u8 targetInterrupt = prioNONE;

    if (!topPacket.acknowledged) {
      if (topPacket.interruptType > currentPriority) {
        // Top is higher than current priority -> acknowledge it
        targetInterrupt = topPacket.interruptType;
      } else if (topPacket.interruptType == currentPriority) {
        // Special case, top equals current priority -> find next strictly higher than currentPriority
        // We scan the queue and pick the first matching.
        std::vector<sInterruptPacket> buffer;
        buffer.reserve(pq.size());

        u8 nextHigher = prioNONE;
        while (!pq.empty()) {
          const auto pkt = pq.top();
          pq.pop();
          buffer.emplace_back(pkt);
          if (!pkt.acknowledged && pkt.interruptType > currentPriority) {
            nextHigher = pkt.interruptType;
            break;
          }
        }
        // Restore queue contents (including any not yet popped)
        for (const auto& pkt : buffer) { pq.push(pkt); }

        targetInterrupt = nextHigher; // may remain prioNONE if none found
      } else {
        // Top is lower than currentPriority -> no eligible interrupt
        targetInterrupt = prioNONE;
      }
    } else {
      // Top already acknowledged; try to find a non-acknowledged packet strictly higher than currentPriority
      std::vector<sInterruptPacket> buffer;
      buffer.reserve(pq.size());

      u8 nextHigher = prioNONE;
      while (!pq.empty()) {
        const auto pkt = pq.top();
        pq.pop();
        buffer.emplace_back(pkt);
        if (!pkt.acknowledged && pkt.interruptType > currentPriority) {
          nextHigher = pkt.interruptType;
          break; // first higher-than-currentPriority encountered
        }
      }
      for (const auto& pkt : buffer) { pq.push(pkt); }

      targetInterrupt = nextHigher;
    }

    // If we found an interrupt to acknowledge, mark exactly one matching packet as acknowledged.
    if (targetInterrupt != prioNONE) {
      outInterrupt = targetInterrupt;

      bool marked = false;
      std::vector<sInterruptPacket> buffer;
      buffer.reserve(pq.size());
      // Pop all elements; the top is highest priority due to operator < inversion.
      while (!pq.empty()) {
        auto pkt = pq.top();
        pq.pop();
        if (!marked && !pkt.acknowledged && pkt.interruptType == targetInterrupt) {
          pkt.acknowledged = true;
          marked = true;
        }
        buffer.emplace_back(pkt);
      }
      // Restore the packets back into the priority queue.
      for (const auto& pkt : buffer) { pq.push(pkt); }
    }
  }

  return outInterrupt;
}

// Returns the name of the register being accessed based on the offset and what block it belongs to.
std::string Xe::XCPU::XenonIIC::getSOCINTAccess(u32 offset) {
  std::ostringstream ss;

  if (offset < ProcessorBlocksEnd) {
    const u32 pid = offset / ProcessorBlockSize;     // 0..5
    const u32 inner = offset % ProcessorBlockSize;   // 0..0xFFF
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
    std::ostringstream os; os << "Reserved5[" << idx << "]";
    return os.str();
  }
  if (offset >= 0x6078 && offset < 0x6FF0) {
    const u32 idx = (offset - 0x6078) / 8; // Reserved6[495]
    std::ostringstream os; os << "Reserved6[" << idx << "]";
    return os.str();
  }
  if (offset >= 0x7038 && offset < 0x7080) {
    const u32 idx = (offset - 0x7038) / 8; // Reserved11[9]
    std::ostringstream os; os << "Reserved11[" << idx << "]";
    return os.str();
  }
  if (offset >= 0x7088 && offset < 0x8000) {
    const u32 idx = (offset - 0x7088) / 8; // Reserved12[495]
    std::ostringstream os; os << "Reserved12[" << idx << "]";
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

static constexpr IRQ_NAME interruptMap[]{
  { 0x08, "Inter Processor Interrupt 4" },
  { 0x10, "Inter Processor Interrupt 3" },
  { 0x14, "System Management Mode Interrupt" },
  { 0x18, "Secure Flash Controller for Xbox Interrupt" },
  { 0x20, "SATA Hard Drive Disk Interrupt" },
  { 0x24, "SATA Optical Disk Drive Interrupt" },
  { 0x2C, "OHCI USB Controller 0 Interrupt" },
  { 0x30, "EHCI USB Controller 0 Interrupt" },
  { 0x34, "OHCI USB Controller 1 Interrupt" },
  { 0x38, "EHCI USB Controller 1 Interrupt" },
  { 0x40, "Xbox Media Audio Interrupt" },
  { 0x44, "Audio Controller Interrupt" },
  { 0x4C, "Ethernet Controller Interrupt" },
  { 0x54, "Xbox Procedural Synthesis Interrupt" },
  { 0x58, "Xenos Graphics Engine Interrupt" },
  { 0x60, "Profiler Interrupt" },
  { 0x64, "BUS Interface Unit Interrupt" },
  { 0x68, "I/O Controller Interrupt" },
  { 0x6C, "Front Side Bus Interrupt" },
  { 0x70, "Inter Processor Interrupt 2" },
  { 0x74, "Clock Interrupt" },
  { 0x78, "Inter Processor Interrupt 1" },
  { 0x7C, "No Interrupt" }
};

// Returns the name of the interrupt based on its type
std::string Xe::XCPU::XenonIIC::getIntName(eXeIntVectors interruptType)
{
  for (auto& interrupt : interruptMap) {
    if (interrupt.interruptType == interruptType) {
      return interrupt.interruptName.data();
    }
  }

  // No match. Should not happen although Linux and the Xbox kernel both set the priority to 0/2 sometimes.
  return "Unknown Interrupt";
}
