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
#include "ShaderConstants.h"
#include "Microcode/ASTBlock.h"
#include "Microcode/ASTNodeWriter.h"

#include "Base/CRCHash.h"
#include "Base/Thread.h"

#include "Render/Abstractions/Renderer.h"

namespace Xe::XGPU {
  
CommandProcessor::CommandProcessor(RAM *ramPtr, XenosState *statePtr, Render::Renderer *renderer, PCIBridge *pciBridge) :
  ram(ramPtr),
  state(statePtr), render(renderer),
  parentBus(pciBridge) {
  cpWorkerThread = std::thread(&CommandProcessor::cpWorkerThreadLoop, this);

  // Initialize uCode buffers.
  // According to free60/libxenon, these are the correct uCode sizes.
  cpMEuCodeData.resize(0x900);
  cpPFPuCodeData.resize(0x120);
}

CommandProcessor::~CommandProcessor() {
  cpWorkerThreadRunning = false;
  if (cpWorkerThread.joinable()) {
    cpWorkerThread.join();
  }
}

void CommandProcessor::CPWriteMicrocodeData(eCPMicrocodeType uCodeType, u32 data) {
  switch (uCodeType) {
  case Xe::XGPU::uCodeTypeME:
    // Sanity Check
    if (cpMEuCodeWriteAdddress <= cpMEuCodeData.size()) {
      // Write elements at specified offset.
      cpMEuCodeData[cpMEuCodeWriteAdddress] = data;
      cpMEuCodeWriteAdddress++;
    } else {
      LOG_ERROR(Xenos, "[CP] ME uCode Address bigger than uCode buffer, addr = {:#x}.", cpMEuCodeWriteAdddress);
    }
    break;
  case Xe::XGPU::uCodeTypePFP:
    // Sanity Check
    if (cpPFPuCodeAdddress <= cpMEuCodeData.size()) {
      // Write elements at specified offset.
      cpPFPuCodeData[cpPFPuCodeAdddress] = data;
      cpPFPuCodeAdddress++;
    } else {
      LOG_ERROR(Xenos, "[CP] PFP uCode Address bigger than uCode buffer, addr = {:#x}.", cpPFPuCodeAdddress);
    }
    break;
  }
}

u32 CommandProcessor::CPReadMicrocodeData(eCPMicrocodeType uCodeType) {
  u32 tmp = 0;
  switch (uCodeType) {
  case Xe::XGPU::uCodeTypeME:
    // Sanity check.
    if (cpMEuCodeReadAdddress <= cpMEuCodeData.size()) {
      tmp = byteswap_be(cpMEuCodeData[cpMEuCodeReadAdddress]); // Data was byteswapped, so we need to reverse that.
      cpMEuCodeReadAdddress++;
    }
    break;
  case Xe::XGPU::uCodeTypePFP:
    // Sanity check.
    if (cpPFPuCodeAdddress <= cpPFPuCodeData.size()) {
      tmp = byteswap_be(cpPFPuCodeData[cpPFPuCodeAdddress]); // Data was byteswapped, so we need to reverse that.
      cpPFPuCodeAdddress++;
    }
    break;
  }
  return tmp;
}

void CommandProcessor::CPUpdateRBBase(u32 address) {
  if (!address)
    return;

  state->WriteRegister(XeRegister::CP_RB_BASE, address);

  cpRingBufferBasePtr = ram->getPointerToAddress(address);
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Base Address: 0x{:X}", address);
  
  // Reset CP Read Ptr Idx.
  cpReadPtrIndex = 0;
}

void CommandProcessor::CPUpdateRBSize(size_t newSize) {
  if (!(newSize & CP_RB_CNTL_RB_BUFSZ_MASK))
    return;

  state->WriteRegister(XeRegister::CP_RB_CNTL, newSize);

  cpRingBufferSize = static_cast<size_t>(1u << (newSize + 3));
  LOG_DEBUG(Xenos, "CP: Updating RingBuffer Size: 0x{:X}", cpRingBufferSize.load());
}

void CommandProcessor::CPUpdateRBWritePointer(u32 offset) {
  state->WriteRegister(XeRegister::CP_RB_WPTR, offset);
  cpWritePtrIndex = offset;
}

void CommandProcessor::cpWorkerThreadLoop() {
  Base::SetCurrentThreadName("[Xe] Command Processor");
  while (cpWorkerThreadRunning) {
    u32 writePtrIndex = cpWritePtrIndex.load();
    while (cpWorkerThreadRunning && (cpRingBufferBasePtr == nullptr || cpReadPtrIndex == writePtrIndex)) {
      // Stall until we're told otherwise
      std::this_thread::sleep_for(10ns);
      writePtrIndex = cpWritePtrIndex.load();
    }

    // Shutdown if we were told to
    if (!cpWorkerThreadRunning)
      break;

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
      // TODO(bitsh1ft3r): Check whether this should be a fatal crash.
      LOG_ERROR(Xenos, "CP[PrimaryBuffer]: Failed to execute a packet.");
      assert(true);
      break;
    }
  } while (cpRingBufer.readCount());

  return writeIndex; // Set Read and Write index equal, signaling buffer processed.
}

void CommandProcessor::cpExecuteIndirectBuffer(u32 bufferPtr, u32 bufferSize) {
  // Create the ring buffer instance for the indirect buffer.
  RingBuffer ringBufer(ram->getPointerToAddress(bufferPtr), bufferSize * sizeof(u32));

  // Set write offset.
  ringBufer.setWriteOffset(bufferSize * sizeof(u32));

  do {
    if (!ExecutePacket(&ringBufer)) {
      // TODO(bitsh1ft3r): Check whether this should be a fatal crash.
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
  
  // TODO: Check if we should actually return if it's a nul-packet
  if (!packetData) {
    LOG_WARNING(Xenos, "CP[PrimaryBuffer]: found packet with zero data!");
    return true;
  }
  
  if (packetData == 0xCDCDCDCD) {
    LOG_WARNING(Xenos, "CP[PrimaryBuffer]: found packet with uninitialized data!");
    return true;
  }

  LOG_DEBUG(Xenos, "Executing packet type {} (0x{:X})", static_cast<u32>(packetType), packetData);

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
    LOG_ERROR(Xenos, "CP[ExecutePacketType0]: Data overflow, read count 0x{:X}, registers count 0x{:X})",
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
    LOG_TRACE(Xenos, "CP[ExecutePacketType0]: Writing register at index 0x{:X}, data 0x{:X}", targetRegIndex, registerData);
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
  LOG_TRACE(Xenos, "CP[ExecutePacketType1]: Writing register at index 0x{:X}, data 0x{:X}", regIndex0, reg0Data);
  LOG_TRACE(Xenos, "CP[ExecutePacketType1]: Writing register at index 0x{:X}, data 0x{:X}", regIndex1, reg1Data);
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
    LOG_ERROR(Xenos, "CP[ExecutePacketType3]: Data overflow, read count 0x{:X}, registers count 0x{:X})",
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

  LOG_TRACE(Xenos, "CP[ExecutePacketType3]: Executing OpCode 0x{:X}", static_cast<u32>(currentOpCode));

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
    result = ExecutePacketType3_REG_RMW(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_DRAW_INDX:
    result = ExecutePacketType3_DRAW_INDX(ringBuffer, packetData, dataCount);
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
    result = ExecutePacketType3_IM_LOAD(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_DRAW_INDIRECT:
    break;
  case Xe::XGPU::PM4_DRAW_INDX_INDIRECT:
    break;
  case Xe::XGPU::PM4_IM_LOAD_IMMEDIATE:
    result = ExecutePacketType3_IM_LOAD_IMMEDIATE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_IM_STORE:
    break;
  case Xe::XGPU::PM4_SET_CONSTANT:
    result = ExecutePacketType3_SET_CONSTANT(ringBuffer, packetData, dataCount);
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
    result = ExecutePacketType3_DRAW_INDX_2(ringBuffer, packetData, dataCount);
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
    result = ExecutePacketType3_INVALIDATE_STATE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_WAIT_REG_MEM:
    result = ExecutePacketType3_WAIT_REG_MEM(ringBuffer, packetData, dataCount);
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
    result = ExecutePacketType3_COND_WRITE(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_EVENT_WRITE:
    result = ExecutePacketType3_EVENT_WRITE(ringBuffer, packetData, dataCount);
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
    result = ExecutePacketType3_SET_BIN_MASK(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT:
    result = ExecutePacketType3_SET_BIN_SELECT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_WAIT_REG_EQ:
    break;
  case Xe::XGPU::PM4_WAIT_REG_GTE:
    break;
  case Xe::XGPU::PM4_INTERRUPT:
    result = ExecutePacketType3_INTERRUPT(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_CONSTANT2:
    result = ExecutePacketType3_SET_CONSTANT2(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_SHADER_CONSTANTS:
    result = ExecutePacketType3_SET_SHADER_CONSTANTS(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_EVENT_WRITE_SHD:
    result = ExecutePacketType3_EVENT_WRITE_SHD(ringBuffer, packetData, dataCount);
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
    result = ExecutePacketType3_SET_BIN_MASK_LO(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_MASK_HI:
    result = ExecutePacketType3_SET_BIN_MASK_HI(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT_LO:
    result = ExecutePacketType3_SET_BIN_SELECT_LO(ringBuffer, packetData, dataCount);
    break;
  case Xe::XGPU::PM4_SET_BIN_SELECT_HI:
    result = ExecutePacketType3_SET_BIN_SELECT_HI(ringBuffer, packetData, dataCount);
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

bool CommandProcessor::ExecutePacketType3_NOP(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Skips N 32-bit words to get to the next packet.
  ringBuffer->AdvanceRead(dataCount * sizeof(u32));
  return true;
}

bool CommandProcessor::ExecutePacketType3_REG_RMW(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 rmwSetup = ringBuffer->ReadAndSwap<u32>();
  const u32 andMask = ringBuffer->ReadAndSwap<u32>();
  const u32 orMask = ringBuffer->ReadAndSwap<u32>();

  const u32 regAddr = (rmwSetup & 0x1FFF);
  u32 value = state->ReadRawRegister(regAddr);
  const u32 oldValue = value;

  // OR value (with reg or immediate value)
  if ((rmwSetup >> 30) & 0x1) {
    // | reg
    const u32 orAddr = (orMask & 0x1FFF);
    const u32 orValue = state->ReadRawRegister(orAddr);
    value |= orValue;
  } else {
    // | imm
    value |= orMask;
  }

  // AND value (with reg or immediate value)
  if ((rmwSetup >> 30) & 0x1) {
    // & reg
    const u32 andAddr = (andMask & 0x1FFF);
    const u32 andValue = state->ReadRawRegister(andAddr);
    value ^= andValue;
  } else {
    // & imm  
    value &= andMask;
  }

  // Wrrite the value back
  state->WriteRawRegister(regAddr, value);
  return true;
}

bool CommandProcessor::ExecutePacketType3_EVENT_WRITE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Generates an event that creates a write to memory when completed
  const u32 initiator = ringBuffer->ReadAndSwap<u32>();

  // Writeback
  state->WriteRegister(XeRegister::VGT_EVENT_INITIATOR, initiator & 0x3F);

  if (dataCount == 1) {
    // Unknown what should be done here
  } else {
    LOG_ERROR(Xenos, "CP[EP3] | EVENT_WRITE: Invalid type!");
    ringBuffer->AdvanceRead(dataCount - 1);
  }

  return true;
}


bool CommandProcessor::ExecutePacketType3_COND_WRITE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Determines how long to wait for, and what to wait for
  const u32 waitInfo = ringBuffer->ReadAndSwap<u32>();
  const XeRegister pollReg = static_cast<XeRegister>(ringBuffer->ReadAndSwap<u32>());
  const u32 ref = ringBuffer->ReadAndSwap<u32>();
  const u32 mask = ringBuffer->ReadAndSwap<u32>();
  // Write data
  const XeRegister writeReg = static_cast<XeRegister>(ringBuffer->ReadAndSwap<u32>());
  const u32 writeData = ringBuffer->ReadAndSwap<u32>();

  u32 value = 0;
  if (waitInfo & 0x10) {
    u8 *addrPtr = ram->getPointerToAddress(static_cast<u32>(pollReg));
    memcpy(&value, addrPtr, sizeof(value));
  } else {
    value = state->ReadRegister(pollReg);
  }
  bool matched = false;
  switch (waitInfo & 0x7) {
  case 0: // Never
    matched = false;
    break;
  case 1: // Less than reference
    matched = (value & mask) < ref;
    break;
  case 2: // Less than or equal to reference
    matched = (value & mask) <= ref;
    break;
  case 3: // Equal to reference
    matched = (value & mask) == ref;
    break;
  case 4: // Not equal to reference
    matched = (value & mask) != ref;
    break;
  case 5: // Greater than or equal to reference
    matched = (value & mask) >= ref;
    break;
  case 6: // Greater than reference
    matched = (value & mask) > ref;
    break;
  case 7: // Always
    matched = true;
    break;
  }

  if (matched) {
    if (waitInfo & 0x100) {
      u8 *addrPtr = ram->getPointerToAddress(static_cast<u32>(writeReg));
      memcpy(addrPtr, &writeData, sizeof(writeData));
    } else {
      state->WriteRegister(writeReg, writeData);
    }
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_INVALIDATE_STATE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 stateMask = ringBuffer->ReadAndSwap<u32>();
  return true;
}

bool CommandProcessor::ExecutePacketType3_ME_INIT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Initializes Command Processor's ME.
  cpME_PM4_ME_INIT_Data.clear();
  for (size_t i = 0; i < dataCount; i++) {
    cpME_PM4_ME_INIT_Data.push_back(ringBuffer->ReadAndSwap<u32>());
  }
  return true;
}

class GlobalInstructionExtractor : public Microcode::AST::ExpressionNode::Visitor {
public:
  virtual void OnExprStart(Microcode::AST::ExpressionNode::Ptr n) override final {
    if (n->GetType() == Microcode::AST::eExprType::VFETCH) {
      vfetch.push_back(static_cast<Microcode::AST::VertexFetch::Ptr>(n));
    }
    else if (n->GetType() == Microcode::AST::eExprType::TFETCH) {
      tfetch.push_back(static_cast<Microcode::AST::TextureFetch::Ptr>(n));
    }
    else if (n->GetType() == Microcode::AST::eExprType::EXPORT) {
      exports.push_back(static_cast<Microcode::AST::WriteExportRegister::Ptr>(n));
    }
    else {
      const s32 regIndex = n->GetRegisterIndex();
      if (regIndex != -1) {
        usedRegisters.insert((u32)regIndex);
      }
    }
  }

  virtual void OnExprEnd(Microcode::AST::ExpressionNode::Ptr n) override final
  {}

  std::vector<Microcode::AST::VertexFetch::Ptr> vfetch;
  std::vector<Microcode::AST::TextureFetch::Ptr> tfetch;
  std::vector<Microcode::AST::WriteExportRegister::Ptr> exports;
  std::set<u32> usedRegisters;
};

class AllExpressionVisitor : public Microcode::AST::StatementNode::Visitor {
public:
  AllExpressionVisitor(Microcode::AST::ExpressionNode::Visitor *vistor)
    : exprVisitor(vistor)
  {}

  virtual void OnWrite(Microcode::AST::ExpressionNode::Ptr dest, Microcode::AST::ExpressionNode::Ptr src, std::array<eSwizzle, 4> mask) override final {
    dest->Visit(*exprVisitor);
    src->Visit(*exprVisitor);
  }
  
  virtual void OnConditionPush(Microcode::AST::ExpressionNode::Ptr condition) override final {
    condition->Visit(*exprVisitor);
  }

  virtual void OnConditionPop() override final
  {}

private:
  Microcode::AST::ExpressionNode::Visitor *exprVisitor;
};

void VisitAll(const Microcode::AST::Block *b, Microcode::AST::StatementNode::Visitor &v) {
  if (b->GetCondition())
    v.OnConditionPush(b->GetCondition()->shared_from_this());

  if (b->GetCode())
    b->GetCode()->Visit(v);

  if (b->GetCondition())
    v.OnConditionPop();
}

void VisitAll(const Microcode::AST::ControlFlowGraph *cf, Microcode::AST::StatementNode::Visitor &v) {
  for (u32 i = 0; i < cf->GetNumBlocks(); ++i) {
    const Microcode::AST::Block *b = cf->GetBlock(i);
    VisitAll(b, v);
  }
}

std::pair<u32, std::vector<u32>> LoadShader(eShaderType shaderType, const std::vector<u32> &data) {
  u32 crc = CRC32::CRC32::calc(reinterpret_cast<const u8*>(data.data()), data.size() * 4);
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "cache" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  std::string baseString = fmt::format("{}_shader_{:X}", typeString, crc);
  fs::path path{ shaderPath / (baseString + ".spv") };
  std::vector<u32> code{};
  {
    std::ifstream file{ path, std::ios::in | std::ios::binary };
    std::error_code error;
    if (fs::exists(path, error) && file.is_open()) {
      u64 fileSize = fs::file_size(path);
      code.resize(fileSize / 4);
      file.read(reinterpret_cast<char*>(code.data()), fileSize);
      file.close();
      return { crc, code };
    }
    file.close();
  }
  Microcode::AST::ShaderCodeWriterSirit writer{ shaderType };
  Microcode::AST::ControlFlowGraph *cf = Microcode::AST::ControlFlowGraph::DecompileMicroCode(reinterpret_cast<const u8*>(data.data()), data.size() * 4, shaderType);
  if (cf) {
    GlobalInstructionExtractor instructionExtractor;
    AllExpressionVisitor vistor(&instructionExtractor);
    VisitAll(cf, vistor);
  }
  writer.BeginMain();
  writer.EndMain();
  code = writer.module.Assemble();
  std::ofstream f{ shaderPath / (baseString + ".spv"), std::ios::out | std::ios::binary };
  f.write(reinterpret_cast<char*>(code.data()), code.size() * 4);
  f.close();
  return { crc, code };
}

bool CommandProcessor::ExecutePacketType3_IM_LOAD(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Load sequencer instruction memory (pointer-based)
  const u32 addrType = ringBuffer->ReadAndSwap<u32>();
  const eShaderType shaderType = static_cast<eShaderType>(addrType & 0x3);
  const u32 addr = addrType & ~0x3;
  const u32 startSize = ringBuffer->ReadAndSwap<u32>();
  const u32 start = startSize >> 16;
  const u64 size = (startSize & 0xFFFF) * 4;
  u8 *addrPtr = ram->getPointerToAddress(addr);

  std::vector<u32> data{};
  for (u64 i = 0; i != size; ++i) {
    u32 value = 0;
    memcpy(&value, &addrPtr[i], sizeof(value));
    data.push_back(value);
  }

  std::pair<u32, std::vector<u32>> shader = LoadShader(shaderType, data);
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "cache" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  std::string baseString = fmt::format("{}_shader_{:X}", typeString, shader.first);
  {
    std::ofstream f{ shaderPath / (baseString + ".bin"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char*>(data.data()), data.size() * 4);
    f.close();
  }

  switch (shaderType) {
  case eShaderType::Pixel:{
    LOG_DEBUG(Xenos, "[CP::IM_LOAD] PixelShader CRC: 0x{:08X}", shader.first);
  } break;
  case eShaderType::Vertex:{
    LOG_DEBUG(Xenos, "[CP::IM_LOAD] VertexShader CRC: 0x{:08X}", shader.first);
  } break;
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_IM_LOAD_IMMEDIATE(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Load sequencer instruction memory (pointer-based)
  const u32 shaderTypeValue = ringBuffer->ReadAndSwap<u32>();
  const eShaderType shaderType = static_cast<eShaderType>(shaderTypeValue);
  const u32 startSize = ringBuffer->ReadAndSwap<u32>();
  const u32 start = startSize >> 16;
  const u64 sizeDwords = (startSize & 0xFFFF);
  
  std::vector<u32> data{};
  data.resize(sizeDwords);
  ringBuffer->Read(data.data(), data.size());
  for (u32 &value : data) {
    value = byteswap_be(value);
  }

  std::pair<u32, std::vector<u32>> shader = LoadShader(shaderType, data);
  fs::path shaderPath{ Base::FS::GetUserPath(Base::FS::PathType::ShaderDir) / "cache" };
  std::string typeString = shaderType == Xe::eShaderType::Pixel ? "pixel" : "vertex";
  std::string baseString = fmt::format("{}_shader_{:X}", typeString, shader.first);
  {
    std::ofstream f{ shaderPath / (baseString + ".bin"), std::ios::out | std::ios::binary };
    f.write(reinterpret_cast<char*>(data.data()), data.size() * 4);
    f.close();
  }
  switch (shaderType) {
  case eShaderType::Pixel:{
    LOG_DEBUG(Xenos, "[CP::IM_LOAD_IMMEDIATE] PixelShader CRC: 0x{:08X}", shader.first);
  } break;
  case eShaderType::Vertex:{
    LOG_DEBUG(Xenos, "[CP::IM_LOAD_IMMEDIATE] VertexShader CRC: 0x{:08X}", shader.first);
  } break;
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_CONSTANT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Get base index
  const u32 offsetType = ringBuffer->ReadAndSwap<u32>();
  // PM4_REG(reg) ((0x4 << 16) | (GSL_HAL_SUBBLOCK_OFFSET(reg)))
  const u32 index = offsetType & 0xFFFF;
  const u32 type = (offsetType >> 16) & 0xFF;

  u32 baseIndex = index;
  switch (type) {
  // ALU
  case 0: baseIndex += 0x4000; break;
  // FETCH
  case 1: baseIndex += 0x4800; break;
  // BOOL
  case 2: baseIndex += 0x4900; break;
  // LOOP
  case 3: baseIndex += 0x4908; break;
  // REGISTER_RAWS
  case 4: baseIndex += 0x2000; break;
  default: ringBuffer->AdvanceRead(dataCount - 1); return true; break;
  }

  // Write constants
  for (u32 n = 0; n != dataCount - 1; ++n) {
    const u32 data = ringBuffer->ReadAndSwap<u32>();
    state->WriteRegister(static_cast<XeRegister>(index + n), data);
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_INDIRECT_BUFFER(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Indirect buffer.
  const u32 bufferPtr = ringBuffer->ReadAndSwap<u32>();
  // Get the list pointer.
  u32 bufferSize = ringBuffer->ReadAndSwap<u32>();
  bufferSize &= 0xFFFFF;

  // Execute indirect buffer.
  LOG_TRACE(Xenos, "CP[IndirectBuffer]: Executing indirect buffer at address 0x{:X}, size 0x{:X}", bufferPtr, bufferSize);
  cpExecuteIndirectBuffer(bufferPtr, bufferSize);
  return true;
}

bool CommandProcessor::ExecutePacketType3_INTERRUPT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // CPU(s) to interrupt
  const u32 cpuMask = ringBuffer->ReadAndSwap<u32>();
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_CONSTANT2(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Get base index
  const u32 offsetType = ringBuffer->ReadAndSwap<u32>();
  const u32 index = offsetType & 0xFFFF;

  // Write constants
  for (u32 n = 0; n != dataCount - 1; ++n) {
    const u32 data = ringBuffer->ReadAndSwap<u32>();
    state->WriteRegister(static_cast<XeRegister>(index + n), data);
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_SHADER_CONSTANTS(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Get base index
  const u32 offsetType = ringBuffer->ReadAndSwap<u32>();
  const u32 index = offsetType & 0xFFFF;

  // Write constants
  for (u32 n = 0; n != dataCount - 1; ++n) {
    const u32 data = ringBuffer->ReadAndSwap<u32>();
    state->WriteRegister(static_cast<XeRegister>(index + n), data);
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_EVENT_WRITE_SHD(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Generates a VS|PS_done event
  const u32 initiator = ringBuffer->ReadAndSwap<u32>();
  const u32 address = ringBuffer->ReadAndSwap<u32>();
  const u32 value = ringBuffer->ReadAndSwap<u32>();

  // Writeback
  state->WriteRegister(XeRegister::VGT_EVENT_INITIATOR, initiator & 0x3F);

  u32 writeValue = 0;
  if ((initiator >> 31) & 0x1) {
    writeValue = swapCount;
  } else {
    writeValue = value;
  }

  u8 *addrPtr = ram->getPointerToAddress(address);
  memcpy(addrPtr, &writeValue, sizeof(writeValue));

  return true;
}

bool CommandProcessor::ExecutePacketType3_WAIT_REG_MEM(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  // Determines how long to wait for, and what to wait for
  const u32 waitInfo = ringBuffer->ReadAndSwap<u32>();
  const XeRegister pollReg = static_cast<XeRegister>(ringBuffer->ReadAndSwap<u32>());
  const u32 ref = ringBuffer->ReadAndSwap<u32>();
  const u32 mask = ringBuffer->ReadAndSwap<u32>();
  // Time to live
  const u32 wait = ringBuffer->ReadAndSwap<u32>();

  bool matched = false;
  do {
    u32 value = 0;
    if (waitInfo & 0x10) {
      u32 addr = static_cast<u32>(pollReg);
      u8 *addrPtr = ram->getPointerToAddress(addr);
      memcpy(&value, addrPtr, sizeof(value));
    } else {
      value = state->ReadRegister(pollReg);
      if (pollReg == XeRegister::COHER_STATUS_HOST) {
        const u32 statusHost = state->ReadRegister(XeRegister::COHER_STATUS_HOST);
        const u32 baseHost = state->ReadRegister(XeRegister::COHER_BASE_HOST);
        const u32 sizeHost = state->ReadRegister(XeRegister::COHER_SIZE_HOST);
        if ((statusHost & 0x80000000ul)) {
          const u32 newStatusHost = statusHost & ~0x80000000ul;
          state->WriteRegister(XeRegister::COHER_STATUS_HOST, newStatusHost);
          // TODO: Dirty Register & clear cache
        }
        value = state->ReadRegister(pollReg);
      }
    }
    switch (waitInfo & 0x7) {
    case 0: // Never
      matched = false;
      break;
    case 1: // Less than reference
      matched = (value & mask) < ref;
      break;
    case 2: // Less than or equal to reference
      matched = (value & mask) <= ref;
      break;
    case 3: // Equal to reference
      matched = (value & mask) == ref;
      break;
    case 4: // Not equal to reference
      matched = (value & mask) != ref;
      break;
    case 5: // Greater than or equal to reference
      matched = (value & mask) >= ref;
      break;
    case 6: // Greater than reference
      matched = (value & mask) > ref;
      break;
    case 7: // Always
      matched = true;
      break;
    }

    if (!matched) {
      if (wait >= 0x100) {
        // Wait
        std::this_thread::sleep_for(std::chrono::milliseconds(wait / 0x100));
      } else {
        // Yield
        std::this_thread::yield();
      }
    }
  } while (!matched);
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_MASK(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const uint64_t maskHigh = ringBuffer->ReadAndSwap<u32>();
  const uint64_t maskLow = ringBuffer->ReadAndSwap<u32>();
  binMask = (maskHigh << 32) | maskLow;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_SELECT(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const uint64_t selectHigh = ringBuffer->ReadAndSwap<u32>();
  const uint64_t selectlow = ringBuffer->ReadAndSwap<u32>();
  binSelect = (selectHigh << 32) | selectlow;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_MASK_LO(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 loMask = ringBuffer->ReadAndSwap<u32>();
  binMask = (binMask & 0xFFFFFFFF00000000ull) | loMask;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_MASK_HI(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 hiMask = ringBuffer->ReadAndSwap<u32>();
  binMask = (binMask & 0xFFFFFFFFull) | (static_cast<u64>(hiMask) << 32);
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_SELECT_LO(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 loSelect = ringBuffer->ReadAndSwap<u32>();
  binSelect = (binSelect & 0xFFFFFFFF00000000ull) | loSelect;
  return true;
}

bool CommandProcessor::ExecutePacketType3_SET_BIN_SELECT_HI(RingBuffer *ringBuffer, u32 packetData, u32 dataCount) {
  const u32 hiSelect = ringBuffer->ReadAndSwap<u32>();
  binSelect = (binSelect & 0xFFFFFFFFull) | (static_cast<u64>(hiSelect) << 32);
  return true;
}

bool CommandProcessor::ExecutePacketType3_DRAW(RingBuffer* ringBuffer, u32 packetData, u32 dataCount, u32 vizQueryCondition, const char* opCodeName)
{
  // Sanity check.
  if (!dataCount) {
    LOG_ERROR(Xenos, "[CP, PT3]: Packet too small, can't read VGT_DRAW_INITIATOR, OpCode {}.", opCodeName);
    return false;
  }

  // Get our VGT register data.
  VGT_DRAW_INITIATOR_REG vgtDrawInitiator;
  vgtDrawInitiator.hexValue = ringBuffer->ReadAndSwap<u32>();
  dataCount--;

  // Write the VGT_DRAW_INITIATOR register.
  state->WriteRegister(XeRegister::VGT_DRAW_INITIATOR, vgtDrawInitiator.hexValue);

  // TODO(Vali004): Do draw on backend

  bool isIndexedDraw = false;
  bool drawOk = true;

  // Our Index Buffer info for indexed draws.
  XeIndexBufferInfo indexBufferInfo;

  switch (vgtDrawInitiator.sourceSelect) {
  case eSourceSelect::xeDMA:
  {
    // Indexed draw.
    isIndexedDraw = true;

    // Read VGT_DMA_BASE from data.
    // Sanity check.
    if (!dataCount) {
      LOG_ERROR(Xenos, "[CP, PT3]: DRAW failed, not enough data for VGT_DMA_BASE.");
      return false; // Failed.
    }

    u32 vgtDMABase = ringBuffer->ReadAndSwap<u32>();
    dataCount--;

    // Write the VGT_DMA_BASE register.
    state->WriteRegister(XeRegister::VGT_DMA_BASE, vgtDMABase);

    // Get our VGT_DMA_SIZE reg data.
    VGT_DMA_SIZE_REG vgtDMASize;

    // Sanity check, again.
    if (!dataCount) {
      LOG_ERROR(Xenos, "[CP, PT3]: DRAW failed, not enough data for VGT_DMA_SIZE.");
      return false; // Failed.
    }

    vgtDMASize.hexValue = ringBuffer->ReadAndSwap<u32>();
    dataCount--;

    // Write the VGT_DMA_SIZE register.
    state->WriteRegister(XeRegister::VGT_DMA_SIZE, vgtDMASize.hexValue);

    // Get our index size from VGT_DRAW_INITIATOR size in bytes.
    u32 indexSizeInBytes = vgtDrawInitiator.indexSize == eIndexFormat::xeInt16 ? sizeof(u16) : sizeof(u32);

    // The base address must already be word-aligned according to the R6xx documentation.
    indexBufferInfo.guestBase = vgtDMABase & ~(indexSizeInBytes - 1);
    indexBufferInfo.endianness = vgtDMASize.swapMode;
    indexBufferInfo.indexFormat = vgtDrawInitiator.indexSize;
    indexBufferInfo.length = vgtDMASize.numWords * indexSizeInBytes;
    indexBufferInfo.count = vgtDrawInitiator.numIndices;
  }
    break;
  case eSourceSelect::xeImmediate:
    // TODO(bitshift3r): Do VGT_IMMED_DATA if any ocurrences are to be found.
    LOG_ERROR(Xenos, "[CP, PT3]{}: Is using immediate vertex indices, which are currently unsupported. "
      "Please submit an issue in Xenon github.", opCodeName);
    drawOk = false;
    break;
  case eSourceSelect::xeAutoIndex:
    // Auto draw.
    indexBufferInfo.guestBase = 0;
    indexBufferInfo.length = 0;
    break;
  default:
    // Unhandled or invalid source selection.
    drawOk = false;
    LOG_ERROR(Xenos, "[CP, PT3]: DRAW failed, invalid source select.");
    break;
  }

  // Skip to the next command, if there are immediate indexes that we don't support yet.
  ringBuffer->AdvanceRead(dataCount * sizeof(u32));

  if (drawOk) {
    if (isIndexedDraw) {
      render->DrawIndexed(indexBufferInfo);
    } else {
      render->Draw();
    }
  }

  return true;
}

bool CommandProcessor::ExecutePacketType3_DRAW_INDX(RingBuffer* ringBuffer, u32 packetData, u32 dataCount) {
  // Initiate fetch of index buffer and draw.
  // Generally used by Xbox 360 Direct3D 9 for kDMA and kAutoIndex sources.
  // With a VIZ Query token as the first one.
  
  u32 dataCountRemaining = dataCount;

  if (!dataCountRemaining) {
    LOG_ERROR(Xenos, "[CP,PT3]: PM4_DRAW_INDX: Packet too small, can't read the VIZ Query token.");
    return false;
  }

  // Get our VIZ Query Conditon.
  u32 vizQueryCondition = ringBuffer->ReadAndSwap<u32>();
  --dataCountRemaining;

  return ExecutePacketType3_DRAW(ringBuffer, packetData, dataCountRemaining, vizQueryCondition, "PM4_DRAW_INDX");
}

bool CommandProcessor::ExecutePacketType3_DRAW_INDX_2(RingBuffer* ringBuffer, u32 packetData, u32 dataCount) {
  // Draw using supplied indices in packet.
  // Generally used by Xbox 360 Direct3D 9 for kAutoIndex source.
  // No VIZ query token.

  return ExecutePacketType3_DRAW(ringBuffer, packetData, dataCount, 0, "PM4_DRAW_INDX_2");
}

} // namespace Xe::XGPU
