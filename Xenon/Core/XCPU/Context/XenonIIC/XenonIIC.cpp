/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "Core/XCPU/Context/XenonIIC/XenonIIC.h"

// Constructor
Xe::XCPU::XeIIC::XeIIC() {
  socINTBlock = std::make_unique<STRIP_UNIQUE(socINTBlock)>();
}

// Destructor
Xe::XCPU::XeIIC::~XeIIC() {
  socINTBlock.reset();
}

// Write routine
void Xe::XCPU::XeIIC::Write(u64 writeAddress, const u8* data, u64 size) {
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
  const auto access = getSOCINTAccess(offset);
  LOG_DEBUG(Xenon_IIC, "[IIC]: Write to {}, size {:#x}, inData {:#x}", access.c_str(), size, dataIn);
#endif // IIC_DEBUG
}

// Read routine
void Xe::XCPU::XeIIC::Read(u64 readAddress, u8* data, u64 size) {
  // Set a lock
  std::lock_guard lock(iicMutex);
  // Offset to our structure
  u32 offset = static_cast<u32>(readAddress & 0x7FFF);
  // Read the data from our structure
  u64 dataOut = 0;
  memcpy(&dataOut, reinterpret_cast<u8*>(socINTBlock.get()) + offset, size);

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

// Returns the name of the register being accessed based on the offset and what block it belongs to.
std::string Xe::XCPU::XeIIC::getSOCINTAccess(u32 offset) {
  std::ostringstream ss;

  constexpr u32 kProcessorBlockSize = 0x1000;
  constexpr u32 kProcessorBlocksEnd = 6 * kProcessorBlockSize; // 0x6000

  if (offset < kProcessorBlocksEnd) {
    const u32 pid = offset / kProcessorBlockSize;     // 0..5
    const u32 inner = offset % kProcessorBlockSize;   // 0..0xFFF
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