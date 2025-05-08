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

#include "CommandProcessor.h"

#include "Base/Thread.h"

namespace Xe::XGPU {
  
CommandProcessor::CommandProcessor(RAM *ramPtr, XenosState * statePtr) : ram(ramPtr), state(statePtr) {
  cpWorkerThread = std::thread(&CommandProcessor::cpWorkerThreadLoop, this);
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

  state->WriteRegister(XeRegister::CP_RB_BASE, address);

  cpRingBufferBasePtr = ram->getPointerToAddress(address);
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Base Address: {:#x}", address);
  
  // Reset CP Read Ptr Idx.
  cpReadPtrIndex = 0;
}

void CommandProcessor::CPUpdateRBSize(size_t newSize) {
  if ((newSize & CP_RB_CNTL_RB_BUFSZ_MASK) == 0)
    return;

  state->WriteRegister(XeRegister::CP_RB_CNTL, newSize);

  cpRingBufferSize = static_cast<size_t>(1u << (newSize + 3));
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Size: {:#x}", cpRingBufferSize.load());
}

void CommandProcessor::CPUpdateRBWritePointer(u32 offset) {
  state->WriteRegister(XeRegister::CP_RB_WPTR, offset);
  cpWritePtrIndex = offset;
}

void CommandProcessor::cpWorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Command Processor");
  while (cpWorkerThreadRunning) {

    u32 writePtrIndex = cpWritePtrIndex.load();
    while (cpRingBufferBasePtr == nullptr || cpReadPtrIndex == writePtrIndex) {
      // Stall until we're told otherwise
      std::this_thread::sleep_for(100ns);
      writePtrIndex = cpWritePtrIndex.load();
    }

    LOG_INFO(Xenos, "CP: Command processor setup.");

    cpReadPtrIndex = cpExecutePrimaryBuffer(cpReadPtrIndex, writePtrIndex);
  }
}

u32 CommandProcessor::cpExecutePrimaryBuffer(u32 readIndex, u32 writeIndex) {
  
  // Create the ring buffer instance for the primary buffer.
  RingBuffer cpRingBufer(cpRingBufferBasePtr, cpRingBufferSize);

  // Set read and write offsets.
  cpRingBufer.setReadOffset(readIndex * sizeof(u32));
  cpRingBufer.setWriteOffset(writeIndex * sizeof(u32));

  do {
    if (!ExecutePacket(&cpRingBufer)) {
      // TODO(bitsh1ft3r): Check wheter this should be a fatal crash.
      LOG_ERROR(Xenos, "CP[PrimaryBuffer]: Failed to execute a packet.");
      assert(true);
      break;
    }
  } while (cpRingBufer.readCount());

  return writeIndex; // Set Read and Write index equal, signaling buffer processed.
}

void CommandProcessor::cpExecuteIndirectBuffer(u32 bufferPtr, u32 bufferSize)
{
  // Create the ring buffer instance for the indirect buffer.
  RingBuffer ringBufer(ram->getPointerToAddress(bufferPtr), bufferSize * sizeof(u32));

  // Set write offset.
  ringBufer.setWriteOffset(bufferSize * sizeof(u32));

  do {
    if (!ExecutePacket(&ringBufer)) {
      // TODO(bitsh1ft3r): Check wheter this should be a fatal crash.
      LOG_ERROR(Xenos, "CP[IndirectRingBuffer]: Failed to execute a packet.");
      assert(true);
      break;
    }
  } while (ringBufer.readCount());
  return;
}

// Executes a single packet from the ringbuffer.
bool CommandProcessor::ExecutePacket(RingBuffer *ringBuffer) {
  // Get packet data.
  const u32 packetData = ringBuffer->ReadAndSwap<u32>();
  // Get the packet type.
  const CPPacketType packetType = static_cast<CPPacketType>(packetData >> 30);
  
  // TODO: Check this.
  if (packetData == 0) {
    LOG_WARNING(Xenos, "CP[PrimaryBuffer]: found packet with zero data!");
    return true;
  }
  
  if (packetData == 0xCDCDCDCD) {
    LOG_WARNING(Xenos, "CP[PrimaryBuffer]: found packet with uninitialized data!");
    return true;
  }

  // Execute packet based on type.
  switch (packetType) {
  case Xe::XGPU::CPPacketType0: return ExecutePacketType0(ringBuffer, packetData);
  case Xe::XGPU::CPPacketType1: return ExecutePacketType1(ringBuffer, packetData);
  case Xe::XGPU::CPPacketType2: return ExecutePacketType2(ringBuffer, packetData);
  case Xe::XGPU::CPPacketType3: return ExecutePacketType3(ringBuffer, packetData);
  default:
    // This should never happen.
    LOG_ERROR(Xenos, "CP[PrimaryBuffer]: found packet with unknown type!");
    return false;
    break;
  }
}

// Executes a packet type 0. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType0(RingBuffer *ringBuffer, u32 packetData) {
  const u32 regCount = ((packetData >> 16) & 0x3FFF) + 1;
  
  if (ringBuffer->readCount() < regCount * sizeof(u32)) {
    LOG_ERROR(Xenos, "CP[ExecutePacketType0]: Data overflow, read count {:#x}, registers count {:#x})",
      ringBuffer->readCount(), regCount * sizeof(u32));
    return false;
  }

  // Base register to start from.
  const u32 baseIndex = (packetData & 0x7FFF); 
  // Tells wheter the write is to one or multiple regs starting at specified register at base index.
  const u32 singleRegWrite = (packetData >> 15) & 0x1;

  for (size_t idx = 0; idx < regCount; idx++) {
    // Get the data to be written to the (internal) Register.
    u32 registerData = ringBuffer->ReadAndSwap<u32>();
    // Target register index.
    u32 targetRegIndex = singleRegWrite ? baseIndex : baseIndex + idx;
    LOG_TRACE(Xenos, "CP[ExecutePacketType0]: Writing register at index {:#x}, data {:#x}", targetRegIndex, registerData);
    state->WriteRegister(static_cast<XeRegister>(targetRegIndex), registerData);
  }

  return true;
}

// Executes a packet type 1. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType1(Xe::XGPU::RingBuffer *ringBuffer, u32 packetData) {
  // Get both registers index.
  const u32 regIndex0 = packetData & 0x7FF;
  const u32 regIndex1 = (packetData >> 11) & 0x7FF;
  // Get both registers data.
  const u32 reg0Data = ringBuffer->ReadAndSwap<u32>();
  const u32 reg1Data = ringBuffer->ReadAndSwap<u32>();
  // Do the write.
  LOG_TRACE(Xenos, "CP[ExecutePacketType1]: Writing register at index {:#x}, data {:#x}", regIndex0, reg0Data);
  LOG_TRACE(Xenos, "CP[ExecutePacketType1]: Writing register at index {:#x}, data {:#x}", regIndex1, reg1Data);
  // Write registers.
  state->WriteRegister(static_cast<XeRegister>(regIndex0), reg0Data);
  state->WriteRegister(static_cast<XeRegister>(regIndex1), reg1Data);
  return true;
}

// Executes a packet type 2. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType2(RingBuffer *ringBuffer, u32 packetData) {
  // Type 2 is a No-op. :)
  return true;
}

// Executes a packet type 3. Description is in CPPacketType enum.
bool CommandProcessor::ExecutePacketType3(RingBuffer *ringBuffer, u32 packetData) {
  // Current opcode we're executing.
  const CPPacketType3Opcode currentOpCode = static_cast<CPPacketType3Opcode>((packetData >> 8) & 0x7F);
  // Amount of data we're reading.
  const u32 dataCount = ((packetData >> 16) & 0x3FFF) + 1;
  // Get the read offset.
  auto data_start_offset = ringBuffer->readOffset();
  
  if (ringBuffer->readCount() < dataCount * sizeof(u32)) {
    LOG_ERROR(Xenos, "CP[ExecutePacketType3]: Data overflow, read count {:#x}, registers count {:#x})",
      ringBuffer->readCount(), dataCount * sizeof(u32));
    return false;
  }

  // Info from Xenia devs:
  // & 1 == predicate. When set, we do bin check to see if we should execute
  // the packet. Only type 3 packets are affected.
  // We also skip predicated swaps, as they are never valid (probably?).
  if (packetData & 1) {
    bool anyPass = (binSelect & binMask) != 0;
    if (!anyPass) {
      ringBuffer->AdvanceRead(dataCount * sizeof(u32));
      return true;
    }
  }

  bool result = false;

  LOG_TRACE(Xenos, "CP[ExecutePacketType3]: Executing OpCode {:#x}", static_cast<u32>(currentOpCode));

  // PM4 Commands execution, basically the heart of the command processor.

  switch (currentOpCode) {
  case Xe::XGPU::PM4_NOP:
    result = ExecutePacketType3_NOP(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_RECORD_PFP_TIMESTAMP:
    break;
  case Xe::XGPU::PM4_WAIT_MEM_WRITES:
    break;
  case Xe::XGPU::PM4_WAIT_FOR_ME:
    break;
  case Xe::XGPU::PM4_UNKNOWN_19:
    break;
  case Xe::XGPU::PM4_UNKNOWN_1A:
    break;
  case Xe::XGPU::PM4_PREEMPT_ENABLE:
    break;
  case Xe::XGPU::PM4_SKIP_IB2_ENABLE_GLOBAL:
    break;
  case Xe::XGPU::PM4_PREEMPT_TOKEN:
    break;
  case Xe::XGPU::PM4_REG_RMW:
    break;
  case Xe::XGPU::PM4_DRAW_INDX:
    break;
  case Xe::XGPU::PM4_VIZ_QUERY:
    break;
  case Xe::XGPU::PM4_DRAW_AUTO:
    break;
  case Xe::XGPU::PM4_SET_STATE:
    break;
  case Xe::XGPU::PM4_WAIT_FOR_IDLE:
    break;
  case Xe::XGPU::PM4_IM_LOAD:
    break;
  case Xe::XGPU::PM4_DRAW_INDIRECT:
    break;
  case Xe::XGPU::PM4_DRAW_INDX_INDIRECT:
    break;
  case Xe::XGPU::PM4_IM_LOAD_IMMEDIATE:
    break;
  case Xe::XGPU::PM4_IM_STORE:
    break;
  case Xe::XGPU::PM4_SET_CONSTANT:
    break;
  case Xe::XGPU::PM4_LOAD_CONSTANT_CONTEXT:
    break;
  case Xe::XGPU::PM4_LOAD_ALU_CONSTANT:
    break;
  case Xe::XGPU::PM4_LOAD_STATE:
    break;
  case Xe::XGPU::PM4_RUN_OPENCL:
    break;
  case Xe::XGPU::PM4_COND_INDIRECT_BUFFER_PFD:
    break;
  case Xe::XGPU::PM4_EXEC_CS:
    break;
  case Xe::XGPU::PM4_DRAW_INDX_BIN:
    break;
  case Xe::XGPU::PM4_DRAW_INDX_2_BIN:
    break;
  case Xe::XGPU::PM4_DRAW_INDX_2:
    break;
  case Xe::XGPU::PM4_INDIRECT_BUFFER_PFD:
    result = ExecutePacketType3_INDIRECT_BUFFER(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_DRAW_INDX_OFFSET:
    break;
  case Xe::XGPU::PM4_UNK_39:
    break;
  case Xe::XGPU::PM4_COND_INDIRECT_BUFFER_PFE:
    break;
  case Xe::XGPU::PM4_INVALIDATE_STATE:
    break;
  case Xe::XGPU::PM4_WAIT_REG_MEM:
    break;
  case Xe::XGPU::PM4_MEM_WRITE:
    break;
  case Xe::XGPU::PM4_REG_TO_MEM:
    break;
  case Xe::XGPU::PM4_INDIRECT_BUFFER:
    result = ExecutePacketType3_INDIRECT_BUFFER(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_EXEC_CS_INDIRECT:
    break;
  case Xe::XGPU::PM4_MEM_TO_REG:
    break;
  case Xe::XGPU::PM4_SET_DRAW_STATE:
    break;
  case Xe::XGPU::PM4_COND_EXEC:
    break;
  case Xe::XGPU::PM4_COND_WRITE:
    break;
  case Xe::XGPU::PM4_EVENT_WRITE:
    break;
  case Xe::XGPU::PM4_COND_REG_EXEC:
    break;
  case Xe::XGPU::PM4_ME_INIT:
    result = ExecutePacketType3_ME_INIT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_SHADER_BASES:
    break;
  case Xe::XGPU::PM4_SET_BIN_BASE_OFFSET:
    break;
  case Xe::XGPU::PM4_SET_BIN:
    break;
  case Xe::XGPU::PM4_SCRATCH_TO_REG:
    break;
  case Xe::XGPU::PM4_UNKNOWN_4E:
    break;
  case Xe::XGPU::PM4_MEM_WRITE_CNTR:
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK:
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT:
    break;
  case Xe::XGPU::PM4_WAIT_REG_EQ:
    break;
  case Xe::XGPU::PM4_WAIT_REG_GTE:
    break;
  case Xe::XGPU::PM4_INTERRUPT:
    break;
  case Xe::XGPU::PM4_SET_CONSTANT2:
    break;
  case Xe::XGPU::PM4_SET_SHADER_CONSTANTS:
    break;
  case Xe::XGPU::PM4_EVENT_WRITE_SHD:
    break;
  case Xe::XGPU::PM4_EVENT_WRITE_CFL:
    break;
  case Xe::XGPU::PM4_EVENT_WRITE_EXT:
    break;
  case Xe::XGPU::PM4_EVENT_WRITE_ZPD:
    break;
  case Xe::XGPU::PM4_WAIT_UNTIL_READ:
    break;
  case Xe::XGPU::PM4_WAIT_IB_PFD_COMPLETE:
    break;
  case Xe::XGPU::PM4_CONTEXT_UPDATE:
    break;
  case Xe::XGPU::PM4_SET_PROTECTED_MODE:
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK_LO:
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK_HI:
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT_LO:
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT_HI:
    break;
  case Xe::XGPU::PM4_SET_VISIBILITY_OVERRIDE:
    break;
  case Xe::XGPU::PM4_SET_SECURE_MODE:
    break;
  case Xe::XGPU::PM4_PREEMPT_ENABLE_GLOBAL:
    break;
  case Xe::XGPU::PM4_PREEMPT_ENABLE_LOCAL:
    break;
  case Xe::XGPU::PM4_CONTEXT_SWITCH_YIELD:
    break;
  case Xe::XGPU::PM4_SET_RENDER_MODE:
    break;
  case Xe::XGPU::PM4_COMPUTE_CHECKPOINT:
    break;
  case Xe::XGPU::PM4_TEST_TWO_MEMS:
    break;
  case Xe::XGPU::PM4_MEM_TO_MEM:
    break;
  case Xe::XGPU::PM4_WIDE_REG_WRITE:
    break;
  case Xe::XGPU::PM4_REG_WR_NO_CTXT:
    break;
  default:
    break;
  }

  return result;
}

bool CommandProcessor::ExecutePacketType3_NOP(RingBuffer* ringBuffer, u32 packetData, u32 dataCount) {
  // Skips N 32-bit words to get to the next packet.
  ringBuffer->AdvanceRead(dataCount * sizeof(u32));
  return true;
}

bool CommandProcessor::ExecutePacketType3_ME_INIT(RingBuffer* ringBuffer, u32 packetData, u32 dataCount) {
  // Initializes Command Processor's ME.
  cpME_PM4_ME_INIT_Data.clear();
  for (size_t i = 0; i < dataCount; i++) {
    cpME_PM4_ME_INIT_Data.push_back(ringBuffer->ReadAndSwap<u32>());
  }
  return true;
}

bool CommandProcessor::ExecutePacketType3_INDIRECT_BUFFER(RingBuffer* ringBuffer, u32 packetData, u32 dataCount) {
  // Indirect buffer.
  const u32 bufferPtr = ringBuffer->ReadAndSwap<u32>();
  // Get the list pointer.
  u32 bufferSize = ringBuffer->ReadAndSwap<u32>();
  bufferSize &= 0xFFFFF;

  // Execute indirect buffer.
  LOG_TRACE(Xenos, "CP[IndirectBuffer]: Executing indirect buffer at address {:#x}, size {:#x}", bufferPtr, bufferSize);
  cpExecuteIndirectBuffer(bufferPtr, bufferSize);
  return true;
}

} // namespace Xe::XGPU